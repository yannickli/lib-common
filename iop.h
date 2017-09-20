/**************************************************************************/
/*                                                                        */
/*  Copyright (C) 2004-2017 INTERSEC SA                                   */
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

#include "container-qhash.h"
#include "container-qvector.h"

#if __has_feature(nullability)
#pragma GCC diagnostic push
#pragma GCC diagnostic error "-Wnullability-completeness"
#if __has_warning("-Wnullability-completeness-on-arrays")
#pragma GCC diagnostic ignored "-Wnullability-completeness-on-arrays"
#endif
#endif

#include "iop-cfolder.h"
#include "iop-internals.h"

#define IOP_ABI_VERSION  2

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

/* {{{ IOP various useful typedefs and functions */

qvector_t(iop_struct, const iop_struct_t * nonnull);

/** Convert an IOP identifier from CamelCase naming to C underscored naming */
lstr_t t_camelcase_to_c(lstr_t name);

/** Convert an IOP type name (pkg.CamelCase) to C underscored naming */
lstr_t t_iop_type_to_c(lstr_t fullname);

/** Returns the maximum/minimum possible value of an iop_type_t */
iop_value_t iop_type_to_max(iop_type_t type);
iop_value_t iop_type_to_min(iop_type_t type);

/** Convert an identifier from C underscored naming to CamelCase naming.
 *
 * \param[in] name The name to convert.
 * \param[in] is_first_upper Specify wether the output should start with an
 *                           upper case character, otherwise a lower case
 *                           character is emitted.
 * \param[out] out The string receiving the camelCase/CamelCase result.
 * \return 0 in case of success -1 in case of error.
 */
int c_to_camelcase(lstr_t name, bool is_first_upper, sb_t * nonnull out);

/* }}} */
/* {{{ IOP attributes and constraints */

/** Checks the constraints associated to a given field.
 *
 * \param[in] desc     Struct descriptor.
 * \param[in] fdesc    Field descriptor.
 * \param[in] values   Pointer on the field value to check.
 * \param[in] len      Number of values to check (the field can be repeated).
 * \param[in] recurse  If set, subfields should be tested too (only apply to
 *                     struct/class/union fields).
 *
 * \note The values for parameters \ref values and \ref len can easily be
 *       obtained with function \ref iop_get_field_values.
 *
 * \return -1 in case of constraints violation in the field, in that case, the
 *         error message can be retrieved with \ref iop_get_err.
 */
int iop_field_check_constraints(const iop_struct_t * nonnull desc,
                                const iop_field_t * nonnull fdesc,
                                const void * nonnull values, int len,
                                bool recurse);

static inline
const iop_field_attrs_t * nullable
iop_field_get_attrs(const iop_struct_t * nonnull desc,
                    const iop_field_t * nonnull fdesc)
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

static inline
const iop_rpc_attrs_t * nullable
iop_rpc_get_attrs(const iop_iface_t * nonnull desc,
                  const iop_rpc_t * nonnull fdesc)
{
    unsigned desc_flags = desc->flags;

    if (TST_BIT(&desc_flags, IOP_IFACE_EXTENDED) && desc->rpc_attrs) {
        const iop_rpc_attrs_t *attrs;

        attrs = &desc->rpc_attrs[fdesc - desc->funs];
        return attrs;
    }
    return NULL;
}

/** Find a generic attribute value for an IOP interface.
 *
 * See \ref iop_field_get_gen_attr for the description of exp_type and
 * val_type.
 *
 * \param[in]  iface The IOP interface definition (__if).
 * \param[in]  key   The generic attribute key.
 * \param[in]  exp_type  The expected value type.
 * \param[out] val_type  The actual value type.
 * \param[out] value The value to put the result in.
 *
 * \return 0 if the generic attribute is found, -1 otherwise.
 */
int iop_iface_get_gen_attr(const iop_iface_t * nonnull iface, lstr_t key,
                           iop_type_t exp_type,
                           iop_type_t * nullable val_type,
                           iop_value_t * nonnull value);

/** Find a generic attribute value for an IOP rpc.
 *
 * See \ref iop_field_get_gen_attr for the description of exp_type and
 * val_type.
 *
 * \param[in]  iface The IOP interface definition (__if).
 * \param[in]  rpc   The IOP rpc definition (__rpc).
 * \param[in]  key   The generic attribute key.
 * \param[in]  exp_type  The expected value type.
 * \param[out] val_type  The actual value type.
 * \param[out] value The value to put the result in.
 *
 * \return 0 if the generic attribute is found, -1 otherwise.
 */
int iop_rpc_get_gen_attr(const iop_iface_t * nonnull iface,
                         const iop_rpc_t * nonnull rpc,
                         lstr_t key, iop_type_t exp_type,
                         iop_type_t * nullable val_type,
                         iop_value_t * nonnull value);

static inline check_constraints_f * nullable
iop_field_get_constraints_cb(const iop_struct_t * nonnull desc,
                             const iop_field_t * nonnull fdesc)
{
    unsigned fdesc_flags = fdesc->flags;

    if (TST_BIT(&fdesc_flags, IOP_FIELD_CHECK_CONSTRAINTS)) {
        const iop_field_attrs_t *attrs = iop_field_get_attrs(desc, fdesc);

        return attrs->check_constraints;
    }
    return NULL;
}

static inline
bool iop_field_has_constraints(const iop_struct_t * nonnull desc,
                               const iop_field_t * nonnull fdesc)
{
    if (iop_field_get_constraints_cb(desc, fdesc))
        return true;
    if (fdesc->type == IOP_T_ENUM && fdesc->u1.en_desc->flags)
        return true;
    return false;
}

/*}}}*/
/* {{{ IOP introspection */

const iop_iface_t * nullable
iop_mod_find_iface(const iop_mod_t * nonnull mod, uint32_t tag);
const iop_rpc_t * nullable
iop_iface_find_rpc(const iop_iface_t * nonnull iface, uint32_t tag);
const iop_rpc_t * nullable
iop_mod_find_rpc(const iop_mod_t * nonnull mod, uint32_t cmd);

/** Get the string description of an iop type.
 *
 * \param[in]  type iop type
 *
 * \return the string description.
 */
const char * nonnull iop_type_get_string_desc(iop_type_t type);

/** Return whether the IOP type is scalar or not.
 *
 * A scalar type is a type that holds one and only one value (no structure,
 * class or union).
 *
 * \param[in] type The IOP type
 * \return true if the IOP type is scalar, false otherwise.
 */
bool iop_type_is_scalar(iop_type_t type);

static inline bool iop_field_is_reference(const iop_field_t * nonnull fdesc)
{
    unsigned fdesc_flags = fdesc->flags;

    return TST_BIT(&fdesc_flags, IOP_FIELD_IS_REFERENCE);
}

/** Return whether the C representation of the IOP field is a pointer or not.
 *
 * \param[in] fdesc IOP field description.
 * \return true if the associated C field is a pointer, false otherwise.
 *
 * \note For repeated fields, the function returns true if the elements of the
 *       associated array are pointers.
 */
bool iop_field_is_pointed(const iop_field_t * nonnull fdesc);

/** Get an iop_field from its name.
 *
 * Get an iop_field_t in an iop_struct_t if it exists. If \p st is a class,
 * its parents will be walked through in order to check if they contain a
 * field named \p name.
 *
 * \param[in]  st  the iop_struct_t in which the field \p name is searched.
 * \param[in]  name  the name of the field to look for.
 * \param[out] found_st  set to the class that contains the field if \p st is
 *                       a class - or to \p st otherwise - if the field is
 *                       found.
 * \param[out] found_fdesc  set to the field descriptor if the field is found.
 *
 * \return  index of the field in a structure if the field is found, -1
 *          otherwise.
 */
int iop_field_find_by_name(const iop_struct_t * nonnull st, const lstr_t name,
                           const iop_struct_t * nullable * nullable found_st,
                           const iop_field_t * nullable * nullable found_fdesc);

/** Fill a field in an iop structure.
 *
 * Fill a field of an iop structure if it is possible to set it empty or to
 * set a default value.
 *
 * \param[in]  mp  the memory pool on which the missing data will be
 *                 allocated.
 * \param[in]  value  the pointer to an iop structure of the type \p fdesc
 *                    belongs to.
 * \param[in]  desc   IOP structure description.
 * \param[in]  fdesc  the descriptor of the field to fill.
 *
 * \return  0 if the field was filled, -1 otherwise.
 */
__must_check__
int iop_skip_absent_field_desc(mem_pool_t * nonnull mp, void * nonnull value,
                               const iop_struct_t * nonnull sdesc,
                               const iop_field_t * nonnull fdesc);

int iop_ranges_search(int const * nonnull ranges, int ranges_len, int tag);

/* }}} */
/* {{{ IOP Introspection: iop_for_each_field() */

#ifdef __has_blocks

/** Anonymous type for IOP field stack. */
typedef struct iop_field_stack_t iop_field_stack_t;

/** Print an IOP field stack as a path.
 *
 * Field paths printed with this function will look like 'foo.bar[42].param'.
 */
void sb_add_iop_field_stack(sb_t *nonnull buf,
                            const iop_field_stack_t *nonnull fstack);

/** Write the result of 'sb_add_iop_field_stack' in a t-allocated lstring. */
lstr_t t_iop_field_stack_to_lstr(const iop_field_stack_t *nonnull fstack);

