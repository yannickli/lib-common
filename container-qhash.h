/**************************************************************************/
/*                                                                        */
/*  Copyright (C) 2004-2012 INTERSEC SAS                                  */
/*                                                                        */
/*  Should you receive a copy of this source code, you must check you     */
/*  have a proper, written authorization of INTERSEC to hold it. If you   */
/*  don't have such an authorization, you must DELETE all source code     */
/*  files in your possession, and inform INTERSEC of the fact you obtain  */
/*  these files. Should you not comply to these terms, you can be         */
/*  prosecuted in the extent permitted by applicable law.                 */
/*                                                                        */
/**************************************************************************/

#if !defined(IS_LIB_COMMON_CONTAINER_H) || defined(IS_LIB_COMMON_CONTAINER_QHASH_H)
#  error "you must include container.h instead"
#else
#define IS_LIB_COMMON_CONTAINER_QHASH_H

#include "hash.h"

/*
 * QHashes: Real Time hash tables
 *
 *   The trick is during resize. When we resize, we keep the old qhash_hdr_t
 *   around. the keys/values arrays are shared during a resize between the old
 *   and new values, in place.
 *
 *   When we look for a given value, there are two functions:
 *
 *   qhash_get_safe::
 *       which doesn't modify the hash table, but has to look up in both the
 *       old and new view of the values to be sure the value doesn't exist.
 *
 *   qhash_get::
 *       this function preemptively moves all the collision chain that
 *       correspond to the searched key to the new hashtable, plus makes the
 *       move progress if one is in progress.
 *
 *       This function must not be used in a qhash enumeration.
 *
 *
 *   To reserve a new slot, one must use __qhash_put or a wrapper. __qhash_put
 *   returns the position where the key lives in the 31 least significant bits
 *   of the result. The most significant bit is used to notify that there is a
 *   value in that slot already.
 *
 *
 *   When we insert an element, it always goes in the "new" view of the
 *   hashtable. But the slot it has to be put may be occupied by the "old"
 *   view of the hashtable. Should that happen, we reinsert the value that
 *   lives at the offending place (note that the value may actually never
 *   move, but it doesn't really matter). Such a move may trigger more moves,
 *   but we assume that collision chains are usually short due to our double
 *   hashing.
 *
 */

#define QHASH_COLLISION     (1U << 31)
#define QHASH_OVERWRITE     (1  <<  0)

/*
 * len holds:
 *   - the number of elements in the hash when accessed through qh->hdr.len
 *   - the maximum position at which the old view still has elements through
 *     qh->old->len.
 */
typedef struct qhash_hdr_t {
    size_t   *bits;
    uint32_t  len;
    uint32_t  size;
} qhash_hdr_t;

#define STRUCT_QHASH_T(key_t, val_t) \
    struct {                                 \
        qhash_hdr_t  hdr;                    \
        qhash_hdr_t *old;                    \
        key_t       *keys;                   \
        val_t       *values;                 \
        uint32_t    *hashes;                 \
        uint32_t     ghosts;                 \
        uint8_t      h_size;                 \
        uint8_t      k_size;                 \
        uint16_t     v_size;                 \
        uint32_t     minsize;                \
    }

/* uint8_t allow us to use pointer arith on ->{values,vec} */
typedef STRUCT_QHASH_T(uint8_t, uint8_t) qhash_t;

typedef uint32_t (qhash_khash_f)(const qhash_t *, const void *);
typedef bool     (qhash_kequ_f)(const qhash_t *, const void *, const void *);


/****************************************************************************/
/* templatization and module helpers                                        */
/****************************************************************************/

/* helper functions, module functions {{{ */

uint32_t qhash_scan(const qhash_t *qh, uint32_t pos)
    __leaf;
void qhash_init(qhash_t *qh, uint16_t k_size, uint16_t v_size, bool doh)
    __leaf;
void qhash_clear(qhash_t *qh)
    __leaf;
void qhash_set_minsize(qhash_t *qh, uint32_t minsize)
    __leaf;
void qhash_wipe(qhash_t *qh)
    __leaf;

static inline void qhash_slot_inv_flags(size_t *bits, uint32_t pos)
{
    size_t off = (2 * pos) % bitsizeof(size_t);

    bits[2 * pos / bitsizeof(size_t)] ^= (size_t)3 << off;
}
static inline size_t qhash_slot_get_flags(const size_t *bits, uint32_t pos)
{
    size_t off = (2 * pos) % bitsizeof(size_t);
    return (bits[2 * pos / bitsizeof(size_t)] >> off) & (size_t)3;
}
static inline size_t qhash_slot_is_set(const size_t *bits, uint32_t pos)
{
    return TST_BIT(bits, 2 * pos);
}

