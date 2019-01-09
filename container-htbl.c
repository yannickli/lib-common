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
#include "hash.h"

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

uint32_t htbl_get_size(uint32_t len)
{
    uint32_t size = 2 * (len + 1);
    int bsr = bsr32(size);

    if (unlikely(size >= INT32_MAX))
        e_panic("out of memory");
    while (prime_list[bsr] < size)
        bsr++;
    return prime_list[bsr];
}

uint32_t htbl_scan_pos(generic_htbl *t, uint32_t pos)
{
    /* XXX: see htbl_init to understand why this stops */
    for (;;) {
        const size_t bits = bitsizeof(t->setbits[0]);
        size_t word = t->setbits[pos / bits];

        word &= BITMASK_GE(size_t, pos);
        pos  &= ~(bits - 1);
        if (word)
            return pos + bsfsz(word);
        pos += bits;
    }
}

uint32_t htbl_free_id(generic_htbl *t)
{
    static uint32_t seed = 0xfaceb00c;

    if (!t->size)
        return seed;

    do {
        seed = 1664525 * seed + 1013904223;
    } while (TST_BIT(t->setbits, seed % t->size));
    return seed % t->size;
}

void htbl_init(generic_htbl *t, int size)
{
    t->size      = size;
    t->len       = 0;
    t->ghosts    = 0;
    t->setbits   = p_new(size_t, BITS_TO_ARRAY_LEN(size_t, size));
    t->ghostbits = p_new(size_t, BITS_TO_ARRAY_LEN(size_t, size));
    /* XXX: at least 2 bits beyond t->size are set to stop htbl_scan_pos */
    SET_BIT(t->setbits, size);
}

void htbl_wipe(generic_htbl *t)
{
    p_delete(&t->tab);
    p_delete(&t->setbits);
    p_delete(&t->ghostbits);
}

void htbl_invalidate(generic_htbl *t, uint32_t pos)
{
    if (TST_BIT(t->setbits, pos)) {
        RST_BIT(t->setbits, pos);
        t->len--;
        SET_BIT(t->ghostbits, pos);
        t->ghosts++;
    }
}

uint64_t htbl_hash_string(const void *s, int len)
{
    if (len < 0)
        len = strlen(s);
    return MAKE64(hsieh_hash((const byte *)s, len), len);
}

bool htbl_keyequal(uint64_t h, const void *k1, const void *k2)
{
    return !memcmp(k1, k2, (h & INT_MAX));
}
