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

#include "arith.h"

#define BIT(x, n)   (((x) >> (n)) & 1)
#define BC4(x)      BIT(x, 0) + BIT(x, 1) + BIT(x, 2) + BIT(x, 3)
#define BC12(x)     BC4(x) + BC4((x) >> 4) + BC4((x) >> 8)

/* __firstbit8[n] is the index of the least significant non 0 bit in
 * `n' or 8 if n has all bits 0.
 */
uint8_t const __firstbit_fwd8[256] = {
#define A(m)   (((m) & 0x01) ? 0 : ((m) & 0x02) ? 1 : \
                ((m) & 0x04) ? 2 : ((m) & 0x08) ? 3 : \
                ((m) & 0x10) ? 4 : ((m) & 0x20) ? 5 : \
                ((m) & 0x40) ? 6 : ((m) & 0x80) ? 7 : 8)
#define B(n)  A(n),A(n+1),A(n+2),A(n+3),A(n+4),A(n+5),A(n+6),A(n+7)
    B(0x00), B(0x08), B(0x10), B(0x18), B(0x20), B(0x28), B(0x30), B(0x38),
    B(0x40), B(0x48), B(0x50), B(0x58), B(0x60), B(0x68), B(0x70), B(0x78),
    B(0x80), B(0x88), B(0x90), B(0x98), B(0xA0), B(0xA8), B(0xB0), B(0xB8),
    B(0xC0), B(0xC8), B(0xD0), B(0xD8), B(0xE0), B(0xE8), B(0xF0), B(0xF8),
#undef B
#undef A
};

/* __firstbit8[n] is the index of the most significant non 0 bit in
 * `n' or 8 if n has all bits 0.
 */
uint8_t const __firstbit_rev8[256] = {
#define A(m)   (((m) & 0x80) ? 7 : ((m) & 0x40) ? 6 : \
                ((m) & 0x20) ? 5 : ((m) & 0x10) ? 4 : \
                ((m) & 0x08) ? 3 : ((m) & 0x04) ? 2 : \
                ((m) & 0x02) ? 1 : ((m) & 0x01) ? 0 : 0)
#define B(n)  A(n),A(n+1),A(n+2),A(n+3),A(n+4),A(n+5),A(n+6),A(n+7)
    B(0x00), B(0x08), B(0x10), B(0x18), B(0x20), B(0x28), B(0x30), B(0x38),
    B(0x40), B(0x48), B(0x50), B(0x58), B(0x60), B(0x68), B(0x70), B(0x78),
    B(0x80), B(0x88), B(0x90), B(0x98), B(0xA0), B(0xA8), B(0xB0), B(0xB8),
    B(0xC0), B(0xC8), B(0xD0), B(0xD8), B(0xE0), B(0xE8), B(0xF0), B(0xF8),
#undef B
#undef A
};

uint8_t const __bitcount11[1 << 11] = {
#define X4(n)       BC12(n), BC12(n + 1), BC12(n + 2),  BC12(n + 3)
#define X16(n)      X4(n),   X4(n + 4),   X4(n + 8),    X4(n + 12)
#define X64(n)      X16(n),  X16(n + 16), X16(n + 32),  X16(n + 48)
#define X256(n)     X64(n),  X64(n + 64), X64(n + 128), X64(n + 192)
    X256(0x000), X256(0x100), X256(0x200), X256(0x300),
    X256(0x400), X256(0x500), X256(0x600), X256(0x700),
#undef X256
#undef X64
#undef X16
#undef X4
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
        count += bitcount32(((uint32_t *)p)[4]);
        count += bitcount32(((uint32_t *)p)[5]);
        count += bitcount32(((uint32_t *)p)[6]);
        count += bitcount32(((uint32_t *)p)[7]);
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

/* Tests {{{ */
TEST_DECL("arith-bithacks: check bsr8", 0)
{
    for (uint8_t i = 1; i < 255; i++) {
        TEST_FAIL_IF(bsr8(i) != bsr16(i),
                     "[%u] Expected %zu, got %zu", i, bsr16(i), bsr8(i));
    }

    TEST_DONE();
}

TEST_DECL("arith-bithacks: check bsf8", 0)
{
    for (uint8_t i = 1; i < 255; i++) {
        TEST_FAIL_IF(bsf8(i) != bsf16(i),
                     "[%u] Expected %zu, got %zu", i, bsf16(i), bsf8(i));
    }

    TEST_DONE();
}