#define IOP_FIELD_SKIP  1

/** Callback for function 'iop_for_each_field'.
 *
 * \param[in]     st_desc  Description of the current struct/union/class.
 *
 * \param[in]     fdesc    Description of the current field.
 *
 * \param[in,out] st_ptr   Pointer on the structure.
 *
 * \param[in]     stack    Data structure containing the context of the
 *                         currently explored field starting from the root
 *                         IOP struct/union/class. Its only use for now is
 *                         allowing to print the path to the field (example:
 *                         "a.b[0].c[42]").
 *
 * \return A negative value to stop the exploration,
 *         IOP_FIELD_SKIP to avoid exploring current field (no effect if the
 *         field is not a struct/union/class), 0 otherwise.
 */
typedef int
(BLOCK_CARET iop_for_each_field_cb_b)
    (const iop_struct_t *nonnull st_desc,
     const iop_field_t *nonnull fdesc,
     void *nonnull st_ptr,
     const iop_field_stack_t *nonnull stack);

/** Explore an IOP struct/class/union recursively and call a block for each
 *  field.
 *
 *  \param[in] st_desc  Description of the struct/union/class to explore.
 *                      Can be left NULL for IOP classes.
 *  \param[in] st_ptr   Pointer on the struct/union/class to explore.
 *  \param[in] cb       See documentation for type 'iop_for_each_field_cb_b'.
 *
 *  \return A negative value (the one returned by the callback) if the
 *          exploration was interrupted, 0 otherwise.
 */
int iop_for_each_field(const iop_struct_t *nullable st_desc,
                       void *nonnull st_ptr,
                       iop_for_each_field_cb_b nonnull cb);

/** Const version for 'iop_for_each_field_cb_b'. */
typedef int
(BLOCK_CARET iop_for_each_field_const_cb_b)
    (const iop_struct_t *nonnull st_desc,
     const iop_field_t *nonnull fdesc,
     const void *nonnull st_ptr,
     const iop_field_stack_t *nonnull stack);

/** Const version for 'iop_for_each_field'. */
int iop_for_each_field_const(
    const iop_struct_t *nullable st_desc,
    const void *nonnull st_ptr,
    iop_for_each_field_const_cb_b nonnull cb);

/** Callback for function 'iop_for_each_field_fast'.
 *
 * Same as 'iop_for_each_field_cb_b' without the parameter 'stack'.
 */
typedef int
(BLOCK_CARET iop_for_each_field_fast_cb_b)(
    const iop_struct_t * nonnull st_desc,
    const iop_field_t * nonnull fdesc,
    void * nonnull st_ptr);

/** Fast version of 'iop_for_each_field'.
 *
 * This version doesn't maintain the context of exploration with the data
 * structure carried by 'iop_field_stack_t'. Should be used when this data
 * structure is not needed.
 *
 * Using this version instead of 'iop_for_each_field' brings an estimate gain
 * of 33% in CPU time.
 */
int iop_for_each_field_fast(const iop_struct_t * nullable st_desc,
                            void * nonnull st_ptr,
                            iop_for_each_field_fast_cb_b nonnull cb);

/** Const version for 'iop_for_each_field_fast_cb_b'. */
typedef int
(BLOCK_CARET iop_for_each_field_const_fast_cb_b)(
    const iop_struct_t * nonnull st_desc,
    const iop_field_t * nonnull fdesc,
    const void * nonnull st_ptr);

/** Const version of 'iop_for_each_field_fast'. */
int iop_for_each_field_const_fast(
    const iop_struct_t * nullable st_desc,
    const void * nonnull st_ptr,
    iop_for_each_field_const_fast_cb_b nonnull cb);

/** Callback for function 'iop_for_each_st'.
 *
 * \param[in]     st_desc  Description of the current struct/union/class.
 * \param[in,out] st_ptr   Pointer on the structure.
 * \param[in]     stack    Context for the field containing the current
 *                         struct/union/class.
 *
 * \return A negative value to stop the exploration,
 *         IOP_FIELD_SKIP to avoid exploring the fields of current
 *         struct/union/class.
 */
typedef int
(BLOCK_CARET iop_for_each_st_cb_b)(const iop_struct_t *nonnull st_desc,
                                   void *nonnull st_ptr,
                                   const iop_field_stack_t *nonnull stack);

/** Explore an IOP struct/union/class recursively and call a block for each
 *  struct/union/class.
 *
 *  Same as iop_for_each_field() but the callback will be called for each
 *  struct/union/class contained in the input IOP including itself, not for
 *  scalar fields.
 */
int iop_for_each_st(const iop_struct_t * nullable st_desc,
                    void * nonnull st_ptr,
                    iop_for_each_st_cb_b nonnull cb);

/** Const version for 'iop_for_each_st_cb_b'. */
typedef int
(BLOCK_CARET iop_for_each_st_const_cb_b)(
    const iop_struct_t *nonnull st_desc,
    const void *nonnull st_ptr,
    const iop_field_stack_t *nonnull stack);

/** Const version for 'iop_for_each_st'. */
int iop_for_each_st_const(const iop_struct_t *nullable st_desc,
                          const void *nonnull st_ptr,
                          iop_for_each_st_const_cb_b nonnull cb);

/** Callback for function 'iop_for_each_st_fast'.
 *
 * Same as 'iop_for_each_st_cb_b' without the parameter 'stack'.
 */
typedef int
(BLOCK_CARET iop_for_each_st_fast_cb_b)(const iop_struct_t *nonnull st_desc,
                                        void *nonnull st_ptr);

/** Fast version of 'iop_for_each_st'.
 *
 * See 'iop_for_each_field_fast'.
 */
int iop_for_each_st_fast(const iop_struct_t *nullable st_desc,
                         void *nonnull st_ptr,
                         iop_for_each_st_fast_cb_b nonnull cb);

/** Const version for 'iop_for_each_st_fast_cb_b'. */
typedef int
(BLOCK_CARET iop_for_each_st_const_fast_cb_b)(
    const iop_struct_t * nonnull st_desc,
    const void * nonnull st_ptr);

/** Const version of 'iop_for_each_st_fast'. */
int iop_for_each_st_const_fast(const iop_struct_t *nullable st_desc,
                               const void *nonnull st_ptr,
                               iop_for_each_st_const_fast_cb_b nonnull cb);

#endif

/* }}} */
/* {{{ IOP structures manipulation */

/** Initialize an IOP structure with the correct default values.
 *
 * You always need to initialize your IOP structure before packing it, however
 * it is useless when you unpack a structure it will be done for you.
 *
 * Prefer the macro version iop_init() instead of this low-level API.
 *
 * \param[in] st    The IOP structure definition (__s).
 * \param[in] value Pointer on the IOP structure to initialize.
 */
void iop_init_desc(const iop_struct_t * nonnull st, void * nonnull value);

#define iop_init(pfx, value)  ({                                             \
        pfx##__t *__v = (value);                                             \
                                                                             \
        iop_init_desc(&pfx##__s, (void *)__v);                               \
        __v;                                                                 \
    })

/** Allocate an IOP structure and initialize it with the correct
 *  default values.
 *
 * \param[in] mp  The memory pool on which the allocation will be done.
 *                Can be NULL to use malloc.
 * \param[in] st  The IOP structure definition (__s).
 */
__attribute__((malloc, warn_unused_result))
void * nonnull mp_iop_new_desc(mem_pool_t *nullable mp,
                               const iop_struct_t * nonnull st);

__attribute__((malloc, warn_unused_result))
static inline void * nonnull iop_new_desc(const iop_struct_t * nonnull st)
{
    return mp_iop_new_desc(NULL, st);
}
__attribute__((malloc, warn_unused_result))
static inline void * nonnull t_iop_new_desc(const iop_struct_t * nonnull st)
{
    return mp_iop_new_desc(t_pool(), st);
}
__attribute__((malloc, warn_unused_result))
static inline void * nonnull r_iop_new_desc(const iop_struct_t * nonnull st)
{
    return mp_iop_new_desc(r_pool(), st);
}

#define mp_iop_new(mp, pfx)  (pfx##__t *)mp_iop_new_desc(mp, &pfx##__s)
#define iop_new(pfx)         (pfx##__t *)iop_new_desc(&pfx##__s)
#define t_iop_new(pfx)       (pfx##__t *)t_iop_new_desc(&pfx##__s)
#define r_iop_new(pfx)       (pfx##__t *)r_iop_new_desc(&pfx##__s)

/** Return whether two IOP structures are equals or not.
 *
 * Prefer the macro version iop_equals instead of this low-level API.
 *
 * v1 and v2 can be NULL. If both v1 and v2 are NULL they are considered as
 * equals.
 *
 * \param[in] st  The IOP structures definition (__s).
 * \param[in] v1  Pointer on the IOP structure to be compared.
 * \param[in] v2  Pointer on the IOP structure to be compared with.
 */
bool  iop_equals_desc(const iop_struct_t * nonnull st,
                      const void * nullable v1,
                      const void * nullable v2);

#define iop_equals(pfx, v1, v2)  ({                                          \
        const pfx##__t *__v1 = (v1);                                         \
        const pfx##__t *__v2 = (v2);                                         \
                                                                             \
        iop_equals_desc(&pfx##__s, (const void *)__v1, (const void *)__v2);  \
    })