static inline void qhash_del_at(qhash_t *qh, uint32_t pos)
{
    qhash_hdr_t *hdr = &qh->hdr;
    qhash_hdr_t *old = qh->old;

    if (likely(qhash_slot_is_set(hdr->bits, pos))) {
        qhash_slot_inv_flags(hdr->bits, pos);
        hdr->len--;
        qh->ghosts++;
    } else
    if (unlikely(old != NULL) && qhash_slot_is_set(old->bits, pos)) {
        qhash_slot_inv_flags(old->bits, pos);
        hdr->len--;
    }
}

static inline uint32_t qhash_hash_u32(const qhash_t *qh, uint32_t u32)
{
    return u32;
}
static inline uint32_t qhash_hash_u64(const qhash_t *qh, uint64_t u64)
{
    return (uint32_t)(u64) ^ (uint32_t)(u64 >> 32);
}

static inline uint32_t qhash_hash_ptr(const qhash_t *qh, const void *ptr)
{
    if (sizeof(void *) == 4)
        return (uintptr_t)ptr;
    return qhash_hash_u64(qh, (uintptr_t)ptr);
}

#define __qhash_for_each(i, qh, doit) \
    for (uint32_t i = (qh)->hdr.len ? qhash_scan(qh, 0) : UINT32_MAX; \
         i != UINT32_MAX && (doit, true);                             \
         i = qhash_scan(qh, i + 1))

#define qhash_for_each_pos(i, qh)       __qhash_for_each(i, qh, (void)0)


int32_t  qhash_safe_get32(const qhash_t *qh, uint32_t h, uint32_t k)
    __leaf;
int32_t  qhash_get32(qhash_t *qh, uint32_t h, uint32_t k)
    __leaf;
uint32_t __qhash_put32(qhash_t *qh, uint32_t h, uint32_t k, uint32_t flags)
    __leaf;

int32_t  qhash_safe_get64(const qhash_t *qh, uint32_t h, uint64_t k)
    __leaf;
int32_t  qhash_get64(qhash_t *qh, uint32_t h, uint64_t k)
    __leaf;
uint32_t __qhash_put64(qhash_t *qh, uint32_t h, uint64_t k, uint32_t flags)
    __leaf;

int32_t  qhash_safe_get_ptr(const qhash_t *qh, uint32_t h, const void *k,
                            qhash_khash_f *hf, qhash_kequ_f *equ);
int32_t  qhash_get_ptr(qhash_t *qh, uint32_t h, const void *k,
                       qhash_khash_f *hf, qhash_kequ_f *equ);
uint32_t __qhash_put_ptr(qhash_t *qh, uint32_t h, const void *k,
                         uint32_t flags, qhash_khash_f *hf,
                         qhash_kequ_f *equ);

int32_t  qhash_safe_get_vec(const qhash_t *qh, uint32_t h, const void *k,
                            qhash_khash_f *hf, qhash_kequ_f *equ);
int32_t  qhash_get_vec(qhash_t *qh, uint32_t h, const void *k,
                       qhash_khash_f *hf, qhash_kequ_f *equ);
uint32_t __qhash_put_vec(qhash_t *qh, uint32_t h, const void *k,
                         uint32_t flags, qhash_khash_f *hf,
                         qhash_kequ_f *equ);

/* }}} */
/*----- base macros to define QH's and QM's -{{{-*/

#define __QH_BASE(sfx, pfx, name, key_t, val_t, v_size) \
    typedef union pfx##_t {                                                  \
        qhash_t qh;                                                          \
        STRUCT_QHASH_T(key_t, val_t);                                        \
    } pfx##_t;                                                               \
                                                                             \
    static inline void pfx##_init(pfx##_t *qh, bool chahes) {                \
        STATIC_ASSERT(sizeof(key_t) < 256);                                  \
        qhash_init(&qh->qh, sizeof(key_t), v_size, chahes);                  \
    }                                                                        \
    static inline void pfx##_wipe(pfx##_t *qh) {                             \
        qhash_wipe(&qh->qh);                                                 \
    }                                                                        \
    static inline void pfx##_clear(pfx##_t *qh) {                            \
        qhash_clear(&qh->qh);                                                \
    }                                                                        \
    static inline void pfx##_del_at(pfx##_t *qh, uint32_t pos) {             \
        qhash_del_at(&qh->qh, pos);                                          \
    }                                                                        \
    static inline int32_t pfx##_len(const pfx##_t *qh) {                     \
        return qh->hdr.len;                                                  \
    }

