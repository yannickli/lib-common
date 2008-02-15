/**************************************************************************/
/*                                                                        */
/*  Copyright (C) 2004-2008 INTERSEC SAS                                  */
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

#include "mem.h"

typedef struct generic_array {
    void **tab;
    int len;
    int size;
} generic_array;
typedef void array_item_dtor_f(void *item);

/**************************************************************************/
/* Misc                                                                   */
/**************************************************************************/

void generic_array_wipe(generic_array *array, array_item_dtor_f *dtor);

void generic_array_insert(generic_array *array, int pos, void *item)
    __attr_nonnull__((1));

void generic_array_splice(generic_array *array, int pos, int len,
                          void **item, int count)
    __attr_nonnull__((1));

void *generic_array_take(generic_array *array, int pos)
    __attr_nonnull__((1));

#define ARRAY_SORT_IS_STABLE  1
void generic_array_sort(generic_array *array,
                        int (*cmp)(const void *p1, const void *p2, void *parm),
                        void *parm);

/**************************************************************************/
/* Typed Arrays                                                           */
/**************************************************************************/

#define ARRAY_TYPE(el_typ, prefix)  \
    typedef struct prefix##_array { \
        el_typ ** tab;              \
        int len, size;              \
    } prefix##_array

#define ARRAY_FUNCTIONS(el_typ, prefix, dtor)                                 \
    GENERIC_INIT(prefix##_array, prefix##_array);                             \
    GENERIC_NEW(prefix##_array, prefix##_array);                              \
    static inline void prefix##_array_reset(prefix##_array *array) {          \
        array->len = 0;                                                       \
    }                                                                         \
    static inline void                                                        \
    prefix##_array_wipe(prefix##_array *array)                                \
    {                                                                         \
        generic_array_wipe((generic_array*)array, (array_item_dtor_f *)dtor); \
    }                                                                         \
    GENERIC_DELETE(prefix##_array, prefix##_array);                           \
                                                                              \
    /* module functions */                                                    \
    static inline void                                                        \
    prefix##_array_resize(prefix##_array *a, int newlen) {                    \
        p_allocgrow(&a->tab, newlen, &a->size);                               \
        a->len = newlen;                                                      \
    }                                                                         \
    static inline void                                                        \
    prefix##_array_insert(prefix##_array *array, int pos, el_typ *item)       \
    {                                                                         \
        generic_array_insert((generic_array *)array, pos, (void*)item);       \
    }                                                                         \
    static inline void                                                        \
    prefix##_array_splice(prefix##_array *array, int pos, int len,            \
                          el_typ **items, int count)                          \
    {                                                                         \
        generic_array_splice((generic_array *)array, pos, len,                \
                             (void **)items, count);                          \
    }                                                                         \
    static inline void                                                        \
    prefix##_array_append(prefix##_array *array, el_typ *item) {              \
        prefix##_array_insert(array, array->len, item);                       \
    }                                                                         \
    static inline void                                                        \
    prefix##_array_push(prefix##_array *array, el_typ *item) {                \
        prefix##_array_insert(array, 0, item);                                \
    }                                                                         \
    static inline void                                                        \
    prefix##_array_swap(prefix##_array *array, int i, int j)                  \
    {                                                                         \
        SWAP(array->tab[i], array->tab[j]);                                   \
    }                                                                         \
    static inline el_typ *                                                    \
    prefix##_array_take(prefix##_array *array, int pos)                       \
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

#define DO_ARRAY(type, prefix, dtor)  ARRAY_TYPE(type, prefix);               \
                                      ARRAY_FUNCTIONS(type, prefix, dtor)

/* XXX: This macro only works for arrays of elements having a member
 * named 'name'. */
#define ARRAY_FIND_BY_NAME(el_type, prefix)                               \
    static inline el_type *                                               \
    prefix##_array_find(const prefix##_array *array, const char *name)    \
    {                                                                     \
        int i;                                                            \
                                                                          \
        for (i = 0; i < array->len; i++) {                                \
            el_type *el = array->tab[i];                                  \
            if (strequal(el->name, name)) {                               \
                return el;                                                \
            }                                                             \
        }                                                                 \
        return NULL;                                                      \
    }

#endif /* IS_LIB_COMMON_ARRAY_H */
