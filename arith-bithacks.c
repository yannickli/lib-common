/**************************************************************************/
/*                                                                        */
/*  Copyright (C) 2004-2013 INTERSEC SA                                   */
/*                                                                        */
/*  Should you receive a copy of this source code, you must check you     */
/*  have a proper, written authorization of INTERSEC to hold it. If you   */
/*  don't have such an authorization, you must DELETE all source code     */
/*  files in your possession, and inform INTERSEC of the fact you obtain  */
/*  these files. Should you not comply to these terms, you can be         */
/*  prosecuted in the extent permitted by applicable law.                 */
/*                                                                        */
/**************************************************************************/

#include "z.h"
#include "arith.h"

#define BIT(x, n)   (((x) >> (n)) & 1)
#define BC4(x)      BIT(x, 0) + BIT(x, 1) + BIT(x, 2) + BIT(x, 3)
#define BC12(x)     BC4(x) + BC4((x) >> 4) + BC4((x) >> 8)

/* bsf/bsr {{{ */

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

ssize_t bsr(const void *data, size_t start_bit, size_t len, bool reverse)
{
    const uint64_t *pos;

    if (len == 0) {
        return -1;
    }

    /* Align the pointer if needed */
    pos = (const uint64_t *)(((uintptr_t)data) & ~7ul);;
    if (pos != data) {
        start_bit += 8 * ((const byte *)data - (const byte *)pos);
    }
    if (start_bit >= 64) {
        pos       += start_bit / 64;
        start_bit %= 64;
    }
    pos += DIV_ROUND_UP(len + start_bit, 64) - 1;

#define SCAN_WORD(Low, Up)  do {                                             \
        uint64_t w = reverse ? ~*pos : *pos;                                 \
                                                                             \
        if (Low)      w &= BITMASK_GE(uint64_t, Low);                        \
        if (Up != 64) w &= BITMASK_LT(uint64_t, Up);                         \
                                                                             \
        if (w) {                                                             \
            return len + bsr64(w) - (Up);                                    \
        }                                                                    \
        len -= (Up) - (Low);                                                 \
        pos--;                                                               \
    } while (0)

    if (start_bit + len <= 64) {
        SCAN_WORD(start_bit, start_bit + len);
        return -1;
    }
    if ((start_bit + len) % 64 != 0) {
        SCAN_WORD(0, (start_bit + len) % 64);
    }
    while (len >= 64) {
        SCAN_WORD(0, 64);
    }
    if (len) {
        SCAN_WORD(start_bit, 64);
    }
    return -1;

#undef SCAN_WORD
}

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

ssize_t bsf(const void *data, size_t start_bit, size_t len, bool reverse)
{
    const uint64_t *pos;
    size_t off = 0;

    /* Align the pointer if needed */
    pos = (const uint64_t *)(((uintptr_t)data) & ~7ul);;
    if (pos != data) {
        start_bit += 8 * ((const byte *)data - (const byte *)pos);
    }
    if (start_bit >= 64) {
        pos       += start_bit / 64;
        start_bit %= 64;
    }

#define SCAN_WORD(Low, Up)  do {                                             \
        uint64_t w = reverse ? ~*pos : *pos;                                 \
                                                                             \
        if (Low)      w &= BITMASK_GE(uint64_t, Low);                        \
        if (Up != 64) w &= BITMASK_LT(uint64_t, Up);                         \
                                                                             \
        if (w) {                                                             \
            return off + bsf64(w) - Low;                                     \
        }                                                                    \
        len -= (Up) - (Low);                                                 \
        off += (Up) - (Low);                                                 \
        pos++;                                                               \
    } while (0)

    if (start_bit + len <= 64) {
        SCAN_WORD(start_bit, start_bit + len);
        return -1;
    }
    if (start_bit > 0) {
        SCAN_WORD(start_bit, 64);
    }
    while (len >= 64) {
        SCAN_WORD(0, 64);
    }
    if (len) {
        SCAN_WORD(0, len);
    }
    return -1;

#undef SCAN_WORD
}

