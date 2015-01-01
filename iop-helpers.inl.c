/**************************************************************************/
/*                                                                        */
/*  Copyright (C) 2004-2015 INTERSEC SA                                   */
/*                                                                        */
/*  Should you receive a copy of this source code, you must check you     */
/*  have a proper, written authorization of INTERSEC to hold it. If you   */
/*  don't have such an authorization, you must DELETE all source code     */
/*  files in your possession, and inform INTERSEC of the fact you obtain  */
/*  these files. Should you not comply to these terms, you can be         */
/*  prosecuted in the extent permitted by applicable law.                 */
/*                                                                        */
/**************************************************************************/

#ifndef IS_IOP_HELPERS_IN1_C
#define IS_IOP_HELPERS_IN1_C

#include "arith.h"

#define IOP_WIRE_FMT(o)          ((uint8_t)(o) >> 5)
#define IOP_WIRE_MASK(m)         (IOP_WIRE_##m << 5)
#define IOP_TAG(o)               ((o) & ((1 << 5) - 1))
#define IOP_LONG_TAG(n)          ((1 << 5) - 3 + (n))

#define TO_BIT(type)  (1 << (IOP_T_##type))
#define IOP_INT_OK    0x3ff
#define IOP_QUAD_OK   (TO_BIT(I64) | TO_BIT(U64) | TO_BIT(DOUBLE))
#define IOP_BLK_OK    (TO_BIT(STRING) | TO_BIT(DATA) | TO_BIT(STRUCT) \
                       | TO_BIT(UNION) | TO_BIT(XML))
#define IOP_STRUCTS_OK    (TO_BIT(STRUCT) | TO_BIT(UNION))
#define IOP_REPEATED_OPTIMIZE_OK  (TO_BIT(I8) | TO_BIT(U8) | TO_BIT(I16) \
                                   | TO_BIT(U16) | TO_BIT(BOOL))

#define IOP_MAKE_U32(a, b, c, d) \
    ((a) | ((unsigned)(b) << 8) | ((unsigned)(c) << 16) | ((unsigned)(d) << 24))

static ALWAYS_INLINE uint8_t get_len_len(uint32_t u)
{
    uint8_t bits = bsr32(u | 1);
    return 0x04040201 >> (bits & -8);
}

static ALWAYS_INLINE uint8_t get_vint32_len(int32_t i)
{
    const uint8_t zzbits = bsr32(((i >> 31) ^ (i << 1)) | 1);
    return 0x04040201 >> (zzbits & -8);
}

static ALWAYS_INLINE unsigned get_vint64_len(int64_t i)
{
    static uint8_t const sizes[8] = { 1, 2, 4, 4, 8, 8, 8, 8 };
    return sizes[bsr64(((i >> 63) ^ (i << 1)) | 1) / 8];
}

static inline bool iop_value_has(const iop_field_t *f, const void *v)
{
    switch (f->type) {
      case IOP_T_I8:  case IOP_T_U8:
        return ((opt_u8_t *)v)->has_field != 0;
      case IOP_T_I16: case IOP_T_U16:
        return ((opt_u16_t *)v)->has_field != 0;
      case IOP_T_I32: case IOP_T_U32: case IOP_T_ENUM:
        return ((opt_u32_t *)v)->has_field != 0;
      case IOP_T_I64: case IOP_T_U64:
        return ((opt_u64_t *)v)->has_field != 0;
      case IOP_T_BOOL:
        return ((opt_bool_t *)v)->has_field != 0;
      case IOP_T_DOUBLE:
        return ((opt_double_t *)v)->has_field != 0;
      case IOP_T_STRING:
      case IOP_T_XML:
      case IOP_T_DATA:
        return ((lstr_t *)v)->s != NULL;
      case IOP_T_UNION:
      case IOP_T_STRUCT:
      default:
        return *(void **)v != NULL;
    }
}

