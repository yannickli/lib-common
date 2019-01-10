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

/* XXX Make this file syntastic-compatible. */
#ifndef F_NAME
#  include "iop.h"
#  include "iop-helpers.in.c"
#  define F(x)  x
#  define F_NAME  __unused__ static default_func
#  define ON_FIELD  default_on_field
#  define ON_STRUCT  default_on_struct

static int default_on_field(const iop_struct_t *st, void *st_ptr,
                            const iop_field_t *field)
{
    return 0;
}

static int default_on_struct(const iop_struct_t *st, void *st_ptr)
{
    return 0;
}

#endif

#ifdef F_PROTO
#  define  __F_PROTO  , F_PROTO
#  define  __F_ARGS   , F_ARGS
#else
#  define  __F_PROTO
#  define  __F_ARGS
#endif

#ifndef MODIFIER
#  define MODIFIER
#endif

#ifdef ON_STRUCT
#  define __ON_STRUCT(st_desc, st_ptr)                                       \
    do {                                                                     \
        int res = RETHROW(ON_STRUCT(st_desc, st_ptr __F_ARGS));              \
                                                                             \
        if (res == IOP_FIELD_SKIP) {                                         \
            return 0;                                                        \
        }                                                                    \
    } while (0)
#else
#  define __ON_STRUCT(...)
#endif

static int F(on_field)(const iop_struct_t *st_desc, const iop_field_t *fdesc,
                       void *st_ptr __F_PROTO);
static int F(for_each_field)(const iop_struct_t *st_desc, void *st_ptr
                             __F_PROTO);

static int F(for_each_st_field)(const iop_struct_t *st_desc, void *st_ptr
                                __F_PROTO)
{
    const iop_field_t *field = st_desc->fields;

    for (int i = 0; i < st_desc->fields_len; i++) {
        RETHROW(F(on_field)(st_desc, field++, st_ptr __F_ARGS));
    }

    return 0;
}


static int
F(for_each_repeated_field)(const iop_field_t *fdesc, void *fptr __F_PROTO)
{
    bool field_is_pointed = iop_field_is_pointed(fdesc);
    size_t fsize;

    if (field_is_pointed) {
        fsize = sizeof(void *);
    } else {
        fsize = fdesc->size;
    }

    for (iop_array_i8_t array = *(iop_array_i8_t *)fptr; array.len--;
         array.tab += fsize)
    {
        fptr = field_is_pointed ? *(void **)array.tab : array.tab;
        RETHROW(F(for_each_field)(fdesc->u1.st_desc, fptr __F_ARGS));
    }

    return 0;
}

static int F(on_field)(const iop_struct_t *st_desc, const iop_field_t *fdesc,
                       void *st_ptr __F_PROTO)
{
    void *fptr;

#ifdef ON_FIELD
    int res = RETHROW(ON_FIELD(st_desc, st_ptr, fdesc __F_ARGS));

    if (res == IOP_FIELD_SKIP) {
        return 0;
    }
#endif

    if (iop_type_is_scalar(fdesc->type)) {
        return 0;
    }

    fptr = (byte *)st_ptr + fdesc->data_offs;

    if (fdesc->repeat == IOP_R_REPEATED) {
        return F(for_each_repeated_field)(fdesc, fptr __F_ARGS);
    }

    if (fdesc->repeat == IOP_R_OPTIONAL
    ||  iop_field_is_reference(fdesc)
    ||  iop_field_is_class(fdesc))
    {
        fptr = *(void **)fptr;
        if (!fptr) {
            assert (fdesc->repeat == IOP_R_OPTIONAL);
            return 0;
        }
    }

    return F(for_each_field)(fdesc->u1.st_desc, fptr __F_ARGS);
}

static int F(for_each_class_field)(const iop_struct_t *st_desc, void *v
                                   __F_PROTO)
{
    if (st_desc->class_attrs->parent) {
        RETHROW(F(for_each_class_field)(st_desc->class_attrs->parent, v
                                        __F_ARGS));
    }

    return F(for_each_st_field)(st_desc, v __F_ARGS);
}

static int F(for_each_field)(const iop_struct_t *st_desc, void *st_ptr
                             __F_PROTO)
{
    if (iop_struct_is_class(st_desc)) {
        st_desc = *(const iop_struct_t **)st_ptr;

        __ON_STRUCT(st_desc, st_ptr);

        return F(for_each_class_field)(st_desc, st_ptr __F_ARGS);
    }

    __ON_STRUCT(st_desc, st_ptr);

    if (st_desc->is_union) {
        const iop_field_t *field = get_union_field(st_desc, st_ptr);

        return F(on_field)(st_desc, field, st_ptr __F_ARGS);
    }

    return F(for_each_st_field)(st_desc, st_ptr __F_ARGS);
}

int F_NAME(const iop_struct_t * nullable st_desc, MODIFIER void *st_ptr
           __F_PROTO)
{
    if (!st_desc) {
        st_desc = *(const iop_struct_t **)st_ptr;
        assert (iop_struct_is_class(st_desc));
    }

    return F(for_each_field)(st_desc, (void *)st_ptr __F_ARGS);
}

#undef __ON_STRUCT
#undef ON_STRUCT
#undef ON_FIELD
#undef F
#undef F_ARGS
#undef F_PROTO
#undef F_NAME
#undef MODIFIER