/** Print a description of the first difference between two IOP structures.
 *
 * Mainly designed for testing: give additional information when two IOP
 * structures differ when they are not supposed to.
 *
 * \return -1 if the IOP structs are equal.
 */
int iop_first_diff_desc(const iop_struct_t * nonnull st,
                        const void * nonnull v1, const void * nonnull v2,
                        sb_t * nonnull diff_desc);

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
 * Prefer the macro versions iop_sort() and iop_obj_sort() instead of this
 * low-level API.
 *
 *  \param[in] st          The IOP structure definition (__s).
 *  \param[in] vec         Array of objects to sort. If st is a class, this
 *                         must be an array of pointers on the elements, and
 *                         not an array of elements.
 *  \param[in] len         Length of the array
 *  \param[in] field_path  Path of the field of reference for sorting,
 *                         containing the names of the fields and subfield,
 *                         separated by dots
 *                         Example: "field.subfield1.subfield2"
 *  \param[in] flags       Binary combination of sorting flags (see enum
 *                         iop_sort_flags)
 *  \param[out] err        In case of error, the error description.
 */
int iop_sort_desc(const iop_struct_t * nonnull st, void * nonnull vec,
                  int len, lstr_t field_path, int flags, sb_t * nullable err);

#define iop_sort(pfx, vec, len, field_path, flags, err)  ({                  \
        pfx##__t *__vec = (vec);                                             \
                                                                             \
        iop_sort_desc(&pfx##__s, (void *)__vec, (len), (field_path),         \
                      (flags), (err));                                       \
    })

#define iop_obj_sort(pfx, vec, len, field_path, flags, err)  ({              \
        pfx##__t **__vec = (vec);                                            \
                                                                             \
        iop_sort_desc(&pfx##__s, (void *)__vec, (len), (field_path),         \
                      (flags), (err));                                       \
    })

typedef struct iop_sort_t {
    lstr_t field_path;
    int flags;
} iop_sort_t;
GENERIC_FUNCTIONS(iop_sort_t, iop_sort)

qvector_t(iop_sort, iop_sort_t);

/** Sort a vector of IOP as iop_sort, but on multiple fields.
 *
 *
 *  \param[in] st          The IOP structure definition (__s).
 *  \param[in] vec         The array to sort \see iop_sort.
 *  \param[in] len         Length of the array
 *  \param[in] params      All the path to sort on, the array will be sorted
 *                         on the first element, and in case of equality,
 *                         on the seconde one, and so on.
 *                         \see iop_sort for field path syntax and flags desc.
 *  \param[out] err        In case of error, the error description.
 */
int iop_msort_desc(const iop_struct_t * nonnull st, void * nonnull vec,
                   int len, const qv_t(iop_sort) * nonnull params,
                   sb_t * nullable err);

#define iop_msort(pfx, vec, len, params, err)  ({                            \
        pfx##__t *__vec = (vec);                                             \
                                                                             \
        iop_msort_desc(&pfx##__s, (void *)__vec, (len), (params), (err));    \
    })

#define iop_obj_msort(pfx, vec, len, params, err)  ({                        \
        pfx##__t **__vec = (vec);                                            \
                                                                             \
        iop_msort_desc(&pfx##__s, (void *)__vec, (len), (params), (err));    \
    })

/** Flags for IOP filter. */
enum iop_filter_flags {
    /** Perform a SQL-like pattern matching for strings. */
    IOP_FILTER_SQL_LIKE = (1U << 0),
};

/** Filter a vector of IOP based on a given field or subfield of reference.
 *
 *  It takes an array of IOP objets and an array of values, and filters out
 *  the objects whose field value is not in the values array.
 *
 *  \param[in] st             The IOP structure definition (__s).
 *  \param[in/out] vec        Array of objects to filter. If st is a class,
 *                            this must be an array of pointers on the
 *                            elements, and not an array of elements.
 *  \param[in/out] len        Length of the array. It is adjusted with the new
 *                            value once the filter is done.
 *  \param[in] field_path     Path of the field of reference for filtering,
 *                            containing the names of the fields and subfield,
 *                            separated by dots
 *                            Example: "field.subfield1.subfield2"
 *  \param[in] allowed_values Array of pointer on allowed values to keep
 *                            inside vec.
 *                            \warning the type of values must be the right
 *                            one because no check is done inside the
 *                            function.
 *  \param[in] values_len     Length of the array of allowed values.
 *  \param[in] flags          A combination of enum iop_filter_flags.
 *  \param[out] err           In case of error, the error description.
 */
int iop_filter(const iop_struct_t * nonnull st, void * nonnull vec,
               int * nonnull len, lstr_t field_path,
               void * const nonnull * nonnull allowed_values, int values_len,
               unsigned flags, sb_t * nullable err);

/** Filter a vector of IOP based on the presence of a given optional or
 *  repeated field or subfield.
 *
 * Same as \ref iop_filter but for optional or repeated fields only. It does
 * not take an array of value, but a parameter \p is_set telling if the fields
 * must be set (for optional fields) or non-empty (for repeated fields) to be
 * kept.
 */
int iop_filter_opt(const iop_struct_t * nonnull st, void * nonnull vec,
                   int * nonnull len, lstr_t field_path, bool is_set,
                   sb_t * nullable err);

/** Duplicate an IOP structure.
 *
 * The resulting IOP structure will fully contained in one block of memory.
 *
 * Prefer the macro versions instead of this low-level API.
 *
 * \param[in] mp The memory pool to use for the new allocation. If mp is NULL
 *               the libc malloc() will be used.
 * \param[in] st The IOP structure definition (__s).
 * \param[in] v  The IOP structure to duplicate.
 * \param[out] sz If set, filled with the size of the allocated buffer.
 */
void * nullable mp_iop_dup_desc_sz(mem_pool_t * nullable mp,
                                   const iop_struct_t * nonnull st,
                                   const void * nullable v,
                                   size_t * nullable sz);

#define mp_iop_dup_sz(mp, pfx, v, sz)  ({                                    \
        const pfx##__t *_id_v = (v);                                         \
                                                                             \
        (pfx##__t *)mp_iop_dup_desc_sz((mp), &pfx##__s, (const void *)_id_v, \
                                       (sz));                                \
    })

#define mp_iop_dup(mp, pfx, v)  mp_iop_dup_sz((mp), pfx, (v), NULL)
#define iop_dup(pfx, v)         mp_iop_dup(NULL, pfx, (v))
#define t_iop_dup(pfx, v)       mp_iop_dup(t_pool(), pfx, (v))
#define r_iop_dup(pfx, v)       mp_iop_dup(r_pool(), pfx, (v))

/** Copy an IOP structure into another one.
 *
 * The destination IOP structure will reallocated to handle the source
 * structure.
 *
 * Prefer the macro versions instead of this low-level API.
 *
 * \param[in] mp   The memory pool to use for the reallocation. If mp is NULL
 *                 the libc realloc() will be used.
 * \param[in] st   The IOP structure definition (__s).
 * \param[in] outp Pointer on the destination structure that will be
 *                 reallocated to retrieve the v IOP structure.
 * \param[in] v    The IOP structure to copy.
 * \param[out] sz If set, filled with the size of the allocated buffer.
 */
void mp_iop_copy_desc_sz(mem_pool_t * nullable mp,
                         const iop_struct_t * nonnull st,
                         void * nullable * nonnull outp,
                         const void * nullable v, size_t * nullable sz);

#define mp_iop_copy_sz(mp, pfx, outp, v, sz)  do {                           \
        pfx##__t **__outp = (outp);                                          \
        const pfx##__t *__v = (v);                                           \
                                                                             \
        mp_iop_copy_desc_sz(mp, &pfx##__s, (void **)__outp,                  \
                            (const void *)__v, (sz));                        \
    } while (0)

#define mp_iop_copy(mp, pfx, outp, v)  \
    mp_iop_copy_sz((mp), pfx, (outp), (v), NULL)
#define iop_copy(pfx, outp, v)  mp_iop_copy(NULL, pfx, (outp), (v))

/** Copy an iop object into another one.
 *
 * The destination should be an object of same or child type of the source.
 *
 * \param[in] mp    The memory pool to use in case of deep copy
 * \param[in] out   The destination IOP object address.
 * \param[in] v     The IOP object address to copy, its class type should be
 *                  an ancestor of the class type of out.
 * \param[in] flags Flags controling the copy
 *                  IOP_OBJ_DEEP_COPY: if set, data of v is duplicated
 */
#define IOP_OBJ_DEEP_COPY  (1 << 0)
void mp_iop_obj_copy(mem_pool_t * nullable mp, void * nonnull out,
                     const void * nonnull v, unsigned flags);

/** Generate a signature of an IOP structure.
 *
 * This function generates a salted SHA256 signature of an IOP structure.
 *
 * \param[in] st     IOP structure description.
 * \param[in] v      IOP structure to sign.
 * \param[in] flags  Flags modifying the hashing algorithm. The same flags
 *                   must be used when computing and checking the signature.
 */
lstr_t t_iop_compute_signature(const iop_struct_t * nonnull st,
                               const void * nonnull v, unsigned flags);