#define __QH_FIND(sfx, pfx, name, ckey_t, key_t, hashK) \
    static inline int32_t pfx##_find(pfx##_t *qh, ckey_t key) {              \
        return qhash_get##sfx(&qh->qh, hashK(&qh->qh, key), key);            \
    }                                                                        \
    static inline int32_t                                                    \
    pfx##_find_h(pfx##_t *qh, uint32_t h, ckey_t key) {                      \
        return qhash_get##sfx(&qh->qh, h, key);                              \
    }                                                                        \
    static inline int32_t                                                    \
    pfx##_find_safe(const pfx##_t *qh, ckey_t key) {                         \
        return qhash_safe_get##sfx(&qh->qh, hashK(&qh->qh, key), key);       \
    }                                                                        \
    static inline int32_t                                                    \
    pfx##_find_safe_h(const pfx##_t *qh, uint32_t h, ckey_t key) {           \
        return qhash_safe_get##sfx(&qh->qh, h, key);                         \
    }

#define __QH_FIND2(sfx, pfx, name, ckey_t, key_t, hashK, iseqK) \
    static inline int32_t pfx##_find(pfx##_t *qh, ckey_t key) {              \
        uint32_t (*hf)(const qhash_t *, ckey_t) = &hashK;                    \
        bool     (*ef)(const qhash_t *, ckey_t, ckey_t) = &iseqK;            \
        uint32_t h = (*hf)(&qh->qh, key);                                    \
        return qhash_get##sfx(&qh->qh, h, key,                               \
                              (qhash_khash_f *)hf, (qhash_kequ_f *)ef);      \
    }                                                                        \
    static inline int32_t                                                    \
    pfx##_find_h(pfx##_t *qh, uint32_t h, ckey_t key) {                      \
        uint32_t (*hf)(const qhash_t *, ckey_t) = &hashK;                    \
        bool     (*ef)(const qhash_t *, ckey_t, ckey_t) = &iseqK;            \
        return qhash_get##sfx(&qh->qh, h, key,                               \
                              (qhash_khash_f *)hf, (qhash_kequ_f *)ef);      \
    }                                                                        \
    static inline int32_t                                                    \
    pfx##_find_safe(const pfx##_t *qh, ckey_t key) {                         \
        uint32_t (*hf)(const qhash_t *, ckey_t) = &hashK;                    \
        bool     (*ef)(const qhash_t *, ckey_t, ckey_t) = &iseqK;            \
        uint32_t h = (*hf)(&qh->qh, key);                                    \
        return qhash_safe_get##sfx(&qh->qh, h, key,                          \
                              (qhash_khash_f *)hf, (qhash_kequ_f *)ef);      \
    }                                                                        \
    static inline int32_t                                                    \
    pfx##_find_safe_h(const pfx##_t *qh, uint32_t h, ckey_t key) {           \
        uint32_t (*hf)(const qhash_t *, ckey_t) = &hashK;                    \
        bool     (*ef)(const qhash_t *, ckey_t, ckey_t) = &iseqK;            \
        return qhash_safe_get##sfx(&qh->qh, h, key,                          \
                                   (qhash_khash_f *)hf, (qhash_kequ_f *)ef); \
    }

/* }}} */
/*----- macros to define QH's -{{{-*/

#define __QH_ADD(sfx, pfx, name, key_t, hashK) \
    static inline uint32_t pfx##_hash(pfx##_t *qh, key_t key) {              \
        return hashK(&qh->qh, key);                                          \
    }                                                                        \
    static inline uint32_t                                                   \
    __##pfx##_put(pfx##_t *qh, key_t key, uint32_t fl) {                     \
        return __##pfx##_put_h(qh, hashK(&qh->qh, key), key, fl);            \
    }                                                                        \
    static inline int pfx##_add_h(pfx##_t *qh, uint32_t h, key_t key)        \
    {                                                                        \
        return (int)__##pfx##_put_h(qh, h, key, 0) >> 31;                    \
    }                                                                        \
    static inline int pfx##_add(pfx##_t *qh, key_t key) {                    \
        return pfx##_add_h(qh, hashK(&qh->qh, key), key);                    \
    }                                                                        \
    static inline int pfx##_replace_h(pfx##_t *qh, uint32_t h, key_t key) {  \
        return (int)__##pfx##_put_h(qh, h, key, QHASH_OVERWRITE) >> 31;      \
    }                                                                        \
    static inline int pfx##_replace(pfx##_t *qh, key_t key) {                \
        return pfx##_replace_h(qh, hashK(&qh->qh, key), key);                \
    }

