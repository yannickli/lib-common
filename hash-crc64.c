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

/*
 *
 *  \file       crc64.c
 *  \brief      CRC64 calculation
 *
 *  Calculate the CRC64 using the slice-by-four algorithm. This is the same
 *  idea that is used in crc32_fast.c, but for CRC64 we use only four tables
 *  instead of eight to avoid increasing CPU cache usage.
 *
 *  Author:     Lasse Collin
 *
 *  This file has been put into the public domain.
 *  You can do whatever you want with this file.
 *
 */

#include "hash-crc.h"
#include "hash-crc64-table.in.c"

static ALWAYS_INLINE
uint64_t naive_icrc64(uint64_t crc, const uint8_t *buf, ssize_t len)
{
    if (len) {
        do {
            crc = crc64table[0][*buf++ ^ A1(crc)] ^ S8(crc);
        } while (--len);
    }
    return crc;
}

static uint64_t fast_icrc64(uint64_t crc, const uint8_t *buf, size_t size)
{
    size_t words;

    if (unlikely((uintptr_t)buf & 3)) {
        size -= 4 - ((uintptr_t)buf & 3);
        do {
            crc = crc64table[0][*buf++ ^ A1(crc)] ^ S8(crc);
        } while ((uintptr_t)buf & 3);
    }

    words = size >> 2;
    do {
#if __BYTE_ORDER == __BIG_ENDIAN
        const uint32_t tmp = (crc >> 32) ^ *(uint32_t *)(buf);
#else
        const uint32_t tmp = crc ^ *(uint32_t *)(buf);
#endif
        buf += 4;

        crc = crc64table[3][A(tmp)]
            ^ crc64table[2][B(tmp)]
            ^ S32(crc)
            ^ crc64table[1][C(tmp)]
            ^ crc64table[0][D(tmp)];
    } while (--words);

    return naive_icrc64(crc, buf, size & (size_t)3);
}

__flatten
uint64_t icrc64(uint64_t crc, const void *data, ssize_t len)
{
    crc = ~le_to_cpu64(crc);
    if (len < 64)
        return ~le_to_cpu64(naive_icrc64(crc, data, len));
    return ~le_to_cpu64(fast_icrc64(crc, data, len));
}