Z_GROUP_EXPORT(bsr_bsf)
{
    uint8_t data[128];

    Z_TEST(bsf_1, "forward bit scan") {
        p_clear(&data, 1);
        Z_ASSERT_NEG(bsf(data, 0, 0, false));
        Z_ASSERT_NEG(bsf(data, 0, 1024, false));

        SET_BIT(data, 3);
        SET_BIT(data, 165);
        Z_ASSERT_EQ(bsf(data, 0, 1024, false), 3);
        Z_ASSERT_EQ(bsf(data, 1, 1023, false), 2);
        Z_ASSERT_EQ(bsf(data, 3, 1021, false), 0);
        Z_ASSERT_EQ(bsf(data, 5, 1019, false), 160);
        Z_ASSERT_EQ(bsf(data, 5, 161, false), 160);
        Z_ASSERT_EQ(bsf(data, 0, 4, false), 3);
        Z_ASSERT_NEG(bsf(data, 5, 150, false));
        Z_ASSERT_NEG(bsf(data, 5, 33, false));
        Z_ASSERT_NEG(bsf(data, 5, 160, false));
        Z_ASSERT_NEG(bsf(data, 0, 3, false));

        Z_ASSERT_EQ(bsf(&data[1], 3, 1013, false), 154);
    } Z_TEST_END;

    Z_TEST(bsf_0, "forward bit scan, scan of 0") {
        p_clear(&data, 1);
        Z_ASSERT_NEG(bsf(data, 0, 0, true));
        Z_ASSERT_ZERO(bsf(data, 0, 1024, true));

        memset(data, 0xff, 128);
        RST_BIT(data, 3);
        RST_BIT(data, 165);
        Z_ASSERT_EQ(bsf(data, 0, 1024, true), 3);
        Z_ASSERT_EQ(bsf(data, 1, 1023, true), 2);
        Z_ASSERT_EQ(bsf(data, 3, 1021, true), 0);
        Z_ASSERT_EQ(bsf(data, 5, 1019, true), 160);
        Z_ASSERT_EQ(bsf(data, 5, 161, true), 160);
        Z_ASSERT_EQ(bsf(data, 0, 4, true), 3);
        Z_ASSERT_NEG(bsf(data, 5, 150, true));
        Z_ASSERT_NEG(bsf(data, 5, 33, true));
        Z_ASSERT_NEG(bsf(data, 5, 160, true));
        Z_ASSERT_NEG(bsf(data, 0, 3, true));

        Z_ASSERT_EQ(bsf(&data[1], 3, 1013, true), 154);
    } Z_TEST_END;


    Z_TEST(bsr_1, "reverse bit scan") {
        p_clear(&data, 1);
        Z_ASSERT_NEG(bsr(data, 0, 0, false));
        Z_ASSERT_NEG(bsr(data, 0, 1024, false));

        SET_BIT(data, 3);
        SET_BIT(data, 165);
        Z_ASSERT_EQ(bsr(data, 0, 1024, false), 165);
        Z_ASSERT_EQ(bsr(data, 1, 1023, false), 164);
        Z_ASSERT_EQ(bsr(data, 3, 1021, false), 162);
        Z_ASSERT_EQ(bsr(data, 1, 100, false), 2);
        Z_ASSERT_EQ(bsr(data, 3, 100, false), 0);
        Z_ASSERT_EQ(bsr(data, 5, 161, false), 160);
        Z_ASSERT_EQ(bsr(data, 0, 4, false), 3);
        Z_ASSERT_NEG(bsr(data, 5, 150, false));
        Z_ASSERT_NEG(bsr(data, 5, 33, false));
        Z_ASSERT_NEG(bsr(data, 5, 160, false));
        Z_ASSERT_NEG(bsr(data, 0, 3, false));

        Z_ASSERT_EQ(bsr(&data[1], 3, 1013, false), 154);

        /* Check that we read inside boundaries */
        memset(data, 0xff, 8);
        memset(data + 8, 0, 16);
        memset(data + 24, 0xff, 8);
        for (int i = 64; i < 114; i++) {
            SET_BIT(data, i);
        }
        /* --- blank on 40 bits --- */
        for (int i = 154; i <= 191; i++) {
            SET_BIT(data, i);
        }
        Z_ASSERT_NEG(bsr(data + 8, 50, 40, false));
    } Z_TEST_END;

    Z_TEST(bsr_0, "reverse bit scan, scan of 0") {
        p_clear(&data, 1);
        Z_ASSERT_NEG(bsr(data, 0, 0, true));
        Z_ASSERT_EQ(bsr(data, 0, 1024, true), 1023);

        memset(data, 0xff, 128);
        RST_BIT(data, 3);
        RST_BIT(data, 165);
        Z_ASSERT_EQ(bsr(data, 0, 1024, true), 165);
        Z_ASSERT_EQ(bsr(data, 1, 1023, true), 164);
        Z_ASSERT_EQ(bsr(data, 3, 1021, true), 162);
        Z_ASSERT_EQ(bsr(data, 1, 100, true), 2);
        Z_ASSERT_EQ(bsr(data, 3, 100, true), 0);
        Z_ASSERT_EQ(bsr(data, 5, 161, true), 160);
        Z_ASSERT_EQ(bsr(data, 0, 4, true), 3);
        Z_ASSERT_NEG(bsr(data, 5, 150, true));
        Z_ASSERT_NEG(bsr(data, 5, 33, true));
        Z_ASSERT_NEG(bsr(data, 5, 160, true));
        Z_ASSERT_NEG(bsr(data, 0, 3, true));

        Z_ASSERT_EQ(bsr(&data[1], 3, 1013, true), 154);
    } Z_TEST_END;
} Z_GROUP_END;