#define __QH_IKEY(sfx, pfx, name, key_t)  \
    __QH_BASE(sfx, pfx, name, key_t, void, 0);                               \
    __QH_FIND(sfx, pfx, name, key_t const, key_t, qhash_hash_u##sfx);        \
                                                                             \
    static inline uint32_t                                                   \
    __##pfx##_put_h(pfx##_t *qh, uint32_t h, key_t key, uint32_t fl) {       \
        uint32_t pos = __qhash_put##sfx(&qh->qh, h, key, fl);                \
                                                                             \
        if ((fl & QHASH_OVERWRITE) || !(pos & QHASH_COLLISION)) {            \
            qh->keys[pos & ~QHASH_COLLISION] = key;                          \
        }                                                                    \
        return pos;                                                          \
    }                                                                        \
    __QH_ADD(sfx, pfx, name, key_t, qhash_hash_u##sfx)

#define __QH_PKEY(pfx, name, ckey_t, key_t, hashK, iseqK) \
    __QH_BASE(_ptr, pfx, name, key_t *, void, 0);                            \
    __QH_FIND2(_ptr, pfx, name, ckey_t *, key_t *, hashK, iseqK);            \
                                                                             \
    static inline uint32_t                                                   \
    __##pfx##_put_h(pfx##_t *qh, uint32_t h, key_t *key, uint32_t fl) {      \
        uint32_t (*hf)(const qhash_t *, ckey_t *) = &hashK;                  \
        bool     (*ef)(const qhash_t *, ckey_t *, ckey_t *) = &iseqK;        \
        uint32_t pos = __qhash_put_ptr(&qh->qh, h, key, fl,                  \
                              (qhash_khash_f *)hf, (qhash_kequ_f *)ef);      \
                                                                             \
        if ((fl & QHASH_OVERWRITE) || !(pos & QHASH_COLLISION)) {            \
            qh->keys[pos & ~QHASH_COLLISION] = key;                          \
        }                                                                    \
        return pos;                                                          \
    }                                                                        \
    __QH_ADD(_ptr, pfx, name, key_t *, hashK)

#define __QH_VKEY(pfx, name, ckey_t, key_t, hashK, iseqK) \
    __QH_BASE(_vec, pfx, name, key_t, void, 0);                              \
    __QH_FIND2(_vec, pfx, name, ckey_t *, key_t *, hashK, iseqK);            \
                                                                             \
    static inline uint32_t                                                   \
    __##pfx##_put_h(pfx##_t *qh, uint32_t h, ckey_t *key, uint32_t fl) {     \
        uint32_t (*hf)(const qhash_t *, ckey_t*) = &hashK;                   \
        bool     (*ef)(const qhash_t *, ckey_t*, ckey_t*) =  &iseqK;         \
        uint32_t pos = __qhash_put_vec(&qh->qh, h, key, fl,                  \
                                       (qhash_khash_f *)hf,                  \
                                       (qhash_kequ_f *)ef);                  \
                                                                             \
        if ((fl & QHASH_OVERWRITE) || !(pos & QHASH_COLLISION)) {            \
            qh->keys[pos & ~QHASH_COLLISION] = *key;                         \
        }                                                                    \
        return pos;                                                          \
    }                                                                        \
    __QH_ADD(_vec, pfx, name, ckey_t *, hashK)

/* }}} */
/*----- macros to define QM's -{{{-*/

