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

#if !defined(IS_LIB_COMMON_ASN1_H) || defined(IS_LIB_COMMON_ASN1_WRITER_H)
#  error "you must include asn1.h instead"
#else
#define IS_LIB_COMMON_ASN1_WRITER_H

#include "asn1-helpers-inl.c"

/* ASN1 writing API
 * Need an example ? Please read tst-asn1-writer.[hc] .
 */

struct asn1_desc_t;

typedef struct asn1_ext_t {
    /* Packing */
    const void                *data;      /* Data.      */
    const struct asn1_desc_t  *desc;      /* Meta-data. */

    /* Unpacking */
    flag_t                     has_value;  /* For OPTIONAL ext fields only */
    pstream_t                  value;      /* ASN.1 frame */
} asn1_ext_t;

/* Optional scalar types. */
#define ASN1_OPT_OF(ctype_t)    struct { ctype_t v; bool has_field; }
typedef ASN1_OPT_OF(bool)       asn1_opt_bool_t;
typedef ASN1_OPT_OF(int8_t)     asn1_opt_int8_t;
typedef ASN1_OPT_OF(uint8_t)    asn1_opt_uint8_t;
typedef ASN1_OPT_OF(int16_t)    asn1_opt_int16_t;
typedef ASN1_OPT_OF(uint16_t)   asn1_opt_uint16_t;
typedef ASN1_OPT_OF(int32_t)    asn1_opt_int32_t;
typedef ASN1_OPT_OF(uint32_t)   asn1_opt_uint32_t;
typedef ASN1_OPT_OF(int64_t)    asn1_opt_int64_t;
typedef ASN1_OPT_OF(uint64_t)   asn1_opt_uint64_t;
#define ASN1_OPT_SET(pfx, val)  \
    (asn1_opt_##pfx##_t){ .v = (val), .has_field = true }
#define ASN1_OPT_TYPE(pfx)      asn1_opt_##pfx##_t
#define ASN1_OPT_CLEAR(pfx)     (ASN1_OPT_TYPE(pfx)){ .has_field = false }

typedef struct {
    const uint8_t *data;
    int bit_len;
} asn1_bit_string_t;

static ALWAYS_INLINE size_t asn1_bit_string_size(const asn1_bit_string_t *bs)
{
    return bs->bit_len / 8 + (bs->bit_len % 8 ? 1 : 0) + 1;
}

#define ASN1_VECTOR_OF(ctype_t)       struct { ctype_t *data; int len; }
typedef ASN1_VECTOR_OF(const void)    asn1_data_t;
typedef ASN1_VECTOR_OF(const char)    asn1_string_t;
#define ASN1_VECTOR_TYPE(sfx)         asn1_##sfx##_vector_t
#define ASN1_DEF_VECTOR(sfx, type_t) \
    typedef ASN1_VECTOR_OF(type_t) ASN1_VECTOR_TYPE(sfx)
typedef ASN1_VECTOR_OF(bool) asn1_bool_vector_t;
ASN1_DEF_VECTOR(int8, const int8_t);
ASN1_DEF_VECTOR(uint8, const uint8_t);
ASN1_DEF_VECTOR(int16, const int16_t);
ASN1_DEF_VECTOR(uint16, const uint16_t);
ASN1_DEF_VECTOR(int32, const int32_t);
ASN1_DEF_VECTOR(uint32, const uint32_t);
ASN1_DEF_VECTOR(int64, const int64_t);
ASN1_DEF_VECTOR(uint64, const uint64_t);
ASN1_DEF_VECTOR(data, const asn1_data_t);
ASN1_DEF_VECTOR(string, const asn1_string_t);
ASN1_DEF_VECTOR(bit_string, const asn1_bit_string_t);
ASN1_DEF_VECTOR(ext, const asn1_ext_t);
ASN1_DEF_VECTOR(void, void);
#define ASN1_VECTOR(ctype_t, dt, ln)  (ctype_t){ .data = dt, .len = ln }
#define ASN1_SVECTOR(ctype_t, dt) \
    ASN1_VECTOR(ctype_t, dt, sizeof(dt) / sizeof(ctype_t))

#define ASN1_DATA(data, len)  ASN1_VECTOR(asn1_data_t, data, len)
#define ASN1_DATA_NULL        ASN1_DATA(NULL, 0)
#define ASN1_SDATA(data)      ASN1_SVECTOR(asn1_data_t, data) /* static */
#define ASN1_STRDATA(data)    ASN1_VECTOR(asn1_data_t, data, sizeof(data) - 1)

#define ASN1_STRING(data, len)  ASN1_VECTOR(asn1_string_t, data, len)
#define ASN1_STRING_NULL        ASN1_STRING(NULL, 0)
#define ASN1_SSTRING(str)       ASN1_STRING(str, sizeof(str) - 1)
#define ASN1_STRSTRING(data)    ASN1_STRING(data, strlen(data)) /* dynamic */

#define ASN1_BIT_STRING(dt, bln) \
    (asn1_bit_string_t){ .data = dt, .bit_len = bln }
#define ASN1_BIT_STRING_NULL        ASN1_BIT_STRING(NULL, 0)

#define ASN1_ARRAY_OF(ctype_t)        ASN1_VECTOR_OF(ctype_t *)
#define ASN1_ARRAY_TYPE(sfx)          asn1_##sfx##_array_t
#define ASN1_DEF_ARRAY(sfx, type_t) \
    typedef ASN1_ARRAY_OF(type_t) ASN1_ARRAY_TYPE(sfx)
ASN1_DEF_ARRAY(void, void);

#define ASN1_EXT(pfx, ptr) \
    ({                                                                           \
        __unused__                                                               \
        const pfx##_t *_tmp = ptr;                                               \
        (asn1_ext_t){                                                            \
            .data = ptr,                                                         \
            .desc = ASN1_GET_DESC(pfx)                                           \
        };                                                                       \
    })

#define ASN1_EXT_CLEAR()  (asn1_ext_t){ .data = NULL }

#define ASN1_EXPLICIT(fld, val) \
{                                             \
    .fld = val,                               \
}

#define ASN1_CHOICE(typ_fld, type, ...) \
{                                             \
    .typ_fld = type,                          \
    {                                         \
        __VA_ARGS__                           \
    }                                         \
}

/** \enum enum obj_type
  * \brief Built-in types.
  */
enum obj_type {
    /* Scalar types */
    ASN1_OBJ_TYPE(bool),
    ASN1_OBJ_TYPE(_Bool) = ASN1_OBJ_TYPE(bool), /* bool is defined by macro */
    ASN1_OBJ_TYPE(int8_t),
    ASN1_OBJ_TYPE(uint8_t),
    ASN1_OBJ_TYPE(int16_t),
    ASN1_OBJ_TYPE(uint16_t),
    ASN1_OBJ_TYPE(int32_t),
    ASN1_OBJ_TYPE(uint32_t),
    ASN1_OBJ_TYPE(int64_t),
    ASN1_OBJ_TYPE(uint64_t),
    ASN1_OBJ_TYPE(enum),
    ASN1_OBJ_TYPE(NULL),
    ASN1_OBJ_TYPE(OPT_NULL),

    /* String types */
    ASN1_OBJ_TYPE(asn1_data_t),
    ASN1_OBJ_TYPE(asn1_string_t),
    ASN1_OBJ_TYPE(OPEN_TYPE),
    ASN1_OBJ_TYPE(asn1_bit_string_t),

    /* Opaque -- External */
    ASN1_OBJ_TYPE(OPAQUE),
    ASN1_OBJ_TYPE(asn1_ext_t),

    /* Sub-struct types */
    ASN1_OBJ_TYPE(SEQUENCE),
    ASN1_OBJ_TYPE(CHOICE),
    ASN1_OBJ_TYPE(UNTAGGED_CHOICE),

    /* Skip */
    ASN1_OBJ_TYPE(SKIP),
};

/** \enum asn1_object_mode
  */
enum obj_mode {
    ASN1_OBJ_MODE(MANDATORY),
    ASN1_OBJ_MODE(SEQ_OF),
    ASN1_OBJ_MODE(OPTIONAL),
};

/** \typedef asn1_pack_size_t
 *  \brief Size calculation function type.
 *  \return Size if OK, negative error code if something is wrong.
 */
typedef int32_t (asn1_pack_size_f)(const void *data);

/** \typedef asn1_pack_t
 *  \brief Serialization function type.
 *  \note Use it when calculating the length is costless or useless
 *        during serialization.
 */
typedef uint8_t *(asn1_pack_f)(uint8_t *dst, const void *data);

/** \typedef asn1_unpack_t
 *  \brief Unpacking function type.
 *  \note Take a properly delimited input stream to unserialize
 *        ASN.1 frame.
 */
typedef int (asn1_unpack_f)(pstream_t *value, mem_pool_t *mem_pool,
                            void *out);

/** \brief User side structure for opaque (user defined) mode callbacks.
 */
typedef struct asn1_void_t {
    asn1_pack_size_f *pack_size;
    asn1_pack_f      *pack;
    asn1_unpack_f    *unpack;
} asn1_void_t;

/****************************/
/* Serialization core stuff */
/****************************/

/** \enum asn1_cstd_type
  * \brief Constructed field type enumeration.
  */
enum asn1_cstd_type {
    ASN1_CSTD_TYPE_SEQUENCE,
    ASN1_CSTD_TYPE_CHOICE,
    ASN1_CSTD_TYPE_SET,
};

/* Special field information {{{ */

/* XXX we use special values (INT64_MIN, INT64_MAX) because PER support far
 *     more than 64 bits integers
 */
typedef struct asn1_int_info_t {
    int64_t       min; /* XXX INT64_MIN if minus infinity */
    int64_t       max; /* XXX INT64_MAX if infinity */

    /* Pre-processed information */
    flag_t        constrained;   /* XXX means fully constrained        */
    uint16_t      max_blen;      /* XXX needed only if constrained     */
    uint8_t       max_olen_blen; /* XXX needed only for max_blen > 16  */

    /* Extensions */
    flag_t        extended;
    int64_t       ext_min; /* XXX INT64_MIN if minus infinity */
    int64_t       ext_max; /* XXX INT64_MAX if infinity */
} asn1_int_info_t;

static inline void
asn1_int_info_update(asn1_int_info_t *info)
{
    int64_t d_max;

    if (!info)
        return;

    if (info->min == INT64_MIN || info->max == INT64_MAX) {
        info->constrained = false;

        return;
    }

    info->constrained = true;

    d_max = info->max - info->min;
    info->max_blen = u64_blen(d_max);

    if (info->max_blen > 16) {
        info->max_olen_blen = u64_blen(u64_olen(d_max));
    }
}

static inline asn1_int_info_t *asn1_int_info_init(asn1_int_info_t *info)
{
    p_clear(info, 1);

    info->min     = INT64_MIN;
    info->max     = INT64_MAX;
    info->ext_min = INT64_MIN;
    info->ext_max = INT64_MAX;

    return info;
}

typedef struct asn1_cnt_info_t {
    size_t        min;
    size_t        max; /* XXX SIZE_MAX if infinity */

    flag_t        extended;
    size_t        ext_min;
    size_t        ext_max; /* XXX SIZE_MAX if infinity */
} asn1_cnt_info_t;

static inline asn1_cnt_info_t *asn1_cnt_info_init(asn1_cnt_info_t *info)
{
    p_clear(info, 1);
    info->max     = SIZE_MAX;
    info->ext_max = SIZE_MAX;

    return info;
}

typedef struct asn1_enum_info_t {
    qv_t(u32)     values;  /* XXX Enumeration values in canonical order */
    size_t        blen;

    flag_t        extended;
} asn1_enum_info_t;

static inline asn1_enum_info_t *asn1_enum_info_init(asn1_enum_info_t *e)
{
    p_clear(e, 1);
    qv_init(u32, &e->values);

    return e;
}

GENERIC_NEW(asn1_enum_info_t, asn1_enum_info);

/* }}} */

/** \brief Define specification of an asn1 field.
  * \note This structure is designed to be used only
  *       with dedicated functions and macros.
  */
typedef struct {
    const char     *name;      /**< API field type. */
    const char     *oc_t_name; /**< C field type. */

    uint32_t        tag;       /* TODO use uint8_t */
    uint8_t         tag_len;   /* TODO remove      */
    enum obj_mode   mode      : 7;
    flag_t          pointed   : 1;

    uint16_t        offset;
    enum obj_type   type;
    uint16_t        size;       /**< Message content structure size. */

    union {
        const struct asn1_desc_t   *comp;
        asn1_void_t                 opaque;
    } u;

    asn1_int_info_t             int_info;
    asn1_cnt_info_t             str_info;
    const asn1_enum_info_t     *enum_info;

    /* XXX SEQUENCE OF only */
    asn1_cnt_info_t       seq_of_info;

    /* Only for open type fields */
    /* XXX eg. type is <...>.&<...> */
    flag_t                      is_open_type;
    size_t                      open_type_buf_len;
} asn1_field_t;

static inline void asn1_field_init_info(asn1_field_t *field)
{
    asn1_int_info_init(&field->int_info);
    asn1_cnt_info_init(&field->str_info);
    asn1_cnt_info_init(&field->seq_of_info);
}

qvector_t(asn1_field, asn1_field_t);

/** \brief Message descriptor.
  */
typedef struct asn1_desc_t {
    qv_t(asn1_field)      vec;
    enum asn1_cstd_type   type;

    /* TODO add SEQUENCE OF into constructed type enum */
    flag_t                is_seq_of;

    /* XXX CHOICE only */
    asn1_int_info_t       choice_info;

    /* PER information */
    qv_t(u16)             opt_fields;
    flag_t                extended;
    uint16_t              ext_pos;
} asn1_desc_t;

static inline asn1_desc_t *asn1_desc_init(asn1_desc_t *desc)
{
    p_clear(desc, 1);
    qv_init(asn1_field, &desc->vec);
    qv_init(u16, &desc->opt_fields);
    asn1_int_info_init(&desc->choice_info);

    return desc;
}

GENERIC_NEW(asn1_desc_t, asn1_desc);

typedef struct asn1_choice_desc_t {
    asn1_desc_t desc;
    uint8_t     choice_table[256];
} asn1_choice_desc_t;

int asn1_pack_size_(const void *st, const asn1_desc_t *desc,
                    qv_t(i32) *stack);
uint8_t *asn1_pack_(uint8_t *dst, const void *st, const asn1_desc_t *desc,
                    qv_t(i32) *stack);
int asn1_unpack_(pstream_t *ps, const asn1_desc_t *desc,
                 mem_pool_t *mem_pool, void *st, bool copy);

void asn1_reg_field(asn1_desc_t *desc, asn1_field_t *field);
void asn1_build_choice_table(asn1_choice_desc_t *desc);

const char *t_asn1_oid_print(const asn1_data_t *oid);

/* Private */
const void *asn1_opt_field(const void *field, enum obj_type type);
void *asn1_opt_field_w(void *field, enum obj_type type, bool has_field);

#endif /* IS_LIB_SIGTRAN_ASN1_WRITER_H */