/* }}} */
/* Bitreverse {{{ */

uint8_t const __bit_reverse8[1 << 8] = {
    0x00, 0x80, 0x40, 0xc0, 0x20, 0xa0, 0x60, 0xe0,
    0x10, 0x90, 0x50, 0xd0, 0x30, 0xb0, 0x70, 0xf0,
    0x08, 0x88, 0x48, 0xc8, 0x28, 0xa8, 0x68, 0xe8,
    0x18, 0x98, 0x58, 0xd8, 0x38, 0xb8, 0x78, 0xf8,
    0x04, 0x84, 0x44, 0xc4, 0x24, 0xa4, 0x64, 0xe4,
    0x14, 0x94, 0x54, 0xd4, 0x34, 0xb4, 0x74, 0xf4,
    0x0c, 0x8c, 0x4c, 0xcc, 0x2c, 0xac, 0x6c, 0xec,
    0x1c, 0x9c, 0x5c, 0xdc, 0x3c, 0xbc, 0x7c, 0xfc,
    0x02, 0x82, 0x42, 0xc2, 0x22, 0xa2, 0x62, 0xe2,
    0x12, 0x92, 0x52, 0xd2, 0x32, 0xb2, 0x72, 0xf2,
    0x0a, 0x8a, 0x4a, 0xca, 0x2a, 0xaa, 0x6a, 0xea,
    0x1a, 0x9a, 0x5a, 0xda, 0x3a, 0xba, 0x7a, 0xfa,
    0x06, 0x86, 0x46, 0xc6, 0x26, 0xa6, 0x66, 0xe6,
    0x16, 0x96, 0x56, 0xd6, 0x36, 0xb6, 0x76, 0xf6,
    0x0e, 0x8e, 0x4e, 0xce, 0x2e, 0xae, 0x6e, 0xee,
    0x1e, 0x9e, 0x5e, 0xde, 0x3e, 0xbe, 0x7e, 0xfe,
    0x01, 0x81, 0x41, 0xc1, 0x21, 0xa1, 0x61, 0xe1,
    0x11, 0x91, 0x51, 0xd1, 0x31, 0xb1, 0x71, 0xf1,
    0x09, 0x89, 0x49, 0xc9, 0x29, 0xa9, 0x69, 0xe9,
    0x19, 0x99, 0x59, 0xd9, 0x39, 0xb9, 0x79, 0xf9,
    0x05, 0x85, 0x45, 0xc5, 0x25, 0xa5, 0x65, 0xe5,
    0x15, 0x95, 0x55, 0xd5, 0x35, 0xb5, 0x75, 0xf5,
    0x0d, 0x8d, 0x4d, 0xcd, 0x2d, 0xad, 0x6d, 0xed,
    0x1d, 0x9d, 0x5d, 0xdd, 0x3d, 0xbd, 0x7d, 0xfd,
    0x03, 0x83, 0x43, 0xc3, 0x23, 0xa3, 0x63, 0xe3,
    0x13, 0x93, 0x53, 0xd3, 0x33, 0xb3, 0x73, 0xf3,
    0x0b, 0x8b, 0x4b, 0xcb, 0x2b, 0xab, 0x6b, 0xeb,
    0x1b, 0x9b, 0x5b, 0xdb, 0x3b, 0xbb, 0x7b, 0xfb,
    0x07, 0x87, 0x47, 0xc7, 0x27, 0xa7, 0x67, 0xe7,
    0x17, 0x97, 0x57, 0xd7, 0x37, 0xb7, 0x77, 0xf7,
    0x0f, 0x8f, 0x4f, 0xcf, 0x2f, 0xaf, 0x6f, 0xef,
    0x1f, 0x9f, 0x5f, 0xdf, 0x3f, 0xbf, 0x7f, 0xff,
};