#define __QM_ADD(sfx, pfx, name, key_t, val_t, hashK) \
    static inline uint32_t pfx##_hash(pfx##_t *qh, key_t key) {              \
        return hashK(&qh->qh, key);                                          \
    }                                                                        \
    static inline uint32_t                                                   \
    __##pfx##_put(pfx##_t *qh, key_t key, val_t v, uint32_t fl) {            \
        return __##pfx##_put_h(qh, hashK(&qh->qh, key), key, v, fl);         \
    }                                                                        \
    static inline int                                                        \
    pfx##_add_h(pfx##_t *qh, uint32_t h, key_t key, val_t v) {               \
        return (int)__##pfx##_put_h(qh, h, key, v, 0) >> 31;                 \
    }                                                                        \
    static inline int pfx##_add(pfx##_t *qh, key_t key, val_t v) {           \
        return pfx##_add_h(qh, hashK(&qh->qh, key), key, v);                 \
    }                                                                        \
    static inline int                                                        \
    pfx##_replace_h(pfx##_t *qh, uint32_t h, key_t key, val_t v) {           \
        return (int)__##pfx##_put_h(qh, h, key, v, QHASH_OVERWRITE) >> 31;   \
    }                                                                        \
    static inline int pfx##_replace(pfx##_t *qh, key_t key, val_t v)         \
    {                                                                        \
        return pfx##_replace_h(qh, hashK(&qh->qh, key), key, v);             \
    }

#define __QM_IKEY(sfx, pfx, name, key_t, val_t)  \
    __QH_BASE(sfx, pfx, name, key_t, val_t, sizeof(val_t));                  \
    __QH_FIND(sfx, pfx, name, key_t const, key_t, qhash_hash_u##sfx);        \
                                                                             \
    static inline uint32_t                                                   \
    __##pfx##_put_h(pfx##_t *qh, uint32_t h,                                 \
                    key_t key, val_t v, uint32_t fl) {                       \
        uint32_t pos = __qhash_put##sfx(&qh->qh, h, key, fl);                \
                                                                             \
        if ((fl & QHASH_OVERWRITE) || !(pos & QHASH_COLLISION)) {            \
            qh->keys[pos & ~QHASH_COLLISION] = key;                          \
            qh->values[pos & ~QHASH_COLLISION] = v;                          \
        }                                                                    \
        return pos;                                                          \
    }                                                                        \
    __QM_ADD(sfx, pfx, name, key_t, val_t, qhash_hash_u##sfx)

#define __QM_PKEY(pfx, name, ckey_t, key_t, val_t, hashK, iseqK) \
    __QH_BASE(_ptr, pfx, name, key_t *, val_t, sizeof(val_t));               \
    __QH_FIND2(_ptr, pfx, name, ckey_t *, key_t *, hashK, iseqK);            \
                                                                             \
    static inline uint32_t                                                   \
    __##pfx##_put_h(pfx##_t *qh, uint32_t h,                                 \
                    key_t *key, val_t v, uint32_t fl) {                      \
        uint32_t (*hf)(const qhash_t *, ckey_t*) = &hashK;                   \
        bool     (*ef)(const qhash_t *, ckey_t*, ckey_t*) = &iseqK;          \
        uint32_t pos = __qhash_put_ptr(&qh->qh, h, key, fl,                  \
                              (qhash_khash_f *)hf, (qhash_kequ_f *)ef);      \
                                                                             \
        if ((fl & QHASH_OVERWRITE) || !(pos & QHASH_COLLISION)) {            \
            qh->keys[pos & ~QHASH_COLLISION]   = key;                        \
            qh->values[pos & ~QHASH_COLLISION] = v;                          \
        }                                                                    \
        return pos;                                                          \
    }                                                                        \
    __QM_ADD(_ptr, pfx, name, key_t *, val_t, hashK)

#define __QM_VKEY(pfx, name, ckey_t, key_t, val_t, hashK, iseqK) \
    __QH_BASE(_vec, pfx, name, key_t, val_t, sizeof(val_t));                 \
    __QH_FIND2(_vec, pfx, name, ckey_t *, key_t *, hashK, iseqK);            \
    static inline uint32_t                                                   \
    __##pfx##_put_h(pfx##_t *qh, uint32_t h,                                 \
                    ckey_t *key, val_t v, uint32_t fl) {                     \
        uint32_t (*hf)(const qhash_t *, ckey_t*) = &hashK;                   \
        bool     (*ef)(const qhash_t *, ckey_t*, ckey_t*) = &iseqK;          \
        uint32_t pos = __qhash_put_vec(&qh->qh, h, key, fl,                  \
                                       (qhash_khash_f *)hf,                  \
                                       (qhash_kequ_f *)ef);                  \
                                                                             \
        if ((fl & QHASH_OVERWRITE) || !(pos & QHASH_COLLISION)) {            \
            qh->keys[pos & ~QHASH_COLLISION]   = *key;                       \
            qh->values[pos & ~QHASH_COLLISION] = v;                          \
        }                                                                    \
        return pos;                                                          \
    }                                                                        \
    __QM_ADD(_vec, pfx, name, ckey_t *, val_t, hashK)

