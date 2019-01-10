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

#include "container-qhash.h"
#include "container-qvector.h"
#include "arith.h"

#define QH_SETBITS_MASK  ((size_t)0x5555555555555555ULL)

/* 2^i < prime[i] */
static uint32_t const prime_list[32] = {
    11,         11,         11,         11,
    23,         53,         97,         193,
    389,        769,        1543,       3079,
    6151,       12289,      24593,      49157,
    98317,      196613,     393241,     786433,
    1572869,    3145739,    6291469,    12582917,
    25165843,   50331653,   100663319,  201326611,
    402653189,  805306457,  1610612741, 3221225473,
};

static uint32_t qhash_get_size(uint64_t targetsize)
{
    int b = bsr32(targetsize);

    if (unlikely(targetsize >= INT32_MAX))
        e_panic("out of memory");
    while (prime_list[b] < targetsize)
        b++;
    return prime_list[b];
}

static bool qhash_should_resize(const qhash_t *qh)
{
    const qhash_hdr_t *hdr = &qh->hdr;

    THROW_FALSE_IF(qh->old != NULL);

    if (unlikely((uint64_t)(hdr->len + qh->ghosts) * 3 >=
                 (uint64_t)hdr->size * 2))
    {
        return true;
    }

    if (unlikely(hdr->size > qh->minsize && hdr->len < hdr->size / 16)) {
        return true;
    }

    return false;
}

void qhash_unseal(qhash_t *qh)
{
    if (expect(qh->ghosts == UINT32_MAX)) {
        assert (!qh->old);
        qh->ghosts = 0;
    }
}

static void qhash_resize_start(qhash_t *qh)
{
    qhash_hdr_t *hdr = &qh->hdr;
    uint64_t newsize = qh->minsize;
    uint64_t len = hdr->len;

    if (newsize < 2 * (len + 1))
        newsize = 2 * (len + 1);
    if (newsize < hdr->size / 4)
        newsize = hdr->size / 4;

    newsize = qhash_get_size(newsize);
    if (newsize > hdr->size) {
        assert (!hdr->mp || !hdr->mp->realloc_fallback);
        qh->keys = mp_irealloc(hdr->mp, qh->keys, hdr->size * qh->k_size,
                               newsize * qh->k_size, 8, MEM_RAW);
        if (qh->v_size) {
            qh->values = mp_irealloc(hdr->mp, qh->values,
                                     hdr->size * qh->v_size,
                                     newsize * qh->v_size, 8, MEM_RAW);
        }
        if (qh->h_size) {
            qh->hashes = mp_irealloc(hdr->mp, qh->hashes,
                                     hdr->size * 4, newsize * 4, 4, MEM_RAW);
        }
    }
    if (hdr->len) {
        qh->old      = mp_dup(hdr->mp, hdr, 1);
        qh->old->len = hdr->size;
    } else {
        mp_delete(hdr->mp, &hdr->bits);
    }
    qh->ghosts     = 0;
    hdr->size      = newsize;
    hdr->bits      = mp_new(hdr->mp, size_t,
                            BITS_TO_ARRAY_LEN(size_t, 2 * newsize));
    SET_BIT(hdr->bits, 2 * newsize);
}

static void qhash_resize_done(qhash_t *qh)
{
    qhash_hdr_t *hdr = &qh->hdr;
    uint64_t size = hdr->size;

    if (qh->old->size > size) {
        qh->keys = mp_irealloc(hdr->mp, qh->keys, qh->old->size * qh->k_size,
                               size * qh->k_size, 8, MEM_RAW);
        if (qh->v_size) {
            qh->values = mp_irealloc(hdr->mp, qh->values,
                                     qh->old->size * qh->v_size,
                                     size * qh->v_size, 8, MEM_RAW);
        }
        if (qh->h_size) {
            qh->hashes = mp_irealloc(hdr->mp, qh->hashes,
                                     qh->old->size * 4,
                                     size * 4, 4, MEM_RAW);
        }
    }

    mp_delete(hdr->mp, &qh->old->bits);
    mp_delete(hdr->mp, &qh->old);
}

void qhash_init(qhash_t *qh, uint16_t k_size, uint16_t v_size, bool doh,
                mem_pool_t *mp)
{
    p_clear(qh, 1);
    qh->k_size = k_size;
    qh->v_size = v_size;
    qh->h_size = !!doh;
    qh->hdr.mp = mp;
}

void qhash_set_minsize(qhash_t *qh, uint32_t minsize)
{
    if (minsize) {
        qh->minsize = qhash_get_size(2 * (uint64_t)minsize);
        if (!qh->old && qh->hdr.size < qh->minsize)
            qhash_resize_start(qh);
    } else {
        qh->minsize = 0;
    }
}

