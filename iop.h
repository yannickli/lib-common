/**************************************************************************/
/*                                                                        */
/*  Copyright (C) 2004-2016 INTERSEC SA                                   */
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
    int                 enum_len;
    int                 ranges_len;
};

struct iop_struct_t {
    const lstr_t        fullname;
    const iop_field_t  *fields;
    const int          *ranges;
    uint16_t            ranges_len;
    uint16_t            fields_len;
    unsigned            size     : 31;  /* sizeof(type);       */
    unsigned            is_union :  1;  /* struct or union ?   */
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

int   iop_ranges_search(int const *ranges, int ranges_len, int tag);

__must_check__
int __iop_skip_absent_field_desc(void *value, const iop_field_t *fdesc);

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
 * Prefer the `t_iop_bpack` macro (see iop-macros.h).
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

/** Unpack a packed IOP structure.
 *
 * This function unpacks a packed IOP structure from a pstream_t. It unpacks
 * one structure beyonds several other structures and leaves the pstream_t and
 * the end of the unpacked structure.
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

/** Skip a packer IOP structure without unpacking it.
 *
 * This function skips a packed IOP structure in a pstream_t. It leaves the
 * pstream_t and the end of the structure. This function is efficient because
 * it will not fully unpack the structure to skip. But it will not fully check
 * its validity either.
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

/* }}} */

#include "iop-macros.h"
#include "iop-xml.h"
#include "iop-json.h"
#include "iop-dso.h"

#endif