/* }}} */


/****************************************************************************/
/* Macros to abstract the templating mess                                   */
/****************************************************************************/

/**
 * \fn qm_wipe(name, qh)
 *     Wipes the hash table structure, but not the indexed elements.
 *     You'll have to use qhash_for_each_pos to do that.
 */

#define qh_k32_t(name)                  __QH_IKEY(32, qh_##name, name, uint32_t)
#define qh_k64_t(name)                  __QH_IKEY(64, qh_##name, name, uint64_t)
#define qh_kvec_t(name, key_t, hf, ef)  __QH_VKEY(qh_##name, name, key_t const, key_t, hf, ef)
#define qh_kptr_t(name, key_t, hf, ef)  __QH_PKEY(qh_##name, name, key_t const, key_t, hf, ef)
#define qh_kvec_t(name, key_t, hf, ef)  __QH_VKEY(qh_##name, name, key_t const, key_t, hf, ef)
#define qh_kptr_t(name, key_t, hf, ef)  __QH_PKEY(qh_##name, name, key_t const, key_t, hf, ef)
#define qh_kptr_ckey_t(name, key_t, hf, ef)  __QH_PKEY(qh_##name, name, key_t const, key_t const, hf, ef)

#define qm_k32_t(name, val_t)                  __QM_IKEY(32, qm_##name, name, uint32_t, val_t)
#define qm_k64_t(name, val_t)                  __QM_IKEY(64, qm_##name, name, uint64_t, val_t)
#define qm_kvec_t(name, key_t, val_t, hf, ef)  __QM_VKEY(qm_##name, name, key_t const, key_t, val_t, hf, ef)
#define qm_kptr_t(name, key_t, val_t, hf, ef)  __QM_PKEY(qm_##name, name, key_t const, key_t, val_t, hf, ef)
#define qm_kptr_ckey_t(name, key_t, val_t, hf, ef)  __QM_PKEY(qm_##name, name, key_t const, key_t const, val_t, hf, ef)

#define QH_INIT(name, var, cacheh) \
    { .qh = {                                   \
        .k_size = sizeof((var).keys[0]),        \
        .h_size = !!(cacheh),                   \
    } }

#define QM_INIT(name, var, cacheh) \
    { .qh = {                                   \
        .k_size = sizeof((var).keys[0]),        \
        .v_size = sizeof((var).values[0]),      \
        .h_size = !!(cacheh),                   \
    } }

#define QH(name, var, cacheh)  qh_t(name) var = QH_INIT(name, var, cacheh)
#define QM(name, var, cacheh)  qm_t(name) var = QM_INIT(name, var, cacheh)

/*
 * The difference between the qh_ and qm_ functions is for the `add` and
 * `replace` ones, since maps have to deal with the associated value.
 */
#define qh_for_each_pos(name, pos, h)  \
    STATIC_ASSERT(__builtin_types_compatible_p(typeof(h), qh_t(name) *)      \
              ||  __builtin_types_compatible_p(typeof(h),                    \
                                               const qh_t(name) *));         \
    qhash_for_each_pos(pos, &(h)->qh)

#define qh_t(name)                          qh_##name##_t
#define qh_init(name, qh, chahes)           qh_##name##_init(qh, chahes)
#define qh_len(name, qh)                    qh_##name##_len(qh)
#define qh_hash(name, qh, key)              qh_##name##_hash(qh, key)
#define qh_set_minsize(name, h, sz)         qhash_set_minsize(&(h)->qh, sz)
#define qh_wipe(name, qh)                   qh_##name##_wipe(qh)
#define qh_clear(name, qh)                  qh_##name##_clear(qh)
#define qh_find(name, qh, key)              qh_##name##_find(qh, key)
#define qh_find_h(name, qh, h, key)         qh_##name##_find_h(qh, h, key)
#define qh_find_safe(name, qh, key)         qh_##name##_find_safe(qh, key)
#define qh_find_safe_h(name, qh, h, key)    qh_##name##_find_safe_h(qh, h, key)
/** \see __qm_put */
#define __qh_put(name, qh, key, fl)         __qh_##name##_put(qh, key, fl)
#define __qh_put_h(name, qh, h, key, fl)    __qh_##name##_put_h(qh, h, key, fl)
/** \see __qm_add */
#define qh_add(name, qh, key)               qh_##name##_add(qh, key)
#define qh_add_h(name, qh, h, key)          qh_##name##_add_h(qh, h, key)
/** \see __qm_replace */
#define qh_replace(name, qh, key)           qh_##name##_replace(qh, key)
#define qh_replace_h(name, qh, h, key)      qh_##name##_replace_h(qh, h, key)
#define qh_del_at(name, qh, pos)            qh_##name##_del_at(qh, pos)
#define qh_del_key(name, qh, key)  \
    ({ int32_t __pos = qh_find(name, qh, key);                           \
       if (likely(__pos >= 0)) qh_del_at(name, qh, __pos);               \
       __pos; })