void qhash_wipe(qhash_t *qh)
{
    if (qh->old) {
        mp_delete(qh->hdr.mp, &qh->old->bits);
        mp_delete(qh->hdr.mp, &qh->old);
    }
    mp_delete(qh->hdr.mp, &qh->hdr.bits);
    mp_delete(qh->hdr.mp, &qh->values);
    mp_delete(qh->hdr.mp, &qh->hashes);
    mp_delete(qh->hdr.mp, &qh->keys);
    qhash_init(qh, 0, 0, false, qh->hdr.mp);
}

void qhash_clear(qhash_t *qh)
{
#ifndef NDEBUG
    e_assert(panic, qh->ghosts != UINT32_MAX,
             "tried to clear a sealed hash table");
#endif

    if (qh->old) {
        mp_delete(qh->hdr.mp, &qh->old->bits);
        mp_delete(qh->hdr.mp, &qh->old);
    }
    if (qh->hdr.bits) {
        uint64_t size = qh->hdr.size;

        p_clear(qh->hdr.bits, BITS_TO_ARRAY_LEN(size_t, 2 * size));
        SET_BIT(qh->hdr.bits, 2 * size);
    }
    qh->hdr.len = 0;
    qh->ghosts = 0;
}

uint32_t qhash_scan(const qhash_t *qh, uint32_t pos)
{
    const qhash_hdr_t *hdr = &qh->hdr;
    const qhash_hdr_t *old = qh->old;

    size_t  maxsize = hdr->size;
    size_t *maxbits = hdr->bits;

    maxsize = 2 * maxsize;
    pos = 2 * pos;

    if (unlikely(old != NULL)) {
        size_t  minsize = old->len;
        size_t *minbits = old->bits;

        minsize = 2 * minsize;
        if (hdr->size < old->len) {
            minsize = hdr->size;
            minsize = 2 * minsize;
            minbits = hdr->bits;
            maxsize = old->len;
            maxsize = 2 * maxsize;
            maxbits = old->bits;
        }

        if (pos < minsize) {
            do {
                size_t word = minbits[pos / bitsizeof(size_t)]
                    | maxbits[pos / bitsizeof(size_t)];

                word &= (QH_SETBITS_MASK << (pos % bitsizeof(size_t)));
                pos  &= -bitsizeof(size_t);
                if (likely(word)) {
                    pos += bsfsz(word);
                    /* test for guard bit */
                    if (pos >= minsize)
                        break;
                    return pos / 2;
                }
                pos += bitsizeof(size_t);
            } while (pos < minsize);
            pos = minsize;
        }
    }

    for (;;) {
        size_t word = maxbits[pos / bitsizeof(size_t)];

        word &= (QH_SETBITS_MASK << (pos % bitsizeof(size_t)));
        pos  &= -bitsizeof(size_t);
        if (likely(word)) {
            pos += bsfsz(word);
            if (pos >= maxsize)
                return UINT32_MAX;
            return pos / 2;
        }
        pos += bitsizeof(size_t);
    }
}

#define F(x)               x##32
#define key_t              uint32_t
#define getK(qh, pos)      (((key_t *)(qh)->keys)[pos])
#define putK(qh, pos, k)   (getK(qh, pos) = (k))
#define hashK(qh, pos, k)  qhash_hash_u32(qh, k)
#define iseqK(qh, k1, k2)  ((k1) == (k2))
#include "container-qhash.in.c"

#define F(x)               x##64
#define key_t              uint64_t
#define getK(qh, pos)      (((key_t *)(qh)->keys)[pos])
#define putK(qh, pos, k)   (getK(qh, pos) = (k))
#define hashK(qh, pos, k)  qhash_hash_u64(qh, k)
#define iseqK(qh, k1, k2)  ((k1) == (k2))
#include "container-qhash.in.c"

#define MAY_CACHE_HASHES   1
#define F(x)               x##_ptr
#define F_PROTO            qhash_khash_f *hf, qhash_kequ_f *equ
#define F_ARGS             hf, equ
#define key_t              void *
#define getK(qh, pos)      (((key_t *)(qh)->keys)[pos])
#define putK(qh, pos, k)   (getK(qh, pos) = (k))
#define hashK(qh, pos, k)  ((qh)->hashes ? (qh)->hashes[pos] : (*hf)(qh, k))
#define iseqK(qh, k1, k2)  (*equ)(qh, k1, k2)
#include "container-qhash.in.c"

#define MAY_CACHE_HASHES   1
#define QH_DEEP_COPY
#define F(x)               x##_vec
#define F_PROTO            qhash_khash_f *hf, qhash_kequ_f *equ
#define F_ARGS             hf, equ
#define key_t              void *
#define getK(qh, pos)      ((qh)->keys + (pos) * (qh)->k_size)
#define putK(qh, pos, k)   memcpy(getK(qh, pos), k, (qh)->k_size)
#define hashK(qh, pos, k)  ((qh)->hashes ? (qh)->hashes[pos] : (*hf)(qh, k))
#define iseqK(qh, k1, k2)  (*equ)(qh, k1, k2)
#include "container-qhash.in.c"
