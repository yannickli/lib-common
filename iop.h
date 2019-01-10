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

#include "container-qhash.h"
#include "container-qvector.h"
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

qvector_t(iop_struct, const iop_struct_t *);

static inline uint32_t
qhash_iop_struct_hash(const qhash_t *h, const iop_struct_t *st)
{
    return qhash_hash_ptr(h, st);
}

static inline bool qhash_iop_struct_equal(const qhash_t *h,
                                          const iop_struct_t *st1,
                                          const iop_struct_t *st2)
{
    return st1 == st2;
}

/** Convert an IOP identifier from CamelCase naming to C underscored naming */
lstr_t t_camelcase_to_c(lstr_t name);

/** Convert an IOP type name (pkg.CamelCase) to C underscored naming */
lstr_t t_iop_type_to_c(lstr_t fullname);

/* }}} */
/* {{{ IOP attributes and constraints */

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

static inline
const iop_rpc_attrs_t *iop_rpc_get_attrs(const iop_iface_t *desc,
                                         const iop_rpc_t *fdesc)
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
int iop_iface_get_gen_attr(const iop_iface_t *iface, lstr_t key,
                           iop_type_t exp_type, iop_type_t *val_type,
                           iop_value_t *value);

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
int iop_rpc_get_gen_attr(const iop_iface_t *iface, const iop_rpc_t *rpc,
                         lstr_t key, iop_type_t exp_type,
                         iop_type_t *val_type, iop_value_t *value);

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

static inline
bool iop_field_has_constraints(const iop_struct_t *desc,
                               const iop_field_t *fdesc)
{
    if (iop_field_get_constraints_cb(desc, fdesc))
        return true;
    if (fdesc->type == IOP_T_ENUM && fdesc->u1.en_desc->flags)
        return true;
    return false;
}

/*}}}*/
/* {{{ IOP introspection */

const iop_iface_t *iop_mod_find_iface(const iop_mod_t *mod, uint32_t tag);
const iop_rpc_t   *iop_iface_find_rpc(const iop_iface_t *iface, uint32_t tag);
const iop_rpc_t   *iop_mod_find_rpc(const iop_mod_t *mod, uint32_t cmd);

/** Get the string description of an iop type.
 *
 * \param[in]  type iop type
 *
 * \return the string description.
 */
const char *iop_type_get_string_desc(iop_type_t type);

/** Return whether the IOP type is scalar or not.
 *
 * A scalar type is a type that holds one and only one value (no structure,
 * class or union).
 *
 * \param[in] type The IOP type
 * \return true if the IOP type is scalar, false otherwise
 */
bool iop_type_is_scalar(iop_type_t type);

static inline
bool iop_field_is_reference(const iop_field_t *fdesc)
{
    unsigned fdesc_flags = fdesc->flags;

    return TST_BIT(&fdesc_flags, IOP_FIELD_IS_REFERENCE);
}

int __iop_field_find_by_name(const iop_struct_t *st, const void *s, int len,
                             const iop_struct_t **found_st,
                             const iop_field_t  **found_fdesc);

__must_check__
int __iop_skip_absent_field_desc(mem_pool_t *mp, void *value,
                                 const iop_field_t *fdesc);

int iop_ranges_search(int const *ranges, int ranges_len, int tag);

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

/** Allocate an IOP structure and initialize it with the correct
 *  default values.
 *
 * \param[in] mp  The memory pool on which the allocation will be done.
 *                Can be NULL to use malloc.
 * \param[in] st  The IOP structure definition (__s).
 */
__attribute__((malloc, warn_unused_result))
void *mp_iop_new_desc(mem_pool_t *mp, const iop_struct_t *st);

__attribute__((malloc, warn_unused_result))
static inline void *iop_new_desc(const iop_struct_t *st)
{
    return mp_iop_new_desc(NULL, st);
}
__attribute__((malloc, warn_unused_result))
static inline void *t_iop_new_desc(const iop_struct_t *st)
{
    return mp_iop_new_desc(t_pool(), st);
}

#define mp_iop_new(mp, pfx)  (pfx##__t *)mp_iop_new_desc(mp, &pfx##__s)
#define iop_new(pfx)         (pfx##__t *)iop_new_desc(&pfx##__s)
#define t_iop_new(pfx)       (pfx##__t *)t_iop_new_desc(&pfx##__s)

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
int iop_sort(const iop_struct_t *st, void *vec, int len,
             lstr_t field_path, int flags, sb_t *err);

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
int iop_msort(const iop_struct_t *st, void *vec, int len,
              const qv_t(iop_sort) *params, sb_t *err);

