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

#include "hash.h"

uint32_t hsieh_hash(const void *_data, ssize_t len)
{
    const byte *data = _data;
    uint32_t hash, tmp;
    size_t rem;

    if (len < 0)
        len = strlen((const char *)data);

    hash  = len;
    rem   = len & 3;
    len >>= 2;

    /* Main loop */
    for (; len > 0; len--, data += 4) {
        hash  += cpu_to_le16pu(data);
        tmp    = (cpu_to_le16pu(data + 2) << 11) ^ hash;
        hash   = (hash << 16) ^ tmp;
        hash  += hash >> 11;
    }

    /* Handle end cases */
    switch (rem) {
      case 3:
        hash += cpu_to_le16pu(data);
        hash ^= hash << 16;
        hash ^= data[2] << 18;
        hash += hash >> 11;
        break;
      case 2:
        hash += cpu_to_le16pu(data);
        hash ^= hash << 11;
        hash += hash >> 17;
        break;
      case 1:
        hash += *data;
        hash ^= hash << 10;
        hash += hash >> 1;
        break;
    }

    /* Force "avalanching" of final 127 bits */
    hash ^= hash << 3;
    hash += hash >> 5;
    hash ^= hash << 4;
    hash += hash >> 17;
    hash ^= hash << 25;
    hash += hash >> 6;
    return hash;
}

uint32_t jenkins_hash(const void *_s, ssize_t len)
{
    const byte *s = _s;
    uint32_t hash = 0;

    if (len < 0) {
        while (*s) {
            hash += *s++;
            hash += hash << 10;
            hash ^= hash >> 6;
        }
    } else {
        while (len-- > 0) {
            hash += *s++;
            hash += hash << 10;
            hash ^= hash >> 6;
        }
    }
    hash += hash << 3;
    hash ^= hash >> 11;
    hash += hash << 15;
    return hash;
}

/*
 * murmur_hash3 was written by Austin Appleby, and is placed in the public
 * domain. The author hereby disclaims copyright to this source code.
 *
 * Note - The x86 and x64 versions do _not_ produce the same results, as the
 * algorithms are optimized for their respective platforms. You can still
 * compile and run any of them on any platform, but your performance with the
 * non-native version will be less than optimal.
 *
 * From http://code.google.com/p/smhasher/ rev-136
 */

static ALWAYS_INLINE uint32_t rotl32(uint32_t x, int8_t r)
{
    return (x << r) | (x >> (32 - r));
}

static ALWAYS_INLINE uint64_t rotl64(uint64_t x, int8_t r)
{
    return (x << r) | (x >> (64 - r));
}

static ALWAYS_INLINE uint32_t fmix32(uint32_t h)
{
    h ^= h >> 16;
    h *= 0x85ebca6b;
    h ^= h >> 13;
    h *= 0xc2b2ae35;
    h ^= h >> 16;

    return h;
}

static ALWAYS_INLINE uint64_t fmix64(uint64_t k)
{
    k ^= k >> 33;
    k *= 0xff51afd7ed558ccdLLU;
    k ^= k >> 33;
    k *= 0xc4ceb9fe1a85ec53LLU;
    k ^= k >> 33;

    return k;
}

uint32_t murmur_hash3_x86_32(const void *key, size_t len, uint32_t seed)
{
    const uint8_t *data = (const uint8_t*)key;
    const size_t nblocks = len / 4;

    uint32_t h1 = seed;

    uint32_t c1 = 0xcc9e2d51;
    uint32_t c2 = 0x1b873593;

    //----------
    // body

    const uint32_t *blocks = (const uint32_t *)(data + nblocks*4);

    for (ssize_t i = -nblocks; i; i++) {
        uint32_t k1 = get_unaligned_cpu32(blocks + i);

        k1 *= c1;
        k1 = rotl32(k1, 15);
        k1 *= c2;

        h1 ^= k1;
        h1 = rotl32(h1, 13);
        h1 = h1 * 5 + 0xe6546b64;
    }

    //----------
    // tail
    {
        const uint8_t *tail = (const uint8_t*)(data + nblocks*4);

        uint32_t k1 = 0;

        switch (len & 3) {
          case 3: k1 ^= tail[2] << 16;
          case 2: k1 ^= tail[1] << 8;
          case 1: k1 ^= tail[0];
                  k1 *= c1; k1 = rotl32(k1, 15); k1 *= c2; h1 ^= k1;
        };
    }

    //----------
    // finalization
    h1 ^= len;
    h1  = fmix32(h1);
    return h1;
}

