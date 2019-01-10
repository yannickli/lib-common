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

#ifndef IS_LIB_COMMON_CONTAINER_HTBL_H
#define IS_LIB_COMMON_CONTAINER_HTBL_H

#include "core.h"

#define CONTAINER_TYPE(kind, type_t, pfx)                                    \
    typedef struct pfx##_##kind {                                            \
        type_t *tab;                                                         \
        size_t *setbits;                                                     \
        size_t *ghostbits;                                                   \
        int len, size, ghosts;                                               \
        flag_t name_inline : 1;                                              \
        flag_t deny_shrink : 1;                                              \
        short name_offs;                                                     \
    } pfx##_##kind

CONTAINER_TYPE(htbl, void, generic);

uint32_t htbl_get_size(uint32_t len);
uint32_t htbl_scan_pos(generic_htbl *t, uint32_t pos);
uint32_t htbl_free_id(generic_htbl *t);

void htbl_init(generic_htbl *t, int size);
void htbl_wipe(generic_htbl *t);
void htbl_invalidate(generic_htbl *t, uint32_t pos);

#define __htbl_for_each(i, t, doit) \
    for (int i = (t)->len ? (int)htbl_scan_pos((generic_htbl *)(t), 0)       \
                          : (t)->size;                                       \
         i < (t)->size && (doit, true);                                      \
         i = htbl_scan_pos((generic_htbl *)(t), i + 1))

#define htbl_for_each_pos(i, t)  __htbl_for_each(i, t, (void)0)
#define htbl_for_each(e, t)      __htbl_for_each(e##_i, t, e = (t)->tab[e##_i])
#define htbl_for_each_p(e, t)                                                \
    __htbl_for_each(e##_i, t, e = (t)->tab + e##_i)
#define htbl_str_for_each(v, t)                                              \
    __htbl_for_each(v##_i, t, v = (t)->tab[v##_i].e)

#define DO_LL_CONTAINER(kind, type_t, idx_t, pfx, get_h, get_k, key_equal)   \
    static inline void pfx##_##kind##_wipe(pfx##_##kind *t) {                \
        htbl_wipe((generic_htbl *)t);                                        \
    }                                                                        \
    GENERIC_DELETE(pfx##_##kind, pfx##_##kind)                               \
                                                                             \
    static inline type_t *pfx##_##kind##_ll_insert(pfx##_##kind *, type_t);  \
    static inline void pfx##_##kind##_resize(pfx##_##kind *t) {              \
        uint32_t size = htbl_get_size(t->len);                               \
        pfx##_##kind old = *t;                                               \
                                                                             \
        htbl_init((generic_htbl *)t, size);                                  \
        t->deny_shrink = true;                                               \
        t->tab  = p_new(type_t, size);                                       \
        htbl_for_each_pos(i, &old) {                                         \
            pfx##_##kind##_ll_insert(t, old.tab[i]);                         \
        }                                                                    \
        htbl_wipe((generic_htbl *)&old);                                     \
        t->deny_shrink = false;                                              \
    }                                                                        \
                                                                             \
    static inline type_t *                                                   \
    pfx##_##kind##_ll_find(const pfx##_##kind *t, uint64_t h, idx_t key) {   \
        unsigned size = (unsigned)t->size;                                   \
        unsigned inc, pos;                                                   \
        if (!size)                                                           \
            return NULL;                                                     \
        pos = h % size;                                                      \
        inc = 1 + h % (size - 1);                                            \
        for (;;) {                                                           \
            if (!TST_BIT(t->ghostbits, pos)) {                               \
                type_t *ep = t->tab + pos;                                   \
                if (!TST_BIT(t->setbits, pos))                               \
                    return NULL;                                             \
                if (get_h(ep) == h && key_equal(h, get_k(t, ep), key))       \
                    return t->tab + pos;                                     \
            }                                                                \
            pos = pos + inc - (pos + inc < size ? 0 : size);                 \
        }                                                                    \
    }                                                                        \
                                                                             \
    static inline type_t *                                                   \
    pfx##_##kind##_ll_insert(pfx##_##kind *t, type_t e) {                    \
        uint64_t k;                                                          \
        unsigned size, pos, inc;                                             \
        int ghost = -1;                                                      \
                                                                             \
        if ((t->len + t->ghosts) * 3 >= t->size * 2                          \
        ||  (t->len < t->size / 16 && !t->deny_shrink))                      \
        {                                                                    \
            pfx##_##kind##_resize(t);                                        \
        }                                                                    \
                                                                             \
        size = (unsigned)t->size;                                            \
        k = get_h(&e);                                                       \
        pos = k % size;                                                      \
        inc = 1 + k % (size - 1);                                            \
        for (;;) {                                                           \
            if (!TST_BIT(t->ghostbits, pos)) {                               \
                type_t *ep = t->tab + pos;                                   \
                if (!TST_BIT(t->setbits, pos))                               \
                    break;                                                   \
                if (get_h(ep) == get_h(&e)                                   \
                &&  key_equal(get_h(&e), get_k(t, &e), get_k(t, ep)))        \
                {                                                            \
                    return t->tab + pos;                                     \
                }                                                            \
            } else if (ghost < 0) {                                          \
                ghost = pos;                                                 \
            }                                                                \
            pos = pos + inc - (pos + inc < size ? 0 : size);                 \
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
    static inline uint32_t pfx##_##kind##_free_id(pfx##_##kind *t) {         \
        return htbl_free_id((generic_htbl *)t);                              \
    }                                                                        \
    static inline void                                                       \
    pfx##_##kind##_invalidate(pfx##_##kind *t, int pos) {                    \
        htbl_invalidate((generic_htbl *)t, pos);                             \
    }                                                                        \
    static inline void                                                       \
    pfx##_##kind##_ll_remove(pfx##_##kind *t, type_t *e) {                   \
        if (e) {                                                             \
            assert (t->tab <= e && e < t->tab + t->size);                    \
            pfx##_##kind##_invalidate(t, e - t->tab);                        \
        }                                                                    \
    }