#define qh_del_key_h(name, qh, h, key)  \
    ({ int32_t __pos = qh_find_h(name, qh, h, key);                      \
       if (likely(__pos >= 0)) qh_del_at(name, qh, __pos);               \
       __pos; })

#define qh_deep_clear(name, h, wipe)                                     \
    do {                                                                 \
        qh_t(name) *__h = (h);                                           \
        qh_for_each_pos(name, __pos, __h) {                              \
            wipe(&(__h)->keys[__pos]);                                   \
        }                                                                \
        qh_clear(name, __h);                                             \
    } while (0)

#define qh_deep_wipe(name, h, wipe)                                      \
    do {                                                                 \
        qh_t(name) *__h = (h);                                           \
        qh_for_each_pos(name, __pos, __h) {                              \
            wipe(&(__h)->keys[__pos]);                                   \
        }                                                                \
        qh_wipe(name, __h);                                              \
    } while (0)

#define qm_for_each_pos(name, pos, h)  \
    STATIC_ASSERT(__builtin_types_compatible_p(typeof(h), qm_t(name) *)      \
              ||  __builtin_types_compatible_p(typeof(h),                    \
                                               const qm_t(name) *));         \
    qhash_for_each_pos(pos, &(h)->qh)
#define qm_t(name)                          qm_##name##_t
#define qm_init(name, qh, chahes)           qm_##name##_init(qh, chahes)
#define qm_len(name, qh)                    qm_##name##_len(qh)
#define qm_hash(name, qh, key)              qm_##name##_hash(qh, key)
#define qm_set_minsize(name, h, sz)         qhash_set_minsize(&(h)->qh, sz)
#define qm_wipe(name, qh)                   qm_##name##_wipe(qh)
#define qm_clear(name, qh)                  qm_##name##_clear(qh)
#define qm_find(name, qh, key)              qm_##name##_find(qh, key)
#define qm_find_h(name, qh, h, key)         qm_##name##_find_h(qh, h, key)
#define qm_find_safe(name, qh, key)         qm_##name##_find_safe(qh, key)
#define qm_find_safe_h(name, qh, h, key)    qm_##name##_find_safe_h(qh, h, key)

#define qm_deep_clear(name, h, k_wipe, v_wipe)                           \
    do {                                                                 \
        qm_t(name) *__h = (h);                                           \
        qm_for_each_pos(name, __pos, __h) {                              \
            k_wipe(&(__h)->keys[__pos]);                                 \
            v_wipe(&(__h)->values[__pos]);                               \
        }                                                                \
        qm_clear(name, __h);                                             \
    } while (0)

#define qm_deep_wipe(name, h, k_wipe, v_wipe)                            \
    do {                                                                 \
        qm_t(name) *__h = (h);                                           \
        qm_for_each_pos(name, __pos, __h) {                              \
            k_wipe(&(__h)->keys[__pos]);                                 \
            v_wipe(&(__h)->values[__pos]);                               \
        }                                                                \
        qm_wipe(name, __h);                                              \
    } while (0)