void murmur_hash3_x86_128(const void *key, size_t len,
                          uint32_t seed, char out[static 16])
{
    const uint8_t *data = key;
    const size_t nblocks = len / 16;

    uint32_t h1 = seed;
    uint32_t h2 = seed;
    uint32_t h3 = seed;
    uint32_t h4 = seed;

    uint32_t c1 = 0x239b961b;
    uint32_t c2 = 0xab0e9789;
    uint32_t c3 = 0x38b34ae5;
    uint32_t c4 = 0xa1e38b93;

    //----------
    // body

    const uint32_t *blocks = (const uint32_t *)(data + nblocks*16);

    for (ssize_t i = -nblocks; i; i++) {
        uint32_t k1 = get_unaligned_cpu32(blocks + i * 4 + 0);
        uint32_t k2 = get_unaligned_cpu32(blocks + i * 4 + 1);
        uint32_t k3 = get_unaligned_cpu32(blocks + i * 4 + 2);
        uint32_t k4 = get_unaligned_cpu32(blocks + i * 4 + 3);

        k1 *= c1; k1  = rotl32(k1, 15); k1 *= c2; h1 ^= k1;

        h1 = rotl32(h1, 19); h1 += h2; h1 = h1 * 5 + 0x561ccd1b;

        k2 *= c2; k2  = rotl32(k2, 16); k2 *= c3; h2 ^= k2;

        h2 = rotl32(h2, 17); h2 += h3; h2 = h2 * 5 + 0x0bcaa747;

        k3 *= c3; k3  = rotl32(k3, 17); k3 *= c4; h3 ^= k3;

        h3 = rotl32(h3, 15); h3 += h4; h3 = h3 * 5 + 0x96cd1c35;

        k4 *= c4; k4  = rotl32(k4, 18); k4 *= c1; h4 ^= k4;

        h4 = rotl32(h4, 13); h4 += h1; h4 = h4 * 5 + 0x32ac3b17;
    }

    //----------
    // tail
    {
        const uint8_t *tail = (const uint8_t*)(data + nblocks*16);

        uint32_t k1 = 0;
        uint32_t k2 = 0;
        uint32_t k3 = 0;
        uint32_t k4 = 0;

        switch (len & 15) {
          case 15: k4 ^= tail[14] << 16;
          case 14: k4 ^= tail[13] << 8;
          case 13: k4 ^= tail[12] << 0;
                   k4 *= c4; k4  = rotl32(k4, 18); k4 *= c1; h4 ^= k4;

          case 12: k3 ^= tail[11] << 24;
          case 11: k3 ^= tail[10] << 16;
          case 10: k3 ^= tail[ 9] << 8;
          case  9: k3 ^= tail[ 8] << 0;
                   k3 *= c3; k3  = rotl32(k3, 17); k3 *= c4; h3 ^= k3;

          case  8: k2 ^= tail[ 7] << 24;
          case  7: k2 ^= tail[ 6] << 16;
          case  6: k2 ^= tail[ 5] << 8;
          case  5: k2 ^= tail[ 4] << 0;
                   k2 *= c2; k2  = rotl32(k2, 16); k2 *= c3; h2 ^= k2;

          case  4: k1 ^= tail[ 3] << 24;
          case  3: k1 ^= tail[ 2] << 16;
          case  2: k1 ^= tail[ 1] << 8;
          case  1: k1 ^= tail[ 0] << 0;
                   k1 *= c1; k1  = rotl32(k1, 15); k1 *= c2; h1 ^= k1;
        };
    }

    //----------
    // finalization

    h1 ^= len; h2 ^= len; h3 ^= len; h4 ^= len;

    h1 += h2; h1 += h3; h1 += h4;
    h2 += h1; h3 += h1; h4 += h1;

    h1 = fmix32(h1);
    h2 = fmix32(h2);
    h3 = fmix32(h3);
    h4 = fmix32(h4);

    h1 += h2; h1 += h3; h1 += h4;
    h2 += h1; h3 += h1; h4 += h1;

    ((uint32_t *)out)[0] = h1;
    ((uint32_t *)out)[1] = h2;
    ((uint32_t *)out)[2] = h3;
    ((uint32_t *)out)[3] = h4;
}

