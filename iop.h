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

#ifndef IS_LIB_COMMON_IOP_H
#define IS_LIB_COMMON_IOP_H

#include "container.h"
#include "iop-cfolder.h"

/* {{{ IOP library internals */

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

enum iop_field_flags_t {
    IOP_FIELD_CHECK_CONSTRAINTS,    /**< check_constraints function exists  */
    IOP_FIELD_NO_EMPTY_ARRAY,       /**< indicates presence of @minOccurs   */
};

typedef struct iop_field_t {
    lstr_t       name;
    uint16_t     tag;
    unsigned     tag_len:  2; /**< 0 to 2                                   */
    unsigned     flags  : 14; /**< bitfield of iop_field_flags_t            */
    uint16_t     repeat;      /**< iop_repeat_t                             */
    uint16_t     type;        /**< iop_type_t                               */
    uint16_t     size;        /**< sizeof(type);                            */
    uint16_t     data_offs;   /**< offset to the data                       */
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
    IOP_STRUCT_EXTENDED,        /**< st_attrs and field_attrs exist */
    IOP_STRUCT_HAS_CONSTRAINTS, /**< will iop_check_constraints do smth? */
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
    IOP_FIELD_CDATA,
    IOP_FIELD_MIN,
    IOP_FIELD_MAX,
    IOP_FIELD_NON_EMPTY,
    IOP_FIELD_NON_ZERO,
    IOP_FIELD_MIN_LENGTH,
    IOP_FIELD_MAX_LENGTH,
    IOP_FIELD_PATTERN,
    IOP_FIELD_PRIVATE,
} iop_field_attr_type_t;

typedef struct iop_field_attr_t {
    iop_field_attr_type_t        type;
    const iop_field_attr_arg_t  *args;
} iop_field_attr_t;

typedef struct iop_field_attrs_t {
    check_constraints_f      check_constraints;
    unsigned                 flags;  /**< bitfield of iop_field_attr_type_t */
    uint16_t                 attrs_len;
    uint8_t                  version;   /**< version 0 */
    uint8_t                  padding;
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
typedef lstr_t iop_data_t;

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

int   iop_ranges_search(int const *ranges, int ranges_len, int tag);

int         iop_set_err(const char *fmt, ...) __cold __attr_printf__(1, 2);
__attribute__((format(printf, 1, 0)))
void        iop_set_verr(const char *fmt, va_list ap) __cold ;
int         iop_set_err2(const lstr_t *s) __cold;
void        iop_clear_err(void) ;

bool iop_field_has_constraints(const iop_struct_t *desc, const iop_field_t
                               *fdesc);
int iop_field_check_constraints(const iop_struct_t *desc, const iop_field_t
                                *fdesc, const void *ptr, int n, bool recurse);

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
    unsigned fdesc_flags = fdesc->flags;

    if (TST_BIT(&fdesc_flags, IOP_FIELD_CHECK_CONSTRAINTS)) {
        const iop_field_attrs_t *attrs = iop_field_get_attrs(desc, fdesc);

        return attrs->check_constraints;
    }
    return NULL;
}

static inline int
__iop_field_find_by_name(const iop_struct_t *desc, const void *s, int len)
{
    const iop_field_t *field = desc->fields;
    for (int i = 0; i < desc->fields_len; i++) {
        if (len == field->name.len && !memcmp(field->name.s, s, len))
            return i;
        field++;
    }
    return -1;
}

__must_check__
int __iop_skip_absent_field_desc(void *value, const iop_field_t *fdesc);

/*------- IOP introspection -------*/

const iop_iface_t *iop_mod_find_iface(const iop_mod_t *mod, uint32_t tag);
const iop_rpc_t   *iop_iface_find_rpc(const iop_iface_t *iface, uint32_t tag);
const iop_rpc_t   *iop_mod_find_rpc(const iop_mod_t *mod, uint32_t cmd);

/* }}} */
/* {{{ IOP structures manipulation */

/** Initialize an IOP structure with the correct default values.
 *
 * You always need to initialize your IOP structure before packing it, however
 * it is useless when you unpack a structure it will be done for you.
 *
 * Prefer the generated version instead of this low-level API (see IOP_GENERIC
 * in iop-macros.h).
 *
 * \param[in] st    The IOP structure definition (__s).
 * \param[in] value Pointer on the IOP structure to initialize.
 */
void  iop_init(const iop_struct_t *st, void *value);

