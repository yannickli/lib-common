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

#ifndef IS_LIB_COMMON_HTBL
#define IS_LIB_COMMON_HTBL

#include "mem.h"

#define HTBL_TYPE(type_t, pfx)                                               \
    typedef struct pfx##_htbl {                                              \
        type_t *tab;                                                         \
        unsigned *setbits;                                                   \
        unsigned *ghostbits;                                                 \
        int len, size, ghosts;                                               \
        flag_t inmap : 1;                                                    \
    } pfx##_htbl

HTBL_TYPE(void, generic);

void htbl_init(generic_htbl *t, int size);
void htbl_wipe(generic_htbl *t);
void htbl_invalidate(generic_htbl *t, int pos);

#define DO_HTBL_LL(type_t, idx_t, pfx, hm, get_key, key_equal)               \
    HTBL_TYPE(type_t, pfx);                                                  \
                                                                             \
    GENERIC_INIT(pfx##_htbl, pfx##_htbl)                                     \
    static inline void pfx##_htbl_wipe(pfx##_htbl *t) {                      \
        htbl_wipe((generic_htbl *)t);                                        \
    }                                                                        \
    GENERIC_NEW(pfx##_htbl, pfx##_htbl)                                      \
    GENERIC_DELETE(pfx##_htbl, pfx##_htbl)                                   \
                                                                             \
    static inline type_t *pfx##_htbl_ll_insert(pfx##_htbl *, type_t);        \
    static inline void pfx##_htbl_resize(pfx##_htbl *t, int newsize) {       \
        pfx##_htbl old = *t;                                                 \
        htbl_init((generic_htbl *)t, newsize);                               \
        t->tab  = p_new(type_t, newsize);                                    \
        for (int i = 0; i < old.size; i++) {                                 \
            if (TST_BIT(old.setbits, i))                                     \
                pfx##_htbl_ll_insert(t, old.tab[i]);                         \
        }                                                                    \
        htbl_wipe((generic_htbl *)&old);                                     \
    }                                                                        \
                                                                             \
    static inline type_t *                                                   \
    pfx##_htbl_ll_find(const pfx##_htbl *t, uint64_t h, idx_t key) {         \
        unsigned size = (unsigned)t->size;                                   \
        for (unsigned pos = h % size;; pos++) {                              \
            if (pos == size)                                                 \
                 pos = 0;                                                    \
            if (!TST_BIT(t->ghostbits, pos)) {                               \
                type_t *ep = t->tab + pos;                                   \
                if (!TST_BIT(t->setbits, pos))                               \
                    return NULL;                                             \
                if (ep->hm == h && key_equal(h, get_key(ep), key))           \
                    return t->tab + pos;                                     \
            }                                                                \
        }                                                                    \
    }                                                                        \
                                                                             \
    static inline type_t *pfx##_htbl_ll_insert(pfx##_htbl *t, type_t e) {    \
        unsigned size, pos;                                                  \
        int ghost = -1;                                                      \
        __attribute__((unused)) idx_t key = get_key(&e);                     \
                                                                             \
        assert (!t->inmap);                                                  \
        if (t->len >= t->size / 2) {                                         \
            pfx##_htbl_resize(t, p_alloc_nr(t->size));                       \
        } else                                                               \
        if (t->len + t->ghosts >= t->size / 2) {                             \
            pfx##_htbl_resize(t, t->size);                                   \
        }                                                                    \
                                                                             \
        size = (unsigned)t->size;                                            \
        for (pos = e.hm % size; ; pos++) {                                   \
            if (pos == size)                                                 \
                pos = 0;                                                     \
            if (!TST_BIT(t->ghostbits, pos)) {                               \
                type_t *ep = t->tab + pos;                                   \
                if (!TST_BIT(t->setbits, pos))                               \
                    break;                                                   \
                if (ep->hm == e.hm && key_equal(e.hm, key, get_key(ep)))     \
                    return t->tab + pos;                                     \
            } else if (ghost < 0) {                                          \
                ghost = pos;                                                 \
            }                                                                \
        }                                                                    \
        if (ghost >= 0) {                                                    \
            pos = ghost;                                                     \
            RST_BIT(t->ghostbits, pos);                                      \
            t->ghosts--;                                                     \
        }                                                                    \
        SET_BIT(t->setbits, pos);                                            \
        t->len++;                                                            \
        t->tab[pos] = e;                                                     \
        return NULL;                                                         \
    }                                                                        \
                                                                             \
    static inline void pfx##_htbl_ll_remove(pfx##_htbl *t, type_t *e) {      \
        if (e) {                                                             \
            assert (t->tab <= e && e < t->tab + t->size);                    \
            htbl_invalidate((generic_htbl *)t, e - t->tab);                  \
            if (!t->inmap && 8 * (t->len + 16) < t->size)                    \
                pfx##_htbl_resize(t, 4 * (t->len + 16));                     \
        }                                                                    \
    }

