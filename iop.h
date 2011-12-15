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

#ifndef IS_LIB_COMMON_IOP_H
#define IS_LIB_COMMON_IOP_H

#include "container.h"
#include "iop-macros.h"
#include "iop-cfolder.h"

#define IOP_ABI_VERSION  2

typedef enum iop_repeat_t {
    IOP_R_REQUIRED,
    IOP_R_DEFVAL,
    IOP_R_OPTIONAL,
    IOP_R_REPEATED,
} iop_repeat_t;

typedef enum iop_wire_type_t {
    IOP_WIRE_BLK1,
    IOP_WIRE_BLK2,
    IOP_WIRE_BLK4,
    IOP_WIRE_QUAD,
    IOP_WIRE_INT1,
    IOP_WIRE_INT2,
    IOP_WIRE_INT4,
    IOP_WIRE_REPEAT,
} iop_wire_type_t;

typedef enum iop_type_t {
    IOP_T_I8,     IOP_T_U8,
    IOP_T_I16,    IOP_T_U16,
    IOP_T_I32,    IOP_T_U32,
    IOP_T_I64,    IOP_T_U64,
    IOP_T_BOOL,
    IOP_T_ENUM,
    IOP_T_DOUBLE,
    IOP_T_STRING,
    IOP_T_DATA,
    IOP_T_UNION,
    IOP_T_STRUCT,
    IOP_T_XML,
} iop_type_t;

typedef struct iop_struct_t iop_struct_t;
typedef struct iop_enum_t   iop_enum_t;

typedef struct iop_field_t {
    lstr_t       name;
    uint16_t     tag;
    uint16_t     tag_len;     /* 0 to 3                   */
    uint16_t     repeat;      /* iop_repeat_t             */
    uint16_t     type;        /* iop_type_t               */
    uint16_t     size;        /* sizeof(type);            */
    uint16_t     data_offs;   /* offset to the data       */
    /**
     *   unused for IOP_T_{U,I}{8,16,32,64}, IOP_T_DOUBLE
     *   unused for IOP_T_{UNION,STRUCT}
     *   defval_enum holds the default value for IOP_T_ENUM
     *   defval_len  holds the default value length for IOP_T_{XML,STRING,DATA}
     */
    union {
        int      defval_enum;
        int      defval_len;
    } u0;
    /**
     *   defval_u64  holds the default value for IOP_T_{U,I}{8,16,32,64}
     *   defval_d    holds the default value for IOP_T_DOUBLE
     *   defval_data holds the default value data for IOP_T_{XML,STRING,DATA}
     *   st_desc     holds a pointer to the struct desc for IOP_T_{STRUCT,UNION}
     *   en_desc     holds a pointer to the enum desc for IOP_T_ENUM
     */
    union {
        const void         *defval_data;
        uint64_t            defval_u64;
        double              defval_d;
        const iop_struct_t *st_desc;
        const iop_enum_t   *en_desc;
    } u1;
} iop_field_t;

/*
 * .ranges helps finding tags into .fields.
 * ----------------------------------------
 *
 * it's a suite of [i_1, tag_1, ... i_k, tag_k, ... t_n, i_{n+1}]
 *
 * it says that at .fields[i_k] starts a contiguous range of tags from
 * value tag_k to tag_k + i_{k+1} - i_k.
 * Of course n == .ranges_len and i_{n+1} == .fields_len.
 *
 *
 * Example:
 * -------
 *
 * If you have the tags [1, 2, 3,  7,  9, 10, 11], the ranges will read:
 *                       --v----   v   ----v----
 *                      [0, 1,   3, 7,   4, 9,   7]
 *
 * Looking for a given tag needs a binary search the odd places of the
 * ranges array. Then, if you're tag T on the kth range, the index of your
 * description in .fields is:  (i_k + T - t_k)
 *
 */
struct iop_enum_t {
    const lstr_t        name;
    const lstr_t        fullname;
    const lstr_t       *names;
    const int          *values;
    const int          *ranges;
    uint16_t            enum_len;
    uint16_t            flags;
    int                 ranges_len;
};

enum iop_enum_flags_t {
    IOP_ENUM_EXTENDED,      /**< reserved for future use */
    IOP_ENUM_STRICT,        /**< strict packing/unpacking of enum values */
};

