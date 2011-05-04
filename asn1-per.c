/**************************************************************************/
/*                                                                        */
/*  Copyright (C) 2004-2011 INTERSEC SAS                                  */
/*                                                                        */
/*  Should you receive a copy of this source code, you must check you     */
/*  have a proper, written authorization of INTERSEC to hold it. If you   */
/*  don't have such an authorization, you must DELETE all source code     */
/*  files in your possession, and inform INTERSEC of the fact you obtain  */
/*  these files. Should you not comply to these terms, you can be         */
/*  prosecuted in the extent permitted by applicable law.                 */
/*                                                                        */
/**************************************************************************/

#include "asn1-per.h"

/* XXX Tracing policy:
 *     5: Low level writer/reader
 *     4: PER packer/unpacker
 */

static struct {
    int decode_log_level;
} asn1_per_g = {
#define _G  asn1_per_g
    .decode_log_level = -1,
};

/* Big Endian generic helpers {{{ */

static ALWAYS_INLINE void
write_i64_o(sb_t *sb, int64_t i64, uint8_t olen)
{
    be64_t be64 = cpu_to_be64(i64);

    assert (olen <= 8);
    sb_add(sb, (uint8_t *)&be64 + 8 - olen, olen);
}

static ALWAYS_INLINE void write_u8(sb_t *sb, uint8_t u8)
{
    sb_addc(sb, u8);
}

static ALWAYS_INLINE void write_u16(sb_t *sb, uint16_t u16)
{
    be16_t be16 = cpu_to_be16(u16);

    sb_add(sb, &be16, 2);
}

static ALWAYS_INLINE uint8_t __read_u8(pstream_t *ps)
{
    return __ps_getc(ps);
}

static ALWAYS_INLINE uint16_t __read_u16(pstream_t *ps)
{
    be16_t be16 = *(const be16_t *)ps->b;

    __ps_skip(ps, 2);

    return be_to_cpu16(be16);
}

static ALWAYS_INLINE int64_t __read_i64_o(pstream_t *ps, size_t olen)
{
    be64_t be64 = 0ll;

    assert (olen);

    memcpy((byte *)&be64 + 8 - olen, ps->b, olen);

    if (*ps->b & 0x80) { /* Negative number */
        memset(&be64, 0xff, 8 - olen);
    }

    __ps_skip(ps, olen);

    return be_to_cpu64(be64);
}

static ALWAYS_INLINE uint64_t __read_u64_o(pstream_t *ps, size_t olen)
{
    be64_t be64 = 0ll;

    assert (olen);

    memcpy((byte *)&be64 + 8 - olen, ps->b, olen);
    __ps_skip(ps, olen);

    return be_to_cpu64(be64);
}

/* }}} */
/* Write {{{ */

/* Helpers {{{ */

/* Fully constrained integer - d_max < 65536 */
static ALWAYS_INLINE void
aper_write_u16_m(bb_t *bb, uint16_t u16, uint16_t blen)
{
    bb_push_mark(bb);

    if (!blen)
        goto end;

    if (blen < 8) {
        bb_add_bits(bb, u16, blen);
        goto end;
    }

    bb_align(bb);

    if (blen == 8) {
        write_u8(&bb->sb, u16); /* We can: bb is aligned */
        goto end;
    }

    assert (blen <= 16);

    write_u16(&bb->sb, u16); /* We can: bb is aligned */
    /* FALLTHROUGH */

  end:
    e_trace_bb_tail(5, bb, "Constrained number (n = %u)", u16);
    bb_pop_mark(bb);
}

static ALWAYS_INLINE int
aper_write_ulen(bb_t *bb, size_t l) /* Unconstrained length */
{
    bb_push_mark(bb);

    bb_align(bb);

    e_trace_bb_tail(5, bb, "Align");
    bb_reset_mark(bb);

    if (l <= 127) {
        write_u8(&bb->sb, l); /* we can: bb is aligned */

        e_trace_bb_tail(5, bb, "Unconstrained length (l = %zd)", l);
        bb_pop_mark(bb);

        return 0;
    }

    if (l < (1 << 14)) {
        uint16_t u16  = l | (1 << 15);

        write_u16(&bb->sb, u16); /* we can: bb is aligned */

        e_trace_bb_tail(5, bb, "Unconstrained length (l = %zd)", l);
        bb_pop_mark(bb);

        return 0;
    }

    bb_pop_mark(bb);

    return e_error("ASN.1 PER encoder: fragmentation is not supported");
}

static ALWAYS_INLINE void aper_write_2c_number(bb_t *bb, int64_t v)
{
    uint8_t olen;

    olen = i64_olen(v);
    aper_write_ulen(bb, olen);
    bb_align(bb);
    write_i64_o(&bb->sb, v, olen);
}

/* XXX semi-constrained or constrained numbers */
static ALWAYS_INLINE void
aper_write_number(bb_t *bb, uint64_t v, const asn1_int_info_t *info)
{
    uint8_t olen;

    if (info && info->constrained) {
        if (info->max_blen <= 16) {
            aper_write_u16_m(bb, v, info->max_blen);
            return;
        } else {
            olen = u64_olen(v);
            aper_write_u16_m(bb, olen, info->max_olen_blen);
        }
    } else {
        olen = u64_olen(v);
        aper_write_ulen(bb, olen);
    }

    bb_align(bb);

    write_i64_o(&bb->sb, v, olen);
}

/* Normally small non-negative whole number (SIC) */
/* XXX Used for :
 *         - CHOICE index
 *         - Enumeration extensions
 *         - ???
 */
static void aper_write_nsnnwn(bb_t *bb, size_t n)
{
    if (n <= 63) {
        bb_add_bits(bb, n, 1 + 6);
        return;
    }

    bb_add_bit(bb, true);
    aper_write_number(bb, n, NULL);
}

static int
aper_write_len(bb_t *bb, size_t l, size_t l_min, size_t l_max)
{
    if (l_max != SIZE_MAX) {
        uint32_t d_max = l_max - l_min;
        uint32_t d     = l - l_min;

        assert (l <= l_max);

        if (d_max < (1 << 16)) {
            /* TODO pre-process u16_blen(d_max) */
            aper_write_u16_m(bb, d, u16_blen(d_max));
            return 0;
        }
    }

    return aper_write_ulen(bb, l);
}

/* }}} */
/* Front End Encoders {{{ */

/* Scalar types {{{ */

static int
aper_encode_len(bb_t *bb, size_t l, const asn1_cnt_info_t *info)
{
    if (info) {
        if (l < info->min || l > info->max) {
            if (info->extended) {
                if (l < info->ext_min || l > info->ext_max) {
                    return e_error("Extended constraint not respected");
                }

                /* Extension present */
                bb_add_bit(bb, true);

                if (aper_write_ulen(bb, l) < 0) {
                    return e_error("Failed to write extended length");
                }
            } else {
                return e_error("Constraint not respected");
            }
        } else {
            if (info->extended) {
                /* Extension not present */
                bb_add_bit(bb, false);
            }

            aper_write_len(bb, l, info->min, info->max);
        }
    } else {
        aper_write_len(bb, l, 0, SIZE_MAX);
    }

    return 0;
}

