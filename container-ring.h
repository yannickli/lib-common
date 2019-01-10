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

#if !defined(IS_LIB_COMMON_CONTAINER_H) || defined(IS_LIB_COMMON_CONTAINER_RING_H)
#  error "you must include container.h instead"
#endif
#define IS_LIB_COMMON_CONTAINER_RING_H

#define RING_TYPE(type_t, pfx)  \
    typedef struct pfx##_ring { \
        type_t *tab;            \
        int first, len, size;   \
    } pfx##_ring
RING_TYPE(void, generic);

void generic_ring_ensure(generic_ring *r, int newlen, int el_siz);

#define RING_MAP(r, f, ...)                                            \
    do {                                                               \
        int __pos = (r)->first;                                        \
        for (int __i = (r)->len; __i > 0; __i--) {                     \
            f((r)->tab + __pos, ##__VA_ARGS__);                        \
            if (++__pos == (r)->size)                                  \
                __pos = 0;                                             \
        }                                                              \
    } while (0)

#define RING_FILTER(r, f, ...)                                         \
    do {                                                               \
        int __r = (r)->first, __w = __r;                               \
        for (int __i = (r)->len; __i > 0; __i--) {                     \
            if (f((r)->tab + __r, ##__VA_ARGS__)) {                    \
                (r)->tab[__w] = (r)->tab[__r];                         \
                if (++__w == (r)->size)                                \
                    __w = 0;                                           \
            }                                                          \
            if (++__r == (r)->size)                                    \
                __r = 0;                                               \
        }                                                              \
        (r)->len -= __r >= __w ? __r - __w : (r)->size + __r - __w;    \
    } while (0)

#define RING_FUNCTIONS(type_t, pfx, wipe)                              \
    GENERIC_INIT(pfx##_ring, pfx##_ring);                              \
    static inline void pfx##_ring_wipe(pfx##_ring *r) {                \
        RING_MAP(r, wipe);                                             \
        p_delete(&r->tab);                                             \
    }                                                                  \
                                                                       \
    static inline int pfx##_ring_pos(pfx##_ring *r, int idx) {         \
        int pos = r->first + idx;                                      \
        return pos >= r->size ? pos - r->size : pos;                   \
    }                                                                  \
                                                                       \
    static inline void pfx##_ring_unshift(pfx##_ring *r, type_t e) {   \
        generic_ring_ensure((void *)r, ++r->len, sizeof(type_t));      \
        r->first = r->first ? r->first - 1 : r->size - 1;              \
        r->tab[r->first] = e;                                          \
    }                                                                  \
    static inline void pfx##_ring_push(pfx##_ring *r, type_t e) {      \
        generic_ring_ensure((void *)r, r->len + 1, sizeof(type_t));    \
        r->tab[pfx##_ring_pos(r, r->len++)] = e;                       \
    }                                                                  \
                                                                       \
    static inline bool pfx##_ring_shift(pfx##_ring *r, type_t *e) {    \
        if (r->len <= 0)                                               \
            return false;                                              \
        *e = r->tab[r->first];                                         \
        if (++r->first == r->size)                                     \
            r->first = 0;                                              \
        r->len--;                                                      \
        return true;                                                   \
    }                                                                  \
    static inline bool pfx##_ring_pop(pfx##_ring *r, type_t *e) {      \
        if (r->len <= 0)                                               \
            return false;                                              \
        *e = r->tab[pfx##_ring_pos(r, --r->len)];                      \
        return true;                                                   \
    }

#define DO_RING(type_t, pfx, wipe) \
    RING_TYPE(type_t, pfx); RING_FUNCTIONS(type_t, pfx, wipe)
