/**************************************************************************/
/*                                                                        */
/*  Copyright (C) INTERSEC SA                                             */
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
#include "z.h"

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
write_i64_o_aligned(bb_t *bb, int64_t i64, uint8_t olen)
{
    bb_align(bb);
    assert (olen <= 8);
    bb_be_add_bits(bb, i64, olen * 8);
}

static ALWAYS_INLINE void write_u8_aligned(bb_t *bb, uint8_t u8)
{
    bb_align(bb);
    bb_be_add_bits(bb, u8, 8);
}

static ALWAYS_INLINE void write_u16_aligned(bb_t *bb, uint16_t u16)
{
    bb_align(bb);
    bb_be_add_bits(bb, u16, 16);
}

static ALWAYS_INLINE int __read_u8_aligned(bit_stream_t *bs, uint8_t *res)
{
    uint64_t r64 = 0;

    RETHROW(bs_align(bs));
    RETHROW(bs_be_get_bits(bs, 8, &r64));
    *res = r64;
    return 0;
}

static ALWAYS_INLINE int __read_u16_aligned(bit_stream_t *bs, uint16_t *res)
{
    uint64_t r64 = 0;

    RETHROW(bs_align(bs));
    RETHROW(bs_be_get_bits(bs, 16, &r64));
    *res = r64;
    return 0;
}

static ALWAYS_INLINE int __read_u64_o_aligned(bit_stream_t *bs, size_t olen,
                                              uint64_t *res)
{
    *res = 0;
    RETHROW(bs_align(bs));
    RETHROW(bs_be_get_bits(bs, olen * 8, res));
    return 0;
}

static ALWAYS_INLINE int __read_i64_o_aligned(bit_stream_t *bs, size_t olen,
                                              int64_t *res)
{
    uint64_t u = 0;

    RETHROW(__read_u64_o_aligned(bs, olen, &u));
    *res = sign_extend(u, olen * 8);
    return 0;
}


/* }}} */
/* Write {{{ */

/* Helpers {{{ */

/* Fully constrained integer - d_max < 65536 */
static ALWAYS_INLINE void
aper_write_u16_m(bb_t *bb, uint16_t u16, uint16_t blen, uint16_t d_max)
{
    bb_push_mark(bb);

    if (!blen) {
        goto end;
    }

    if (blen == 8 && d_max == 255) {
        /* "The one-octet case". */
        write_u8_aligned(bb, u16);
        goto end;
    }

    if (blen <= 8) {
        /* "The bit-field case". */
        bb_be_add_bits(bb, u16, blen);
        goto end;
    }

    assert (blen <= 16);

    /* "The two-octet case". */
    write_u16_aligned(bb, u16);
    /* FALLTHROUGH */

  end:
    e_trace_be_bb_tail(5, bb, "Constrained number (n = %u)", u16);
    bb_pop_mark(bb);
}

static ALWAYS_INLINE int
aper_write_ulen(bb_t *bb, size_t l) /* Unconstrained length */
{
    bb_push_mark(bb);

    bb_align(bb);

    e_trace_be_bb_tail(5, bb, "Align");
    bb_reset_mark(bb);

    if (l <= 127) {
        write_u8_aligned(bb, l);

        e_trace_be_bb_tail(5, bb, "Unconstrained length (l = %zd)", l);
        bb_pop_mark(bb);

        return 0;
    }

    if (l < (1 << 14)) {
        uint16_t u16  = l | (1 << 15);

        write_u16_aligned(bb, u16);

        e_trace_be_bb_tail(5, bb, "Unconstrained length (l = %zd)", l);
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
    write_i64_o_aligned(bb, v, olen);
}

/* XXX semi-constrained or constrained numbers */
static ALWAYS_INLINE void
aper_write_number(bb_t *bb, uint64_t v, const asn1_int_info_t *info)
{
    uint8_t olen;

    if (info && info->constrained) {
        if (info->max_blen <= 16) {
            aper_write_u16_m(bb, v, info->max_blen, info->d_max);
            return;
        }

        olen = u64_olen(v);
        aper_write_u16_m(bb, olen - 1, info->max_olen_blen, info->d_max);
    } else {
        olen = u64_olen(v);
        aper_write_ulen(bb, olen);
    }

    write_i64_o_aligned(bb, v, olen);
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
        bb_be_add_bits(bb, n, 1 + 6);
        return;
    }

    bb_be_add_bit(bb, true);
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
            aper_write_u16_m(bb, d, u16_blen(d_max), d_max);
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
                    return e_error("extended constraint not respected");
                }

                /* Extension present */
                bb_be_add_bit(bb, true);

                if (aper_write_ulen(bb, l) < 0) {
                    return e_error("failed to write extended length");
                }
            } else {
                return e_error("constraint not respected");
            }
        } else {
            if (info->extended) {
                /* Extension not present */
                bb_be_add_bit(bb, false);
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
                    return e_error("extended constraint not respected");
                }

                /* Extension present */
                bb_be_add_bit(bb, true);

                /* XXX Extension constraints are not PER-visible */
                aper_write_number(bb, n, NULL);

                return 0;
            } else {
                e_error("root constraint not respected: "
                        "%jd is not in [ %jd, %jd ]",
                        n, info->min, info->max);

                return -1;
            }
        } else {
            if (info->extended) {
                /* Extension not present */
                bb_be_add_bit(bb, false);
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
            bb_be_add_bit(bb, true);
            aper_write_nsnnwn(bb, val);

            return 0;
        } else {
            e_info("undeclared enumerated value: %d", val);
            return -1;
        }
    }

    if (e->extended) {
        bb_be_add_bit(bb, false);
    }

    /* XXX We suppose that enumerations cannot hold more than 255 values */
    bb_be_add_bits(bb, pos, e->blen);

    return 0;
}