Z_GROUP_EXPORT(bit_reverse) {
    Z_TEST(bit_reverse, "bit reverse") {
        Z_ASSERT_EQ(bit_reverse16(0x3445), 0xa22c);
        Z_ASSERT_EQ(bit_reverse64(0xabc), 0x3d50000000000000ull);
        Z_ASSERT_EQ(bit_reverse64(0x101010101010101), 0x8080808080808080ull);

        for (unsigned i = 0; i < countof(__bit_reverse8); i++) {
            unsigned word = __bit_reverse8[i];
            for (unsigned j = 0; j < 8; j++) {
                Z_ASSERT_EQ((word >> j) & 1, (i >> (7 - j)) & 1);
            }
            Z_ASSERT_EQ(__bit_reverse8[__bit_reverse8[i]], i);
        }
    } Z_TEST_END;
} Z_GROUP_END

/* }}} */
/* Bitcount {{{ */

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

static size_t membitcount_c(const void *ptr, size_t n)
{
    size_t c1 = 0, c2 = 0, c3 = 0, c4 = 0;
    size_t i = 0;
    const uint32_t *p32 = ptr;

    if (n < 4) {
        const uint8_t *p = ptr;

        if (n & 1)
            c1 = bitcount8(*p);
        if (n & 2)
            c2 = bitcount16(get_unaligned_cpu16(p + (n & 1)));
        return c1 + c2;
    }

    if (unlikely((uintptr_t)ptr & 3)) {
        uint8_t dt = (uintptr_t)ptr & 3;

        n  -= 4 - dt;
        p32 = (uint32_t *)ROUND_UP((uintptr_t)p32, 4);
        /* if valgrinds cringes here, tell hum to STFU */
        c1 += bitcount32(cpu_to_le32(p32[-1])
                         & BITMASK_GE(uint32_t, 8 * dt));
    }
    for (; i + 8 <= n / 4; i += 8) {
        c1 += bitcount32(p32[i + 0]);
        c2 += bitcount32(p32[i + 1]);
        c3 += bitcount32(p32[i + 2]);
        c4 += bitcount32(p32[i + 3]);
        c1 += bitcount32(p32[i + 4]);
        c2 += bitcount32(p32[i + 5]);
        c3 += bitcount32(p32[i + 6]);
        c4 += bitcount32(p32[i + 7]);
    }
    for (; i < n / 4; i++)
        c1 += bitcount32(p32[i]);
    if (n % 4) {
        /* if valgrinds cringes here, tell hum to STFU */
        c1 += bitcount32(cpu_to_le32(p32[i])
                         & BITMASK_LT(uint32_t, 8 * (n % 4)));
    }
    return c1 + c2 + c3 + c4;
}

#if (defined(__x86_64__) || defined(__i386__)) && __GNUC_PREREQ(4, 4)
#include <cpuid.h>
#include <x86intrin.h>

