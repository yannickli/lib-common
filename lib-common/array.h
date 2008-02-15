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

#define VECTOR_TYPE(el_typ, prefix)  \
    typedef struct prefix##_vector { \
        el_typ *tab;                 \
        int len, size;               \
    } prefix##_vector

#define ARRAY_TYPE(el_typ, prefix)   \
    typedef struct prefix##_array {  \
        el_typ **tab;                \
        int len, size;               \
    } prefix##_array
ARRAY_TYPE(void, generic);

typedef void array_item_dtor_f(void *item);

/**************************************************************************/
/* Misc                                                                   */
/**************************************************************************/

void generic_array_wipe(generic_array *array, array_item_dtor_f *dtor);
void *generic_array_take(generic_array *array, int pos)
    __attr_nonnull__((1));

#define ARRAY_SORT_IS_STABLE  1
void generic_array_sort(generic_array *array,
                        int (*cmp)(const void *p1, const void *p2, void *parm),
                        void *parm);

/**************************************************************************/
/* Typed Arrays                                                           */
/**************************************************************************/

#define VECTOR_BASE_FUNCTIONS(el_typ, prefix, suffix)                         \
    GENERIC_INIT(prefix##suffix, prefix##suffix);                             \
    GENERIC_NEW(prefix##suffix, prefix##suffix);                              \
    GENERIC_DELETE(prefix##suffix, prefix##suffix);                           \
    static inline void prefix##suffix##_reset(prefix##suffix *v) {            \
        v->len = 0;                                                           \
    }                                                                         \
                                                                              \
    static inline void                                                        \
    prefix##suffix##_insert(prefix##suffix *v, int pos, el_typ item) {        \
        p_allocgrow(&v->tab, v->len + 1, &v->size);                           \
        if (pos < v->len) {                                                   \
            p_move(v->tab, pos + 1, pos, v->len - pos);                       \
        } else {                                                              \
            pos = v->len;                                                     \
            v->len++;                                                         \
        }                                                                     \
        v->tab[pos] = item;                                                   \
    }                                                                         \
    static inline void                                                        \
    prefix##suffix##_append(prefix##suffix *v, el_typ item) {                 \
        prefix##suffix##_insert(v, v->len, item);                             \
    }                                                                         \
    static inline void                                                        \
    prefix##suffix##_push(prefix##suffix *v, el_typ item) {                   \
        prefix##suffix##_insert(v, 0, item);                                  \
    }                                                                         \
                                                                              \
    static inline void                                                        \
    prefix##suffix##_splice(prefix##suffix *v, int pos, int len,              \
                          el_typ items[], int count)                          \
    {                                                                         \
        if (pos > v->len)                                                     \
            pos = v->len;                                                     \
        if (pos + len > v->len)                                               \
            len = v->len - pos - len;                                         \
        if (len < count)                                                      \
            p_allocgrow(&v->tab, v->len + count - len, &v->size);             \
        if (len != count) {                                                   \
            p_move(v->tab, pos + count, pos + len, v->len - pos - len);       \
            v->len += count - len;                                            \
        }                                                                     \
        memcpy(v->tab + pos, items, count * sizeof(*items));                  \
    }                                                                         \
    static inline void prefix##suffix##_remove(prefix##suffix *v, int pos) {  \
        prefix##suffix##_splice(v, pos, 1, NULL, 0);                          \
    }

#define VECTOR_FUNCTIONS(el_typ, prefix)                                      \
    static inline void prefix##_vector_wipe(prefix##_vector *v) {             \
        p_delete(&v->tab);                                                    \
        p_clear(&v, 1);                                                       \
    }                                                                         \
    VECTOR_BASE_FUNCTIONS(prefix, prefix, _vector)

#define DO_VECTOR(type, prefix)       VECTOR_TYPE(type, prefix);              \
                                      VECTOR_FUNCTIONS(type, prefix)


#define ARRAY_FUNCTIONS(el_typ, prefix, dtor)                                 \
    static inline void                                                        \
    prefix##_array_wipe(prefix##_array *a) {                                  \
        generic_array_wipe((generic_array *)a, (array_item_dtor_f *)dtor);    \
    }                                                                         \
    VECTOR_BASE_FUNCTIONS(el_typ *, prefix, _array);                          \
                                                                              \
    static inline void                                                        \
    prefix##_array_resize(prefix##_array *a, int newlen) {                    \
        p_allocgrow(&a->tab, newlen, &a->size);                               \
        a->len = newlen;                                                      \
    }                                                                         \
    static inline void                                                        \
    prefix##_array_swap(prefix##_array *array, int i, int j) {                \
        SWAP(array->tab[i], array->tab[j]);                                   \
    }                                                                         \
    static inline el_typ *                                                    \
    prefix##_array_take(prefix##_array *array, int pos) {                     \
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
        for (int i = 0; i < array->len; i++) {                            \
            el_type *el = array->tab[i];                                  \
            if (strequal(el->name, name)) {                               \
                return el;                                                \
            }                                                             \
        }                                                                 \
        return NULL;                                                      \
    }

DO_VECTOR(int, int);
DO_ARRAY(char, string, p_delete);

string_array *str_explode(const char *s, const char *tokens);

/*[ CHECK ]::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::{{{*/
#ifdef CHECK
#include <check.h>

Suite *check_str_array_suite(void);

#endif
/*:::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::}}}*/

#endif /* IS_LIB_COMMON_ARRAY_H */