/** Filter a vector of IOP based on a given field or subfield of reference.
 *  It takes an array of IOP objets and an array of values, and filters out
 *  the objects whose field value is not in the values array.
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
 *  \param[out] err           In case of error, the error description.
 */
int iop_filter(const iop_struct_t *st, void *vec, int *len, lstr_t field_path,
               void * const *allowed_values, int values_len, sb_t *err);

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
 * \param[out] sz If set, filled with the size of the allocated buffer.
 */
void *iop_dup(mem_pool_t *mp, const iop_struct_t *st, const void *v,
              size_t *sz);

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
 * \param[out] sz If set, filled with the size of the allocated buffer.
 */
void  iop_copy(mem_pool_t *mp, const iop_struct_t *st, void **outp,
               const void *v, size_t *sz);

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
void iop_obj_copy(mem_pool_t *mp, void *out, const void *v, unsigned flags);

/** Generate a signature of an IOP structure.
 *
 * This function generates a salted SHA256 signature of an IOP structure.
 *
 * \param[in] st     IOP structure description.
 * \param[in] v      IOP structure to sign.
 * \param[in] flags  Flags modifying the hashing algorithm. The same flags
 *                   must be used when computing and checking the signature.
 */
lstr_t t_iop_compute_signature(const iop_struct_t *st, const void *v,
                               unsigned flags);

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
int iop_check_signature(const iop_struct_t *st, const void *v, lstr_t sig,
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
int iop_struct_get_gen_attr(const iop_struct_t *st, lstr_t key,
                            iop_type_t exp_type, iop_type_t *val_type,
                            iop_value_t *value);

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
int iop_field_get_gen_attr(const iop_struct_t *st, const iop_field_t *field,
                           lstr_t key, iop_type_t exp_type,
                           iop_type_t *val_type, iop_value_t *value);

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
int iop_field_by_name_get_gen_attr(const iop_struct_t *st, lstr_t field_name,
                                   lstr_t key, iop_type_t exp_type,
                                   iop_type_t *val_type, iop_value_t *value);

/** Find an IOP field description from a iop object.
 *
 * \param[in]  ptr      The IOP object.
 * \param[in]  st       The iop_struct_t describing the object.
 * \param[in]  path     The path to the field (separate members with a '.').
 * \param[out] out      A pointer to the final IOP object.
 *
 * \return The iop field description if found, NULL otherwise.
 */
const iop_field_t *iop_get_field(const void *ptr, const iop_struct_t *st,
                                 lstr_t path, const void **out_ptr);

/** Get an IOP value from an IOP field and an IOP object.
 *
 * \param[in]  ptr   The IOP object.
 * \param[in]  field The IOP field definition.
 * \param[out] value The value to put the result in.
 *
 * \return 0 if the value is found, -1 otherwise.
 */
int iop_value_from_field(const void *ptr, const iop_field_t *field,
                         iop_value_t *value);

/** Set a field of an IOP object from an IOP value and an IOP field.
 *
 * \param[in] ptr   The IOP object.
 * \param[in] field The IOP field definition.
 * \param[in] value The value to put the field.
 *
 * \return 0 if the value is found, -1 otherwise.
 */
int iop_value_to_field(void *ptr, const iop_field_t *field,
                       const iop_value_t *value);

/** Used for iop_type_vector_to_iop_struct function.
 */
typedef struct iop_field_info_t {
    lstr_t       name;
    iop_type_t   type;
    iop_repeat_t repeat;
    union {
        const iop_struct_t *st_desc;
        const iop_enum_t   *en_desc;
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
iop_struct_t *
iop_type_vector_to_iop_struct(mem_pool_t *mp, lstr_t fullname,
                              const qv_t(iop_field_info) *fields_info);

/* }}} */
/* {{{ IOP class manipulation */

static inline bool iop_struct_is_class(const iop_struct_t *st)
{
    unsigned st_flags = st->flags;

    return TST_BIT(&st_flags, IOP_STRUCT_IS_CLASS);
}

static inline bool iop_field_is_class(const iop_field_t *f)
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
const iop_value_t *iop_get_cvar(const void *obj, lstr_t name);

#define iop_get_cvar_cst(obj, name)  iop_get_cvar(obj, LSTR(name))

/** Gets the value of a class variable (static field) from a class descriptor.
 *
 * Same as iop_get_cvar, but directly takes a class descriptor.
 */
__attr_nonnull__((1))
const iop_value_t *iop_get_cvar_desc(const iop_struct_t *desc, lstr_t name);

#define iop_get_cvar_desc_cst(desc, name)  \
    iop_get_cvar_desc(desc, LSTR(name))

/* The following variants of iop_get_cvar do not recurse on parents */
__attr_nonnull__((1))
const iop_value_t *iop_get_class_cvar(const void *obj, lstr_t name);

#define iop_get_class_cvar_cst(obj, name)  \
    iop_get_class_cvar(obj, LSTR(name))

__attr_nonnull__((1))
const iop_value_t *
iop_get_class_cvar_desc(const iop_struct_t *desc, lstr_t name);

#define iop_get_class_cvar_desc_cst(desc, name)  \
    iop_get_class_cvar_desc(desc, LSTR(name))


/** Check if the static fields types are available for a given class.
 *
 * \param[in]  desc  pointer to the class descriptor
 *
 * \return  true if and only if the type of static fields can be read
 */
__attr_nonnull__((1))
static inline bool iop_class_static_fields_have_type(const iop_struct_t *desc)
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
iop_class_static_field_type(const iop_struct_t *desc,
                            const iop_static_field_t *f)
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
bool iop_class_is_a(const iop_struct_t *cls1, const iop_struct_t *cls2);

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
iop_obj_is_a_desc(const void *obj, const iop_struct_t *desc)
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
__attr_nonnull__((1)) const iop_struct_t *
iop_get_class_by_fullname(const iop_struct_t *st, lstr_t fullname);

/** Get the descriptor of a class from its id.
 *
 * Manipulating class ids should be reserved to some very specific use-cases,
 * so before using this function, be SURE that you really need it.
 */
const iop_struct_t *
iop_get_class_by_id(const iop_struct_t *st, uint16_t class_id);

#ifdef __has_blocks
typedef void (BLOCK_CARET iop_for_each_class_b)(const iop_struct_t *);

/** Loop on all the classes registered by `iop_register_packages`.
 */
void iop_for_each_registered_classes(iop_for_each_class_b cb);
#endif

const iop_field_t *
_iop_class_get_next_field(const iop_struct_t **st, int *it);

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


/** Provide the appropriate arguments to the %*pC modifier.
 *
 * '%*pC' can be used in format string in order to print the JSON-encoded
 * content of an IOP object (instance of an IOP class). You can provide the
 * flags to be used to the JSON packer in the format arguments
 * (\ref iop_jpack_flags).
 *
 * \param[in]  _obj   The object to print.
 * \param[in]  _flags The JSON packing flags
 *
 * See \ref IOP_OBJ_FMT_ARG() for a convenience helper to print compact JSON.
 */
#define IOP_OBJ_FMT_ARG_FLAGS(_obj, _flags)  \
    (_flags), ({ typeof(*_obj) *_fmt_obj = (_obj);                           \
                 assert (_fmt_obj->__vptr != NULL);                          \
                 _fmt_obj; })

/** Provide the appropriate argument to print compact JSON with the %*pC
 * format.
 *
 * This macro is a convienience helper for \ref IOP_OBJ_FMT_ARG_FLAGS to
 * cover the usual use case where compact JSON is needed.
 *
 * \param[in]  _obj  The object to print.
 */
#define IOP_OBJ_FMT_ARG(_obj)  \
    IOP_OBJ_FMT_ARG_FLAGS(_obj, IOP_JPACK_COMPACT | IOP_JPACK_NO_TRAILING_EOL)

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
    return unlikely(res < 0) ? LSTR_NULL_V : ed->names[res];
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
int iop_enum_get_gen_attr(const iop_enum_t *ed, lstr_t key,
                          iop_type_t exp_type, iop_type_t *val_type,
                          iop_value_t *value);

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
int iop_enum_get_gen_attr_from_val(const iop_enum_t *ed, int val, lstr_t key,
                                   iop_type_t exp_type, iop_type_t *val_type,
                                   iop_value_t *value);

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
int iop_enum_get_gen_attr_from_str(const iop_enum_t *ed, lstr_t val,
                                   lstr_t key, iop_type_t exp_type,
                                   iop_type_t *val_type, iop_value_t *value);

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

/** Pack an IOP structure into IOP binary format using a specific mempool.
 *
 * This version of `iop_bpack` allows to pack an IOP structure in one
 * operation and uses the given mempool to allocate the resulting buffer.
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
lstr_t mp_iop_bpack_struct_flags(mem_pool_t *mp, const iop_struct_t *st,
                                 const void *v, const unsigned flags);

/** Pack an IOP structure into IOP binary format using the t_pool().
 *
 * Prefer the generated version instead of this low-level API (see IOP_GENERIC
 * in iop-macros.h).
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
 * one and only one structure, so the pstream_t must only contain the unique
 * structure to unpack.
 *
 * This function cannot be used to unpack a class; use `iop_bunpack_ptr`
 * instead.
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

/** Unpack a packed IOP object and (re)allocates the destination structure.
 *
 * This function acts as `iop_bunpack` but allocates (or reallocates) the
 * destination structure.
 *
 * This function MUST be used to unpack a class instead of `iop_bunpack`,
 * because the size of a class is not known before unpacking it (this could be
 * a child).
 *
 * Prefer the generated version instead of this low-level API
 * (see IOP_GENERIC/IOP_CLASS in iop-macros.h).
 *
 * \param[in] mp    The memory pool to use when memory allocation is needed;
 *                  will be used at least to allocate the destination
 *                  structure.
 * \param[in] st    The IOP structure/class definition (__s).
 * \param[in] value Double pointer on the destination structure.
 *                  If *value is not NULL, it is reallocated.
 * \param[in] ps    The pstream_t containing the packed IOP object.
 * \param[in] copy  Tell to the unpack whether complex type must be duplicated
 *                  or not (for example string could be pointers on the
 *                  pstream_t or duplicated).
 */
__must_check__
int iop_bunpack_ptr(mem_pool_t *mp, const iop_struct_t *st, void **value,
                    pstream_t ps, bool copy);

/** Unpack a packed IOP union.
 *
 * This function act like `iop_bunpack` but consume the pstream and doesn't
 * check that the pstream has been fully consumed. This allows to unpack
 * a suite of unions.
 *
 * Prefer the generated version instead of this low-level API (see IOP_GENERIC
 * in iop-macros.h).
 *
 * \param[in] mp    The memory pool to use when memory allocation is needed.
 * \param[in] st    The IOP structure definition (__s).
 * \param[in] value Pointer on the destination unpacked IOP union.
 * \param[in] ps    The pstream_t containing the packed IOP union.
 * \param[in] copy  Tell to the unpack whether complex type must be duplicated
 *                  or not (for example string could be pointers on the
 *                  pstream_t or duplicated).
 */
__must_check__
int iop_bunpack_multi(mem_pool_t *mp, const iop_struct_t *st, void *value,
                      pstream_t *ps, bool copy);

/** Unpack a packed IOP union using the t_pool().
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

/** Skip a packed IOP union without unpacking it.
 *
 * This function skips a packed IOP union in a pstream_t.
 * This function is efficient because it will not fully unpack the union to
 * skip. But it will not fully check its validity either.
 *
 * Prefer the generated version instead of this low-level API (see IOP_GENERIC
 * in iop-macros.h).
 *
 * \param[in] st    The IOP union definition (__s).
 * \param[in] ps    The pstream_t containing the packed IOP union.
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
/* {{{ IOP packages registration */

qm_kvec_t(iop_pkg, lstr_t, const iop_pkg_t *,
          qhash_lstr_hash, qhash_lstr_equal);

const iop_pkg_t *iop_get_pkg(lstr_t pkgname);

/** Register a list of packages.
 *
 * Registering a package is necessary if it contains classes; this should be
 * done before trying to pack/unpack any class.
 * This will also perform collision checks on class ids, which cannot be made
 * at compile time.
 *
 * You can use IOP_REGISTER_PACKAGES to avoid the array construction.
 */
void iop_register_packages(const iop_pkg_t **pkgs, int len);

/** Helper to register a list of packages.
 *
 * Just an helper to call iop_register_packages without having to build an
 * array.
 */
#define IOP_REGISTER_PACKAGES(...)  \
    do {                                                                     \
        const iop_pkg_t *__pkgs[] = { __VA_ARGS__ };                         \
        iop_register_packages(__pkgs, countof(__pkgs));                      \
    } while (0)

/** Unregister a list of packages.
 *
 * Note that unregistering a package at shutdown is NOT necessary.
 * This function is used by the DSO module, and there is no reason to use it
 * somewhere else.
 *
 * You can use IOP_UNREGISTER_PACKAGES to avoid the array construction.
 */
void iop_unregister_packages(const iop_pkg_t **pkgs, int len);

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

/* }}} */

/** Module that handles IOP registration data.
 */
MODULE_DECLARE(iop);

void iop_module_register(void);

#include "iop-macros.h"
#include "iop-xml.h"
#include "iop-json.h"
#include "iop-dso.h"

#endif