static inline bool
iop_scalar_equals(const iop_field_t *f, const void *v1, const void *v2, int n)
{
    /* Scalar types (even repeated) could be compared with one big
     * memcmp*/
    switch (f->type) {
      case IOP_T_I8:  case IOP_T_U8:
        return (!memcmp(v1, v2, sizeof(uint8_t) * n));
      case IOP_T_I16: case IOP_T_U16:
        return (!memcmp(v1, v2, sizeof(uint16_t) * n));
      case IOP_T_I32: case IOP_T_U32: case IOP_T_ENUM:
        return (!memcmp(v1, v2, sizeof(uint32_t) * n));
      case IOP_T_I64: case IOP_T_U64:
        return (!memcmp(v1, v2, sizeof(uint64_t) * n));
      case IOP_T_BOOL:
        return (!memcmp(v1, v2, sizeof(bool) * n));
      case IOP_T_DOUBLE:
        return (!memcmp(v1, v2, sizeof(double) * n));
      default:
        return false;
    }
}

static inline
void *iop_value_set_here(mem_pool_t *mp, const iop_field_t *f, void *v)
{
    switch (f->type) {
      case IOP_T_I8:  case IOP_T_U8:
        ((opt_u8_t *)v)->has_field = true;
        return v;
      case IOP_T_I16: case IOP_T_U16:
        ((opt_u16_t *)v)->has_field = true;
        return v;
      case IOP_T_I32: case IOP_T_U32: case IOP_T_ENUM:
        ((opt_u32_t *)v)->has_field = true;
        return v;
      case IOP_T_I64: case IOP_T_U64:
        ((opt_u64_t *)v)->has_field = true;
        return v;
      case IOP_T_BOOL:
        ((opt_bool_t *)v)->has_field = true;
        return v;
      case IOP_T_DOUBLE:
        ((opt_double_t *)v)->has_field = true;
        return v;
      case IOP_T_STRING:
      case IOP_T_DATA:
      case IOP_T_XML:
        return v;
      default:
        return *(void **)v = mp_new(mp, char, f->size);
    }
}

static inline void iop_value_set_absent(const iop_field_t *f, void *v)
{
    switch (f->type) {
      case IOP_T_I8:  case IOP_T_U8:
        ((opt_u8_t *)v)->has_field = false;
        return;
      case IOP_T_I16: case IOP_T_U16:
        ((opt_u16_t *)v)->has_field = false;
        return;
      case IOP_T_I32: case IOP_T_U32: case IOP_T_ENUM:
        ((opt_u32_t *)v)->has_field = false;
        return;
      case IOP_T_I64: case IOP_T_U64:
        ((opt_u64_t *)v)->has_field = false;
        return;
      case IOP_T_BOOL:
        ((opt_bool_t *)v)->has_field = false;
        return;
      case IOP_T_DOUBLE:
        ((opt_double_t *)v)->has_field = false;
        return;
      case IOP_T_STRING:
      case IOP_T_DATA:
      case IOP_T_XML:
        p_clear((lstr_t *)v, 1);
        return;
      default:
        /* Structs and unions are handled in the same way */
        *(void **)v = NULL;
        return;
    }
}

static ALWAYS_INLINE uint8_t *
pack_tag(uint8_t *dst, uint32_t tag, uint32_t taglen, uint8_t wt)
{
    if (likely(taglen < 1)) {
        *dst++ = wt | tag;
        return dst;
    }
    if (likely(taglen == 1)) {
        *dst++ = wt | IOP_LONG_TAG(1);
        *dst++ = tag;
        return dst;
    }
    *dst++ = wt | IOP_LONG_TAG(2);
    return (uint8_t *)put_unaligned_le16((void *)dst, tag);
}

static ALWAYS_INLINE uint8_t *
pack_len(uint8_t *dst, uint32_t tag, uint32_t taglen, uint32_t i)
{
    const uint32_t tags  =
        IOP_MAKE_U32(IOP_WIRE_MASK(BLK1), IOP_WIRE_MASK(BLK2),
                     IOP_WIRE_MASK(BLK4), IOP_WIRE_MASK(BLK4));
    const uint8_t  bits = bsr32(i | 1) & -8;

    dst = pack_tag(dst, tag, taglen, tags >> bits);
    if (likely(bits < 8)) {
        *dst++ = i;
        return dst;
    }
    if (likely(bits == 8))
        return (uint8_t *)put_unaligned_le16((void *)dst, i);
    return (uint8_t *)put_unaligned_le32((void *)dst, i);
}