static int
aper_encode_number(bb_t *bb, int64_t n, const asn1_int_info_t *info)
{
    if (info) {
        if (n < info->min || n > info->max) {
            if (info->extended) {
                if (n < info->ext_min || n > info->ext_max) {
                    return e_error("Extended constraint not respected");
                }

                /* Extension present */
                bb_add_bit(bb, true);

                /* XXX Extension constraints are not PER-visible */
                aper_write_number(bb, n, NULL);

                return 0;
            } else {
                e_error("Root constraint not respected: "
                        "%"PRIi64" is not in [ %"PRIi64", %"PRIi64" ]",
                        n, info->min, info->max);

                return -1;
            }
        } else {
            if (info->extended) {
                /* Extension not present */
                bb_add_bit(bb, false);
            }
        }
    }

    if (info && info->min != INT64_MIN) {
        aper_write_number(bb, n - info->min, info);
    } else { /* Only 2's-complement case */
        aper_write_2c_number(bb, n);
    }

    return 0;
}

static int
aper_encode_enum(bb_t *bb, uint32_t val, const asn1_enum_info_t *e)
{
    int pos = asn1_enum_pos(e, val);

    if (pos < 0) {
        if (e->extended) {
            bb_add_bit(bb, true);
            aper_write_nsnnwn(bb, val);

            return 0;
        } else {
            e_info("Undeclared enumerated value: %d", val);
            return -1;
        }
    }

    if (e->extended) {
        bb_add_bit(bb, false);
    }

    /* XXX We suppose that enumerations cannot hold more than 255 values */
    bb_add_bits(bb, pos, e->blen);

    return 0;
}

/* }}} */
/* String types {{{ */

static int
aper_encode_ostring(bb_t *bb, const asn1_ostring_t *os,
                    const asn1_cnt_info_t *info)
{
    if (aper_encode_len(bb, os->len, info) < 0) {
        return e_error("Octet string : failed to encode length");
    }

    if (info && info->max <= 2 && info->min == info->max
    &&  os->len == info->max)
    {
        for (size_t i = 0; i < os->len; i++) {
            bb_add_bits(bb, os->data[i], 8);
        }

        return 0;
    }

    bb_align(bb);
    sb_add(&bb->sb, os->data, os->len);

    return 0;
}

static int
aper_encode_data(bb_t *bb, const asn1_data_t *data,
                 const asn1_cnt_info_t *info)
{
    asn1_ostring_t os = ASN1_OSTRING(data->data, data->len);

    return aper_encode_ostring(bb, &os, info);
}

static int
aper_encode_string(bb_t *bb, const asn1_string_t *str,
                 const asn1_cnt_info_t *info)
{
    asn1_ostring_t os = ASN1_OSTRING((const uint8_t *)str->data, str->len);

    return aper_encode_ostring(bb, &os, info);
}

static int
aper_encode_bstring(bb_t *bb, const bit_stream_t *bs,
                    const asn1_cnt_info_t *info)
{
    size_t len = bs_len(bs);

    if (aper_encode_len(bb, len, info) < 0) {
        return e_error("Octet string : failed to encode length");
    }

    bb_add_bs(bb, *bs);

    return 0;
}

static int
aper_encode_bit_string(bb_t *bb, const asn1_bit_string_t *b,
                       const asn1_cnt_info_t *info)
{
    bit_stream_t bs = bs_init(b->data, 0, b->bit_len);

    return aper_encode_bstring(bb, &bs, info);
}

static ALWAYS_INLINE void aper_encode_bool(bb_t *bb, bool b)
{
    bb_add_bit(bb, b);
}

/* }}} */
/* Constructed types {{{ */

static int aper_encode_constructed(bb_t *bb, const void *st,
                                   const asn1_desc_t *desc,
                                   const asn1_field_t *field);

static int
aper_encode_value(bb_t *bb, const void *v, const asn1_field_t *field)
{
    switch (field->type) {
      case ASN1_OBJ_TYPE(bool):
        aper_encode_bool(bb, *(const bool *)v);
        break;
#define ASN1_ENCODE_INT_CASE(type_t)                                      \
      case ASN1_OBJ_TYPE(type_t):                                         \
        return aper_encode_number(bb, *(type_t *)v, &field->int_info);
      ASN1_ENCODE_INT_CASE(int8_t);
      ASN1_ENCODE_INT_CASE(uint8_t);
      ASN1_ENCODE_INT_CASE(int16_t);
      ASN1_ENCODE_INT_CASE(uint16_t);
      ASN1_ENCODE_INT_CASE(int32_t);
      ASN1_ENCODE_INT_CASE(uint32_t);
      ASN1_ENCODE_INT_CASE(int64_t);
      ASN1_ENCODE_INT_CASE(uint64_t);
#undef ASN1_ENCODE_INT_CASE
      case ASN1_OBJ_TYPE(enum):
        return aper_encode_enum(bb, *(int32_t *)v, field->enum_info);
      case ASN1_OBJ_TYPE(NULL):
      case ASN1_OBJ_TYPE(OPT_NULL):
        break;
      case ASN1_OBJ_TYPE(asn1_data_t):
        return aper_encode_data(bb, (const asn1_data_t *)v, &field->str_info);
        break;
      case ASN1_OBJ_TYPE(asn1_string_t):
        return aper_encode_string(bb, (const asn1_string_t *)v, &field->str_info);
        break;
      case ASN1_OBJ_TYPE(asn1_bit_string_t):
        return aper_encode_bit_string(bb, (const asn1_bit_string_t *)v,
                                      &field->str_info);
      case ASN1_OBJ_TYPE(SEQUENCE): case ASN1_OBJ_TYPE(CHOICE):
      case ASN1_OBJ_TYPE(UNTAGGED_CHOICE):
        return aper_encode_constructed(bb, v, field->u.comp, field);
      case ASN1_OBJ_TYPE(asn1_ext_t):
        assert (0);
        e_error("Ext type not supported");
        break;
      case ASN1_OBJ_TYPE(OPAQUE):
        assert (0);
        e_error("Opaque type not supported");
        break;
      case ASN1_OBJ_TYPE(SKIP):
        e_error("Skip not supported"); /* We cannot stand squirrels */
        break;
      case ASN1_OBJ_TYPE(OPEN_TYPE):
        e_error("Open type not supported");
        break;
    }

    return 0;
}

static int
aper_encode_field(bb_t *bb, const void *v, const asn1_field_t *field)
{
    int res;

    e_trace(5, "Encoding value %s:%s", field->oc_t_name, field->name);

    bb_push_mark(bb);

    if (field->is_open_type) {
        asn1_ostring_t  os;
        bb_t            buf;

        bb_inita(&buf, field->open_type_buf_len);
        aper_encode_value(&buf, v, field);

        if (!bb_len(&buf)) {
            sb_addc(&buf.sb, 0);
        }

        os = ASN1_OSTRING((const uint8_t *)buf.sb.data, buf.sb.len);
        res = aper_encode_ostring(bb, &os, NULL);
        bb_wipe(&buf);
    } else {
        res = aper_encode_value(bb, v, field);
    }

    e_trace_bb_tail(5, bb, "Value encoding for %s:%s",
                    field->oc_t_name, field->name);
    bb_pop_mark(bb);

    return res;
}

