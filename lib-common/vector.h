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

#ifndef IS_LIB_COMMON_VECTOR_H
#define IS_LIB_COMMON_VECTOR_H

#include "mem.h"

#define VECTOR_TYPE(el_typ, prefix)    \
    typedef struct prefix##_vector {   \
        el_typ *vec;                   \
        int len, size;                 \
    } prefix##_vector

#define VECTOR_FUNCTIONS(el_typ, prefix)                                      \
    GENERIC_INIT(prefix##_vector, prefix##_vector);                           \
    GENERIC_NEW(prefix##_vector, prefix##_vector);                            \
    static inline void prefix##_vector_reset(prefix##_vector *v) {            \
        v->len = 0;                                                           \
    }                                                                         \
    static inline void prefix##_vector_wipe(prefix##_vector *v) {             \
        p_delete(&v->vec);                                                    \
        p_clear(&v, 1);                                                       \
    }                                                                         \
    GENERIC_DELETE(prefix##_vector, prefix##_vector);                         \
                                                                              \
    static inline void                                                        \
    prefix##_vector_insert(prefix##_vector *v, int pos, el_typ item) {        \
        p_allocgrow(&v->vec, v->len + 1, &v->size);                           \
        if (pos < v->len) {                                                   \
            p_move(v->vec, pos + 1, pos, v->len - pos);                       \
        } else {                                                              \
            pos = v->len;                                                     \
            v->len++;                                                         \
        }                                                                     \
        v->vec[pos] = item;                                                   \
    }                                                                         \
    static inline void                                                        \
    prefix##_vector_append(prefix##_vector *v, el_typ item) {                 \
        prefix##_vector_insert(v, v->len, item);                              \
    }                                                                         \
    static inline void                                                        \
    prefix##_vector_push(prefix##_vector *v, el_typ item) {                   \
        prefix##_vector_insert(v, 0, item);                                   \
    }                                                                         \
                                                                              \
    static inline void                                                        \
    prefix##_vector_splice(prefix##_vector *v, int pos, int len,              \
                          el_typ items[], int count)                          \
    {                                                                         \
        if (pos > v->len)                                                     \
            pos = v->len;                                                     \
        if (pos + len > v->len)                                               \
            len = v->len - pos - len;                                         \
        if (len < count)                                                      \
            p_allocgrow(&v->vec, v->len + count - len, &v->size);             \
        if (len != count) {                                                   \
            p_move(v->vec, pos + count, pos + len, v->len - pos - len);       \
            v->len += count - len;                                            \
        }                                                                     \
        memcpy(v->vec + pos, items, count * sizeof(*items));                  \
    }                                                                         \
    static inline void prefix##_vector_remove(prefix##_vector *v, int pos) {  \
        prefix##_vector_splice(v, pos, 1, NULL, 0);                           \
    }

#define DO_VECTOR(type, prefix)       VECTOR_TYPE(type, prefix);              \
                                      VECTOR_FUNCTIONS(type, prefix)

/* XXX: This macro only works for vectors of elements having a member
 * named 'name'. */
#define VECTOR_FIND_BY_NAME(el_type, prefix)                              \
    static inline el_type *                                               \
    prefix##_vector_find(const prefix##_vector *v, const char *name) {    \
    {                                                                     \
        for (int i = 0; i < v->len; i++) {                                \
            if (strequal(v->vec[i].name, name)) {                         \
                v->vec + i;                                               \
                return el;                                                \
            }                                                             \
        }                                                                 \
        return NULL;                                                      \
    }

DO_VECTOR(int, int);

#endif