static ALWAYS_INLINE uint8_t *
pack_int32(uint8_t *dst, uint32_t tag, uint32_t taglen, int32_t i)
{
    const uint32_t tags  =
        IOP_MAKE_U32(IOP_WIRE_MASK(INT1), IOP_WIRE_MASK(INT2),
                     IOP_WIRE_MASK(INT4), IOP_WIRE_MASK(INT4));
    const uint8_t zzbits = (bsr32(((i >> 31) ^ (i << 1)) | 1)) & -8;

    dst = pack_tag(dst, tag, taglen, tags >> zzbits);

    if (likely(zzbits < 8)) {
        *dst++ = i;
        return dst;
    }
    if (likely(zzbits == 8))
        return (uint8_t *)put_unaligned_le16((void *)dst, i);
    return (uint8_t *)put_unaligned_le32((void *)dst, i);
}

static ALWAYS_INLINE uint8_t *
pack_int64(uint8_t *dst, uint32_t tag, uint32_t taglen, int64_t i)
{
    if ((int64_t)(int32_t)i == i)
        return pack_int32(dst, tag, taglen, i);
    dst = pack_tag(dst, tag, taglen, IOP_WIRE_MASK(QUAD));
    return (uint8_t *)put_unaligned_le64((uint8_t *)dst, i);
}

/* Read in a buffer the selected field of a union */
static ALWAYS_INLINE const iop_field_t *
get_union_field(const iop_struct_t *desc, const void *val)
{
    uint16_t utag = *(uint16_t *)val;
    const iop_field_t *f = desc->fields;
    int ifield;

    ifield = iop_ranges_search(desc->ranges, desc->ranges_len, utag);
    assert(ifield >= 0);

    return f + ifield;
}

static inline const iop_field_t *
get_field_by_name(const iop_struct_t *desc, const iop_field_t *start,
                  const char *name, int len)
{
    const iop_field_t *fdesc = start;
    const iop_field_t *end   = desc->fields + desc->fields_len;

    while (fdesc < end) {
        if (fdesc->name.len == len && !memcmp(fdesc->name.s, name, len))
            return fdesc;
        fdesc++;
    }

    return NULL;
}

static inline bool
iop_field_is_defval(const iop_field_t *fdesc, const void *ptr, bool deep)
{
    assert (fdesc->repeat == IOP_R_DEFVAL);

    switch (fdesc->type) {
      case IOP_T_I8: case IOP_T_U8:
        return *(uint8_t *)ptr == (uint8_t)fdesc->u1.defval_u64;
      case IOP_T_I16: case IOP_T_U16:
        return *(uint16_t *)ptr == (uint16_t)fdesc->u1.defval_u64;
      case IOP_T_ENUM:
        return *(int *)ptr == fdesc->u0.defval_enum;
      case IOP_T_I32: case IOP_T_U32:
        return *(uint32_t *)ptr == (uint32_t)fdesc->u1.defval_u64;
      case IOP_T_I64: case IOP_T_U64:
      case IOP_T_DOUBLE:
        /* XXX double is handled like U64 because we want to compare them as
         * bit to bit */
        return *(uint64_t *)ptr == fdesc->u1.defval_u64;
      case IOP_T_BOOL:
        return fdesc->u1.defval_u64 ? *(bool *)ptr : !*(bool *)ptr;
      case IOP_T_STRING:
      case IOP_T_XML:
      case IOP_T_DATA:
        if (!fdesc->u0.defval_len) {
            /* In this case we don't care about the string pointer. An empty
             * string is an empty string whatever its pointer is. */
            return !((lstr_t *)ptr)->len;
        } else {
            /* We consider a NULL string as “take the default value please”;
             * otherwise we first check for the pointer equality and finally
             * for the string equality. */
            if (!((lstr_t *)ptr)->data) {
                return true;
            }
            if (((lstr_t *)ptr)->len != fdesc->u0.defval_len) {
                return false;
            }
            if (((lstr_t *)ptr)->data == fdesc->u1.defval_data) {
                return true;
            }
            if (deep) {
                return memcmp(((lstr_t *)ptr)->s, fdesc->u1.defval_data,
                              fdesc->u0.defval_len) == 0;
            }
            return false;
        }
      default:
        e_panic("unsupported");
    }
}

#endif