/** Return whether two IOP structures are equals or not.
 *
 * Prefer the generated version instead of this low-level API (see IOP_GENERIC
 * in iop-macros.h).
 *
 * v1 and v2 can be NULL. If both v1 and v2 are NULL they are considered as
 * equals.
 *
 * \param[in] st  The IOP structures definition (__s).
 * \param[in] v1  Pointer on the IOP structure to be compared.
 * \param[in] v2  Pointer on the IOP structure to be compared with.
 */
bool  iop_equals(const iop_struct_t *st, const void *v1, const void *v2);

/** Flags for IOP sorter. */
enum iop_sort_flags {
    /* Perform a reversed sort */
    IOP_SORT_REVERSE = (1U << 0),
    /* Let the IOP objects that do not contain the sorting field at the
     * beginning of the vector (otherwise they are left at the end)
     */
    IOP_SORT_NULL_FIRST = (1U << 1),
};

/** Sort a vector of IOP structures or unions based on a given field or
 *  subfield of reference. The comparison function is the canonical comparison
 *  associated to the type of field.
 *
 * Prefer the generated version instead of this low-level API (see IOP_GENERIC
 * in iop-macros.h).
 *
 *  \param[in] st          The IOP structure definition (__s).
 *  \param[in] vec         Array of objects to sort
 *  \param[in] len         Length of the array
 *  \param[in] field_path  Path of the field of reference for sorting,
 *                         containing the names of the fields and subfield,
 *                         separated by dots
 *                         Example: "field.subfield1.subfield2"
 *  \param[in] flags       Binary combination of sorting flags (see enum
 *                         iop_sort_flags)
 */
int iop_sort(const iop_struct_t *st, void *vec, int len,
             lstr_t field_path, int flags);

/** Duplicate an IOP structure.
 *
 * The resulting IOP structure will fully contained in one block of memory.
 *
 * Prefer the generated version instead of this low-level API (see IOP_GENERIC
 * in iop-macros.h).
 *
 * \param[in] mp The memory pool to use for the new allocation. If mp is NULL
 *               the libc malloc() will be used.
 * \param[in] st The IOP structure definition (__s).
 * \param[in] v  The IOP structure to duplicate.
 */
void *iop_dup(mem_pool_t *mp, const iop_struct_t *st, const void *v);

/** Copy an IOP structure into another one.
 *
 * The destination IOP structure will reallocated to handle the source
 * structure.
 *
 * Prefer the generated version instead of this low-level API (see IOP_GENERIC
 * in iop-macros.h).
 *
 * \param[in] mp   The memory pool to use for the reallocation. If mp is NULL
 *                 the libc realloc() will be used.
 * \param[in] st   The IOP structure definition (__s).
 * \param[in] outp Pointer on the destination structure that will be
 *                 reallocated to retrieve the v IOP structure.
 * \param[in] v    The IOP structure to copy.
 */
void  iop_copy(mem_pool_t *mp, const iop_struct_t *st, void **outp,
               const void *v);

/** Generate a signature of an IOP structure.
 *
 * This function generates a salted SHA256 signature of an IOP structure.
 *
 * \param[in] st  IOP structure description.
 * \param[in] v   IOP structure to sign.
 */
lstr_t t_iop_compute_signature(const iop_struct_t *st, const void *v);

/** Check the signature of an IOP structure.
 *
 * This function checks the signature of an IOP structure.
 *
 * \param[in] st   IOP structure description.
 * \param[in] v    IOP structure to check.
 * \param[in] sig  Excepted signature.
 */
__must_check__
int iop_check_signature(const iop_struct_t *st, const void *v, lstr_t sig);

/* }}} */
/* {{{ IOP constraints handling */

/** Get the constraints error buffer.
 *
 * When a structure constraints checking fails, the error description is
 * accessible in a static buffer, accessible with this function.
 */
const char *iop_get_err(void) __cold;

/** Same as iop_get_err() but returns a lstr_t. */
lstr_t iop_get_err_lstr(void) __cold;

/** Check the constraints of an IOP structure.
 *
 * This function will check the constraints on an IOP structure and will
 * return -1 in case of constraint violation. In case of constraint violation,
 * you can use iop_get_err() to get the error message.
 *
 * Prefer the generated version instead of this low-level API (see IOP_GENERIC
 * in iop-macros.h).
 *
 * \param[in] desc  IOP structure description.
 * \param[in] val   Pointer on the IOP structure to check constraints.
 */
int iop_check_constraints(const iop_struct_t *desc, const void *val);

/* }}} */
/* {{{ IOP enum manipulation */

/** Convert IOP enum integer value to lstr_t representation.
 *
 * This function will return NULL if the integer value doesn't exist in the
 * enum set.
 *
 * Prefer the generated version instead of this low-level API (see IOP_ENUM
 * in iop-macros.h).
 *
 * \param[in] ed The IOP enum definition (__e).
 * \param[in] v  Integer value to look for.
 */