__attribute__((target("popcnt")))
static size_t membitcount_popcnt(const void *ptr, size_t n)
{
    size_t c1 = 0, c2 = 0, c3 = 0, c4 = 0;
    uint8_t dt = (uintptr_t)ptr & 7;
    size_t i = 0;

    if (n == 0)
        return 0;

    if (dt + n <= 8) {
        /* if valgrinds cringes here, tell hum to STFU */
        uint64_t x = cpu_to_le64(*(uint64_t *)((uintptr_t)ptr - dt));

        x >>= dt * 8;
        x <<= 64 - n * 8;
#ifdef __x86_64__
        return __builtin_popcountll(x);
#else
        return __builtin_popcount(x) + __builtin_popcount(x >> 32);
#endif
    } else {
#ifdef __x86_64__
        const uint64_t *p = ptr;

        if (unlikely((uintptr_t)ptr & 7)) {
            n    -= 8 - dt;
            p     = (uint64_t *)ROUND_UP((uintptr_t)p, 8);
            /* if valgrinds cringes here, tell hum to STFU */
            c1   += __builtin_popcountll(p[-1] & BITMASK_GE(uint64_t, 8 * dt));
        }
        for (; i + 8 <= n / 8; i += 8) {
            c1 += __builtin_popcountll(p[i + 0]);
            c2 += __builtin_popcountll(p[i + 1]);
            c3 += __builtin_popcountll(p[i + 2]);
            c4 += __builtin_popcountll(p[i + 3]);
            c1 += __builtin_popcountll(p[i + 4]);
            c2 += __builtin_popcountll(p[i + 5]);
            c3 += __builtin_popcountll(p[i + 6]);
            c4 += __builtin_popcountll(p[i + 7]);
        }
        for (; i < n / 8; i++)
            c1 += __builtin_popcountll(p[i]);
        if (n % 8) {
            /* if valgrinds cringes here, tell hum to STFU */
            c1 += __builtin_popcountll(p[i] & BITMASK_LT(uint64_t, 8 * (n % 8)));
        }
#else
        const uint32_t *p = ptr;

        if ((uintptr_t)ptr & 3) {
            uint8_t dt = (uintptr_t)ptr & 3;

            n    -= 4 - dt;
            p     = (uint32_t *)ROUND_UP((uintptr_t)p, 4);
            /* if valgrinds cringes here, tell hum to STFU */
            c1   += __builtin_popcount(p[-1] & BITMASK_GE(uint32_t, 8 * dt));
        }
        for (; i + 8 <= n / 4; i += 8) {
            c1 += __builtin_popcount(p[i + 0]);
            c2 += __builtin_popcount(p[i + 1]);
            c3 += __builtin_popcount(p[i + 2]);
            c4 += __builtin_popcount(p[i + 3]);
            c1 += __builtin_popcount(p[i + 4]);
            c2 += __builtin_popcount(p[i + 5]);
            c3 += __builtin_popcount(p[i + 6]);
            c4 += __builtin_popcount(p[i + 7]);
        }
        for (; i < n / 4; i++)
            c1 += __builtin_popcount(p[i]);
        if (n % 4) {
            /* if valgrinds cringes here, tell hum to STFU */
            c1 += __builtin_popcount(p[i] & BITMASK_LT(uint32_t, 8 * (n % 4)));
        }
#endif
        return c1 + c2 + c3 + c4;
    }
}

