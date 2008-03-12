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

#include <sys/uio.h>
#include "mem.h"

#define VECTOR_TYPE(el_typ, prefix)  \
    typedef struct prefix##_vector { \
        el_typ *tab;                 \
        int len, size;               \
        flag_t allocated :  1;       \
        flag_t skip      : 31;       \
    } prefix##_vector

#define ARRAY_TYPE(el_typ, prefix)   \
    typedef struct prefix##_array {  \
        el_typ **tab;                \
        int len, size;               \
        flag_t allocated :  1;       \
        flag_t skip      : 31;       \
    } prefix##_array
VECTOR_TYPE(void, generic);
ARRAY_TYPE(void, generic);

typedef void array_item_dtor_f(void *item);

/**************************************************************************/
/* Misc                                                                   */
/**************************************************************************/

void generic_vector_ensure(generic_vector *v, int newlen, int el_siz)
    __attr_nonnull__((1));

void *generic_array_take(generic_array *array, int pos)
    __must_check__ __attr_nonnull__((1));

#define ARRAY_SORT_IS_STABLE  1
void generic_array_sort(generic_array *array,
                        int (*cmp)(const void *p1, const void *p2, void *parm),
                        void *parm);

#define vector_inita(v, nb)                                               \
    do {                                                                  \
        p_clear(v, 1);                                                    \
        (v)->len = (nb);                                                  \
        assert ((v)->len * sizeof((v)->tab[0]) < (64 << 10));             \
        (v)->tab = alloca((v)->len * sizeof((v)->tab[0]));                \
    } while (0)
#define array_inita(a, nb)    vector_inita(a, nb)


/**************************************************************************/
/* Typed Arrays                                                           */
/**************************************************************************/

#define VECTOR_BASE_FUNCTIONS(el_typ, prefix, suffix)                         \
    GENERIC_INIT(prefix##suffix, prefix##suffix);                             \
    static inline void                                                        \
    prefix##suffix##_init2(prefix##suffix *v, el_typ buf[], int size) {       \
        assert (size > 0);                                                    \
        *v = (prefix##suffix){ .tab = buf, .size = size };                    \
    }                                                                         \
    GENERIC_NEW(prefix##suffix, prefix##suffix);                              \
    GENERIC_DELETE(prefix##suffix, prefix##suffix);                           \
                                                                              \
    static inline void prefix##suffix##_reset(prefix##suffix *v) {            \
        v->size += v->skip;                                                   \
        v->tab  -= v->skip;                                                   \
        v->skip = 0;                                                          \
        v->len = 0;                                                           \
    }                                                                         \
    static inline void prefix##suffix##_ensure(prefix##suffix *v, int len) {  \
        if (v->size < len) {                                                  \
            generic_vector_ensure((generic_vector *)v, len, sizeof(el_typ));  \
        }                                                                     \
    }                                                                         \
    static inline void                                                        \
    prefix##suffix##_setlen(prefix##suffix *v, int newlen) {                  \
        assert (newlen >= 0);                                                 \
        if (newlen <= 0) {                                                    \
            prefix##suffix##_reset(v);                                        \
        } else {                                                              \
            prefix##suffix##_ensure(v, newlen);                               \
            v->len = newlen;                                                  \
        }                                                                     \
    }                                                                         \
                                                                              \
    static inline void                                                        \
    prefix##suffix##_insert(prefix##suffix *v, int pos, el_typ item) {        \
        assert (pos >= 0);                                                    \
        prefix##suffix##_ensure(v, v->len + 1);                               \
        if (pos < v->len) {                                                   \
            p_move(v->tab, pos + 1, pos, v->len - pos);                       \
        } else {                                                              \
            pos = v->len;                                                     \
        }                                                                     \
        v->len++;                                                             \
        v->tab[pos] = item;                                                   \
    }                                                                         \
    static inline void                                                        \
    prefix##suffix##_append(prefix##suffix *v, el_typ item) {                 \
        prefix##suffix##_ensure(v, v->len + 1);                               \
        v->tab[v->len++] = item;                                              \
    }                                                                         \
    static inline void                                                        \
    prefix##suffix##_push(prefix##suffix *v, el_typ item) {                   \
        prefix##suffix##_insert(v, 0, item);                                  \
    }                                                                         \
                                                                              \
    static inline void                                                        \
    prefix##suffix##_swap(prefix##suffix *v, int i, int j) {                  \
        SWAP(el_typ, v->tab[i], v->tab[j]);                                   \
    }                                                                         \
    static inline void                                                        \
    prefix##suffix##_splice(prefix##suffix *v, int pos, int len,              \
                            el_typ items[], int count)                        \
    {                                                                         \
        assert (pos >= 0 && len >= 0 && count >= 0);                          \
        if (pos > v->len)                                                     \
            pos = v->len;                                                     \
        if ((unsigned)pos + len > (unsigned)v->len)                           \
            len = v->len - pos;                                               \
        if (pos == 0 && v->skip + len >= count) {                             \
            v->skip += len - count;                                           \
            v->tab  += len - count;                                           \
            v->size -= len - count;                                           \
            v->len  -= len - count;                                           \
            len = count;                                                      \
        } else                                                                \
        if (len != count) {                                                   \
            prefix##suffix##_ensure(v, v->len + count - len);                 \
            p_move(v->tab, pos + count, pos + len, v->len - pos - len);       \
            v->len += count - len;                                            \
        }                                                                     \
        memcpy(v->tab + pos, items, count * sizeof(*items));                  \
    }                                                                         \
    /* OG: this API is very error prone. */                                   \
    /*     should have an API to remove array element by value. */            \
    static inline void prefix##suffix##_remove(prefix##suffix *v, int pos) {  \
        prefix##suffix##_splice(v, pos, 1, NULL, 0);                          \
    }