static inline lstr_t iop_enum_to_str(const iop_enum_t *ed, int v) {
    int res = iop_ranges_search(ed->ranges, ed->ranges_len, v);
    return unlikely(res < 0) ? CLSTR_NULL_V : ed->names[res];
}

/** Convert a string to its integer value using an IOP enum mapping.
 *
 * This function will return `err` if the string value doesn't exist in the
 * enum set.
 *
 * Prefer the generated version instead of this low-level API (see IOP_ENUM
 * in iop-macros.h).
 *
 * \param[in] ed  The IOP enum definition (__e).
 * \param[in] s   String value to look for.
 * \param[in] len String length (or -1 if unknown).
 * \param[in] err Value to return in case of conversion error.
 */
int iop_enum_from_str(const iop_enum_t *ed, const char *s, int len, int err);

/** Convert a string to its integer value using an IOP enum mapping.
 *
 * This function will return `-1` if the string value doesn't exist in the
 * enum set and set the `found` variable to false.
 *
 * Prefer the generated version instead of this low-level API (see IOP_ENUM
 * in iop-macros.h).
 *
 * \param[in]  ed    The IOP enum definition (__e).
 * \param[in]  s     String value to look for.
 * \param[in]  len   String length (or -1 if unknown).
 * \param[out] found Will be set to false upon failure, true otherwise.
 */
int iop_enum_from_str2(const iop_enum_t *ed, const char *s, int len,
                       bool *found);

/** Convert a lstr_t to its integer value using an IOP enum mapping.
 *
 * This function will return `-1` if the string value doesn't exist in the
 * enum set and set the `found` variable to false.
 *
 * Prefer the generated version instead of this low-level API (see IOP_ENUM
 * in iop-macros.h).
 *
 * \param[in]  ed    The IOP enum definition (__e).
 * \param[in]  s     String value to look for.
 * \param[in]  len   String length (or -1 if unknown).
 * \param[out] found Will be set to false upon failure, true otherwise.
 */
int iop_enum_from_lstr(const iop_enum_t *ed, const lstr_t s, bool *found);

/* }}} */
/* {{{ IOP binary packing/unpacking */

/** IOP binary packer modifiers. */
enum iop_bpack_flags {
    /** With this flag on, the values still equal to their default will not be
     * packed. This is good to save bandwidth but dangerous for backward
     * compatibility */
    IOP_BPACK_SKIP_DEFVAL   = (1U << 0),
};

/** Do some preliminary work to pack an IOP structure into IOP binary format.
 *
 * This function _must_ be used before the `iop_bpack` function. It will
 * compute some necessary informations.
 *
 * Prefer the generated version instead of this low-level API (see IOP_GENERIC
 * in iop-macros.h).
 *
 * \param[in]  st    The IOP structure definition (__s).
 * \param[in]  v     The IOP structure to pack.
 * \param[in]  flags Packer modifiers (see iop_bpack_flags).
 * \param[out] szs   A qvector of int32 that you have to initialize and give
 *                   after to `iop_bpack`.
 * \return
 *   This function returns the needed buffer size to pack the IOP structure.
 */
__must_check__
int iop_bpack_size_flags(const iop_struct_t *st, const void *v,
                         unsigned flags, qv_t(i32) *szs);

__must_check__
static inline int
iop_bpack_size(const iop_struct_t *st, const void *v, qv_t(i32) *szs)
{
    return iop_bpack_size_flags(st, v, 0, szs);
}


/** Pack an IOP structure into IOP binary format.
 *
 * This structure pack a given IOP structure in an existing buffer that need
 * to be big enough. The required size will be given by the `iop_bpack_size`.
 *
 * Common usage:
 *
 * <code>
 * qv_t(i32) sizes;
 * int len;
 * byte *data;
 *
 * qv_inita(i32, &sizes, 1024);
 *
 * len  = iop_bpack_size(&foo__bar__s, obj, &sizes);
 * data = p_new_raw(byte, len);
 *
 * iop_bpack(data, &foo__bar__s, obj, sizes.tab);
 * </code>
 *
 * Prefer the generated version instead of this low-level API (see IOP_GENERIC
 * in iop-macros.h).
 *
 * \param[in] st  The IOP structure definition (__s).
 * \param[in] v   The IOP structure to pack.
 * \param[in] szs The data of the qvector given to the `iop_bpack_size`
 *                function.
 */
void iop_bpack(void *dst, const iop_struct_t *st, const void *v,
               const int *szs);