__attribute__((target("ssse3")))
static size_t membitcount_ssse3(const void *ptr, size_t n)
{
    static __v16qi const masks[17] = {
#define X  0xff
        { X, X, X, X, X, X, X, X, X, X, X, X, X, X, X, X },
        { 0, X, X, X, X, X, X, X, X, X, X, X, X, X, X, X },
        { 0, 0, X, X, X, X, X, X, X, X, X, X, X, X, X, X },
        { 0, 0, 0, X, X, X, X, X, X, X, X, X, X, X, X, X },
        { 0, 0, 0, 0, X, X, X, X, X, X, X, X, X, X, X, X },
        { 0, 0, 0, 0, 0, X, X, X, X, X, X, X, X, X, X, X },
        { 0, 0, 0, 0, 0, 0, X, X, X, X, X, X, X, X, X, X },
        { 0, 0, 0, 0, 0, 0, 0, X, X, X, X, X, X, X, X, X },
        { 0, 0, 0, 0, 0, 0, 0, 0, X, X, X, X, X, X, X, X },
        { 0, 0, 0, 0, 0, 0, 0, 0, 0, X, X, X, X, X, X, X },
        { 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, X, X, X, X, X, X },
        { 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, X, X, X, X, X },
        { 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, X, X, X, X },
        { 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, X, X, X },
        { 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, X, X },
        { 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, X },
        { 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 },
#undef X
    };
    static __v16qi const lo_4bits = {
        0xf, 0xf, 0xf, 0xf, 0xf, 0xf, 0xf, 0xf,
        0xf, 0xf, 0xf, 0xf, 0xf, 0xf, 0xf, 0xf,
    };
    static __v16qi const popcnt_4bits = {
        0, 1, 1, 2, 1, 2, 2, 3, 1, 2, 2, 3, 2, 3, 3, 4,
    };
    const uint8_t *p = ptr;
    const __v16qi zero = { 0, };
    __v2di x, res = { 0, };
    size_t i = 0;
    uint8_t dl;

    /* lines 1-4:
     *   xmm_lo/xmm_hi <- the 16 lo/hi 4-bits groups of the 16byte vector
     * lines 5-6:
     *   __builtin_ia32_pshufb128(popcnt_4bits, xmm)
     *   shuffles the "popcnt_4bits" according to the values in xmm.
     *   in other words it "converts" the xmm 4bits values with their
     *   popcounts.
     * line 7:
     *   combine the popcounts together.
     */
#define step(x) \
    ({  __v16qi xmm_lo = (__v16qi)(x);                                       \
        __v16qi xmm_hi = (__v16qi)__builtin_ia32_psrlwi128((__v8hi)(x), 4);  \
                                                                             \
        xmm_lo &= lo_4bits;                                                  \
        xmm_hi &= lo_4bits;                                                  \
        xmm_lo  = __builtin_ia32_pshufb128(popcnt_4bits, xmm_lo);            \
        xmm_hi  = __builtin_ia32_pshufb128(popcnt_4bits, xmm_hi);            \
        xmm_lo + xmm_hi;                                                     \
    })
#define stepp(p)  step(*(__m128i *)(p))

    dl = (uintptr_t)p & 15;
    if (unlikely(dl + n <= 16)) {
        /* if valgrinds cringes here, tell hum to STFU */
        x   = *(__v2di *)(p - dl);
        x   = __builtin_ia32_pand128((__v2di)masks[dl], x);
        x   = __builtin_ia32_pandn128((__v2di)masks[dl + n], x);
        res = __builtin_ia32_psadbw128(step(x), zero);
    } else {
        if (unlikely(dl)) {
            n  -= 16 - dl;
            p  += 16 - dl;
            /* if valgrinds cringes here, tell hum to STFU */
            x   = ((__v2di *)p)[-1];
            x   = __builtin_ia32_pand128((__v2di)masks[dl], x);
            res = __builtin_ia32_psadbw128(step(x), zero);
        }
        for (; i + 64 <= n; i += 64) {
            __v16qi acc = { 0, };

            acc += stepp(p + i +  0);
            acc += stepp(p + i + 16);
            acc += stepp(p + i + 32);
            acc += stepp(p + i + 48);
            res += __builtin_ia32_psadbw128(acc, zero);
        }
        for (; i + 16 <= n; i += 16) {
            res += __builtin_ia32_psadbw128(stepp(p + i), zero);
        }
        if (n % 16) {
            /* if valgrinds cringes here, tell hum to STFU */
            x    = *(__v2di *)(p + i);
            x    = __builtin_ia32_pandn128((__v2di)masks[n % 16], x);
            res += __builtin_ia32_psadbw128(step(x), zero);
        }
    }
    res += ((__v2di)__builtin_ia32_movhlps((__v4sf)res, (__v4sf)res));

    {
        /* GCC 4.4 does not allow taking res[0] directly */
        union { __v2di q; uint64_t vq[2]; } vres = { .q = res };
        return vres.vq[0];
    }
#undef stepp
#undef step
}