static int
aper_encode_sequence(bb_t *bb, const void *st, const asn1_desc_t *desc)
{
    const void *v;

    /* Put extension bit */
    if (desc->extended) {
        e_trace(5, "Sequence is extended");
        bb_add_bit(bb, false);
    }

    bb_push_mark(bb);

    /* Encode optional fields bit-map */
    qv_for_each_pos(u16, pos, &desc->opt_fields) {
        uint16_t            field_pos = desc->opt_fields.tab[pos];
        const asn1_field_t *field     = &desc->vec.tab[field_pos];
        const void         *opt       = GET_DATA_P(st, field, uint8_t);
        const void         *val       = asn1_opt_field(opt, field->type);

        if (val) { /* Field present */
            bb_add_bit(bb, true);
        } else {
            bb_add_bit(bb, false);
        }
    }

    e_trace_bb_tail(5, bb, "SEQUENCE OPTIONAL fields bit-map");
    bb_pop_mark(bb);

    for (int i = 0; i < desc->vec.len; i++) {
        const asn1_field_t *field = &desc->vec.tab[i];

        assert (field->mode != ASN1_OBJ_MODE(SEQ_OF));

        if (field->mode == ASN1_OBJ_MODE(OPTIONAL)) {
            const void *opt = GET_DATA_P(st, field, uint8_t);

            v   = asn1_opt_field(opt, field->type);

            if (v == NULL) {
                continue; /* XXX field not present */
            }
        } else {
            v = GET_DATA_P(st, field, uint8_t);
        }

        if (aper_encode_field(bb, v, field) < 0) {
            return e_error("Failed to encode value %s:%s",
                           field->oc_t_name, field->name);
        }
    }

    return 0;
}

static int
aper_encode_choice(bb_t *bb, const void *st, const asn1_desc_t *desc)
{
    const asn1_field_t *choice_field;
    const asn1_field_t *enum_field;
    int index;
    const void *v;

    /* Put extension bit */
    if (desc->extended) {
        e_trace(5, "Choice is extended");
        bb_add_bit(bb, false);
    }

    assert (desc->vec.len > 1);

    enum_field = &desc->vec.tab[0];
    assert (enum_field->type == ASN1_OBJ_TYPE(enum));

    index = *GET_DATA_P(st, enum_field, int);
    choice_field = &desc->vec.tab[index];
    assert (choice_field->mode == ASN1_OBJ_MODE(MANDATORY));

    if (index < 1) {
        return e_error("Wrong choice initialization");
    }

    bb_push_mark(bb);

    /* XXX Indexes start from 0 */
    aper_write_number(bb, index - 1, &desc->choice_info);

    e_trace_bb_tail(5, bb, "CHOICE index");
    bb_pop_mark(bb);

    v = GET_DATA_P(st, choice_field, uint8_t);
    assert (v);

    if (aper_encode_field(bb, v, choice_field) < 0) {
        return e_error("Failed to encode choice element %s:%s",
                       choice_field->oc_t_name, choice_field->name);
    }

    return 0;
}

static int
aper_encode_seq_of(bb_t *bb, const void *st, const asn1_field_t *field)
{
    const uint8_t *tab;
    size_t elem_cnt;
    const asn1_field_t *repeated_field;
    const asn1_desc_t *desc = field->u.comp;

    assert (desc->vec.len == 1);
    repeated_field = &desc->vec.tab[0];

    assert (repeated_field->mode == ASN1_OBJ_MODE(SEQ_OF));

    tab     = (const uint8_t *)GET_VECTOR_DATA(st, repeated_field);
    elem_cnt = GET_VECTOR_LEN(st, repeated_field);

    bb_push_mark(bb);

    if (aper_encode_len(bb, elem_cnt, &field->seq_of_info) < 0) {
        bb_pop_mark(bb);

        return e_error("Failed to encode SEQUENCE OF length (n = %zd)",
                       elem_cnt);
    }

    e_trace_bb_tail(5, bb, "SEQUENCE OF length");
    bb_pop_mark(bb);

    if (repeated_field->pointed) {
        for (size_t j = 0; j < elem_cnt; j++) {
            if (aper_encode_field(bb, ((const void **)tab)[j],
                                  repeated_field) < 0)
            {
                e_error("Failed to encode array value [%zd] %s:%s",
                        j, repeated_field->oc_t_name, repeated_field->name);

                return -1;
            }
        }
    } else {
        for (size_t j = 0; j < elem_cnt; j++) {
            if (aper_encode_field(bb, tab + j * repeated_field->size,
                                  repeated_field) < 0)
            {
                e_error("Failed to encode vector value [%zd] %s:%s",
                        j, repeated_field->oc_t_name, repeated_field->name);

                return -1;
            }
        }
    }

    return 0;
}

/* TODO get it cleaner */
static int aper_encode_constructed(bb_t *bb, const void *st,
                                   const asn1_desc_t *desc,
                                   const asn1_field_t *field)
{
    if (desc->is_seq_of) {
        assert (field);
        assert (desc == field->u.comp);

        if (aper_encode_seq_of(bb, st, field) < 0) {
            return e_error("Failed to encode sequence of values");
        }

        return 0;
    }

    switch (desc->type) {
      case ASN1_CSTD_TYPE_SEQUENCE:
        return aper_encode_sequence(bb, st, desc);
      case ASN1_CSTD_TYPE_CHOICE:
        return aper_encode_choice(bb, st, desc);
      case ASN1_CSTD_TYPE_SET:
        e_error("ASN.1 SET not supported yet");
      default:
        return -1;
    }
}

int aper_encode_desc(sb_t *sb, const void *st, const asn1_desc_t *desc)
{
    bb_t bb = bb_init_sb(sb);

    if (aper_encode_constructed(&bb, st, desc, NULL) < 0) {
        return -1;
    }

    *sb = bb.sb;
    bb_wipe(&bb);

    /* Ref : [1] 10.1.3 */
    if (unlikely(!sb->len)) {
        sb_addc(sb, 0);
    }

    return 0;
}

/* }}} */

/* }}} */

/* }}} */
/* Read {{{ */

