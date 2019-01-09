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

uint32_t hsieh_hash(const void *_data, int len)
{
    const byte *data = _data;
    uint32_t hash, tmp;
    int rem;

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

uint32_t jenkins_hash(const void *_s, int len)
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
