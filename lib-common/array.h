/**************************************************************************/
/*                                                                        */
/*  Copyright (C) 2004-2006 INTERSEC                                      */
/*                                                                        */
/*  Should you receive a copy of this source code, you must check you     */
/*  have a proper, written authorization of INTERSEC to hold it. If you   */
/*  don't have such an authorization, you must DELETE all source code     */
/*  files in your possession, and inform INTERSEC of the fact you obtain  */
/*  these files. Should you not comply to these terms, you can be         */
/*  prosecuted in the extent permitted by applicable law.                 */
/*                                                                        */
/**************************************************************************/

#ifndef IS_ARRAY_H
#define IS_ARRAY_H

#include <stdlib.h>

#include "mem.h"

typedef struct generic_array {
    void ** const tab;
    ssize_t const len;

    ssize_t const __size;
} generic_array;
typedef void array_item_dtor_f(void *item);

/**************************************************************************/
/* Memory management                                                      */
/**************************************************************************/

#define generic_array_new() generic_array_init(p_new_raw(generic_array, 1))
generic_array *generic_array_init(generic_array *array);
void generic_array_wipe(generic_array *array, array_item_dtor_f *dtor);
void generic_array_delete(generic_array **array, array_item_dtor_f *dtor);

/**************************************************************************/
/* Misc                                                                   */
/**************************************************************************/

void generic_array_insert(generic_array *array, ssize_t pos, void *item)
    __attribute__((nonnull(1)));

static inline void generic_array_append(generic_array *array, void *item)
{
    generic_array_insert(array, array->len, item);
}
static inline void generic_array_push(generic_array *array, void *item)
{
    generic_array_insert(array, 0, item);
}

void *generic_array_take(generic_array *array, ssize_t pos)
    __attribute__((nonnull(1)));

/**************************************************************************/
/* Typed Arrays                                                           */
/**************************************************************************/

#define ARRAY_TYPE(el_typ, prefix)                                            \
    typedef struct prefix##_array {                                           \
        el_typ ** const tab;                                                  \
        ssize_t const len;                                                    \
                                                                              \
        ssize_t const __size;                                                 \
    } prefix##_array

#define ARRAY_FUNCTIONS(el_typ, prefix)                                       \
                                                                              \
    /* legacy functions */                                                    \
    static inline prefix##_array *prefix##_array_new(void)                    \
    {                                                                         \
        return (prefix##_array *)generic_array_new();                         \
    }                                                                         \
    static inline prefix##_array *                                            \
    prefix##_array_init(prefix##_array *array)                                \
    {                                                                         \
        return (prefix##_array *)generic_array_init((generic_array *)array);  \
    }                                                                         \
    static inline void                                                        \
    prefix##_array_wipe(prefix##_array *array, bool do_elts)                  \
    {                                                                         \
        generic_array_wipe((generic_array*)array,                             \
                do_elts ? (array_item_dtor_f *)prefix##_delete : NULL);       \
    }                                                                         \
    static inline void                                                        \
    prefix##_array_delete(prefix##_array **array, bool do_elts)               \
    {                                                                         \
        generic_array_delete((generic_array **)array,                         \
                do_elts ? (array_item_dtor_f *)prefix##_delete : NULL);       \
    }                                                                         \
                                                                              \
    /* module functions */                                                    \
    static inline void                                                        \
    prefix##_array_insert(prefix##_array *array, ssize_t pos, el_typ *item)   \
    {                                                                         \
        generic_array_insert((generic_array *)array, pos, (void*)item);       \
    }                                                                         \
    static inline void                                                        \
    prefix##_array_append(prefix##_array *array, el_typ *item)                \
    {                                                                         \
        generic_array_append((generic_array *)array, (void*)item);            \
    }                                                                         \
    static inline void                                                        \
    prefix##_array_push(prefix##_array *array, el_typ *item)                  \
    {                                                                         \
        generic_array_push((generic_array *)array, (void*)item);              \
    }                                                                         \
    static inline el_typ *                                                    \
    prefix##_array_take(prefix##_array *array, ssize_t pos)                   \
    {                                                                         \
        return (el_typ *)generic_array_take((generic_array *)array, pos);     \
    }

#endif