#define DO_HTBL_KEY(type_t, idx_t, pfx, km)                                  \
    static inline idx_t pfx##_get_key(type_t *e) {                           \
        return e->km;                                                        \
    }                                                                        \
                                                                             \
    DO_HTBL_LL(type_t, idx_t, pfx, km, pfx##_get_key, TRUEFN)                \
                                                                             \
    static inline type_t *pfx##_htbl_find(const pfx##_htbl *t, idx_t key) {  \
        return pfx##_htbl_ll_find(t, key, key);                              \
    }                                                                        \
    static inline type_t *                                                   \
    pfx##_htbl_insert(pfx##_htbl *t, type_t e) {                             \
        return pfx##_htbl_ll_insert(t, e);                                   \
    }                                                                        \
    static inline void pfx##_htbl_remove(pfx##_htbl *t, type_t *e) {         \
        pfx##_htbl_ll_remove(t, e);                                          \
    }                                                                        \
                                                                             \
    static inline bool                                                       \
    pfx##_htbl_take(pfx##_htbl *t, idx_t key, type_t *buf) {                 \
        type_t *e = pfx##_htbl_ll_find(t, key, key);                         \
        if (e) {                                                             \
            *buf = *e;                                                       \
            pfx##_htbl_remove(t, e);                                         \
            return true;                                                     \
        }                                                                    \
        return false;                                                        \
    }

#define HTBL_MAP(t, f, ...)                                                  \
    do {                                                                     \
        (t)->inmap = true;                                                   \
        for (int var##_i = (t)->size - 1; var##_i >= 0; var##_i--) {         \
            if (!TST_BIT((t)->setbits, var##_i))                             \
                continue;                                                    \
            f((t)->tab + var##_i, ##__VA_ARGS__);                            \
        }                                                                    \
        (t)->inmap = false;                                                  \
    } while (0)

uint64_t htbl_hash_string(const void *s, int len);
bool htbl_keyequal(uint64_t h, const void *k1, const void *k2);

#define DO_HTBL_STROFFS(type_t, pfx, offs, inlined)                          \
    typedef struct { uint64_t h; type_t *e; } pfx##_helem_t;                 \
                                                                             \
    static inline const void *pfx##_get_key(pfx##_helem_t *he) {             \
        void *p = (char *)he->e + offs;                                      \
        return inlined ? p : *(const void **)p;                              \
    }                                                                        \
                                                                             \
    DO_HTBL_LL(pfx##_helem_t, const void *, pfx,                             \
               h, pfx##_get_key, htbl_keyequal);                             \
                                                                             \
    static inline type_t **                                                  \
    pfx##_htbl_insert(pfx##_htbl *t, type_t *e, int klen) {                  \
        pfx##_helem_t *res, he = { .e = e };                                 \
        he.h = htbl_hash_string(pfx##_get_key(&he), klen);                   \
        res = pfx##_htbl_ll_insert(t, he);                                   \
        return res ? &res->e : NULL;                                         \
    }                                                                        \
    static inline type_t **pfx##_htbl_insert2(pfx##_htbl *t, type_t *e) {    \
        return pfx##_htbl_insert(t, e, -1);                                  \
    }                                                                        \
                                                                             \
    static inline type_t **                                                  \
    pfx##_htbl_find(const pfx##_htbl *t, const void *key, int klen) {        \
        uint64_t h = htbl_hash_string(key, klen);                            \
        pfx##_helem_t *res = pfx##_htbl_ll_find(t, h, key);                  \
        return res ? &res->e : NULL;                                         \
    }                                                                        \
    static inline type_t **                                                  \
    pfx##_htbl_find2(const pfx##_htbl *t, const char *key) {                 \
        return pfx##_htbl_find(t, key, -1);                                  \
    }                                                                        \
    static inline type_t *                                                   \
    pfx##_htbl_get(const pfx##_htbl *t, const void *key, int klen) {         \
        uint64_t h = htbl_hash_string(key, klen);                            \
        pfx##_helem_t *res = pfx##_htbl_ll_find(t, h, key);                  \
        return res ? res->e : NULL;                                          \
    }                                                                        \
    static inline type_t *                                                   \
    pfx##_htbl_get2(const pfx##_htbl *t, const char *key) {                  \
        return pfx##_htbl_get(t, key, -1);                                   \
    }                                                                        \
                                                                             \
    static inline void                                                       \
    pfx##_htbl_remove(pfx##_htbl *t, type_t **e) {                           \
        if (e) {                                                             \
            pfx##_helem_t *he =                                              \
                (pfx##_helem_t *)((byte *)e - offsetof(pfx##_helem_t, e));   \
            pfx##_htbl_ll_remove(t, he);                                     \
        }                                                                    \
    }                                                                        \
    static inline type_t *                                                   \
    pfx##_htbl_take(pfx##_htbl *t, const void *key, int klen) {              \
        uint64_t h = htbl_hash_string(key, klen);                            \
        pfx##_helem_t *he = pfx##_htbl_ll_find(t, h, key);                   \
        type_t *res = he ? he->e : NULL;                                     \
        pfx##_htbl_ll_remove(t, he);                                         \
        return res;                                                          \
    }                                                                        \
    static inline type_t *                                                   \
    pfx##_htbl_take2(pfx##_htbl *t, const char *key) {                       \
        return pfx##_htbl_take(t, key, -1);                                  \
    }

#define DO_HTBL_STR(type_t, pfx, member, inlined)                            \
    DO_HTBL_STROFFS(type_t, pfx, offsetof(type_t, member), inlined)

#define HTBL_STR_MAP(t, f, ...)                                              \
    do {                                                                     \
        (t)->inmap = true;                                                   \
        for (int var##_i = (t)->size - 1; var##_i >= 0; var##_i--) {         \
            if (!TST_BIT((t)->setbits, var##_i))                             \
                continue;                                                    \
            f(&(t)->tab[var##_i].e, ##__VA_ARGS__);                          \
        }                                                                    \
        (t)->inmap = false;                                                  \
    } while (0)

DO_HTBL_STROFFS(char, string, 0, true);
static inline void string_htbl_deep_wipe(string_htbl *t) {
    HTBL_STR_MAP(t, p_delete);
    string_htbl_wipe(t);
}

#endif