/* }}} */
/* String types {{{ */

static int
aper_encode_ostring(bb_t *bb, const asn1_ostring_t *os,
                    const asn1_cnt_info_t *info)
{
    if (aper_encode_len(bb, os->len, info) < 0) {
        return e_error("octet string: failed to encode length");
    }

    if (info && info->max <= 2 && info->min == info->max
    &&  os->len == info->max)
    {
        for (size_t i = 0; i < os->len; i++) {
            bb_be_add_bits(bb, os->data[i], 8);
        }

        return 0;
    }

    bb_align(bb);
    bb_be_add_bytes(bb, os->data, os->len);

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
        return e_error("octet string: failed to encode length");
    }

    bb_be_add_bs(bb, bs);

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
    bb_be_add_bit(bb, b);
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
        e_error("ext type not supported");
        break;
      case ASN1_OBJ_TYPE(OPAQUE):
        assert (0);
        e_error("opaque type not supported");
        break;
      case ASN1_OBJ_TYPE(SKIP):
        e_error("skip not supported"); /* We cannot stand squirrels */
        break;
      case ASN1_OBJ_TYPE(OPEN_TYPE):
        e_error("open type not supported");
        break;
    }

    return 0;
}

static int
aper_encode_field(bb_t *bb, const void *v, const asn1_field_t *field)
{
    int res;

    e_trace(5, "encoding value %s:%s", field->oc_t_name, field->name);

    bb_push_mark(bb);

    if (field->is_open_type) {
        asn1_ostring_t  os;
        bb_t            buf;

        bb_inita(&buf, field->open_type_buf_len);
        aper_encode_value(&buf, v, field);

        if (!buf.len) {
            bb_be_add_byte(&buf, 0);
        }

        os = ASN1_OSTRING(buf.bytes, DIV_ROUND_UP(buf.len, 8));
        res = aper_encode_ostring(bb, &os, NULL);
        bb_wipe(&buf);
    } else {
        res = aper_encode_value(bb, v, field);
    }

    e_trace_be_bb_tail(5, bb, "Value encoding for %s:%s",
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
        e_trace(5, "sequence is extended");
        bb_be_add_bit(bb, false);
    }

    bb_push_mark(bb);

    /* Encode optional fields bit-map */
    qv_for_each_pos(u16, pos, &desc->opt_fields) {
        uint16_t            field_pos = desc->opt_fields.tab[pos];
        const asn1_field_t *field     = &desc->vec.tab[field_pos];
        const void         *opt       = GET_DATA_P(st, field, uint8_t);
        const void         *val       = asn1_opt_field(opt, field->type);

        if (val) { /* Field present */
            bb_be_add_bit(bb, true);
        } else {
            bb_be_add_bit(bb, false);
        }
    }

    e_trace_be_bb_tail(5, bb, "SEQUENCE OPTIONAL fields bit-map");
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
            return e_error("failed to encode value %s:%s",
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
        e_trace(5, "choice is extended");
        bb_be_add_bit(bb, false);
    }

    assert (desc->vec.len > 1);

    enum_field = &desc->vec.tab[0];

    index = *GET_DATA_P(st, enum_field, int);
    choice_field = &desc->vec.tab[index];
    assert (choice_field->mode == ASN1_OBJ_MODE(MANDATORY));

    if (index < 1) {
        return e_error("wrong choice initialization");
    }

    bb_push_mark(bb);

    /* XXX Indexes start from 0 */
    aper_write_number(bb, index - 1, &desc->choice_info);

    e_trace_be_bb_tail(5, bb, "CHOICE index");
    bb_pop_mark(bb);

    v = GET_DATA_P(st, choice_field, uint8_t);
    assert (v);

    if (aper_encode_field(bb, v, choice_field) < 0) {
        return e_error("failed to encode choice element %s:%s",
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

        return e_error("failed to encode SEQUENCE OF length (n = %zd)",
                       elem_cnt);
    }

    e_trace_be_bb_tail(5, bb, "SEQUENCE OF length");
    bb_pop_mark(bb);

    if (repeated_field->pointed) {
        for (size_t j = 0; j < elem_cnt; j++) {
            if (aper_encode_field(bb, ((const void **)tab)[j],
                                  repeated_field) < 0)
            {
                e_error("failed to encode array value [%zd] %s:%s",
                        j, repeated_field->oc_t_name, repeated_field->name);

                return -1;
            }
        }
    } else {
        for (size_t j = 0; j < elem_cnt; j++) {
            if (aper_encode_field(bb, tab + j * repeated_field->size,
                                  repeated_field) < 0)
            {
                e_error("failed to encode vector value [%zd] %s:%s",
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
            return e_error("failed to encode sequence of values");
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
    bb_t bb;

    bb_init_sb(&bb, sb);

    if (aper_encode_constructed(&bb, st, desc, NULL) < 0) {
        return -1;
    }

    bb_transfer_to_sb(&bb, sb);

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
aper_read_u16_m(bit_stream_t *bs, size_t blen, uint16_t *u16, uint16_t d_max)
{
    uint64_t res;
    assert (blen); /* u16 is given by constraints */

    if (blen == 8 && d_max == 255) {
        /* "The one-octet case". */
        *u16 = 0;
        if (__read_u8_aligned(bs, (uint8_t *)u16) < 0) {
            e_info("cannot read contrained integer: end of input "
                   "(expected at least one aligned octet)");
            return -1;
        }

        return 0;
    }

    if (blen <= 8) {
        /* "The bit-field case". */
        if (bs_be_get_bits(bs, blen, &res) < 0) {
            e_info("not enough bits to read constrained integer "
                   "(got %zd, need %zd)", bs_len(bs), blen);
            return -1;
        }

        *u16 = res;
        return 0;
    }

    /* "The two-octet case". */
    if (__read_u16_aligned(bs, u16) < 0) {
        e_info("cannot read constrained integer: end of input "
               "(expected at least two aligned octet left)");
        return -1;
    }
    return 0;
}

static ALWAYS_INLINE int
aper_read_ulen(bit_stream_t *bs, size_t *l)
{
    union {
        uint8_t  b[2];
        uint16_t w;
    } res;
    res.w = 0;

    if (__read_u8_aligned(bs, &res.b[1]) < 0) {
        e_info("cannot read unconstrained length: end of input "
               "(expected at least one aligned octet left)");
        return -1;
    }

    if (!(res.b[1] & (1 << 7))) {
        *l = res.b[1];
        return 0;
    }

    if (res.b[1] & (1 << 6)) {
        e_info("cannot read unconstrained length: "
               "fragmented values are not supported");
        return -1;
    }

    if (__read_u8_aligned(bs, &res.b[0]) < 0) {
        e_info("cannot read unconstrained length: end of input "
               "(expected at least a second octet left)");
        return -1;
    }

    *l = res.w & 0x7fff;
    return 0;
}

static ALWAYS_INLINE int
aper_read_2c_number(bit_stream_t *bs, int64_t *v)
{
    size_t olen;

    if (aper_read_ulen(bs, &olen) < 0) {
        e_info("cannot read unconstrained whole number length");
        return -1;
    }

    if (__read_i64_o_aligned(bs, olen, v) < 0) {
        e_info("not enough bytes to read unconstrained number "
               "(got %zd, need %zd)", bs_len(bs) / 8, olen);
        return -1;
    }

    return 0;
}

static ALWAYS_INLINE int
aper_read_number(bit_stream_t *bs, const asn1_int_info_t *info, uint64_t *v)
{
    size_t olen;

    if (info && info->constrained) {
        if (info->max_blen <= 16) {
            uint16_t u16;

            if (info->max_blen == 0) {
                *v = 0;
                return 0;
            }

            if (aper_read_u16_m(bs, info->max_blen, &u16, info->d_max) < 0) {
                e_info("cannot read constrained whole number");
                return -1;
            }

            *v = u16;

            return 0;
        } else {
            uint16_t u16;

            if (aper_read_u16_m(bs, info->max_olen_blen, &u16,
                                info->d_max) < 0)
            {
                e_info("cannot read constrained whole number length");
                return -1;
            }

            olen = u16 + 1;
        }
    } else {
        if (aper_read_ulen(bs, &olen) < 0) {
            e_info("cannot read semi-constrained whole number length");
            return -1;
        }
    }

    if (!olen) {
        e_info("forbidden number length value : 0");
        return -1;
    }

    if (__read_u64_o_aligned(bs, olen, v) < 0) {
        e_info("not enough bytes to read number (got %zd, need %zd)",
               bs_len(bs) / 8, olen);
        return -1;
    }

    return 0;
}

static int aper_read_nsnnwn(bit_stream_t *bs, size_t *n)
{
    bool is_short;
    uint64_t u64;

    if (bs_done(bs)) {
        e_info("cannot read NSNNWN: end of input");
        return -1;
    }

    is_short = !__bs_be_get_bit(bs);

    if (is_short) {
        if (!bs_has(bs, 6)) {
            e_info("cannot read short NSNNWN: not enough bits");
            return -1;
        }

        *n = __bs_be_get_bits(bs, 6);

        return 0;
    }

    if (aper_read_number(bs, NULL, &u64) < 0) {
        e_info("cannot read long form NSNNWN");
        return -1;
    }

    *n = u64;

    return 0;
}

static int
aper_read_len(bit_stream_t *bs, size_t l_min, size_t l_max, size_t *l)
{
    size_t d_max = l_max - l_min;

    if (d_max < (1 << 16)) {
        uint16_t d;

        if (!d_max) {
            *l = l_min;

            return 0;
        }

        if (aper_read_u16_m(bs, u16_blen(d_max), &d, d_max) < 0) {
            e_info("cannot read constrained length");
            return -1;
        }

        *l = l_min + d;
    } else {
        if (aper_read_ulen(bs, l) < 0) {
            e_info("cannot read unconstrained length");
            return -1;
        }
    }

    if (*l > l_max) {
        e_info("length is too high");
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
                e_info("cannot read extension bit: end of input");
                return -1;
            }

            extension_present = __bs_be_get_bit(bs);
        } else {
            extension_present = false;
        }

        if (extension_present) {
            if (aper_read_ulen(bs, l) < 0) {
                e_info("cannot read extended length");
                return -1;
            }

            if (*l < info->ext_min || *l > info->ext_max) {
                e_info("extended length constraint not respected");
                return -1;
            }
        } else {
            if (aper_read_len(bs, info->min, info->max, l) < 0) {
                e_info("cannot read constrained length");
                return -1;
            }

            if (*l < info->min || *l > info->max) {
                e_info("root length constraint not respected");
                return -1;
            }
        }
    } else {
        if (aper_read_len(bs, 0, SIZE_MAX, l) < 0) {
            e_info("cannot read unconstrained length");
            return -1;
        }
    }

    return 0;
}

static int
aper_decode_number(bit_stream_t *bs, const asn1_int_info_t *info, int64_t *n)
{
    int64_t res = 0;

    if (info && info->extended) {
        bool extension_present;

        if (bs_done(bs)) {
            e_info("cannot read extension bit: end of input");
            return -1;
        }

        extension_present = __bs_be_get_bit(bs);

        if (extension_present) {
            if (aper_read_2c_number(bs, n) < 0) {
                e_info("cannot read extended unconstrained number");
                return -1;
            }

            /* TODO check extension constraint */
            return 0;
        }
    }

    if (info && info->min != INT64_MIN) {
        uint64_t u64;

        if (aper_read_number(bs, info, &u64) < 0) {
            e_info("cannot read constrained or semi-constrained number");
            return -1;
        }

        res =  u64;
        res += info->min;

        if (info && (res < info->min || res > info->max)) {
            e_error("root constraint not respected: "
                    "%jd is not in [ %jd, %jd ]",
                    res, info->min, info->max);

            return -1;
        }
    } else {
        assert (!info || !info->constrained);

        if (aper_read_2c_number(bs, &res) < 0) {
            e_info("cannot read unconstrained number");
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
            e_info("cannot read enumerated type: end of input");
            return -1;
        }

        if (__bs_be_get_bit(bs)) {
            size_t nsnnwn;

            if (aper_read_nsnnwn(bs, &nsnnwn) < 0) {
                e_info("cannot read extended enumeration");
                return -1;
            }

            *val = nsnnwn;

            return 0;
        }
    }

    if (!bs_has(bs, e->blen)) {
        e_info("cannot read enumerated value: not enough bits");
        return -1;
    }

    pos = __bs_be_get_bits(bs, e->blen);

    if (pos >= e->values.len) {
        e_info("cannot read enumerated value: unregistered value");
        return -1;
    }

    *val = e->values.tab[pos];

    return 0;
}

static ALWAYS_INLINE int aper_decode_bool(bit_stream_t *bs, bool *b)
{
    if (bs_done(bs)) {
        e_info("cannot decode boolean: end of input");
        return -1;
    }

    *b = __bs_be_get_bit(bs);

    return 0;
}

/* }}} */
/* String types {{{ */

static int
t_aper_decode_ostring(bit_stream_t *bs, const asn1_cnt_info_t *info,
                      flag_t copy, asn1_ostring_t *os)
{
    pstream_t ps;

    if (aper_decode_len(bs, info, &os->len) < 0) {
        e_info("cannot decode octet string length");
        return -1;
    }

    if (info && info->max <= 2 && info->min == info->max
    &&  os->len == info->max)
    {
        uint8_t *buf;

        if (!bs_has(bs, os->len * 2)) {
            e_info("cannot read octet string: not enough bits");
            return -1;
        }

        buf = t_new(uint8_t, os->len + 1);

        for (size_t i = 0; i < os->len; i++) {
            buf[i] = __bs_be_get_bits(bs, 8);
        }

        os->data = buf;

        return 0;
    }

    if (bs_align(bs) < 0 || bs_get_bytes(bs, os->len, &ps) < 0) {
        e_info("cannot read octet string: not enough octets "
               "(want %zd, got %zd)", os->len, bs_len(bs) / 8);
        return -1;
    }

    os->data = ps.b;
    if (copy) {
        os->data = t_dupz(os->data, os->len);
    }

    e_trace_hex(6, "Decoded OCTET STRING", os->data, (int)os->len);

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
        e_info("cannot decode bit string length");
        return -1;
    }

    if (bs_get_bs(bs, len, str) < 0) {
        e_info("cannot read bit string: not enough bits");
        return -1;
    }

    e_trace_be_bs(6, str, "Decoded bit string");

    if (copy) {
        size_t olen = str->e.p - str->s.p;

        if (str->e.offset) {
            str->s.p = t_dup(str->e.p, olen + 1);
        } else {
            str->s.p = t_dup(str->e.p, olen);
        }
        str->e.p = str->s.p + olen;
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

    size = DIV_ROUND_UP(bs_len(&bstring), 8);
    bb_inita(&bb, size);
    bb_be_add_bs(&bb, &bstring);
    data = memp_dup(t_pool(), bb.data, size);
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
            e_trace(5, "decoded number value (n = %jd)", i64);            \
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
        e_trace(5, "decoded enum value (n = %u)", *(uint32_t *)v);
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
        e_error("ext type not supported");
        break;
      case ASN1_OBJ_TYPE(OPAQUE):
        assert (0);
        e_error("opaque type not supported");
        break;
      case ASN1_OBJ_TYPE(SKIP):
        break;
      case ASN1_OBJ_TYPE(OPEN_TYPE):
        e_error("open type not supported");
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
            e_info("cannot read OPEN TYPE field");
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
            e_info("cannot read extension bit: end of input");
            return -1;
        }

        extension_present = __bs_be_get_bit(bs);

        if (extension_present) {
            e_info("extension are not supported in sequences");
            return -1;
        }
    }

    if (!bs_has(bs, desc->opt_fields.len)) {
        e_info("cannot read optional fields bit-map: not enough bits");
        return -1;
    }

    opt_bitmap = __bs_get_bs(bs, desc->opt_fields.len);

    for (int i = 0; i < desc->vec.len; i++) {
        const asn1_field_t *field = &desc->vec.tab[i];
        void               *v;

        if (field->mode == ASN1_OBJ_MODE(OPTIONAL)) {
            if (unlikely(bs_done(&opt_bitmap))) {
                assert (0);
                return e_error("sequence is broken");
            }

            if (!__bs_be_get_bit(&opt_bitmap)) {
                asn1_opt_field_w(GET_PTR(st, field, void), field->type, false);
                continue;  /* XXX Field not present */
            }

            v = t_alloc_if_pointed(field, st);
            v = asn1_opt_field_w(GET_PTR(st, field, void), field->type, true);
        } else {
            assert (field->mode != ASN1_OBJ_MODE(SEQ_OF));

            v = t_alloc_if_pointed(field, st);
        }

        e_trace(5, "decoding SEQUENCE value %s:%s",
                field->oc_t_name, field->name);

        if (t_aper_decode_field(bs, field, copy, v) < 0) {
            e_info("cannot read sequence field %s:%s",
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
            e_info("cannot read extension bit: end of input");
            return -1;
        }

        extension_present = __bs_be_get_bit(bs);

        if (extension_present) {
            e_info("extension are not supported in choices");
            return -1;
        }

        e_trace(5, "extension not present");
    } else {
        e_trace(5, "choice is not extended");
    }

    if (aper_read_number(bs, &desc->choice_info, &u64) < 0) {
        e_info("cannot read choice index");
        return -1;
    }

    index = u64;

    e_trace(5, "decoded choice index (index = %zd)", index);

    if ((int)index + 1 >= desc->vec.len) {
        e_info("the choice index read is not compatible with the "
               "description: either the data is invalid or the description "
               "incomplete");
        return -1;
    }

    enum_field = &desc->vec.tab[0];
    choice_field = &desc->vec.tab[index + 1];   /* XXX Indexes start from 0 */
    *GET_PTR(st, enum_field, int) = index + 1;  /* Write enum value         */
    v = t_alloc_if_pointed(choice_field, st);

    assert (choice_field->mode == ASN1_OBJ_MODE(MANDATORY));
    assert (enum_field->mode == ASN1_OBJ_MODE(MANDATORY));

    e_trace(5, "decoding CHOICE value %s:%s",
            choice_field->oc_t_name, choice_field->name);

    if (t_aper_decode_field(bs, choice_field, copy, v) < 0) {
        e_info("cannot decode choice value");
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
        e_info("failed to decode SEQUENCE OF length");
        return -1;
    }

    e_trace(5, "decoded element count of SEQUENCE OF %s:%s (n = %zd)",
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

        e_trace(5, "decoding SEQUENCE OF %s:%s value [%zu/%zu]",
                repeated_field->oc_t_name, repeated_field->name, j, elem_cnt);

        if (t_aper_decode_field(bs, repeated_field, copy, v) < 0) {
            e_info("failed to decode SEQUENCE OF element");
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
        e_panic("ASN.1 SET is not supported yet; please use SEQUENCE");
        break;
    }

    return 0;
}

int t_aper_decode_desc(pstream_t *ps, const asn1_desc_t *desc,
                       flag_t copy, void *st)
{
    bit_stream_t bs = bs_init_ps(ps, 0);

    RETHROW(t_aper_decode_constructed(&bs, desc, NULL, copy, st));

    RETHROW(bs_align(&bs));
    *ps = __bs_get_bytes(&bs, bs_len(&bs) / 8);

    return 0;
}

/* }}} */

/* }}} */

#undef e_info

/* }}} */
/* Check {{{ */

Z_GROUP_EXPORT(asn1_aligned_per) {
    bit_stream_t bs;
    size_t len;

    Z_TEST(u16, "aligned per: aper_write_u16_m/aper_read_u16_m") {
        t_scope;
        BB_1k(bb);

        struct {
            size_t d, d_max, skip;
            const char *s;
        } t[] = {
            {     0,     0,  0, "" },
            {   0xe,    57,  0, ".001110" },
            {  0x8d,   255,  0, ".10001101" },
            {  0x8d,   254,  1, ".01000110.1" },
            {  0x8d,   255,  1, ".00000000.10001101" },
            { 0xabd, 33000,  0, ".00001010.10111101" },
        };

        for (int i = 0; i < countof(t); i++) {
            bb_reset(&bb);
            bb_add0s(&bb, t[i].skip);

            len = u64_blen(t[i].d_max);
            aper_write_u16_m(&bb, t[i].d, u64_blen(t[i].d_max), t[i].d_max);
            bs = bs_init_bb(&bb);
            if (len) {
                uint16_t u16 = t[i].d - 1;

                Z_ASSERT_N(bs_skip(&bs, t[i].skip));
                Z_ASSERT_N(aper_read_u16_m(&bs, len, &u16, t[i].d_max),
                           "[i:%d]", i);
                Z_ASSERT_EQ(u16, t[i].d, "[i:%d] len=%zu", i, len);
            }
            Z_ASSERT_STREQUAL(t[i].s, t_print_be_bb(&bb, NULL), "[i:%d]", i);
        }
    } Z_TEST_END;

    Z_TEST(len, "aligned per: aper_write_len/aper_read_len") {
        t_scope;
        BB_1k(bb);

        struct {
            size_t l, l_min, l_max, skip;
            const char *s;
        } t[] = {
            { 15,    15,           15, 0, "" },
            { 7,      3,           18, 0, ".0100" },
            { 15,     0, ASN1_MAX_LEN, 0, ".00001111" },
            { 0x1b34, 0, ASN1_MAX_LEN, 0, ".10011011.00110100" },
            { 32,     1,          160, 1, ".00001111.1" },
        };

        for (int i = 0; i < countof(t); i++) {
            bb_reset(&bb);
            bb_add0s(&bb, t[i].skip);

            aper_write_len(&bb, t[i].l, t[i].l_min, t[i].l_max);
            bs = bs_init_bb(&bb);
            Z_ASSERT_N(bs_skip(&bs, t[i].skip));
            Z_ASSERT_N(aper_read_len(&bs, t[i].l_min, t[i].l_max, &len),
                       "[i:%d]", i);
            Z_ASSERT_EQ(len, t[i].l, "[i:%d]", i);
            Z_ASSERT_STREQUAL(t[i].s, t_print_be_bb(&bb, NULL), "[i:%d]", i);
        }
    } Z_TEST_END;

    Z_TEST(nsnnwn, "aligned per: aper_write_nsnnwn/aper_read_nsnnwn") {
        t_scope;
        BB_1k(bb);

        struct {
            size_t n;
            const char *s;
        } t[] = {
            {   0,  ".0000000" },
            { 0xe,  ".0001110" },
            { 96,   ".10000000.00000001.01100000" },
            { 128,  ".10000000.00000001.10000000" },
        };

        for (int i = 0; i < countof(t); i++) {
            bb_reset(&bb);
            aper_write_nsnnwn(&bb, t[i].n);
            bs = bs_init_bb(&bb);
            Z_ASSERT_N(aper_read_nsnnwn(&bs, &len), "[i:%d]", i);
            Z_ASSERT_EQ(len, t[i].n, "[i:%d]", i);
            Z_ASSERT_STREQUAL(t[i].s, t_print_be_bb(&bb, NULL), "[i:%d]", i);
        }
    } Z_TEST_END;

    Z_TEST(number, "aligned per: aper_{encode,decode}_number") {
        t_scope;
        BB_1k(bb);

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

        struct {
            int64_t         i;
            asn1_int_info_t *info;
            const char      *s;
        } t[] = {
            { 1234,  &uc,  ".00000010.00000100.11010010" },
            { 1234,  NULL, ".00000010.00000100.11010010" },
            { -1234, NULL, ".00000010.11111011.00101110" },
            { 0,     NULL, ".00000001.00000000" },
            { 0,     &sc,  ".00000001.00000101" },
            { -3,    &fc1, ".010" },
            { -1,    &fc1, ".100" },
            { -1,    NULL, ".00000001.11111111" },
            { 45,    &fc2, ".00000000.00101101" },
            { 128,   &fc2, ".00000000.10000000" },
            { 256,   &fc2, ".01000000.00000001.00000000" },
            { 666,   &fc3, "" },
            { 5,     &ext, ".0101" },
            { 8,     &ext, ".10000000.00000001.00001000" },
        };

        for (int i = 0; i < countof(t); i++) {
            int64_t i64;

            bb_reset(&bb);
            asn1_int_info_update(t[i].info);
            aper_encode_number(&bb, t[i].i, t[i].info);
            bs = bs_init_bb(&bb);
            Z_ASSERT_N(aper_decode_number(&bs, t[i].info, &i64), "[i:%d]", i);
            Z_ASSERT_EQ(i64, t[i].i, "[i:%d]", i);
            Z_ASSERT_STREQUAL(t[i].s, t_print_be_bb(&bb, NULL), "[i:%d]", i);
        }
    } Z_TEST_END;

    Z_TEST(ostring, "aligned per: aper_{encode,decode}_ostring") {
        t_scope;
        BB_1k(bb);

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

        struct {
            const char      *os;
            asn1_cnt_info_t *info;
            bool             copy;
            const char      *s;
        } t[] = {
            { "aaa", &uc,   true,  ".00000011.01100001.01100001.01100001" },
            { "aaa", NULL,  true,  ".00000011.01100001.01100001.01100001" },
            { "aaa", NULL,  false, ".00000011.01100001.01100001.01100001" },
            { "aaa", &fc1,  false, ".01100001.01100001.01100001" },
            { "aaa", &fc2,  false, ".00011000.01100001.01100001.01100001" },
            { "aa",  &ext1, true,  ".01000000.01100001.01100001" },
            { "aaa", &ext1, true,  ".10000000.00000011.01100001.01100001.01100001" },
            { "aa",  &ext2, true,  ".00110000.10110000.1" },
            { "a",   &ext2, true,  ".10000000.00000001.01100001" },
        };

        for (int i = 0; i < countof(t); i++) {
            asn1_ostring_t src = {
                .data = (const uint8_t *)t[i].os,
                .len = strlen(t[i].os),
            };
            asn1_ostring_t dst;

            bb_reset(&bb);
            aper_encode_ostring(&bb, &src, t[i].info);
            if (src.len < 4) {
                Z_ASSERT_STREQUAL(t[i].s, t_print_be_bb(&bb, NULL),"[i:%d]", i);
            }
            bs = bs_init_bb(&bb);
            Z_ASSERT_N(t_aper_decode_ostring(&bs, t[i].info, t[i].copy, &dst),
                       "[i:%d]", i);
            Z_ASSERT_LSTREQUAL(LSTR_INIT_V((void *)dst.data, dst.len),
                               LSTR_INIT_V((void *)src.data, src.len),
                               "[i:%d]", i);
        }
    } Z_TEST_END;

    Z_TEST(bstring, "aligned per: aper_{encode,decode}_bstring") {
        t_scope;
        BB_1k(bb);
        BB_1k(src_bb);

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

        asn1_cnt_info_t ext3 = { /* Extended */
            .min      = 1,
            .max      = 160,
            .extended = true,
            .ext_max  = SIZE_MAX,
        };

        struct {
            const char      *bs;
            asn1_cnt_info_t *info;
            bool             copy;
            const char      *s;
        } t[] = {
            { "01010101", &uc,   false, ".00001000.01010101" },
            { "01010101", NULL,  true,  ".00001000.01010101" },
            { "101",      &fc1,  true,  ".101" },
            { "101",      &fc2,  true,  ".00011101" },
            { "10",       &ext1, true,  ".0110" },
            { "011",      &ext1, true,  ".10000000.00000011.011" },
            { "11",       &ext2, true,  ".011" },
            { "011",      &ext2, true,  ".10000000.00000011.011" },
            { "00",       &ext3, true,  ".00000000.100" },
        };

        for (int i = 0; i < countof(t); i++) {
            bit_stream_t src;
            bit_stream_t dst;

            bb_reset(&bb);
            bb_reset(&src_bb);
            for (const char *s = t[i].bs; *s; s++) {
                if (*s == '1' || *s == '0') {
                    bb_be_add_bit(&src_bb, *s == '1');
                }
            }
            src = bs_init_bb(&src_bb);
            aper_encode_bstring(&bb, &src, t[i].info);
            Z_ASSERT_STREQUAL(t[i].s, t_print_be_bb(&bb, NULL), "[i:%d]", i);
            bs = bs_init_bb(&bb);
            Z_ASSERT_N(t_aper_decode_bstring(&bs, t[i].info, t[i].copy, &dst),
                       "[i:%d]", i);
            Z_ASSERT_EQ(bs_len(&dst), bs_len(&src), "[i:%d]", i);
            Z_ASSERT(bs_equals(dst, src), "[i:%d]", i);
        }
    } Z_TEST_END;

    Z_TEST(enum, "aligned per: aper_{encode,decode}_enum") {
        t_scope;
        BB_1k(bb);

        asn1_enum_info_t e1;
        asn1_enum_info_t e2;
        asn1_enum_info_t e3;

        struct {
            uint32_t          val;
            asn1_enum_info_t *e;
            const char       *s;
        } t[] = {
            { 5,   &e1, ".0" },
            { 18,  &e1, ".1" },
            { 48,  &e2, ".00110000" },
            { 104, &e2, ".11000000.00000001.01101000" },
            { 192, &e2, ".11000000.00000001.11000000" },
            { 20,  &e3, ".10010100" },
        };

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

        for (int i = 0; i < countof(t); i++) {
            uint32_t res;

            bb_reset(&bb);
            Z_ASSERT_N(aper_encode_enum(&bb, t[i].val, t[i].e), "[i:%d]", i);
            bs = bs_init_bb(&bb);
            Z_ASSERT_N(aper_decode_enum(&bs, t[i].e, &res), "[i:%d]", i);
            Z_ASSERT_EQ(res, t[i].val, "[i:%d]", i);
            Z_ASSERT_STREQUAL(t[i].s, t_print_be_bb(&bb, NULL), "[i:%d]", i);
        }
    } Z_TEST_END;
} Z_GROUP_END

/* }}} */