#endif

static size_t membitcount_resolve(const void *ptr, size_t n)
{
    membitcount = &membitcount_c;

#if (defined(__x86_64__) || defined(__i386__)) && __GNUC_PREREQ(4, 4)
    {
        int eax, ebx, ecx, edx;

        __cpuid(1, eax, ebx, ecx, edx);
        if (ecx & bit_POPCNT) {
            membitcount = &membitcount_popcnt;
        } else
        if (ecx & bit_SSSE3) {
            membitcount = &membitcount_ssse3;
        }
    }
#endif

    return membitcount(ptr, n);
}

size_t (*membitcount)(const void *ptr, size_t n) = &membitcount_resolve;

static size_t membitcount_naive(const void *_p, size_t n)
{
    const uint8_t *p = _p;
    size_t res = 0;

    for (size_t i = 0; i < n; i++) {
        res += bitcount8(p[i]);
    }
    return res;
}

static int membitcount_check_small(size_t (*fn)(const void *, size_t))
{
    static uint8_t v[64] = {
        1, 2, 3, 4, 5, 6, 7, 8, 1, 2, 3, 4, 5, 6, 7, 8,
        1, 2, 3, 4, 5, 6, 7, 8, 1, 2, 3, 4, 5, 6, 7, 8,
        1, 2, 3, 4, 5, 6, 7, 8, 1, 2, 3, 4, 5, 6, 7, 8,
        1, 2, 3, 4, 5, 6, 7, 8, 1, 2, 3, 4, 5, 6, 7, 8,
    };
    for (int i = 0; i < countof(v); i++) {
        for (int j = i; j < countof(v); j++) {
            Z_ASSERT_EQ(membitcount_naive(v + i, j - i), fn(v + i, j - i),
                        "i:%d j:%d", i, j);
        }
    }
    Z_HELPER_END;
}

static int membitcount_check_rand(size_t (*fn)(const void *, size_t))
{
#define N (1U << 12)
    t_scope;
    char *v = t_new_raw(char, N);

    for (size_t i = 0; i < N; i++)
        v[i] = i;
    for (size_t i = 0; i < 32; i++) {
        Z_ASSERT_EQ(membitcount_naive(v + i, N - i), fn(v + i, N - i));
    }
    for (size_t i = 0; i < 32; i++) {
        Z_ASSERT_EQ(membitcount_naive(v, N - i), fn(v, N - i));
    }
    Z_HELPER_END;
#undef N
}

Z_GROUP_EXPORT(membitcount)
{
    Z_TEST(fast_c, "") {
        Z_HELPER_RUN(membitcount_check_rand(membitcount_c));
        Z_HELPER_RUN(membitcount_check_small(membitcount_c));
    } Z_TEST_END;

    Z_TEST(ssse3, "") {
#if (defined(__x86_64__) || defined(__i386__)) && __GNUC_PREREQ(4, 4)
        int eax, ebx, ecx, edx;

        __cpuid(1, eax, ebx, ecx, edx);
        if (ecx & bit_SSSE3) {
            Z_HELPER_RUN(membitcount_check_rand(membitcount_ssse3));
            Z_HELPER_RUN(membitcount_check_small(membitcount_ssse3));
        } else {
            Z_SKIP("your CPU doesn't support ssse3");
        }
#else
        Z_SKIP("neither amd64 nor i386 or unsupported compiler");
#endif
    } Z_TEST_END;

    Z_TEST(popcnt, "") {
#if (defined(__x86_64__) || defined(__i386__)) && __GNUC_PREREQ(4, 4)
        int eax, ebx, ecx, edx;

        __cpuid(1, eax, ebx, ecx, edx);
        if (ecx & bit_POPCNT) {
            Z_HELPER_RUN(membitcount_check_rand(membitcount_popcnt));
            Z_HELPER_RUN(membitcount_check_small(membitcount_popcnt));
        } else {
            Z_SKIP("your CPU doesn't support popcnt");
        }
#else
        Z_SKIP("neither amd64 nor i386 or unsupported compiler");
#endif
    } Z_TEST_END;
} Z_GROUP_END

/* }}} */