/** Find-reserve slot to insert {key,v} pair.
 *
 * This function finds the slot where to insert {\a key, \a v} in the qmap.
 * If there is no collision, the slot is reserved and filled with
 * {\a key, \a v}, else the behavior depends upon the \a fl argument.
 *
 * This function is useful to write efficient code (spare one lookup) in code
 * that could naively be written:
 * <code>
 * if (qm_find(..., qh, key) < 0) {
 *     // prepare 'v'
 *     qm_add(..., qh, key, v);
 * }
 * </code>
 * and can instead be written:
 * <code>
 * pos = __qm_put(..., qh, key, v, 0);
 * if (!(pos & QHASH_COLLISION)) {
 *     // fixup qh->{keys,values}[pos];
 * }
 * </code>
 *
 * @param name the base name of the qmap
 * @param qh   pointer to the qmap in wich the value shall be inserted
 * @param key  the value of the key
 * @param v    the value associated to the key
 * @param fl
 *   0 or QHASH_OVERWRITE. If QHASH_OVERWRITE is set, \a key and \a v are used
 *   to overwrite the {key,value} stored in the qmap when there is a
 *   collision.
 * @return
 *   the position of the slot is returned in the 31 least significant bits.
 *   The most significant bit is used to tell if there was a collision.
 *   The constant #QHASH_COLLISION is provided to extract and mask this most
 *   significant bit.
 */
#define __qm_put(name, qh, key, v, fl)      __qm_##name##_put(qh, key, v, fl)
#define __qm_put_h(name, qh, h, key, v, fl) __qm_##name##_put_h(qh, h, key, v, fl)

/** Adds a new key/value pair into the qmap.
 *
 * When key already exists in the qmap, the add function fails.  If you want
 * to insert or overwrite existing values the replace macro shall be used.
 *
 * @param name the base name of the qmap
 * @param qh   pointer to the qmap in wich the value shall be inserted
 * @param key  the value of the key
 * @param v    the value associated to the key
 *
 * @return     0 if inserted in the qmap, -1 if insertion fails.
 */
#define qm_add(name, qh, key, v)            qm_##name##_add(qh, key, v)
#define qm_add_h(name, qh, h, key, v)       qm_##name##_add_h(qh, h, key, v)

/** Replaces value for a given key the qmap.
 *
 * If the key does not exists in the current qmap, then the pair key/value
 * is created and inserted.
 *
 * @param name the base name of the qmap
 * @param qh   pointer to the qmap in wich the value shall be inserted
 * @param key  the value of the key
 * @param v    the value associated to the key
 *
 * @return     0 if it's an insertion, -1 if it's a replace.
 */
#define qm_replace(name, qh, key, v)        qm_##name##_replace(qh, key, v)
#define qm_replace_h(name, qh, h, key, v)   qm_##name##_replace_h(qh, h, key, v)
#define qm_del_at(name, qh, pos)            qm_##name##_del_at(qh, pos)
#define qm_del_key(name, qh, key)  \
    ({ int32_t __pos = qm_find(name, qh, key);                           \
       if (likely(__pos >= 0)) qm_del_at(name, qh, __pos);               \
       __pos; })
#define qm_del_key_h(name, qh, h, key)  \
    ({ int32_t __pos = qm_find_h(name, qh, h, key);                      \
       if (likely(__pos >= 0)) qm_del_at(name, qh, __pos);               \
       __pos; })

static inline uint32_t qhash_str_hash(const qhash_t *qh, const char *s)
{
    return mem_hash32(s, -1);
}

static inline bool
qhash_str_equal(const qhash_t *qh, const char *s1, const char *s2)
{
    return strequal(s1, s2);
}

static inline uint32_t qhash_lstr_hash(const qhash_t *qh, const lstr_t *ls)
{
    return mem_hash32(ls->s, ls->len);
}

static inline bool
qhash_lstr_equal(const qhash_t *qh, const lstr_t *s1, const lstr_t *s2)
{
    return lstr_equal(s1, s2);
}

static inline uint32_t qhash_clstr_hash(const qhash_t *qh, const lstr_t *ls)
{
    return mem_hash32(ls->s, ls->len);
}

static inline bool
qhash_clstr_equal(const qhash_t *qh, const lstr_t *s1, const lstr_t *s2)
{
    return lstr_equal(s1, s2);
}

static inline bool
qhash_ptr_equal(const qhash_t *qh, const void *ptr1, const void *ptr2)
{
    return ptr1 == ptr2;
}

qh_k32_t(u32);
qh_k64_t(u64);
qh_kptr_t(str,   char,    qhash_str_hash,  qhash_str_equal);
qh_kvec_t(lstr,  lstr_t,  qhash_lstr_hash, qhash_lstr_equal);
qh_kvec_t(clstr, lstr_t,  qhash_lstr_hash, qhash_lstr_equal);
qh_kptr_t(ptr,   void,    qhash_hash_ptr,  qhash_ptr_equal);

#endif
