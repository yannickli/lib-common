/**************************************************************************/
/*                                                                        */
/*  Copyright (C) 2004-2019 INTERSEC SA                                   */
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
 * \file       crc32.c
 * \brief      CRC32 calculation
 *
 * Calculate the CRC32 using the slice-by-eight algorithm.
 * It is explained in this document:
 * http://www.intel.com/technology/comms/perfnet/download/CRC_generators.pdf
 * The code in this file is not the same as in Intel's paper, but
 * the basic principle is identical.
 *
 *  Author:     Lasse Collin
 *
 *  This file has been put into the public domain.
 *  You can do whatever you want with this file.
 *
 */

#include "hash-crc.h"
#include "hash-crc32-table.in.c"

/* Simplistic crc32 calculator, almost compatible with zlib version,
 * except for crc type as uint32_t instead of unsigned long
 */
static ALWAYS_INLINE
uint32_t naive_icrc32(uint32_t crc, const uint8_t *buf, ssize_t len)
{
    if (len) {
        do {
            crc = crc32table[0][*buf++ ^ A(crc)] ^ S8(crc);
        } while (--len);
    }
    return crc;
}

/* If you make any changes, do some bench marking! Seemingly unrelated
 * changes can very easily ruin the performance (and very probably is
 * very compiler dependent).
 */
static uint32_t fast_icrc32(uint32_t crc, const uint8_t *buf, size_t size)
{
    size_t words;

    if (unlikely((uintptr_t)buf & 7)) {
        size -= 8 - ((uintptr_t)buf & 7);
        do {
            crc = crc32table[0][*buf++ ^ A(crc)] ^ S8(crc);
        } while ((uintptr_t)buf & 7);
    }

    words = size >> 3;
    do {
        uint32_t tmp;

        crc ^= *(uint32_t *)(buf);
        buf += 4;

        crc = crc32table[7][A(crc)]
            ^ crc32table[6][B(crc)]
            ^ crc32table[5][C(crc)]
            ^ crc32table[4][D(crc)];

        tmp  = *(uint32_t *)(buf);
        buf += 4;

        // At least with some compilers, it is critical for
        // performance, that the crc variable is XORed
        // between the two table-lookup pairs.
        crc = crc32table[3][A(tmp)]
            ^ crc32table[2][B(tmp)]
            ^ crc
            ^ crc32table[1][C(tmp)]
            ^ crc32table[0][D(tmp)];
    } while (--words);

    return naive_icrc32(crc, buf, size & (size_t)7);
}

__flatten
uint32_t icrc32(uint32_t crc, const void *data, ssize_t len)
{
    crc = ~le_to_cpu32(crc);
    if (len < 64)
        return ~le_to_cpu32(naive_icrc32(crc, data, len));
    return ~le_to_cpu32(fast_icrc32(crc, data, len));
}