/** Check the signature of an IOP structure.
 *
 * This function checks the signature of an IOP structure.
 *
 * \param[in] st     IOP structure description.
 * \param[in] v      IOP structure to check.
 * \param[in] sig    Excepted signature.
 * \param[in] flags  Flags modifying the hashing algorithm. The same flags
 *                   must be used when computing and checking the signature.
 */
__must_check__
int iop_check_signature(const iop_struct_t * nonnull st,
                        const void * nonnull v, lstr_t sig,
                        unsigned flags);

/** Find a generic attribute value for an IOP structure.
 *
 * See \ref iop_field_get_gen_attr for the description of exp_type and
 * val_type.
 *
 * \param[in]  st    The IOP structure definition (__s).
 * \param[in]  key   The generic attribute key.
 * \param[in]  exp_type  The expected value type.
 * \param[out] val_type  The actual value type.
 * \param[out] value The value to put the result in.
 *
 * \return 0 if the generic attribute is found, -1 otherwise.
 */
int iop_struct_get_gen_attr(const iop_struct_t * nonnull st, lstr_t key,
                            iop_type_t exp_type,
                            iop_type_t * nullable val_type,
                            iop_value_t * nonnull value);

/** Find a generic attribute value for an IOP field.
 *
 * If exp_type is >= 0, the type of the generic attribute value will be
 * checked, and the function will return -1 if the type is not compatible.
 * If val_type is not NULL, the type of the generic attribute value will be
 * set (IOP_T_I64, IOP_T_DOUBLE or IOP_T_STRING).
 *
 * \param[in]  st    The IOP structure definition (__s).
 * \param[in]  field The IOP field definition.
 * \param[in]  key   The generic attribute key.
 * \param[in]  exp_type  The expected value type.
 * \param[out] val_type  The actual value type.
 * \param[out] value The value to put the result in.
 *
 * \return 0 if the generic attribute is found, -1 otherwise.
 */
int iop_field_get_gen_attr(const iop_struct_t * nonnull st,
                           const iop_field_t * nonnull field,
                           lstr_t key, iop_type_t exp_type,
                           iop_type_t * nullable val_type,
                           iop_value_t * nonnull value);

/** Get boolean generic attribute value for an IOP field.
 *
 * \param[in]  st    The IOP structure definition.
 * \param[in]  field The IOP field definition.
 * \param[in]  key   The generic attribute key.
 * \param[in]  def   Default value returned if the attribute \p key did not
 *                   match.
 *
 * \return \p def if the generic attribute is not found and the attribute
 *            value otherwise.
 */
bool iop_field_get_bool_gen_attr(const iop_struct_t * nonnull st,
                                 const iop_field_t * nonnull field, lstr_t key,
                                 bool def);

/** Find a generic attribute value for an IOP field.
 *
 * Same as \ref iop_field_get_gen_attr but a name for the field is given
 * instead of field definition.
 *
 * See \ref iop_field_get_gen_attr for the description of exp_type and
 * val_type.
 *
 * \param[in]  st         The IOP structure definition (__s).
 * \param[in]  field_name The field name.
 * \param[in]  key        The generic attribute key.
 * \param[in]  exp_type   The expected value type.
 * \param[out] val_type   The actual value type.
 * \param[out] value      The value to put the result in.
 *
 * \return 0 if the generic attribute is found, -1 if the field is unknown or
 *         if the generic attribute is not found.
 */
int iop_field_by_name_get_gen_attr(const iop_struct_t * nonnull st,
                                   lstr_t field_name,
                                   lstr_t key, iop_type_t exp_type,
                                   iop_type_t * nullable val_type,
                                   iop_value_t * nonnull value);

/** Get a pointer to the field value of an optional field (if it exists).
 *
 * The return value is useful to distinguish the case where the option
 * is set, but there is no data for it (optional void field).
 *
 * \param[in] type The type of the field.
 * \param[in] data A pointer to the optional field.
 * \param[out] value The value to put the result in.
 *
 * \return true if the option is set, false otherwise.
 */
bool iop_opt_field_is_set(iop_type_t type, void * nonnull data,
                          void * nullable * nonnull value);

/** Find an IOP field description from a iop object.
 *
 * \param[in]  ptr      The IOP object.
 * \param[in]  st       The iop_struct_t describing the object.
 * \param[in]  path     The path to the field (separate members with a '.').
 * \param[out] out_ptr  A pointer to the final IOP object.
 * \param[out] out_st   Descriptor of the structure that contains
 *                      the returned field.
 *
 * \return The iop field description if found, NULL otherwise.
 */
const iop_field_t * nullable
iop_get_field(const void * nullable ptr, const iop_struct_t * nonnull st,
              lstr_t path, const void * nullable * nullable out_ptr,
              const iop_struct_t * nullable * nullable out_st);

/** Get the value(s) associated to a given IOP field.
 *
 * Efficient IOP field value getter that allows to abstract the fact that the
 * field is mandatory, optional, repeated, is a scalar, is a class, is a
 * reference, etc.
 *
 * Purpose: simplify tasks based on IOP introspection, for example systematic
 * treatments to apply to all fields of a given type.
 *
 * ┏━━━━━━━━━━━┯━━━━━━━━━━━━━━┳━━━━━━━━┯━━━━━━━━━━━━━━━━━━━━━━┓
 * ┃ repeat    │ type         ┃ len    │ is_array_of_pointers ┃
 * ┣━━━━━━━━━━━┿━━━━━━━━━━━━━━╋━━━━━━━━┿━━━━━━━━━━━━━━━━━━━━━━┫
 * ┃ MANDATORY │ *            ┃ 1      │ false                ┃
 * ┃ DEFAULT   │ *            ┃ 1      │ false                ┃
 * ┃ OPTIONAL  │ *            ┃ 0 or 1 │ false                ┃
 * ┃ REPEATED  │ struct/union ┃ N      │ false                ┃
 * ┃           │ class        ┃ N      │ true                 ┃
 * ┗━━━━━━━━━━━┷━━━━━━━━━━━━━━┻━━━━━━━━┷━━━━━━━━━━━━━━━━━━━━━━┛
 *
 * \param[in]  fdesc    Field description.
 *
 * \param[in]  st_ptr   Pointer on the struct containing the field.
 *
 * \param[out] values   Pointer on the value or the array of values. Can be an
 *                      simple array or an array of pointers depending on the
 *                      value in \p is_array_of_pointers. There is no memory
 *                      allocation, it points directly in the IOP data given
 *                      with \p st_ptr.
 *
 * \param[out] len      Number of values in the array (can be zero).
 *
 * \param[out] is_array_of_pointers  Indicates that the output array \p values
 *                                   is an array of pointers.
 */
void iop_get_field_values(const iop_field_t * nonnull fdesc,
                          void * nonnull st_ptr,
                          void * nullable * nonnull values, int * nonnull len,
                          bool * nullable is_array_of_pointers);

/** Read-only version of iop_get_field_values(). */
void iop_get_field_values_const(const iop_field_t * nonnull fdesc,
                                const void * nonnull st_ptr,
                                const void * nullable * nonnull values,
                                int * nonnull len,
                                bool * nullable is_array_of_pointers);

/** Return code for iop_value_from_field. */
typedef enum iop_value_from_field_res_t {
    IOP_FIELD_NOT_SET = -2,
    IOP_FIELD_ERROR   = -1,
    IOP_FIELD_SUCCESS = 0,
} iop_value_from_field_res_t;

/** Get an IOP value from an IOP field and an IOP object.
 *
 * \param[in]  ptr   The IOP object.
 * \param[in]  field The IOP field definition.
 * \param[out] value The value to put the result in.
 *
 * \return \ref iop_value_from_field_res_t.
 */
iop_value_from_field_res_t
iop_value_from_field(const void * nonnull ptr,
                     const iop_field_t * nonnull field,
                     iop_value_t * nonnull value);

/** Set a field of an IOP object from an IOP value and an IOP field.
 *
 * \param[in] ptr   The IOP object.
 * \param[in] field The IOP field definition.
 * \param[in] value The value to put the field.
 */
int iop_value_to_field(void * nonnull ptr, const iop_field_t * nonnull field,
                       const iop_value_t * nonnull value);

/** Set one of the values of a repeated IOP field of an IOP object.
 *
 * \param[in] ptr   The IOP object.
 * \param[in] field The IOP field definition.
 * \param[in] pos   The index at which the value \ref value should be set in
 *                  the repeated field \ref field.
 * \param[in] value The value to put at the \ref pos'th position in the
 *                  repeated field \ref field.
 */
int iop_value_to_repeated_field(void * nonnull ptr,
                                const iop_field_t * nonnull field,
                                uint32_t pos,
                                const iop_value_t * nonnull value);

/** Set an optional field of an IOP object.
 *
 * For optional scalar fields (integers, double, boolean, enum), this function
 * sets the `has_field` flag to true, without modifying the value.
 *
 * For string/data/xml fields, it ensures the `s` field is not NULL (setting
 * it to the empty string if needed).
 *
 * Other types of fields are not supported.
 *
 * \param[in] ptr   The IOP object.
 * \param[in] field The IOP field definition.
 */
void iop_set_opt_field(void * nonnull ptr, const iop_field_t * nonnull field);

/** Used for iop_type_vector_to_iop_struct function.
 */
