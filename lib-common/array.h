/**************************************************************************/
/*                                                                        */
/*  Copyright (C) 2004-2007 INTERSEC SAS                                  */
/*                                                                        */
/*  Should you receive a copy of this source code, you must check you     */
/*  have a proper, written authorization of INTERSEC to hold it. If you   */
/*  don't have such an authorization, you must DELETE all source code     */
/*  files in your possession, and inform INTERSEC of the fact you obtain  */
/*  these files. Should you not comply to these terms, you can be         */
/*  prosecuted in the extent permitted by applicable law.                 */
/*                                                                        */
/**************************************************************************/

#ifndef IS_LIB_COMMON_ARRAY_H
#define IS_LIB_COMMON_ARRAY_H

#include <stdlib.h>

#include "mem.h"

typedef struct generic_array {
    void ** tab;
    ssize_t len;

    ssize_t size;
} generic_array;
typedef void array_item_dtor_f(void *item);

/**************************************************************************/
/* Memory management                                                      */
/**************************************************************************/

GENERIC_INIT(generic_array, generic_array);
GENERIC_NEW(generic_array, generic_array);
void generic_array_wipe(generic_array *array, array_item_dtor_f *dtor);
void generic_array_delete(generic_array **array, array_item_dtor_f *dtor);

/**************************************************************************/
/* Misc                                                                   */
/**************************************************************************/

void generic_array_resize(generic_array *array, ssize_t newlen);

void generic_array_insert(generic_array *array, ssize_t pos, void *item)
    __attr_nonnull__((1));

static inline void generic_array_append(generic_array *array, void *item)
{
    generic_array_insert(array, array->len, item);
}
static inline void generic_array_push(generic_array *array, void *item)
{
    generic_array_insert(array, 0, item);
}

void *generic_array_take(generic_array *array, ssize_t pos)
    __attr_nonnull__((1));

static inline void
generic_array_swap(generic_array *array, ssize_t i, ssize_t j)
{
    void *v = array->tab[i];
    array->tab[i] = array->tab[j];
    array->tab[j] = v;
}

#define ARRAY_SORT_IS_STABLE  1
void generic_array_sort(generic_array *array,
                        int (*cmp)(const void *p1, const void *p2, void *parm),
                        void *parm);

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

#define ARRAY_FUNCTIONS(el_typ, prefix, dtor)                                 \
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
    prefix##_array_wipe(prefix##_array *array)                                \
    {                                                                         \
        generic_array_wipe((generic_array*)array, (array_item_dtor_f *)dtor); \
    }                                                                         \
    static inline void                                                        \
    prefix##_array_delete(prefix##_array **array)                             \
    {                                                                         \
        generic_array_delete((generic_array **)array,                         \
                             (array_item_dtor_f *)dtor);                      \
    }                                                                         \
    static inline void                                                        \
    prefix##_array_reset(prefix##_array *array) {                             \
        ((generic_array*)array)->len = 0;                                     \
    }                                                                         \
                                                                              \
    /* module functions */                                                    \
    static inline void                                                        \
    prefix##_array_resize(prefix##_array *array, ssize_t newlen)              \
    {                                                                         \
        generic_array_resize((generic_array *)array, newlen);                 \
    }                                                                         \
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
    prefix##_array_swap(prefix##_array *array, ssize_t i, ssize_t j)          \
    {                                                                         \
        generic_array_swap((generic_array *)array, i, j);                     \
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
    }                                                                         \
    static inline void                                                        \
    prefix##_array_sort(prefix##_array *array,                                \
                        int (*cmp)(const el_typ *, const el_typ *, void *),   \
                        void *priv)                                           \
    {                                                                         \
        generic_array_sort((generic_array *)array, (void *)cmp, priv);        \
    }

#define DO_ARRAY(prefix, type, dtor)  ARRAY_TYPE(prefix, type);               \
                                      ARRAY_FUNCTIONS(prefix, type, dtor)

#endif /* IS_LIB_COMMON_ARRAY_H */