enum iop_struct_flags_t {
    IOP_STRUCT_EXTENDED,    /**< st_attrs and field_attrs exist */
};

typedef int (*check_constraints_f)(const void *ptr, int n);

typedef struct iop_field_attr_arg_t {
    union {
        int64_t i64;
        double  d;
        lstr_t  s;
    } v;
} iop_field_attr_arg_t;

typedef enum iop_field_attr_type_t {
    IOP_FIELD_MIN_OCCURS,
    IOP_FIELD_MAX_OCCURS,
} iop_field_attr_type_t;

typedef struct iop_field_attr_t {
    iop_field_attr_type_t        type;
    const iop_field_attr_arg_t  *args;
} iop_field_attr_t;

typedef struct iop_field_attrs_t {
    check_constraints_f      check_constraints;
    unsigned                 flags;  /**< bitfield of iop_field_attr_type_t */
    uint16_t                 attrs_len;
    uint16_t                 padding;
    const iop_field_attr_t  *attrs;
} iop_field_attrs_t;

struct iop_struct_t {
    const lstr_t        fullname;
    const iop_field_t  *fields;
    const int          *ranges;
    uint16_t            ranges_len;
    uint16_t            fields_len;
    uint16_t            size;           /**< sizeof(type);                  */
    unsigned            flags    : 15;  /**< bitfield of iop_struct_flags_t */
    unsigned            is_union :  1;  /**< struct or union ?              */
    void               *st_attrs;
    const iop_field_attrs_t *fields_attrs;
};

typedef struct iop_rpc_t {
    const lstr_t        name;
    const iop_struct_t *args;
    const iop_struct_t *result;
    const iop_struct_t *exn;
    uint32_t            tag;
    bool                async;
} iop_rpc_t;

typedef struct iop_iface_t {
    const lstr_t        fullname;
    const iop_rpc_t    *funs;
    int                 funs_len;
} iop_iface_t;

typedef struct iop_iface_alias_t {
    const iop_iface_t  *iface;
    const lstr_t        name;
    uint32_t            tag;
} iop_iface_alias_t;

typedef struct iop_mod_t {
    const lstr_t fullname;
    const iop_iface_alias_t *ifaces;
    int ifaces_len;
} iop_mod_t;

typedef struct iop_pkg_t iop_pkg_t;
struct iop_pkg_t {
    const lstr_t               name;
    iop_enum_t   const *const *enums;
    iop_struct_t const *const *structs;
    iop_iface_t  const *const *ifaces;
    iop_mod_t    const *const *mods;
    iop_pkg_t    const *const *deps;
};

#define IOP_ARRAY_OF(type_t)                     \
    struct {                                     \
        union { type_t *tab; type_t *data; };    \
        int32_t len;                             \
        unsigned flags;                          \
    }
typedef struct { void *data; int32_t len; unsigned flags; } iop_data_t;

#define IOP_OPT_OF(type_t)     struct { type_t v; bool has_field; }
typedef IOP_OPT_OF(int8_t)     iop_opt_i8_t;
typedef IOP_OPT_OF(uint8_t)    iop_opt_u8_t;
typedef IOP_OPT_OF(int16_t)    iop_opt_i16_t;
typedef IOP_OPT_OF(uint16_t)   iop_opt_u16_t;
typedef IOP_OPT_OF(int32_t)    iop_opt_i32_t;
typedef IOP_OPT_OF(uint32_t)   iop_opt_u32_t;
typedef IOP_OPT_OF(int64_t)    iop_opt_i64_t;
typedef IOP_OPT_OF(uint64_t)   iop_opt_u64_t;
typedef IOP_OPT_OF(int)        iop_opt_enum_t;
typedef IOP_OPT_OF(bool)       iop_opt_bool_t;
typedef IOP_OPT_OF(double)     iop_opt_double_t;
typedef iop_opt_bool_t         iop_opt__Bool_t;

typedef struct iop__void__t { } iop__void__t;
extern iop_struct_t const iop__void__s;

void  iop_init(const iop_struct_t *, void *value);
bool  iop_equals(const iop_struct_t *, const void *v1, const void *v2);
void *iop_dup(mem_pool_t *mp, const iop_struct_t *, const void *v);
void  iop_copy(mem_pool_t *mp, const iop_struct_t *, void **, const void *v);
int   iop_ranges_search(int const *ranges, int ranges_len, int tag);