/** Pack an IOP structure into IOP binary format using the t_pool().
 *
 * This version of `iop_bpack` allows to pack an IOP structure in one
 * operation and uses the t_pool() to allocate the resulting buffer.
 *
 * Prefer the generated version instead of this low-level API (see IOP_GENERIC
 * in iop-macros.h).
 *
 * \param[in] st    The IOP structure definition (__s).
 * \param[in] v     The IOP structure to pack.
 * \param[in] flags Packer modifiers (see iop_bpack_flags).
 * \return
 *   The buffer containing the packed structure.
 */
lstr_t t_iop_bpack_struct_flags(const iop_struct_t *st, const void *v,
                                const unsigned flags);

static inline lstr_t t_iop_bpack_struct(const iop_struct_t *st, const void *v)
{
    return t_iop_bpack_struct_flags(st, v, 0);
}

/** Unpack a packed IOP structure.
 *
 * This function unpacks a packed IOP structure from a pstream_t. It unpacks
 * one and only one structure, so the pstream_t must only contains the unique
 * structure to unpack.
 *
 * Prefer the generated version instead of this low-level API (see IOP_GENERIC
 * in iop-macros.h).
 *
 * \param[in] mp    The memory pool to use when memory allocation is needed.
 * \param[in] st    The IOP structure definition (__s).
 * \param[in] value Pointer on the destination structure.
 * \param[in] ps    The pstream_t containing the packed IOP structure.
 * \param[in] copy  Tell to the unpack whether complex type must be duplicated
 *                  or not (for example string could be pointers on the
 *                  pstream_t or duplicated).
 */
__must_check__
int iop_bunpack(mem_pool_t *mp, const iop_struct_t *st, void *value,
                pstream_t ps, bool copy);

/** Unpack a packed IOP structure using the t_pool().
 *
 * Prefer the generated version instead of this low-level API (see IOP_GENERIC
 * in iop-macros.h).
 */
__must_check__ static inline int
t_iop_bunpack_ps(const iop_struct_t *st, void *value, pstream_t ps, bool copy)
{
    return iop_bunpack(t_pool(), st, value, ps, copy);
}

/** Unpack a packed IOP structure.
 *
 * This function unpacks a packed IOP structure from a pstream_t. It unpacks
 * one structure beyonds several other structures and leaves the pstream_t and
 * the end of the unpacked structure.
 *
 * Prefer the generated version instead of this low-level API (see IOP_GENERIC
 * in iop-macros.h).
 *
 * \param[in] mp    The memory pool to use when memory allocation is needed.
 * \param[in] st    The IOP structure definition (__s).
 * \param[in] value Pointer on the destination structure.
 * \param[in] ps    The pstream_t containing the packed IOP structure.
 * \param[in] copy  Tell to the unpack whether complex type must be duplicated
 *                  or not (for example string could be pointers on the
 *                  pstream_t or duplicated).
 */
__must_check__
int iop_bunpack_multi(mem_pool_t *mp, const iop_struct_t *st, void *value,
                      pstream_t *ps, bool copy);

/** Unpack a packed IOP structure using the t_pool().
 *
 * Prefer the generated version instead of this low-level API (see IOP_GENERIC
 * in iop-macros.h).
 */
__must_check__ static inline int
t_iop_bunpack_multi(const iop_struct_t *st, void *value, pstream_t *ps,
                    bool copy)
{
    return iop_bunpack_multi(t_pool(), st, value, ps, copy);
}

/** Skip a packer IOP structure without unpacking it.
 *
 * This function skips a packed IOP structure in a pstream_t. It leaves the
 * pstream_t and the end of the structure. This function is efficient because
 * it will not fully unpack the structure to skip. But it will not fully check
 * its validity either.
 *
 * Prefer the generated version instead of this low-level API (see IOP_GENERIC
 * in iop-macros.h).
 *
 * \param[in] st    The IOP structure definition (__s).
 * \param[in] ps    The pstream_t containing the packed IOP structure.
 */
__must_check__
int iop_bskip(const iop_struct_t *st, pstream_t *ps);

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


/** Flags for IOP (un)packers. */
enum iop_unpack_flags {
    /* Allow the unpacker to skip unknown fields */
    IOP_UNPACK_IGNORE_UNKNOWN = (1U << 0),
    /* Make the unpacker reject private fields */
    IOP_UNPACK_FORBID_PRIVATE = (1U << 1),
};

/* }}} */

#include "iop-macros.h"
#include "iop-xml.h"
#include "iop-json.h"
#include "iop-dso.h"

#endif
