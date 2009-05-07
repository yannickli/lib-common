/**************************************************************************/
/*                                                                        */
/*  Copyright (C) 2004-2009 INTERSEC SAS                                  */
/*                                                                        */
/*  Should you receive a copy of this source code, you must check you     */
/*  have a proper, written authorization of INTERSEC to hold it. If you   */
/*  don't have such an authorization, you must DELETE all source code     */
/*  files in your possession, and inform INTERSEC of the fact you obtain  */
/*  these files. Should you not comply to these terms, you can be         */
/*  prosecuted in the extent permitted by applicable law.                 */
/*                                                                        */
/**************************************************************************/

#include "arith.h"

#define BIT(x, n)   (((x) >> (n)) & 1)
#define BC4(x)      BIT(x, 0) + BIT(x, 1) + BIT(x, 2) + BIT(x, 3)
#define BC12(x)     BC4(x) + BC4((x) >> 4) + BC4((x) >> 8)

uint8_t const __bitcount11[1 << 11] = {
#define X4(n)       BC12(n), BC12(n + 1), BC12(n + 2),  BC12(n + 3)
#define X16(n)      X4(n),   X4(n + 4),   X4(n + 8),    X4(n + 12)
#define X64(n)      X16(n),  X16(n + 16), X16(n + 32),  X16(n + 48)
#define X256(n)     X64(n),  X64(n + 64), X64(n + 128), X64(n + 192)
    X256(0x000), X256(0x100), X256(0x200), X256(0x300),
    X256(0x400), X256(0x500), X256(0x600), X256(0x700),
};

static size_t membitcount_naive(const uint8_t *p, const uint8_t *end)
{
    size_t count = 0;

    while (p < end) {
        count += bitcount8(*p++);
    }
    return count;
}

static size_t membitcount_aligned(const uint8_t *p, const uint8_t *end)
{
    size_t count = 0;

    while ((uintptr_t)p & 31)
        count += bitcount8(*p++);

    while (p + 32 <= end) {
        count += bitcount32(((uint32_t *)p)[0]);
        count += bitcount32(((uint32_t *)p)[1]);
        count += bitcount32(((uint32_t *)p)[2]);
        count += bitcount32(((uint32_t *)p)[3]);
        p += 32;
    }

    return count + membitcount_naive(p, end);
}

size_t membitcount(const void *ptr, size_t n)
{
    const uint8_t *end = (const uint8_t *)ptr + n;
    if (n < 1024)
        return membitcount_naive(ptr, end);
    return membitcount_aligned(ptr, end);
}