#define e_info(fmt, ...)  \
    if (_G.decode_log_level < 0)                                             \
        e_info(fmt, ##__VA_ARGS__);                                          \
    else                                                                     \
        e_trace(_G.decode_log_level, fmt, ##__VA_ARGS__);

void aper_set_decode_log_level(int level)
{
    _G.decode_log_level = level;
}

/* Helpers {{{ */

static ALWAYS_INLINE int
aper_read_u16_m(bit_stream_t *bs, size_t blen, uint16_t *u16)
{
    assert (blen); /* u16 is given by constraints */

    if (blen < 8) {
        if (!bs_has(bs, blen)) {
            e_info("Not enough bits to read constrained integer "
                   "(got %zd, need %zd)", bs_len(bs), blen);
            return -1;
        }

        *u16 = __bs_get_bits(bs, blen);
        return 0;
    }

    bs_align(bs);

    if (blen == 8) {
        if (ps_done(&bs->ps)) {
            e_info("Cannot read constrained integer: end of input "
                   "(expected at least one octet left)");
            return -1;
        }

        *u16 = __read_u8(&bs->ps); /* We can: bs is aligned */
        return 0;
    }

    assert (blen <= 16);

    if (!bs_has_bytes(bs, 2)) {
        e_info("Cannot read constrained integer: input too short "
               "(expected at least two octets left)");
        return -1;
    }

    *u16 = __read_u16(&bs->ps); /* We can: bs is aligned */

    return 0;
}

static ALWAYS_INLINE int
aper_read_ulen(bit_stream_t *bs, size_t *l)
{
    uint16_t u16;

    bs_align(bs);

    if (ps_done(&bs->ps)) {
        e_info("Cannot read unconstrained length: end of input "
               "(expected at least one octet left)");
        return -1;
    }

    u16 = __read_u8(&bs->ps);

    if (!(u16 & (1 << 7))) {
        *l = u16;
        return 0;
    }

    if (ps_done(&bs->ps)) {
        e_info("Cannot read unconstrained length: end of input "
               "(expected at least a second octet left)");
        return -1;
    }

    if (u16 & (1 << 6)) {
        e_info("Cannot read unconstrained length: "
               "fragmented values are not supported");
        return -1;
    }

    u16 =  __read_u8(&bs->ps) | (u16 << 8);
    u16 &= 0x7fff;

    *l = u16;

    return 0;
}

static ALWAYS_INLINE int
aper_read_2c_number(bit_stream_t *bs, int64_t *v)
{
    size_t olen;

    if (aper_read_ulen(bs, &olen) < 0) {
        e_info("Cannot read unconstrained whole number length");
        return -1;
    }

    bs_align(bs);

    if (!bs_has_bytes(bs, olen)) {
        e_info("Not enough bytes to read unconstrained number "
               "(got %zd, need %zd)", ps_len(&bs->ps), olen);
        return -1;
    }

    *v = __read_i64_o(&bs->ps, olen);

    return 0;
}

static ALWAYS_INLINE int
aper_read_number(bit_stream_t *bs, const asn1_int_info_t *info, uint64_t *v)
{
    size_t olen;

    if (info && info->constrained) {
        if (info->max_blen <= 16) {
            uint16_t u16;

            assert (info->max_blen);

            if (aper_read_u16_m(bs, info->max_blen, &u16) < 0) {
                e_info("Cannot read constrained whole number");
                return -1;
            }

            *v = u16;

            return 0;
        } else {
            uint16_t u16;

            if (aper_read_u16_m(bs, info->max_olen_blen, &u16) < 0) {
                e_info("Cannot read constrained whole number length");
                return -1;
            }

            olen = u16;
        }
    } else {
        if (aper_read_ulen(bs, &olen) < 0) {
            e_info("Cannot read semi-constrained whole number length");
            return -1;
        }
    }

    if (!olen) {
        e_info("Forbidden number length value : 0");
        return -1;
    }

    bs_align(bs);

    if (!bs_has_bytes(bs, olen)) {
        e_info("Not enough bytes to read number "
               "(got %zd, need %zd)", ps_len(&bs->ps), olen);
        return -1;
    }

    *v = __read_u64_o(&bs->ps, olen);

    return 0;
}

static int aper_read_nsnnwn(bit_stream_t *bs, size_t *n)
{
    bool is_short;
    uint64_t u64;

    if (bs_done(bs)) {
        e_info("Cannot read NSNNWN: end of input");
        return -1;
    }

    is_short = !__bs_get_bit(bs);

    if (is_short) {
        if (!bs_has(bs, 6)) {
            e_info("Cannot read short NSNNWN: not enough bits");
            return -1;
        }

        *n = __bs_get_bits(bs, 6);

        return 0;
    }

    if (aper_read_number(bs, NULL, &u64) < 0) {
        e_info("Cannot read long form NSNNWN");
        return -1;
    }

    *n = u64;

    return 0;
}

static int
aper_read_len(bit_stream_t *bs, size_t l_min, size_t l_max, size_t *l)
{
    size_t   d_max = l_max - l_min;

    if (d_max < (1 << 16)) {
        uint16_t d;

        if (!d_max) {
            *l = l_min;

            return 0;
        }

        if (aper_read_u16_m(bs, u16_blen(d_max), &d) < 0) {
            e_info("Cannot read constrained length");
            return -1;
        }

        *l = l_min + d;
    } else {
        if (aper_read_ulen(bs, l) < 0) {
            e_info("Cannot read unconstrained length");
            return -1;
        }
    }

    if (*l > l_max) {
        e_info("Length is too high");
        return -1;
    }

    return 0;
}

/* }}} */
/* Front End Decoders {{{ */

/* Scalar types {{{ */

static int
aper_decode_len(bit_stream_t *bs, const asn1_cnt_info_t *info, size_t *l)
{
    if (info) {
        bool extension_present;

        if (info->extended) {
            if (bs_done(bs)) {
                e_info("Cannot read extension bit: end of input");
                return -1;
            }

            extension_present = __bs_get_bit(bs);
        } else {
            extension_present = false;
        }

        if (extension_present) {
            if (aper_read_ulen(bs, l) < 0) {
                e_info("Cannot read extended length");
                return -1;
            }

            if (*l < info->ext_min || *l > info->ext_max) {
                e_info("Extended length constraint not respected");
                return -1;
            }
        } else {
            if (aper_read_len(bs, info->min, info->max, l) < 0) {
                e_info("Cannot read constrained length");
                return -1;
            }

            if (*l < info->min || *l > info->max) {
                e_info("Root length constraint not respected");
                return -1;
            }
        }
    } else {
        if (aper_read_len(bs, 0, SIZE_MAX, l) < 0) {
            e_info("Cannot read unconstrained length");
            return -1;
        }
    }

    return 0;
}

static int
aper_decode_number(bit_stream_t *bs, const asn1_int_info_t *info, int64_t *n)
{
    int64_t res;

    if (info && info->extended) {
        bool extension_present;

        if (bs_done(bs)) {
            e_info("Cannot read extension bit: end of input");
            return -1;
        }

        extension_present = __bs_get_bit(bs);

        if (extension_present) {
            if (aper_read_2c_number(bs, n) < 0) {
                e_info("Cannot read extended unconstrained number");
                return -1;
            }

            /* TODO check extension constraint */
            return 0;
        }
    }

    if (info && info->min != INT64_MIN) {
        uint64_t u64;

        if (info->min == info->max) {
            *n = info->min;

            return 0;
        }

        if (aper_read_number(bs, info, &u64) < 0) {
            e_info("Cannot read constrained or semi-constrained number");
            return -1;
        }

        res =  u64;
        res += info->min;

        if (info && (res < info->min || res > info->max)) {
            e_error("Root constraint not respected: "
                    "%"PRIi64" is not in [ %"PRIi64", %"PRIi64" ]",
                    res, info->min, info->max);

            return -1;
        }
    } else {
        assert (!info || !info->constrained);

        if (aper_read_2c_number(bs, &res) < 0) {
            e_info("Cannot read unconstrained number");
            return -1;
        }
    }

    *n = res;

    return 0;
}

/* XXX Negative values are forbidden */
static int
aper_decode_enum(bit_stream_t *bs, const asn1_enum_info_t *e, uint32_t *val)
{
    uint8_t pos;

    if (e->extended) {
        if (bs_done(bs)) {
            e_info("Cannot read enumerated type: end of input");
            return -1;
        }

        if (__bs_get_bit(bs)) {
            size_t nsnnwn;

            if (aper_read_nsnnwn(bs, &nsnnwn) < 0) {
                e_info("Cannot read extended enumeration");
                return -1;
            }

            *val = nsnnwn;

            return 0;
        }
    }

    if (!bs_has(bs, e->blen)) {
        e_info("Cannot read enumerated value: not enough bits");
        return -1;
    }

    pos = __bs_get_bits(bs, e->blen);

    if (pos >= e->values.len) {
        e_info("Cannot read enumerated value: unregistered value");
        return -1;
    }

    *val = e->values.tab[pos];

    return 0;
}

static ALWAYS_INLINE int aper_decode_bool(bit_stream_t *bs, bool *b)
{
    if (bs_done(bs)) {
        e_info("Cannot decode boolean: end of input");
        return -1;
    }

    *b = __bs_get_bit(bs);

    return 0;
}

/* }}} */
/* String types {{{ */

static int
t_aper_decode_ostring(bit_stream_t *bs, const asn1_cnt_info_t *info,
                      flag_t copy, asn1_ostring_t *os)
{
    if (aper_decode_len(bs, info, &os->len) < 0) {
        e_info("Cannot decode octet string length");
        return -1;
    }

    if (info && info->max <= 2 && info->min == info->max
    &&  os->len == info->max)
    {
        uint8_t *buf;

        if (!bs_has(bs, os->len * 2)) {
            e_info("Cannot read octet string: not enough bits");
            return -1;
        }

        buf = t_new(uint8_t, os->len + 1);

        for (size_t i = 0; i < os->len; i++) {
            buf[i] = __bs_get_bits(bs, 8);
        }

        os->data = buf;

        return 0;
    }

    bs_align(bs);

    if (!bs_has_bytes(bs, os->len)) {
        e_info("Cannot read octet string: not enough octets "
               "(want %zd, got %zd)", os->len, bs_len(bs) / 8);
        return -1;
    }

    if (copy) {
        os->data = t_dupz(bs->ps.b, os->len);
    } else {
        os->data = bs->ps.b;
    }

    e_trace_hex(6, "Decoded OCTET STRING", os->data, (int)os->len);

    __ps_skip(&bs->ps, os->len);

    return 0;
}

static int
t_aper_decode_data(bit_stream_t *bs, const asn1_cnt_info_t *info,
                   flag_t copy, asn1_data_t *data)
{
    asn1_ostring_t os;

    RETHROW(t_aper_decode_ostring(bs, info, copy, &os));

    *data = ASN1_DATA(os.data, os.len);

    return 0;
}

static int
t_aper_decode_string(bit_stream_t *bs, const asn1_cnt_info_t *info,
                   flag_t copy, asn1_string_t *str)
{
    asn1_ostring_t os;

    RETHROW(t_aper_decode_ostring(bs, info, copy, &os));

    *str = ASN1_STRING((const char *)os.data, os.len);

    return 0;
}

static int
t_aper_decode_bstring(bit_stream_t *bs, const asn1_cnt_info_t *info,
                      flag_t copy, bit_stream_t *str)
{
    size_t len;

    if (aper_decode_len(bs, info, &len) < 0) {
        e_info("Cannot decode bit string length");
        return -1;
    }

    if (!bs_has(bs, len)) {
        e_info("Cannot read bit string: not enough bits");
        return -1;
    }

    *str = __bs_get_bs(bs, len);
    e_trace_bs(6, str, "Decoded bit string");

    if (copy) {
        size_t olen = ps_len(&str->ps);

        str->ps = ps_init(t_dup(str->ps.b, olen), olen);
    }

    return 0;
}

static int
t_aper_decode_bit_string(bit_stream_t *bs, const asn1_cnt_info_t *info,
                         asn1_bit_string_t *bit_string)
{
    bit_stream_t bstring;
    bb_t bb;
    uint8_t *data;
    size_t size;

    RETHROW(t_aper_decode_bstring(bs, info, false, &bstring));

    size = ps_len(&bstring.ps) + 1;
    bb_inita(&bb, size);
    bb_add_bs(&bb, bstring);
    data = memp_dup(t_pool(), bb.sb.data, size);
    *bit_string = ASN1_BIT_STRING(data, bs_len(&bstring));
    bb_wipe(&bb);

    return 0;
}

/* }}} */
/* Constructed types {{{ */

static int
t_aper_decode_constructed(bit_stream_t *bs, const asn1_desc_t *desc,
                          const asn1_field_t *field, flag_t copy, void *st);

static int
t_aper_decode_value(bit_stream_t *bs, const asn1_field_t *field,
                  flag_t copy, void *v)
{
    switch (field->type) {
      case ASN1_OBJ_TYPE(bool):
        return aper_decode_bool(bs, (bool *)v);
        break;
#define ASN1_DECODE_INT_CASE(type_t)  \
      case ASN1_OBJ_TYPE(type_t):                                         \
        {                                                                 \
            int64_t i64;                                                  \
                                                                          \
            RETHROW(aper_decode_number(bs, &field->int_info, &i64));      \
            e_trace(5, "Decoded number value (n = %"PRIi64")", i64);      \
                                                                          \
            *(type_t *)v = i64;                                           \
                                                                          \
        }                                                                 \
        return 0;
      ASN1_DECODE_INT_CASE(int8_t);
      ASN1_DECODE_INT_CASE(uint8_t);
      ASN1_DECODE_INT_CASE(int16_t);
      ASN1_DECODE_INT_CASE(uint16_t);
      ASN1_DECODE_INT_CASE(int32_t);
      ASN1_DECODE_INT_CASE(uint32_t);
      ASN1_DECODE_INT_CASE(int64_t);
      ASN1_DECODE_INT_CASE(uint64_t);
#undef ASN1_DECODE_INT_CASE
      case ASN1_OBJ_TYPE(enum):
        RETHROW(aper_decode_enum(bs, field->enum_info, (uint32_t *)v));
        e_trace(5, "Decoded enum value (n = %u)", *(uint32_t *)v);
        return 0;
      case ASN1_OBJ_TYPE(NULL):
      case ASN1_OBJ_TYPE(OPT_NULL):
        break;
      case ASN1_OBJ_TYPE(asn1_data_t):
        return t_aper_decode_data(bs, &field->str_info, copy,
                                  (asn1_data_t *)v);
      case ASN1_OBJ_TYPE(asn1_string_t):
        return t_aper_decode_string(bs, &field->str_info, copy,
                                    (asn1_string_t *)v);
      case ASN1_OBJ_TYPE(asn1_bit_string_t):
        return t_aper_decode_bit_string(bs, &field->str_info,
                                        (asn1_bit_string_t *)v);
      case ASN1_OBJ_TYPE(SEQUENCE): case ASN1_OBJ_TYPE(CHOICE):
      case ASN1_OBJ_TYPE(UNTAGGED_CHOICE):
        return t_aper_decode_constructed(bs, field->u.comp, field, copy, v);
      case ASN1_OBJ_TYPE(asn1_ext_t):
        assert (0);
        e_error("Ext type not supported");
        break;
      case ASN1_OBJ_TYPE(OPAQUE):
        assert (0);
        e_error("Opaque type not supported");
        break;
      case ASN1_OBJ_TYPE(SKIP):
        break;
      case ASN1_OBJ_TYPE(OPEN_TYPE):
        e_error("Open type not supported");
        break;
    }

    return 0;
}

static int
t_aper_decode_field(bit_stream_t *bs, const asn1_field_t *field,
                    flag_t copy, void *v)
{
    if (field->is_open_type) {
        asn1_ostring_t os;
        bit_stream_t   open_type_bs;

        if (t_aper_decode_ostring(bs, NULL, false, &os) < 0) {
            e_info("Cannot read OPEN TYPE field");
            return -1;
        }

        open_type_bs = bs_init(os.data, 0, os.len * 8);

        return t_aper_decode_value(&open_type_bs, field, copy, v);
    }

    return t_aper_decode_value(bs, field, copy, v);
}

static void *t_alloc_if_pointed(const asn1_field_t *field, void *st)
{
    if (field->pointed) {
        return (*GET_PTR(st, field, void *) = t_new_raw(char, field->size));
    }

    return GET_PTR(st, field, void);
}

static int
t_aper_decode_sequence(bit_stream_t *bs, const asn1_desc_t *desc,
                       flag_t copy, void *st)
{
    bit_stream_t opt_bitmap;

    if (desc->extended) {
        flag_t extension_present;

        if (bs_done(bs)) {
            e_info("Cannot read extension bit: end of input");
            return -1;
        }

        extension_present = __bs_get_bit(bs);

        if (extension_present) {
            e_info("Extension are not supported in sequences");
            return -1;
        }
    }

    if (!bs_has(bs, desc->opt_fields.len)) {
        e_info("Cannot read optional fields bit-map: not enough bits");
        return -1;
    }

    opt_bitmap = __bs_get_bs(bs, desc->opt_fields.len);

    for (int i = 0; i < desc->vec.len; i++) {
        const asn1_field_t *field = &desc->vec.tab[i];
        void               *v;

        if (field->mode == ASN1_OBJ_MODE(OPTIONAL)) {
            if (unlikely(bs_done(&opt_bitmap))) {
                assert (0);
                return e_error("Sequence is broken");
            }

            if (!__bs_get_bit(&opt_bitmap)) {
                asn1_opt_field_w(GET_PTR(st, field, void), field->type, false);
                continue;  /* XXX Field not present */
            }

            v = t_alloc_if_pointed(field, st);
            v = asn1_opt_field_w(GET_PTR(st, field, void), field->type, true);
        } else {
            assert (field->mode != ASN1_OBJ_MODE(SEQ_OF));

            v = t_alloc_if_pointed(field, st);
        }

        e_trace(5, "Decoding SEQUENCE value %s:%s",
                field->oc_t_name, field->name);

        if (t_aper_decode_field(bs, field, copy, v) < 0) {
            e_info("Cannot read sequence field %s:%s",
                   field->oc_t_name, field->name);
            return -1;
        }
    }

    return 0;
}

static int
t_aper_decode_choice(bit_stream_t *bs, const asn1_desc_t *desc, flag_t copy,
                     void *st)
{
    const asn1_field_t  *choice_field;
    const asn1_field_t  *enum_field;
    uint64_t             u64;
    size_t               index;
    void                *v;

    if (desc->extended) {
        flag_t extension_present;

        if (bs_done(bs)) {
            e_info("Cannot read extension bit: end of input");
            return -1;
        }

        extension_present = __bs_get_bit(bs);

        if (extension_present) {
            e_info("Extension are not supported in choices");
            return -1;
        }

        e_trace(5, "Extension not present");
    } else {
        e_trace(5, "Choice is not extended");
    }

    if (aper_read_number(bs, &desc->choice_info, &u64) < 0) {
        e_info("Cannot read choice index");
        return -1;
    }

    index = u64;

    e_trace(5, "Decoded choice index (index = %zd)", index);

    enum_field = &desc->vec.tab[0];
    choice_field = &desc->vec.tab[index + 1];     /* XXX Indexes start from 0 */
    *GET_PTR(st, enum_field, int) = index + 1;    /* Write enum value         */
    v = t_alloc_if_pointed(choice_field, st);

    assert (choice_field->mode == ASN1_OBJ_MODE(MANDATORY));
    assert (enum_field->mode == ASN1_OBJ_MODE(MANDATORY));

    e_trace(5, "Decoding CHOICE value %s:%s",
            choice_field->oc_t_name, choice_field->name);

    if (t_aper_decode_field(bs, choice_field, copy, v) < 0) {
        e_info("Cannot decode choice value");
        return -1;
    }

    return 0;
}

static int
t_aper_decode_seq_of(bit_stream_t *bs, const asn1_field_t *field,
                     flag_t copy, void *st)
{
    size_t elem_cnt;
    const asn1_field_t *repeated_field;
    const asn1_desc_t *desc = field->u.comp;

    assert (desc->vec.len == 1);
    repeated_field = &desc->vec.tab[0];

    if (aper_decode_len(bs, &field->seq_of_info, &elem_cnt) < 0) {
        e_info("Failed to decode SEQUENCE OF length");
        return -1;
    }

    e_trace(5, "Decoded element count of SEQUENCE OF %s:%s (n = %zd)",
            repeated_field->oc_t_name, repeated_field->name, elem_cnt);

    if (unlikely(!elem_cnt)) {
        *GET_PTR(st, repeated_field, asn1_data_t) = ASN1_DATA_NULL;
        return 0;
    }

    if (repeated_field->pointed) {
        asn1_void_array_t *array;

        array = GET_PTR(st, repeated_field, asn1_void_array_t);
        array->data = t_new_raw(void *, elem_cnt * sizeof(void *));

        for (size_t j = 0; j < elem_cnt; j++) { /* Alloc array pointers */
            array->data[j] = t_new_raw(char, repeated_field->size);
        }
    } else {
        GET_PTR(st, repeated_field, asn1_data_t)->data =
            t_new_raw(char, elem_cnt * repeated_field->size);
    }

    GET_PTR(st, repeated_field, asn1_void_vector_t)->len = elem_cnt;

    for (size_t j = 0; j < elem_cnt; j++) {
        void *v;

        if (repeated_field->pointed) {
            v = GET_PTR(st, repeated_field, asn1_void_array_t)->data[j];
        } else {
            v = (char *)(GET_PTR(st, repeated_field, asn1_void_vector_t)->data)
              + j * repeated_field->size;
        }

        e_trace(5, "Decoding SEQUENCE OF %s:%s value [%zu/%zu]",
                repeated_field->oc_t_name, repeated_field->name, j, elem_cnt);

        if (t_aper_decode_field(bs, repeated_field, copy, v) < 0) {
            e_info("Failed to decode SEQUENCE OF element");
            return -1;
        }
    }

    return 0;
}

/* TODO get it cleaner */
static int
t_aper_decode_constructed(bit_stream_t *bs, const asn1_desc_t *desc,
                          const asn1_field_t *field, flag_t copy, void *st)
{
    if (desc->is_seq_of) {
        assert (field);
        assert (field->u.comp == desc);

        return t_aper_decode_seq_of(bs, field, copy, st);
    }

    switch (desc->type) {
      case ASN1_CSTD_TYPE_SEQUENCE:
        RETHROW(t_aper_decode_sequence(bs, desc, copy, st));
        break;
      case ASN1_CSTD_TYPE_CHOICE:
        RETHROW(t_aper_decode_choice(bs, desc, copy, st));
        break;
      case ASN1_CSTD_TYPE_SET:
        e_panic("ASN.1 SET is not supported yet. Please use SEQUENCE.");
        break;
    }

    return 0;
}

int t_aper_decode_desc(pstream_t *ps, const asn1_desc_t *desc,
                       flag_t copy, void *st)
{
    bit_stream_t bs = bs_init_ps(ps, 0);

    RETHROW(t_aper_decode_constructed(&bs, desc, NULL, copy, st));

    bs_align(&bs);
    *ps = bs.ps;

    return 0;
}

/* }}} */

/* }}} */

#undef e_info

/* }}} */
/* Check {{{ */

/* aper_write_u16_m/aper_read_u16_m {{{ */

TEST_DECL("aligned per: aper_write_u16_m/aper_read_u16_m", 0)
{
    t_scope;
    BB_1k(bb);
    char *str;
    bit_stream_t bs;
    size_t blen;
    uint16_t u16 = 0;

#define APER_U16_M_CHECK(d, d_max, expected)  \
    bb_reset(&bb);                                                     \
    blen = u64_blen(d_max);                                            \
    aper_write_u16_m(&bb, d, u64_blen(d_max));                         \
    bs = bs_init_bb(&bb);                                              \
    if (blen) {                                                        \
        TEST_FAIL_IF(aper_read_u16_m(&bs, blen, &u16) < 0,             \
                     "Call aper_read_u16_m");                          \
        TEST_FAIL_IF(u16 != d, "%u ?= %u", u16, d);                    \
    }                                                                  \
    str = t_print_bb(&bb, NULL);                                       \
    TEST_FAIL_IF(strcmp(expected, str),                                \
                 "expected [ "expected" ] | got [ %s ]", str)

    APER_U16_M_CHECK(    0,     0,  "");
    APER_U16_M_CHECK(  0xe,    57,  ".001110");
    APER_U16_M_CHECK( 0x8d,   255,  ".10001101");
    APER_U16_M_CHECK(0xabd, 33000,  ".00001010.10111101");
#undef APER_U16_M_CHECK

    TEST_DONE();
}

/* }}} */
/* aper_write_len/aper_read_len {{{ */

TEST_DECL("aligned per: aper_write_len/aper_read_len", 0)
{
    t_scope;
    BB_1k(bb);
    char *str;
    bit_stream_t bs;
    size_t len;

#define APER_LEN_CHECK(l, l_min, l_max, expected)  \
    bb_reset(&bb);                                                     \
    aper_write_len(&bb, l, l_min, l_max);                              \
    bs = bs_init_bb(&bb);                                              \
    TEST_FAIL_IF(aper_read_len(&bs, l_min, l_max, &len) < 0,           \
                 "Call aper_read_len");                                \
    TEST_FAIL_IF(len != l, "%zd ?= %u", len, l);                       \
    str = t_print_bb(&bb, NULL);                                       \
    TEST_FAIL_IF(strcmp(expected, str),                                \
                 "expected [ "expected" ] | got [ %s ]", str)

    APER_LEN_CHECK(15,    15,           15, "");
    APER_LEN_CHECK(7,      3,           18, ".0100");
    APER_LEN_CHECK(15,     0, ASN1_MAX_LEN, ".00001111");
    APER_LEN_CHECK(0x1b34, 0, ASN1_MAX_LEN, ".10011011.00110100");
#undef APER_LEN_CHECK

    TEST_DONE();
}

/* }}} */
/* aper_write_nsnnwn/aper_read_nsnnwn {{{ */

TEST_DECL("aligned per: aper_write_nsnnwn/aper_read_nsnnwn", 0)
{
    t_scope;
    BB_1k(bb);
    char *str;
    bit_stream_t bs;
    size_t len;

#define APER_NSNNWN_CHECK(n, expected)  \
    bb_reset(&bb);                                                     \
    aper_write_nsnnwn(&bb, n);                                         \
    bs = bs_init_bb(&bb);                                              \
    TEST_FAIL_IF(aper_read_nsnnwn(&bs, &len) < 0,                      \
                 "Call aper_read_nsnnwn");                             \
    TEST_FAIL_IF(len != n, "%zd ?= %u", len, n);                       \
    str = t_print_bb(&bb, NULL);                                       \
    TEST_FAIL_IF(strcmp(expected, str),                                \
                 "expected [ "expected" ] | got [ %s ]", str)

    APER_NSNNWN_CHECK(  0,  ".0000000");
    APER_NSNNWN_CHECK(0xe,  ".0001110");
    APER_NSNNWN_CHECK(96,   ".10000000.00000001.01100000");
    APER_NSNNWN_CHECK(128,  ".10000000.00000001.10000000");
#undef APER_NSNNWN_CHECK

    TEST_DONE();
}

/* }}} */
/* aper_encode_number/aper_decode_number {{{ */

TEST_DECL("aligned per: aper_encode_number/aper_decode_number", 0)
{
    t_scope;
    BB_1k(bb);
    char *str;
    bit_stream_t bs;
    int64_t i64;

    asn1_int_info_t uc = { /* Unconstrained */
        .min     = INT64_MIN,
        .max     = INT64_MAX,
    };

    asn1_int_info_t sc = { /* Semi-constrained */
        .min     = -5,
        .max     = INT64_MAX,
    };

    asn1_int_info_t fc1 = { /* Fully constrained */
        .min     = -5,
        .max     = -1,
    };

    asn1_int_info_t fc2 = { /* Fully constrained */
        .min     = 0,
        .max     = 100000,
    };

    asn1_int_info_t fc3 = { /* Fully constrained */
        .min     = 666,
        .max     = 666,
    };

    asn1_int_info_t ext = { /* Extended */
        .min       = 0,
        .max       = 7,
        .extended  = true,
        .ext_min   = 0,
        .ext_max   = INT64_MAX,
    };

#define APER_NUMBER_CHECK(i, info, expected)  \
    bb_reset(&bb);                                                     \
    asn1_int_info_update(info);                                        \
    aper_encode_number(&bb, i, info);                                  \
    bs = bs_init_bb(&bb);                                              \
    TEST_FAIL_IF(aper_decode_number(&bs, info, &i64) < 0,              \
                 "Call aper_decode_number");                           \
    TEST_FAIL_IF(i64 != i, "%"PRIi64" ?= "#i, i64);                    \
    str = t_print_bb(&bb, NULL);                                       \
    TEST_FAIL_IF(strcmp(expected, str),                                \
                 "expected [ "expected" ] | got [ %s ]", str)
    APER_NUMBER_CHECK(1234,  &uc,  ".00000010.00000100.11010010");
    APER_NUMBER_CHECK(1234,  NULL, ".00000010.00000100.11010010");
    APER_NUMBER_CHECK(-1234, NULL, ".00000010.11111011.00101110");
    APER_NUMBER_CHECK(0,     NULL, ".00000001.00000000");
    APER_NUMBER_CHECK(0,     &sc,  ".00000001.00000101");
    APER_NUMBER_CHECK(-3,    &fc1, ".010");
    APER_NUMBER_CHECK(-1,    &fc1, ".100");
    APER_NUMBER_CHECK(-1,    NULL, ".00000001.11111111");
    APER_NUMBER_CHECK(45,    &fc2, ".01000000.00101101");
    APER_NUMBER_CHECK(128,   &fc2, ".01000000.10000000");
    APER_NUMBER_CHECK(666,   &fc3, "");
    APER_NUMBER_CHECK(5,     &ext, ".0101");
    APER_NUMBER_CHECK(8,     &ext, ".10000000.00000001.00001000");

#undef APER_NUMBER_CHECK

    TEST_DONE();
}

/* }}} */
/* aper_encode_ostring/t_aper_decode_ostring {{{ */

TEST_DECL("aligned per: aper_encode_ostring/t_aper_decode_ostring", 0)
{
    t_scope;
    BB_1k(bb);
    char *str;
    bit_stream_t bs;
    asn1_ostring_t src;
    asn1_ostring_t dst;

    asn1_cnt_info_t uc = { /* Unconstrained */
        .max     = SIZE_MAX,
    };

    asn1_cnt_info_t fc1 = { /* Fully constrained */
        .min     = 3,
        .max     = 3,
    };

    asn1_cnt_info_t fc2 = { /* Fully constrained */
        .max     = 23,
    };

    asn1_cnt_info_t ext1 = { /* Extended */
        .min      = 1,
        .max      = 2,
        .extended = true,
        .ext_min  = 3,
        .ext_max  = 3,
    };

    asn1_cnt_info_t ext2 = { /* Extended */
        .min      = 2,
        .max      = 2,
        .extended = true,
        .ext_max  = SIZE_MAX,
    };

#define APER_OSTRING_CHECK(os, info, copy, expected)  \
    bb_reset(&bb);                                                     \
    src = (asn1_ostring_t){                                            \
        .data = (const uint8_t *)os,                                   \
        .len = sizeof(os) - 1                                          \
    };                                                                 \
    aper_encode_ostring(&bb, &src, info);                              \
    if (sizeof(os) - 1 < 4) {                                          \
        str = t_print_bb(&bb, NULL);                                   \
        TEST_FAIL_IF(strcmp(expected, str),                            \
                     "expected [ "expected" ] | got [ %s ]", str);     \
    }                                                                  \
    bs = bs_init_bb(&bb);                                              \
    TEST_FAIL_IF(t_aper_decode_ostring(&bs, info, copy, &dst) < 0,     \
                 "Call t_aper_decode_ostring");                        \
    TEST_FAIL_IF(dst.len != src.len, "Length check : %zd ?= %zd",      \
                 dst.len, src.len);                                    \
    TEST_FAIL_IF(memcmp(dst.data, src.data, src.len),                  \
                 "Content check")
    APER_OSTRING_CHECK("aaa", &uc,  true,
                       ".00000011.01100001.01100001.01100001");
    APER_OSTRING_CHECK("aaa", NULL, true,
                       ".00000011.01100001.01100001.01100001");
    APER_OSTRING_CHECK("aaa", NULL, false,
                       ".00000011.01100001.01100001.01100001");
    APER_OSTRING_CHECK("aaa", &fc1, false, ".01100001.01100001.01100001");
    APER_OSTRING_CHECK("aaa", &fc2, false,
                       ".00011000.01100001.01100001.01100001");
    APER_OSTRING_CHECK("aa", &ext1, true, ".01000000.01100001.01100001");
    APER_OSTRING_CHECK("aaa", &ext1, true,
                       ".10000000.00000011.01100001.01100001.01100001");
    APER_OSTRING_CHECK("aa", &ext2, true, ".00110000.10110000.1");
    APER_OSTRING_CHECK("a",  &ext2, true, ".10000000.00000001.01100001");
#undef APER_OSTRING_CHECK

    TEST_DONE();
}

/* }}} */
/* aper_encode_bstring/t_aper_decode_bstring {{{ */

TEST_DECL("aligned per: aper_encode_bstring/t_aper_decode_bstring", 0)
{
    t_scope;
    BB_1k(bb);
    BB_1k(src_bb);
    char *str;
    bit_stream_t bs;
    bit_stream_t src;
    bit_stream_t dst;

    asn1_cnt_info_t uc = { /* Unconstrained */
        .max     = SIZE_MAX,
    };

    asn1_cnt_info_t fc1 = { /* Fully constrained */
        .min     = 3,
        .max     = 3,
    };

    asn1_cnt_info_t fc2 = { /* Fully constrained */
        .max     = 23,
    };

    asn1_cnt_info_t ext1 = { /* Extended */
        .min      = 1,
        .max      = 2,
        .extended = true,
        .ext_min  = 3,
        .ext_max  = 3,
    };

    asn1_cnt_info_t ext2 = { /* Extended */
        .min      = 2,
        .max      = 2,
        .extended = true,
        .ext_max  = SIZE_MAX,
    };

#define APER_BSTRING_CHECK(bit_str, info, copy, expected)  \
    bb_reset(&bb);                                                     \
    bb_reset(&src_bb);                                                 \
                                                                       \
    for (int i = 0; bit_str[i]; i++) {                                 \
        if (bit_str[i] == '1') {                                       \
            bb_add_bit(&src_bb, true);                                 \
        }                                                              \
        if (bit_str[i] == '0') {                                       \
            bb_add_bit(&src_bb, false);                                \
        }                                                              \
    }                                                                  \
                                                                       \
    src = bs_init_bb(&src_bb);                                         \
    aper_encode_bstring(&bb, &src, info);                              \
    str = t_print_bb(&bb, NULL);                                       \
    TEST_FAIL_IF(strcmp(expected, str),                                \
                 "expected [ "expected" ] | got [ %s ]", str);         \
    bs = bs_init_bb(&bb);                                              \
    TEST_FAIL_IF(t_aper_decode_bstring(&bs, info, copy, &dst) < 0,     \
                 "Call t_aper_decode_bstring");                        \
    TEST_FAIL_IF(bs_len(&dst) != bs_len(&src),                         \
                 "Length check : %zd ?= %zd",                          \
                 bs_len(&dst), bs_len(&src));                          \
    TEST_FAIL_UNLESS(bs_equals(dst, src), "Content check")

    APER_BSTRING_CHECK("01010101",  &uc,  false, ".00001000.01010101");
    APER_BSTRING_CHECK("01010101", NULL,  true, ".00001000.01010101");
    APER_BSTRING_CHECK("101",      &fc1,  true, ".101");
    APER_BSTRING_CHECK("101",      &fc2,  true, ".00011101");
    APER_BSTRING_CHECK("10",       &ext1, true, ".0110");
    APER_BSTRING_CHECK("011",      &ext1, true, ".10000000.00000011.011");
    APER_BSTRING_CHECK("11",       &ext2, true, ".011");
    APER_BSTRING_CHECK("011",      &ext2, true, ".10000000.00000011.011");
#undef APER_BSTRING_CHECK

    TEST_DONE();
}

/* }}} */
/* aper_encode_enum/aper_decode_enum {{{ */

TEST_DECL("aligned per: aper_encode_enum/aper_decode_enum", 0)
{
    t_scope;
    BB_1k(bb);
    char *str;
    bit_stream_t bs;
    uint32_t res;

    asn1_enum_info_t e1;
    asn1_enum_info_t e2;
    asn1_enum_info_t e3;

    asn1_enum_info_init(&e1);
    asn1_enum_append(&e1, 5);
    asn1_enum_append(&e1, 18);

    asn1_enum_info_init(&e2);
    e2.extended = true;

    for (uint32_t u = 0; u < 100; u++) {
        asn1_enum_append(&e2, u);
    }

    asn1_enum_info_init(&e3);
    e3.extended = true;

    for (uint32_t u = 0; u < 18; u++) {
        asn1_enum_append(&e3, u);
    }

#define APER_ENUM_CHECK(val, e, expected)  \
    bb_reset(&bb);                                                     \
    TEST_FAIL_IF(aper_encode_enum(&bb, val, e) < 0,                    \
                 "Call aper_encode_enum");                             \
    bs = bs_init_bb(&bb);                                              \
    TEST_FAIL_IF(aper_decode_enum(&bs, e, &res) < 0,                   \
                 "Call aper_decode_enum");                             \
    TEST_FAIL_IF(res != val, "%u ?= %u", res, val);                    \
    str = t_print_bb(&bb, NULL);                                       \
    TEST_FAIL_IF(strcmp(expected, str),                                \
                 "expected [ "expected" ] | got [ %s ]", str)

    APER_ENUM_CHECK(5,   &e1, ".0");
    APER_ENUM_CHECK(18,  &e1, ".1");
    APER_ENUM_CHECK(48,  &e2, ".00110000");
    APER_ENUM_CHECK(104, &e2, ".11000000.00000001.01101000");
    APER_ENUM_CHECK(192, &e2, ".11000000.00000001.11000000");
    APER_ENUM_CHECK(20,  &e3, ".10010100");
#undef APER_ENUM_CHECK

    TEST_DONE();
}

/* }}} */

/* }}} */