static void murmur_hash3_x64_128_(const void *key, const size_t len,
                                  const uint32_t seed, char out[static 16])
{
    const uint8_t *data = (const uint8_t*)key;
    const size_t nblocks = len / 16;

    uint64_t h1 = seed;
    uint64_t h2 = seed;

    uint64_t c1 = 0x87c37b91114253d5LLU;
    uint64_t c2 = 0x4cf5ad432745937fLLU;

    //----------
    // body

    const uint64_t *blocks = (const uint64_t *)(data);

    for (size_t i = 0; i < nblocks; i++) {
        uint64_t k1 = get_unaligned_cpu64(blocks + i * 2 + 0);
        uint64_t k2 = get_unaligned_cpu64(blocks + i * 2 + 1);

        k1 *= c1; k1  = rotl64(k1, 31); k1 *= c2; h1 ^= k1;

        h1 = rotl64(h1, 27); h1 += h2; h1 = h1 * 5 + 0x52dce729;

        k2 *= c2; k2  = rotl64(k2, 33); k2 *= c1; h2 ^= k2;

        h2 = rotl64(h2, 31); h2 += h1; h2 = h2 * 5 + 0x38495ab5;
    }

    //----------
    // tail
    {
        const uint8_t *tail = (const uint8_t*)(data + nblocks*16);

        uint64_t k1 = 0;
        uint64_t k2 = 0;

        switch (len & 15) {
          case 15: k2 ^= (uint64_t)tail[14] << 48;
          case 14: k2 ^= (uint64_t)tail[13] << 40;
          case 13: k2 ^= (uint64_t)tail[12] << 32;
          case 12: k2 ^= (uint64_t)tail[11] << 24;
          case 11: k2 ^= (uint64_t)tail[10] << 16;
          case 10: k2 ^= (uint64_t)tail[ 9] << 8;
          case  9: k2 ^= (uint64_t)tail[ 8] << 0;
                   k2 *= c2; k2  = rotl64(k2, 33); k2 *= c1; h2 ^= k2;

          case  8: k1 ^= (uint64_t)tail[ 7] << 56;
          case  7: k1 ^= (uint64_t)tail[ 6] << 48;
          case  6: k1 ^= (uint64_t)tail[ 5] << 40;
          case  5: k1 ^= (uint64_t)tail[ 4] << 32;
          case  4: k1 ^= (uint64_t)tail[ 3] << 24;
          case  3: k1 ^= (uint64_t)tail[ 2] << 16;
          case  2: k1 ^= (uint64_t)tail[ 1] << 8;
          case  1: k1 ^= (uint64_t)tail[ 0] << 0;
                   k1 *= c1; k1  = rotl64(k1, 31); k1 *= c2; h1 ^= k1;
        };
    }

    //----------
    // finalization

    h1 ^= len; h2 ^= len;

    h1 += h2;
    h2 += h1;

    h1 = fmix64(h1);
    h2 = fmix64(h2);

    h1 += h2;
    h2 += h1;

    ((uint64_t*)out)[0] = h1;
    ((uint64_t*)out)[1] = h2;
}

void murmur_hash3_x64_128(const void *key, size_t len, uint32_t seed, char out[static 16])
{
    murmur_hash3_x64_128_(key, len, seed, out);
}