#define VECTOR_FUNCTIONS(el_typ, prefix)                                      \
    static inline void prefix##_vector_wipe(prefix##_vector *v) {             \
        v->tab -= v->skip;                                                    \
        if (v->allocated) {                                                   \
            p_delete(&v->tab);                                                \
        }                                                                     \
        p_clear(v, 1);                                                        \
    }                                                                         \
    VECTOR_BASE_FUNCTIONS(el_typ, prefix, _vector)

#define VECTOR_BASE_FUNCTIONS2(el_typ, prefix, suffix, wipe)                  \
    static inline void prefix##suffix##_wipe(prefix##suffix *v) {             \
        for (int i = 0; i < v->len; i++) {                                    \
            wipe(&v->tab[i]);                                                 \
        }                                                                     \
        v->tab -= v->skip;                                                    \
        if (v->allocated) {                                                   \
            p_delete(&v->tab);                                                \
        }                                                                     \
        p_clear(v, 1);                                                        \
    }                                                                         \
    VECTOR_BASE_FUNCTIONS(el_typ, prefix, suffix)

#define VECTOR_FUNCTIONS2(el_typ, prefix, wipe)                               \
    VECTOR_BASE_FUNCTIONS2(el_typ, prefix, _vector, wipe)

#define ARRAY_FUNCTIONS(el_typ, prefix, dtor)                                 \
    VECTOR_BASE_FUNCTIONS2(el_typ *, prefix, _array, dtor);                   \
                                                                              \
    __must_check__ static inline el_typ *                                     \
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

#define DO_VECTOR(type, prefix)     \
    VECTOR_TYPE(type, prefix); VECTOR_FUNCTIONS(type, prefix)

#define DO_VECTOR2(type, pfx, wipe) \
    VECTOR_TYPE(type, pfx); VECTOR_FUNCTIONS2(type, pfx, wipe)

#define DO_ARRAY(type, prefix, dtor) \
    ARRAY_TYPE(type, prefix); ARRAY_FUNCTIONS(type, prefix, dtor)

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

#define MAKE_IOVEC(data, len)  (struct iovec){ \
    .iov_base = (void *)(data), .iov_len = (len) }

DO_VECTOR(int, int);
DO_VECTOR(struct iovec, iovec);

void iovec_vector_kill_first(iovec_vector *l, ssize_t len);
int iovec_vector_getlen(iovec_vector *v);

DO_ARRAY(char, string, p_delete);

string_array *str_explode(const char *s, const char *tokens);

/*[ CHECK ]::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::{{{*/
#ifdef CHECK
#include <check.h>

Suite *check_str_array_suite(void);

#endif
/*:::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::}}}*/

#endif /* IS_LIB_COMMON_ARRAY_H */
