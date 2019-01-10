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

#include "container.h"
#include "arith.h"

#define QH_SETBITS_MASK  ((size_t)0x5555555555555555ULL)

/* 2^i < prime[i], and often prime[i] > 2^i */
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
        p_realloc(&qh->keys, newsize * qh->k_size);
        if (qh->v_size)
            p_realloc(&qh->values, newsize * qh->v_size);
        if (qh->h_size)
            p_realloc(&qh->hashes, newsize);
    }
    if (hdr->len) {
        qh->old      = p_dup(hdr, 1);
        qh->old->len = hdr->size;
    } else {
        p_delete(&hdr->bits);
    }
    qh->ghosts     = 0;
    hdr->size      = newsize;
    hdr->bits      = p_new(size_t, BITS_TO_ARRAY_LEN(size_t, 2 * newsize));
    SET_BIT(hdr->bits, 2 * newsize);
}

static void qhash_resize_done(qhash_t *qh)
{
    qhash_hdr_t *hdr = &qh->hdr;
    uint64_t size = hdr->size;

    if (qh->old->size > size) {
        p_realloc(&qh->keys, size * qh->k_size);
        if (qh->v_size)
            p_realloc(&qh->values, size * qh->v_size);
        if (qh->h_size)
            p_realloc(&qh->hashes, size);
    }

    p_delete(&qh->old->bits);
    p_delete(&qh->old);
}

void qhash_init(qhash_t *qh, uint16_t k_size, uint16_t v_size, bool doh)
{
    p_clear(qh, 1);
    qh->k_size = k_size;
    qh->v_size = v_size;
    qh->h_size = !!doh;
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
        p_delete(&qh->old->bits);
        p_delete(&qh->old);
    }
    p_delete(&qh->hdr.bits);
    p_delete(&qh->values);
    p_delete(&qh->hashes);
    p_delete(&qh->keys);
    qhash_init(qh, 0, 0, false);
}

void qhash_clear(qhash_t *qh)
{
    if (qh->old) {
        p_delete(&qh->old->bits);
        p_delete(&qh->old);
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