EXPORT __cold __attr_printf__(1, 2)
void iop_set_err(const char *fmt, ...);

const char *iop_get_err(void);
bool iop_field_has_constraints(const iop_struct_t *desc, const iop_field_t
                               *fdesc);
int iop_field_check_constraints(const iop_struct_t *desc, const iop_field_t
                                *fdesc, const void *ptr, int n, bool recurse);
int iop_check_constraints(const iop_struct_t *desc, const void *val);

static inline
const iop_field_attrs_t *iop_field_get_attrs(const iop_struct_t *desc,
                                             const iop_field_t *fdesc)
{
    unsigned desc_flags = desc->flags;

    if (TST_BIT(&desc_flags, IOP_STRUCT_EXTENDED) && desc->fields_attrs) {
        const iop_field_attrs_t *attrs;

        attrs = &desc->fields_attrs[fdesc - desc->fields];
        assert (attrs);

        return attrs;
    }
    return NULL;
}

static inline check_constraints_f
iop_field_get_constraints_cb(const iop_struct_t *desc,
                             const iop_field_t *fdesc)
{
    const iop_field_attrs_t *attrs = iop_field_get_attrs(desc, fdesc);

    if (attrs)
        return attrs->check_constraints;
    return NULL;
}

static inline lstr_t iop_enum_to_str(const iop_enum_t *ed, int v) {
    int res = iop_ranges_search(ed->ranges, ed->ranges_len, v);
    return unlikely(res < 0) ? CLSTR_NULL_V : ed->names[res];
}
int iop_enum_from_str(const iop_enum_t *, const char *s, int len, int err);
int iop_enum_from_str2(const iop_enum_t *, const char *s, int len, bool *found);
int iop_enum_from_lstr(const iop_enum_t *, const lstr_t s, bool *found);

#define IOP_UNION_TYPE_TO_STR(_data, _type_desc)                        \
    ({                                                                  \
        int _res = iop_ranges_search((_type_desc).ranges,               \
                                     (_type_desc).ranges_len,           \
                                     (_data)->iop_tag);                 \
        _res >= 0 ? (_type_desc).fields[_res].name.s : NULL;            \
    })

/*----- IOP binary native interfaces -----*/
__must_check__
int    iop_bpack_size(const iop_struct_t *, const void *v, qv_t(i32) *);
void   iop_bpack(void *dst, const iop_struct_t *, const void *v, const int *);

lstr_t t_iop_bpack_struct(const iop_struct_t *, const void *v);

__must_check__
int iop_bunpack(mem_pool_t *mp, const iop_struct_t *, void *value,
                pstream_t ps, bool copy);
__must_check__
int iop_bunpack_multi(mem_pool_t *mp, const iop_struct_t *, void *value,
                      pstream_t *ps, bool copy);
__must_check__
int iop_bskip(const iop_struct_t *desc, pstream_t *ps);

/** returns the length of the field examining the first octets only.
 *
 * \warning the field should be a non repeated one.
 *
 * This function is meant to know if iop_bunpack{,_multi} will work, hence it
 * should be applied to stream of IOP unions or IOP fields only.
 *
 * Returns 0 if there isn't enough octets to determine the length.
 * Returns -1 if there is something really wrong.
 */
ssize_t iop_get_field_len(pstream_t ps);

__must_check__
int __iop_skip_absent_field_desc(void *value, const iop_field_t *fdesc);

/* flags for xml/json/... unpacker */
enum iop_unpack_flags {
    IOP_UNPACK_IGNORE_UNKNOWN = (1U << 0),
};

#define IOP_EXPORT_PACKAGES_COMMON \
    __unused__ static void iop_set_err_weak(const char *fmt, ...) {         \
        fputs("ERROR: executable needs to be linked with -rdynamic\n",      \
              stderr);                                                      \
    }                                                                       \
    typeof(iop_set_err) iop_set_err                                         \
                        __attribute__((alias("iop_set_err_weak"),weak));    \
                                                                            \
    iop_struct_t const iop__void__s = {                                     \
        .fullname   = LSTR_IMMED("Void"),                                   \
        .fields_len = 0,                                                    \
        .size       = 0,                                                    \
    }

#include "iop-xml.h"
#include "iop-json.h"
#include "iop-dso.h"

#endif