#define DO_HTBL_KEY_COMMON(kind, type_t, idx_t, get_h, get_k, pfx)           \
    CONTAINER_TYPE(kind, type_t, pfx);                                       \
                                                                             \
    GENERIC_INIT(pfx##_##kind, pfx##_##kind)                                 \
    GENERIC_NEW(pfx##_##kind, pfx##_##kind)                                  \
    DO_LL_CONTAINER(kind, type_t, idx_t, pfx, get_h, get_k, TRUEFN)          \
                                                                             \
    static inline type_t *                                                   \
    pfx##_##kind##_find(const pfx##_##kind *t, idx_t key) {                  \
        return pfx##_##kind##_ll_find(t, key, key);                          \
    }                                                                        \
    static inline type_t *                                                   \
    pfx##_##kind##_insert(pfx##_##kind *t, type_t e) {                       \
        return pfx##_##kind##_ll_insert(t, e);                               \
    }                                                                        \
    static inline type_t *                                                   \
    pfx##_##kind##_find_elem(pfx##_##kind *t, type_t e)                      \
    {                                                                        \
        idx_t key = get_k(t, &e);                                            \
        return pfx##_##kind##_ll_find(t, key, key);                          \
    }                                                                        \
    static inline void pfx##_##kind##_remove_elem(pfx##_##kind *t, type_t e) \
    {                                                                        \
        idx_t key = get_k(t, &e);                                            \
        pfx##_##kind##_ll_remove(t, pfx##_##kind##_ll_find(t, key, key));    \
    }

#define DO_HTBL_PKEY_COMMON(kind, type_t, idx_t, get_h, get_k, pfx)          \
    DO_HTBL_KEY_COMMON(kind, type_t *, idx_t, get_h, get_k, pfx)             \
                                                                             \
    static inline type_t *                                                   \
    pfx##_##kind##_get(const pfx##_##kind *t, idx_t key) {                   \
        type_t **e = pfx##_##kind##_find(t, key);                            \
        return e ? *e : NULL;                                                \
    }                                                                        \
    static inline type_t *pfx##_##kind##_take(pfx##_##kind *t, idx_t key) {  \
        type_t **e = pfx##_##kind##_ll_find(t, key, key);                    \
        if (e) {                                                             \
            type_t *res = *e;                                                \
            pfx##_##kind##_ll_remove(t, e);                                  \
            return res;                                                      \
        }                                                                    \
        return NULL;                                                         \
    }