typedef struct iop_field_info_t {
    lstr_t       name;
    iop_type_t   type;
    iop_repeat_t repeat;
    union {
        const iop_struct_t * nonnull st_desc;
        const iop_enum_t   * nonnull en_desc;
    } u1;
} iop_field_info_t;

qvector_t(iop_field_info, iop_field_info_t);

/** Get an IOP struct from a vector of IOP type. Does not work with types
 * IOP_T_STRUCT, IOP_T_UNION and IOP_T_ENUM.
 *
 * \param[in]  fullname    The full name of the IOP struct.
 * \param[in]  types       The vector of IOP types.
 * \param[in]  fields_name The vector of fields name corresponding to the types.
 *
 * \return the IOP struct.
*/
iop_struct_t * nonnull
iop_type_vector_to_iop_struct(mem_pool_t * nullable mp, lstr_t fullname,
                              const qv_t(iop_field_info) * nonnull fields_info);


/** Private intermediary structure for IOP struct/union formatting. */
struct iop_struct_value {
    /* Struct/union description, can be null only when the element is an
     * object.
     */
    const iop_struct_t * nullable st;

    const void * nonnull val;
};

/** Provide the appropriate arguments to the %*pS modifier.
 *
 * '%*pS' can be used in format string in order to print the JSON-encoded
 * content of an IOP stuct or union. You can provide the flags to be used to
 * the JSON packer in the format arguments (\ref iop_jpack_flags).
 *
 * \param[in]  pfx    IOP struct descriptor prefix.
 * \param[in]  _val   The IOP struct or union to print.
 * \param[in]  _flags The JSON packing flags
 *
 * See \ref IOP_ST_FMT_ARG() for a convenience helper to print compact JSON.
 */
