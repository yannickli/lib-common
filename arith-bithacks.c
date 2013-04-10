/**************************************************************************/
/*                                                                        */
/*  Copyright (C) 2004-2012 INTERSEC SAS                                  */
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
        const uint8_t *p = (const uint8_t *)&p32[i];

        switch (n % 4) {
          case 1:
            c1 += bitcount8(*p);
            break;

          case 2:
            c1 += bitcount16(*(const uint16_t *)p);
            break;

          case 3:
            c1 += bitcount8(*p) + bitcount16(*(const uint16_t *)(p + 1));
            break;
        }
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