#define DO_HTBL_KEY(type_t, idx_t, pfx, km)                                  \
    static inline idx_t pfx##_htbl_get_h(type_t *e) { return e->km; }        \
    static inline idx_t pfx##_htbl_get_k(const void *_u, type_t *e) {        \
        return e->km;                                                        \
    }                                                                        \
    DO_HTBL_KEY_COMMON(htbl, type_t, idx_t,                                  \
                       pfx##_htbl_get_h, pfx##_htbl_get_k, pfx)

#define DO_INT_SET(type_t, pfx)                                              \
    static inline uint64_t pfx##_set_get_h(type_t *e) { return *e; }         \
    static inline type_t   pfx##_set_get_k(const void *_u, type_t *e) {      \
        return *e;                                                           \
    }                                                                        \
    DO_HTBL_KEY_COMMON(set, type_t, type_t,                                  \
                       pfx##_set_get_h, pfx##_set_get_k, pfx)

#define DO_PTR_SET(type_t, pfx)                                              \
    static inline uintptr_t pfx##_set_get_h(type_t **e) {                    \
        return (uintptr_t)(*e);                                              \
    }                                                                        \
    static inline uintptr_t pfx##_set_get_k(const void *_u, type_t **e) {    \
        return (uintptr_t)(*e);                                              \
    }                                                                        \
    DO_HTBL_PKEY_COMMON(set, type_t, uintptr_t,                              \
                        pfx##_set_get_h, pfx##_set_get_k, pfx)

#define DO_HTBL_PKEY(type_t, idx_t, pfx, km)                                 \
    static inline idx_t pfx##_htbl_get_h(type_t **e) { return (*e)->km; }    \
    static inline idx_t pfx##_htbl_get_k(const void *_u, type_t **e) {       \
        return (*e)->km;                                                     \
    }                                                                        \
    DO_HTBL_PKEY_COMMON(htbl, type_t, idx_t,                                 \
                        pfx##_htbl_get_h, pfx##_htbl_get_k, pfx)

/* you can use htbl_ll_remove's in HTBL_MAP's */
#define HTBL_MAP(t, f, ...)                                                  \
    do {                                                                     \
        htbl_for_each_pos(__i, t) {                                          \
            f((t)->tab + __i, ##__VA_ARGS__);                                \
        }                                                                    \
    } while (0)

uint64_t htbl_hash_string(const void *s, int len);
bool htbl_keyequal(uint64_t h, const void *k1, const void *k2);

#define DO_HTBL_STR_COMMON(type_t, pfx)                                      \
    typedef struct { uint64_t h; type_t *e; } pfx##_helem_t;                 \
    CONTAINER_TYPE(htbl, pfx##_helem_t, pfx);                                \
                                                                             \
    static inline uint64_t pfx##_get_h(pfx##_helem_t *he) { return he->h; }  \
    static inline const void *                                               \
    pfx##_get_k(const pfx##_htbl *t, pfx##_helem_t *he) {                    \
        void *p = (char *)he->e + t->name_offs;                              \
        return t->name_inline ? p : *(const void **)p;                       \
    }                                                                        \
    DO_LL_CONTAINER(htbl, pfx##_helem_t, const void *, pfx,                  \
                    pfx##_get_h, pfx##_get_k, htbl_keyequal);                \
                                                                             \
    static inline type_t **                                                  \
    pfx##_htbl_insert(pfx##_htbl *t, type_t *e, int klen) {                  \
        pfx##_helem_t *res, he = { .e = e };                                 \
        he.h = htbl_hash_string(pfx##_get_k(t, &he), klen);                  \
        res = pfx##_htbl_ll_insert(t, he);                                   \
        return res ? &res->e : NULL;                                         \
    }                                                                        \
    static inline type_t **                                                  \
    pfx##_htbl_insert2(pfx##_htbl *t, type_t *e) {                           \
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

#define DO_HTBL_STROFFS(type_t, pfx, offs, inlined)                          \
    DO_HTBL_STR_COMMON(type_t, pfx);                                         \
    static inline pfx##_htbl *pfx##_##htbl_init(pfx##_htbl *t) {             \
        p_clear(t, 1);                                                       \
        t->name_inline = inlined;                                            \
        t->name_offs   = offs;                                               \
        return t;                                                            \
    }                                                                        \
    GENERIC_NEW(pfx##_htbl, pfx##_htbl)

#define DO_HTBL_STR(type_t, pfx, member, inlined)                            \
    DO_HTBL_STROFFS(type_t, pfx, offsetof(type_t, member), inlined)

#define HTBL_STROFFS_INIT(type_t, pfx, offs, inlined)                        \
    { .name_inline = inlined, .name_offs = offs }
#define HTBL_STR_INIT(type_t, pfx, member, inlined)                          \
    HTBL_STROFFS_INIT(type_t, pfx, offsetof(type_t, member), inlined)

#define DO_HTBL_FIELDINIT(type, pfx, sfx, field, inlined)                    \
    static inline pfx##_htbl *pfx##_htbl_##sfx##_init(pfx##_htbl *t) {       \
        p_clear(t, 1);                                                       \
        t->name_offs   = offsetof(type, field);                              \
        t->name_inline = inlined;                                            \
        return t;                                                            \
    }                                                                        \
    GENERIC_NEW(pfx##_htbl, pfx##_htbl_##sfx)

/* htbl_ll_remove can be called from HTBL_STR_MAP invokations via f().
 * calls to the f argument must not be parenthesized to allow macro
 * expansion instead of plain function calls
 */
#define HTBL_STR_MAP(t, f, ...)                                              \
    do {                                                                     \
        htbl_for_each_pos(__i, t) {                                          \
            f(&(t)->tab[__i].e, ##__VA_ARGS__);                              \
            if ((t)->tab[__i].e == NULL)                                     \
                htbl_invalidate((generic_htbl *)(t), __i);                   \
        }                                                                    \
    } while (0)

DO_HTBL_STROFFS(char, string, 0, true);
DO_HTBL_STROFFS(const char, cstring, 0, true);
static inline void string_htbl_deep_wipe(string_htbl *t) {
    HTBL_STR_MAP(t, p_delete);
    string_htbl_wipe(t);
}

#endif