#define IOP_ST_FMT_ARG_FLAGS(pfx, _val, _flags)                              \
    (_flags), &(struct iop_struct_value){                                    \
        .st = &pfx##__s,                                                     \
        .val = ({ const pfx##__t *__val = (_val); __val; }) }

/** Provide the appropriate argument to print compact JSON with the %*pS
 * format.
 *
 * This macro is a convenience helper for \ref IOP_ST_FMT_ARG_FLAGS to
 * cover the usual use case where compact JSON is needed.
 *
 * \param[in]  pfx  IOP struct descriptor prefix.
 * \param[in]  _val The IOP struct or union to print.
 *
 * \note If the struct is actually a class, use \ref IOP_OBJ_FMT_ARG
 *       formatting helper instead to get the code smaller.
 */
#define IOP_ST_FMT_ARG(pfx, _val)                                            \
    IOP_ST_FMT_ARG_FLAGS(pfx, _val,                                          \
                         IOP_JPACK_NO_WHITESPACES | IOP_JPACK_NO_TRAILING_EOL)

/** Provide the appropriate arguments to the %*pU modifier.
 *
 * '%*pU' can be used in format string in order to print the selected field
 * type of the union given as an argument.
 *
 * \param[in]  pfx    IOP union descriptor prefix.
 * \param[in]  _val   The IOP union to print.
 */
#define IOP_UNION_FMT_ARG(pfx, val)                                          \
    ({ const pfx##__t *__val = (val); __val->iop_tag; }), &pfx##__s

/* }}} */
/* {{{ IOP snmp manipulation */

static inline bool iop_struct_is_snmp_obj(const iop_struct_t * nonnull st)
{
    unsigned st_flags = st->flags;

    return TST_BIT(&st_flags, IOP_STRUCT_IS_SNMP_OBJ);
}

static inline bool iop_struct_is_snmp_tbl(const iop_struct_t * nonnull st)
{
    unsigned st_flags = st->flags;

    return TST_BIT(&st_flags, IOP_STRUCT_IS_SNMP_TBL);
}

static inline bool iop_struct_is_snmp_st(const iop_struct_t * nonnull st)
{
    unsigned st_flags = st->flags;

    return TST_BIT(&st_flags, IOP_STRUCT_IS_SNMP_OBJ)
        || TST_BIT(&st_flags, IOP_STRUCT_IS_SNMP_TBL);
}

static inline bool iop_struct_is_snmp_param(const iop_struct_t * nonnull st)
{
    unsigned st_flags = st->flags;

    return TST_BIT(&st_flags, IOP_STRUCT_IS_SNMP_PARAM);
}

static inline bool iop_field_has_snmp_info(const iop_field_t * nonnull f)
{
    unsigned st_flags = f->flags;

    return TST_BIT(&st_flags, IOP_FIELD_HAS_SNMP_INFO);
}

static inline bool iop_iface_is_snmp_iface(const iop_iface_t * nonnull iface)
{
    unsigned st_flags = iface->flags;

    return TST_BIT(&st_flags, IOP_IFACE_IS_SNMP_IFACE);
}

static inline bool iop_field_is_snmp_index(const iop_field_t * nonnull field)
{
    unsigned st_flags = field->flags;

    return TST_BIT(&st_flags, IOP_FIELD_IS_SNMP_INDEX);
}

int iop_struct_get_nb_snmp_indexes(const iop_struct_t * nonnull st);

/** Get the number of SNMP indexes used by the AgentX layer (cf RFC RFC 2578).
 */
int iop_struct_get_nb_snmp_smiv2_indexes(const iop_struct_t * nonnull st);

const iop_snmp_attrs_t * nonnull
iop_get_snmp_attrs(const iop_field_attrs_t * nonnull attrs);
const iop_snmp_attrs_t * nonnull
iop_get_snmp_attr_match_oid(const iop_struct_t * nonnull st, int oid);
const iop_field_attrs_t * nonnull
iop_get_field_attr_match_oid(const iop_struct_t * nonnull st, int tag);

/* }}} */
/* {{{ IOP class manipulation */

static inline bool iop_struct_is_class(const iop_struct_t * nonnull st)
{
    unsigned st_flags = st->flags;

    return TST_BIT(&st_flags, IOP_STRUCT_IS_CLASS);
}

static inline bool iop_field_is_class(const iop_field_t * nonnull f)
{
    if (f->type != IOP_T_STRUCT) {
        return false;
    }
    return iop_struct_is_class(f->u1.st_desc);
}

/** Gets the value of a class variable (static field).
 *
 * This takes a class instance pointer and a class variable name, and returns
 * a pointer of the value of this class variable for the given object type.
 *
 * If the wanted static field does not exist in the given class, this
 * function will return NULL.
 *
 * It also assumes that the given pointer is a valid pointer on a valid class
 * instance. If not, it will probably crash...
 *
 * \param[in]  obj   Pointer on a class instance.
 * \param[in]  name  Name of the wanted class variable.
 */
__attr_nonnull__((1))
const iop_value_t * nullable
iop_get_cvar(const void * nonnull obj, lstr_t name);

#define iop_get_cvar_cst(obj, name)  iop_get_cvar(obj, LSTR(name))

/** Gets the value of a class variable (static field) from a class descriptor.
 *
 * Same as iop_get_cvar, but directly takes a class descriptor.
 */
__attr_nonnull__((1))
const iop_value_t * nullable
iop_get_cvar_desc(const iop_struct_t * nonnull desc, lstr_t name);

#define iop_get_cvar_desc_cst(desc, name)  \
    iop_get_cvar_desc(desc, LSTR(name))

/* The following variants of iop_get_cvar do not recurse on parents */
__attr_nonnull__((1))
const iop_value_t * nullable
iop_get_class_cvar(const void * nonnull obj, lstr_t name);

#define iop_get_class_cvar_cst(obj, name)  \
    iop_get_class_cvar(obj, LSTR(name))

__attr_nonnull__((1))
const iop_value_t * nullable
iop_get_class_cvar_desc(const iop_struct_t * nonnull desc, lstr_t name);

#define iop_get_class_cvar_desc_cst(desc, name)  \
    iop_get_class_cvar_desc(desc, LSTR(name))


/** Check if the static fields types are available for a given class.
 *
 * \param[in]  desc  pointer to the class descriptor
 *
 * \return  true if and only if the type of static fields can be read
 */
__attr_nonnull__((1))
static inline bool
iop_class_static_fields_have_type(const iop_struct_t * nonnull desc)
{
    unsigned flags = desc->flags;
    return TST_BIT(&flags, IOP_STRUCT_STATIC_HAS_TYPE);
}

/** Read the static field type if available.
 *
 * \param[in]  desc  pointer to the class descriptor containing
 *                   the static field
 * \param[in]  f     static field of which we want to read the type
 *
 * \return  the iop_type_t value of the static field type if available
 *          else -1
 */
__attr_nonnull__((1, 2))
static inline int
iop_class_static_field_type(const iop_struct_t * nonnull desc,
                            const iop_static_field_t * nonnull f)
{
    THROW_ERR_UNLESS(iop_class_static_fields_have_type(desc));
    return f->type;
}

/** Checks if a class has another class in its parents.
 *
 * \param[in]  cls1  Pointer on the first class descriptor.
 * \param[in]  cls2  Pointer on the second class descriptor.
 *
 * \return  true if \p cls1 is equal to \p cls2, or has \p cls2 in its parents
 */
__attr_nonnull__((1, 2))
bool iop_class_is_a(const iop_struct_t * nonnull cls1,
                    const iop_struct_t * nonnull cls2);

/** Checks if an object is of a given class or has it in its parents.
 *
 * If the result of this check is true, then this object can be cast to the
 * given type using iop_obj_vcast or iop_obj_ccast.
 *
 * \param[in]  obj   Pointer on a class instance.
 * \param[in]  desc  Pointer on a class descriptor.
 *
 * \return  true if \p obj is an object of class \p desc, or has \p desc in
 *          its parents.
 */
__attr_nonnull__((1, 2)) static inline bool
iop_obj_is_a_desc(const void * nonnull obj,
                  const iop_struct_t * nonnull desc)
{
    return iop_class_is_a(*(const iop_struct_t **)obj, desc);
}

#define iop_obj_is_a(obj, pfx)  ({                                           \
        typeof(*(obj)) *__obj = (obj);                                       \
                                                                             \
        assert (__obj->__vptr);                                              \
        iop_obj_is_a_desc((void *)(__obj), &pfx##__s);                       \
    })

/** Get the descriptor of a class from its fullname.
 *
 * The wanted class must have the same master class than the given class
 * descriptor.
 */
__attr_nonnull__((1)) const iop_struct_t * nullable
iop_get_class_by_fullname(const iop_struct_t * nonnull st, lstr_t fullname);

/** Get the descriptor of a class from its id.
 *
 * Manipulating class ids should be reserved to some very specific use-cases,
 * so before using this function, be SURE that you really need it.
 */
const iop_struct_t * nullable
iop_get_class_by_id(const iop_struct_t * nonnull st, uint16_t class_id);

#ifdef __has_blocks
typedef void (BLOCK_CARET iop_for_each_class_b)(const iop_struct_t * nonnull);

/** Loop on all the classes registered by `iop_register_packages`.
 */
void iop_for_each_registered_classes(iop_for_each_class_b nonnull cb);
#endif

const iop_field_t * nullable
_iop_class_get_next_field(const iop_struct_t * nonnull * nonnull st,
                          int * nonnull it);

/** Loop on all fields of a class and its parents.
 *
 *  f is an already-declared iop_field_t where the results will be put in.
 *
 *  st is an already-declared iop_struct_t where the class of each field put
 *  in f will be put in.
 */
#define iop_class_for_each_field(f, st, _cl)                                \
    st = _cl;                                                               \
    for (int _i_##f = 0; (f = _iop_class_get_next_field(&st, &_i_##f));)

#define iop_obj_for_each_field(f, st, _obj)                                 \
    iop_class_for_each_field(f, st, (_obj)->__vptr)


/** Provide the appropriate arguments to print an IOP class instance in JSON
 *  with the %*pS modifier.
 *
 * '%*pS' can be used in format string in order to print the JSON-encoded
 * content of an IOP object (instance of an IOP class). You can provide the
 * flags to be used to the JSON packer in the format arguments
 * (\ref iop_jpack_flags).
 *
 * \param[in]  _obj   The object to print.
 * \param[in]  _flags The JSON packing flags
 *
 * See \ref IOP_OBJ_FMT_ARG() for a convenience helper to print compact JSON.
 */
#define IOP_OBJ_FMT_ARG_FLAGS(_obj, _flags)                                  \
    (_flags),                                                                \
    &(struct iop_struct_value){                                              \
        .st = NULL,                                                          \
        .val = ({                                                            \
            typeof(*_obj) *_fmt_obj = (_obj);                                \
            __unused__ const iop_struct_t *__ofa_st = _fmt_obj->__vptr;      \
            _fmt_obj;                                                        \
        })                                                                   \
    }

/** Provide the appropriate argument to print an IOP class instance in compact
 *  JSON with the %*pS modifier.
 *
 * This macro is a convenience helper for \ref IOP_OBJ_FMT_ARG_FLAGS to
 * cover the usual use case where compact JSON is needed.
 *
 * \param[in]  _obj  The object to print.
 */
#define IOP_OBJ_FMT_ARG(_obj)                                                \
    IOP_OBJ_FMT_ARG_FLAGS((_obj),                                            \
                          IOP_JPACK_NO_WHITESPACES | IOP_JPACK_NO_TRAILING_EOL)

/* }}} */
/* {{{ IOP constraints handling */

/** Get the constraints error buffer.
 *
 * When a structure constraints checking fails, the error description is
 * accessible in a static buffer, accessible with this function.
 */
const char * nullable iop_get_err(void) __cold;

/** Same as iop_get_err() but returns a lstr_t. */
lstr_t iop_get_err_lstr(void) __cold;

/** Check the constraints of an IOP structure.
 *
 * This function will check the constraints on an IOP structure and will
 * return -1 in case of constraint violation. In case of constraint violation,
 * you can use iop_get_err() to get the error message.
 *
 * Prefer the macro version iop_check_constraints() instead of this low-level
 * API.
 *
 * \param[in] desc  IOP structure description.
 * \param[in] val   Pointer on the IOP structure to check constraints.
 */
int iop_check_constraints_desc(const iop_struct_t * nonnull desc,
                               const void * nonnull val);

#define iop_check_constraints(pfx, val)  ({                                  \
        const pfx##__t *__v = (val);                                         \
                                                                             \
        iop_check_constraints_desc(&pfx##__s, (const void *)__v);            \
    })

/* }}} */
/* {{{ IOP enum manipulation */

qm_kvec_t(iop_enum, lstr_t, const iop_enum_t * nonnull,
          qhash_lstr_hash, qhash_lstr_equal);

/** Get an enumeration from its fullname. */
const iop_enum_t * nullable iop_get_enum(lstr_t fullname);

/** Convert IOP enum integer value to lstr_t representation.
 *
 * This function will return NULL if the integer value doesn't exist in the
 * enum set.
 *
 * Prefer the macro version iop_enum_to_lstr() instead of this low-level API.
 *
 * \param[in] ed The IOP enum definition (__e).
 * \param[in] v  Integer value to look for.
 */
static inline lstr_t iop_enum_to_str_desc(const iop_enum_t * nonnull ed, int v)
{
    int res = iop_ranges_search(ed->ranges, ed->ranges_len, v);
    return unlikely(res < 0) ? LSTR_NULL_V : ed->names[res];
}

#define iop_enum_to_lstr(pfx, v)  ({                                         \
        const pfx##__t _etl_v = (v);                                         \
                                                                             \
        iop_enum_to_str_desc(&pfx##__e, _etl_v);                             \
    })
#define iop_enum_to_str(pfx, v)  iop_enum_to_lstr(pfx, (v)).s

#define iop_enum_exists(pfx, v)  ({                                          \
        const pfx##__t _ee_v = (v);                                          \
                                                                             \
        iop_ranges_search(pfx##__e.ranges, pfx##__e.ranges_len, _ee_v) >= 0; \
    })

/** Convert a string to its integer value using an IOP enum mapping.
 *
 * This function will return `err` if the string value doesn't exist in the
 * enum set.
 *
 * Prefer the macro version iop_enum_from_str() instead of this low-level API.
 *
 * \param[in] ed  The IOP enum definition (__e).
 * \param[in] s   String value to look for.
 * \param[in] len String length (or -1 if unknown).
 * \param[in] err Value to return in case of conversion error.
 */
int iop_enum_from_str_desc(const iop_enum_t * nonnull ed,
                           const char * nonnull s, int len,
                           int err);

#define iop_enum_from_str(pfx, s, len, err)  \
    iop_enum_from_str_desc(&pfx##__e, (s), (len), (err))

/** Convert a string to its integer value using an IOP enum mapping.
 *
 * This function will return `-1` if the string value doesn't exist in the
 * enum set and set the `found` variable to false.
 *
 * Prefer the macro version iop_enum_from_str2() instead of this low-level
 * API.
 *
 * \param[in]  ed    The IOP enum definition (__e).
 * \param[in]  s     String value to look for.
 * \param[in]  len   String length (or -1 if unknown).
 * \param[out] found Will be set to false upon failure, true otherwise.
 */
int iop_enum_from_str2_desc(const iop_enum_t * nonnull ed,
                            const char * nonnull s, int len,
                            bool * nonnull found);

#define iop_enum_from_str2(pfx, s, len, found)  \
    iop_enum_from_str2_desc(&pfx##__e, (s), (len), (found))

/** Convert a lstr_t to its integer value using an IOP enum mapping.
 *
 * This function will return `-1` if the string value doesn't exist in the
 * enum set and set the `found` variable to false.
 *
 * Prefer the macro version iop_enum_from_lstr() instead of this low-level
 * API.
 *
 * \param[in]  ed    The IOP enum definition (__e).
 * \param[in]  s     String value to look for.
 * \param[in]  len   String length (or -1 if unknown).
 * \param[out] found Will be set to false upon failure, true otherwise.
 */
int iop_enum_from_lstr_desc(const iop_enum_t * nonnull ed,
                            const lstr_t s, bool * nonnull found);

#define iop_enum_from_lstr(pfx, s, found)  \
    iop_enum_from_lstr_desc(&pfx##__e, (s), (found))


/** Find a generic attribute value for an IOP enum.
 *
 * See \ref iop_field_get_gen_attr for the description of exp_type and
 * val_type.
 *
 * \param[in]  ed    The IOP enum definition (__e).
 * \param[in]  key   The generic attribute key.
 * \param[in]  exp_type  The expected value type.
 * \param[out] val_type  The actual value type.
 * \param[out] value The value to put the result in.
 *
 * \return 0 if the generic attribute is found, -1 otherwise.
 */
int iop_enum_get_gen_attr(const iop_enum_t * nonnull ed, lstr_t key,
                          iop_type_t exp_type, iop_type_t * nullable val_type,
                          iop_value_t * nonnull value);

/** Find a generic attribute value for an IOP enum value (integer).
 *
 * See \ref iop_field_get_gen_attr for the description of exp_type and
 * val_type.
 *
 * \param[in]  ed    The IOP enum definition (__e).
 * \param[in]  val   The enum value (integer).
 * \param[in]  key   The generic attribute key.
 * \param[in]  exp_type  The expected value type.
 * \param[out] val_type  The actual value type.
 * \param[out] value The value to put the result in.
 *
 * \return 0 if the generic attribute is found, -1 otherwise.
 */
int iop_enum_get_gen_attr_from_val(const iop_enum_t * nonnull ed, int val,
                                   lstr_t key, iop_type_t exp_type,
                                   iop_type_t * nullable val_type,
                                   iop_value_t * nonnull value);

/** Find a generic attribute value for an IOP enum value (string).
 *
 * See \ref iop_field_get_gen_attr for the description of exp_type and
 * val_type.
 *
 * \param[in]  ed    The IOP enum definition (__e).
 * \param[in]  val   The enum value (string).
 * \param[in]  key   The generic attribute key.
 * \param[in]  exp_type  The expected value type.
 * \param[out] val_type  The actual value type.
 * \param[out] value The value to put the result in.
 *
 * \return 0 if the generic attribute is found, -1 otherwise.
 */
int iop_enum_get_gen_attr_from_str(const iop_enum_t * nonnull ed, lstr_t val,
                                   lstr_t key, iop_type_t exp_type,
                                   iop_type_t * nullable val_type,
                                   iop_value_t * nonnull value);

/** Private intermediary structure for IOP enum formatting. */
struct iop_enum_value {
    const iop_enum_t * nonnull desc;
    int v;
};

/** Flag to use in order to display enums the following way:
 *  "<litteral value>(<int value>)".
 *
 *  Examples: "FOO(0)", "BAR(1)".
 */
#define IOP_ENUM_FMT_FULL  (1 << 0)

/** Provide the appropriate arguments to the %*pE modifier.
 *
 * '%*pE' can be used in format string in order to print enum values. An
 * additional flag \ref IOP_ENUM_FMT_FULL can be provided to display both
 * litteral and integer values.
 *
 * \param[in]  pfx    IOP enum descriptor prefix.
 * \param[in]  _val   The IOP enum value to print.
 * \param[in]  _flags The IOP enum formatting flags.
 */
#define IOP_ENUM_FMT_ARG_FLAGS(pfx, _val, _flags)                            \
    _flags,                                                                  \
    &(struct iop_enum_value){                                                \
        .desc = &pfx##__e,                                                   \
        .v = ({ pfx##__t _v = (_val); _v; })                                 \
    }

/** Provide the appropriate arguments to print the litteral form of an enum
 *  with the %*pE modifier.
 *
 * \note If the value has no litteral form, the numeric value will be printed
 *       instead.
 *
 * \param[in]  pfx    IOP enum descriptor prefix.
 * \param[in]  _val   The IOP enum value to print.
 */
#define IOP_ENUM_FMT_ARG(pfx, _v)  IOP_ENUM_FMT_ARG_FLAGS(pfx, _v, 0)

/* }}} */
/* {{{ IOP binary packing/unpacking */

/** IOP binary packer modifiers. */
enum iop_bpack_flags {
    /** With this flag on, the values still equal to their default will not be
     * packed. This is good to save bandwidth but dangerous for backward
     * compatibility */
    IOP_BPACK_SKIP_DEFVAL   = (1U << 0),

    /** With this flag on, packing can fail if the constraints are not
     * respected. The error message is available with iop_get_err. */
    IOP_BPACK_STRICT        = (1U << 1),

    /** With this flag on, packing will omit private fields.
     */
    IOP_BPACK_SKIP_PRIVATE  = (1U << 2),
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
 *   This function returns the needed buffer size to pack the IOP structure,
 *   or -1 if the IOP_BPACK_STRICT flag was used and a constraint was
 *   violated.
 */
__must_check__
int iop_bpack_size_flags(const iop_struct_t * nonnull st,
                         const void * nonnull v,
                         unsigned flags, qv_t(i32) * nonnull szs);

__must_check__
static inline int
iop_bpack_size(const iop_struct_t * nonnull st, const void * nonnull v,
               qv_t(i32) * nonnull szs)
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
 * qv_inita(&sizes, 1024);
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
void iop_bpack(void * nonnull dst, const iop_struct_t * nonnull st,
               const void * nonnull v, const int * nonnull szs);

/** Pack an IOP structure into IOP binary format using a specific mempool.
 *
 * This version of `iop_bpack` allows to pack an IOP structure in one
 * operation and uses the given mempool to allocate the resulting buffer.
 *
 * \param[in] st    The IOP structure definition (__s).
 * \param[in] v     The IOP structure to pack.
 * \param[in] flags Packer modifiers (see iop_bpack_flags).
 * \return
 *   The buffer containing the packed structure, or LSTR_NULL if the
 *   IOP_BPACK_STRICT flag was used and a constraint was violated.
 */
lstr_t mp_iop_bpack_struct_flags(mem_pool_t * nullable mp,
                                 const iop_struct_t * nonnull st,
                                 const void * nonnull v, const unsigned flags);

/** Pack an IOP structure into IOP binary format using the t_pool().
 */
lstr_t t_iop_bpack_struct_flags(const iop_struct_t * nonnull st,
                                const void * nonnull v,
                                const unsigned flags);

static inline lstr_t t_iop_bpack_struct(const iop_struct_t * nonnull st,
                                        const void * nonnull v)
{
    return t_iop_bpack_struct_flags(st, v, 0);
}

/** Flags for IOP (un)packers. */
enum iop_unpack_flags {
    /** Allow the unpacker to skip unknown fields.
     *
     * This flag applies to the json and xml packers.
     */
    IOP_UNPACK_IGNORE_UNKNOWN = (1U << 0),

    /** Make the unpacker reject private fields.
     *
     * This flag applies to the binary, json and xml packers.
     */
    IOP_UNPACK_FORBID_PRIVATE = (1U << 1),

    /** With this flag, packing will copy strings instead of making them
     * point to the packed value when possible.
     *
     * This flag applies to the binary unpacker.
     */
    IOP_UNPACK_COPY_STRINGS    = (1U << 2),

    /** With this flag set, the unpacker will expect the fields names to be
     * in C case instead of camelCase.
     *
     * This flag applies to the json unpacker.
     */
    IOP_UNPACK_USE_C_CASE = (1U << 3),
};

/** Unpack a packed IOP structure.
 *
 * This function unpacks a packed IOP structure from a pstream_t. It unpacks
 * one and only one structure, so the pstream_t must only contain the unique
 * structure to unpack.
 *
 * This function cannot be used to unpack a class; use `iop_bunpack_ptr`
 * instead.
 *
 * \warning If needed, iop_bunpack will allocate memory for each field. So if
 * the mem pool is not frame based, you may end up with a memory leak.
 *
 * \param[in] mp    The memory pool to use when memory allocation is needed.
 * \param[in] st    The IOP structure definition (__s).
 * \param[in] value Pointer on the destination structure.
 * \param[in] ps    The pstream_t containing the packed IOP structure.
 * \param[in] flags A combination of \ref iop_unpack_flags to alter the
 *                  behavior of the unpacker.
 */
__must_check__
int iop_bunpack_flags(mem_pool_t * nonnull mp,
                      const iop_struct_t * nonnull st,
                      void * nonnull value,
                      pstream_t ps, unsigned flags);

__must_check__
static inline int iop_bunpack(mem_pool_t * nonnull mp,
                              const iop_struct_t * nonnull st,
                              void * nonnull value, pstream_t ps, bool copy)
{
    return iop_bunpack_flags(mp, st, value, ps,
                             copy ? IOP_UNPACK_COPY_STRINGS : 0);
}

/** Unpack a packed IOP structure using the t_pool().
 */
__must_check__ static inline int
t_iop_bunpack_ps(const iop_struct_t * nonnull st, void * nonnull value,
                 pstream_t ps, bool copy)
{
    return iop_bunpack(t_pool(), st, value, ps, copy);
}

/** Unpack a packed IOP object and (re)allocates the destination structure.
 *
 * This function acts as `iop_bunpack` but allocates (or reallocates) the
 * destination structure.
 *
 * This function MUST be used to unpack a class instead of `iop_bunpack`,
 * because the size of a class is not known before unpacking it (this could be
 * a child).
 *
 * \warning If needed, iop_bunpack will allocate memory for each field. So if
 * the mem pool is not frame based, you may end up with a memory leak.
 *
 * \param[in] mp    The memory pool to use when memory allocation is needed;
 *                  will be used at least to allocate the destination
 *                  structure.
 * \param[in] st    The IOP structure/class definition (__s).
 * \param[in] value Double pointer on the destination structure.
 *                  If *value is not NULL, it is reallocated.
 * \param[in] ps    The pstream_t containing the packed IOP object.
 * \param[in] flags A combination of \ref iop_unpack_flags to alter the
 *                  behavior of the unpacker.
 */
__must_check__
int iop_bunpack_ptr_flags(mem_pool_t * nonnull mp,
                          const iop_struct_t * nonnull st,
                          void * nullable * nonnull value, pstream_t ps,
                          unsigned flags);

__must_check__
static inline int iop_bunpack_ptr(mem_pool_t * nonnull mp,
                                  const iop_struct_t * nonnull st,
                                  void * nullable * nonnull value,
                                  pstream_t ps, bool copy)
{
    return iop_bunpack_ptr_flags(mp, st, value, ps,
                                 copy ? IOP_UNPACK_COPY_STRINGS : 0);
}

/** Unpack a packed IOP union.
 *
 * This function act like `iop_bunpack` but consume the pstream and doesn't
 * check that the pstream has been fully consumed. This allows to unpack
 * a suite of unions.
 *
 * \param[in] mp    The memory pool to use when memory allocation is needed.
 * \param[in] st    The IOP structure definition (__s).
 * \param[in] value Pointer on the destination unpacked IOP union.
 * \param[in] ps    The pstream_t containing the packed IOP union. In case of
 *                  unpacking failure, it is left untouched.
 * \param[in] flags A combination of \ref iop_unpack_flags to alter the
 *                  behavior of the unpacker.
 */
__must_check__
int iop_bunpack_multi_flags(mem_pool_t * nonnull mp,
                            const iop_struct_t * nonnull st,
                            void * nonnull value, pstream_t * nonnull ps,
                            unsigned flags);

__must_check__
static inline int iop_bunpack_multi(mem_pool_t * nonnull mp,
                                    const iop_struct_t * nonnull st,
                                    void * nonnull value,
                                    pstream_t * nonnull ps, bool copy)
{
    return iop_bunpack_multi_flags(mp, st, value, ps,
                                   copy ? IOP_UNPACK_COPY_STRINGS : 0);
}

/** Unpack a packed IOP union using the t_pool().
 */
__must_check__ static inline int
t_iop_bunpack_multi(const iop_struct_t * nonnull st, void * nonnull value,
                    pstream_t * nonnull ps, bool copy)
{
    return iop_bunpack_multi(t_pool(), st, value, ps, copy);
}

/** Skip a packed IOP union without unpacking it.
 *
 * This function skips a packed IOP union in a pstream_t.
 * This function is efficient because it will not fully unpack the union to
 * skip. But it will not fully check its validity either.
 *
 * \param[in] st    The IOP union definition (__s).
 * \param[in] ps    The pstream_t containing the packed IOP union.
 */
__must_check__
int iop_bskip(const iop_struct_t * nonnull st, pstream_t * nonnull ps);

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

/** Write a union tag into a struct.
  *
  * Starting with --c-unions-use-enums, iop_tag field in unions struct can now
  * have a different size than uint16_t (8 or 32 bits).
  *
  * Write this iop_tag in a safe and backward compatible way by checking the
  * offsets of union fields.
  *
  * \param[in] desc  The IOP structure definition (__s).
  * \param[in] value The union iop_tag to write.
  * \param[out] st   A pointer to the IOP structure (union only) to fill.
  */
void iop_union_set_tag(const iop_struct_t *nonnull desc, int value,
                       void *nonnull st);

/** Read a union tag from a struct.
 *
 * Read the iop_tag (undetermined size) into an int, based on the offset of
 * the union fields.
 *
 * \param[in] desc The IOP structure definition (__s).
 * \param[in] st   A pointer to the IOP structure (union only) to read.
 *
 * Returns a positive integer (0-uint16_max) on success.
 * Returns -1 if something wrong happened.
 */
int iop_union_get_tag(const iop_struct_t *nonnull desc,
                      const void *nonnull st);

/* }}} */
/* {{{ IOP packages registration / manipulation */

qm_kvec_t(iop_pkg, lstr_t, const iop_pkg_t * nonnull,
          qhash_lstr_hash, qhash_lstr_equal);

const iop_pkg_t * nullable iop_get_pkg(lstr_t pkgname);

enum iop_register_packages_flags {
    IOP_REGPKG_FROM_DSO = (1U << 0),
};

/** Register a list of packages.
 *
 * Registering a package is necessary if it contains classes; this should be
 * done before trying to pack/unpack any class.
 * This will also perform collision checks on class ids, which cannot be made
 * at compile time.
 *
 * You can use IOP_REGISTER_PACKAGES to avoid the array construction.
 */
void iop_register_packages(const iop_pkg_t * nonnull * nonnull pkgs, int len,
                           unsigned flags);

/** Helper to register a list of packages.
 *
 * Just an helper to call iop_register_packages without having to build an
 * array.
 */
#define IOP_REGISTER_PACKAGES(...)  \
    do {                                                                     \
        const iop_pkg_t *__pkgs[] = { __VA_ARGS__ };                         \
        iop_register_packages(__pkgs, countof(__pkgs), 0);                   \
    } while (0)

/** Unregister a list of packages.
 *
 * Note that unregistering a package at shutdown is NOT necessary.
 * This function is used by the DSO module, and there is no reason to use it
 * somewhere else.
 *
 * You can use IOP_UNREGISTER_PACKAGES to avoid the array construction.
 */
void iop_unregister_packages(const iop_pkg_t * nonnull * nonnull pkgs,
                             int len);

/** Helper to unregister a list of packages.
 *
 * Just an helper to call iop_unregister_packages without having to build an
 * array.
 */
#define IOP_UNREGISTER_PACKAGES(...)  \
    do {                                                                     \
        const iop_pkg_t *__pkgs[] = { __VA_ARGS__ };                         \
        iop_unregister_packages(__pkgs, countof(__pkgs));                    \
    } while (0)

#ifdef __has_blocks
typedef void (BLOCK_CARET iop_for_each_pkg_b)(const iop_pkg_t * nonnull);

/** Loop on all the pkg registered by `iop_register_packages`.
 */
void iop_for_each_registered_pkgs(iop_for_each_pkg_b nonnull cb);
#endif

/* }}} */
/* {{{ IOP backward compatibility checks */

enum iop_compat_check_flags {
    IOP_COMPAT_BIN  = (1U << 0),
    IOP_COMPAT_JSON = (1U << 1),
    /* TODO: XML */
    IOP_COMPAT_ALL  = IOP_COMPAT_BIN | IOP_COMPAT_JSON,
};

/** IOP backward compatibility context.
 *
 * A context contains checker metadata. Among other things it allows the
 * checker to skip structs that have been already verified.
 */
typedef struct iop_compat_ctx_t iop_compat_ctx_t;

iop_compat_ctx_t * nonnull iop_compat_ctx_new(void);
void iop_compat_ctx_delete(iop_compat_ctx_t * nullable * nonnull ctx);

/** Checks the backward compatibility of two IOP structures/classes/unions.
 *
 * This function checks if \p st2 is backward-compatible with \p st1 regarding
 * the formats specified in \p flags, that is if any \p st1 packed
 * structure/class/union can be safely unpacked as a \p st2.
 *
 * \p flags are a combination of \ref iop_compat_check_flags.
 *
 * \warning in case \p st1 and \p st2 are classes, it is not checking the
 *          backward compatibility of their children.
 */
int iop_struct_check_backward_compat(const iop_struct_t * nonnull st1,
                                     const iop_struct_t * nonnull st2,
                                     unsigned flags, sb_t * nonnull err);

/** Checks the backward compatibility of two IOP packages.
 *
 * This function checks if \p pkg2 is backward-compatible with \p pkg1
 * regarding the formats specified in \p flags, that is if any
 * packed structure/class/union of \p st1 can be safely unpacked using
 * structure/class/union defined in \p st2.
 *
 * The names of the structures/classes/unions must not change between \p pkg1
 * and \p pkg2.
 *
 * \warning this function does not check the interfaces/RPCs for now.
 */
int iop_pkg_check_backward_compat(const iop_pkg_t * nonnull pkg1,
                                  const iop_pkg_t * nonnull pkg2,
                                  unsigned flags, sb_t * nonnull err);

/** Checks the backward compatibility of two IOP packages with provided
 * context.
 *
 * \see iop_pkg_check_backward_compat
 *
 * This function introduce a way to provide an external compatibility context
 * \p ctx allowing backward compatibility checks between multiple packages.
 */
int iop_pkg_check_backward_compat_ctx(const iop_pkg_t * nonnull pkg1,
                                      const iop_pkg_t * nonnull pkg2,
                                      iop_compat_ctx_t * nonnull ctx,
                                      unsigned flags, sb_t * nonnull err);

/** Get whether a struct is optional or not.
 *
 * Get whether a struct is optional or not. A struct is optional if it
 * contains no mandatory fields (ie. it only contains arrays, optional fields
 * or fields with a default value).
 */
bool iop_struct_is_optional(const iop_struct_t *nonnull st);

/* }}} */

/** Module that handles IOP registration data.
 */
MODULE_DECLARE(iop);

void iop_module_register(void);

#include "iop-macros.h"
#include "iop-xml.h"
#include "iop-json.h"
#include "iop-dso.h"

#if __has_feature(nullability)
#pragma GCC diagnostic pop
#endif

#endif
