/**************************************************************************/
/*                                                                        */
/*  Copyright (C) 2004-2011 INTERSEC SAS                                  */
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

#ifndef __POPCNT__
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
#endif

__attribute__((used))
static size_t membitcount_c(const void *ptr, size_t n)
{
    size_t c1 = 0, c2 = 0, c3 = 0, c4 = 0;
    size_t i = 0;
    const uint8_t *p = ptr;

    for (; (uintptr_t)(p + i) & 3; i++)
        c1 += bitcount8(*p++);

    for (; i + 32 <= n; i += 32) {
        c1 += bitcount32(((uint32_t *)(p + i))[0]);
        c2 += bitcount32(((uint32_t *)(p + i))[1]);
        c3 += bitcount32(((uint32_t *)(p + i))[2]);
        c4 += bitcount32(((uint32_t *)(p + i))[3]);
        c1 += bitcount32(((uint32_t *)(p + i))[4]);
        c2 += bitcount32(((uint32_t *)(p + i))[5]);
        c3 += bitcount32(((uint32_t *)(p + i))[6]);
        c4 += bitcount32(((uint32_t *)(p + i))[7]);
    }
    for (; i < n; i++)
        c1 += bitcount8(*p++);
    return c1 + c2 + c3 + c4;
}

#if __GNUC_PREREQ(4, 6) || __has_attribute(ifunc)
#if defined(__x86_64__) || defined(__i386__)
#include <cpuid.h>
#include <x86intrin.h>

__attribute__((target("popcnt")))
static size_t membitcount_popcnt(const void *ptr, size_t n)
{
    size_t c1 = 0, c2 = 0, c3 = 0, c4 = 0;
    const uint8_t *p = ptr;
    size_t i = 0;

#ifdef __x86_64__
    for (; i + 64 <= n; i += 64) {
        c1 += __builtin_popcountll(get_unaligned_cpu64(p + i +  0));
        c2 += __builtin_popcountll(get_unaligned_cpu64(p + i +  8));
        c3 += __builtin_popcountll(get_unaligned_cpu64(p + i + 16));
        c4 += __builtin_popcountll(get_unaligned_cpu64(p + i + 24));
        c1 += __builtin_popcountll(get_unaligned_cpu64(p + i + 32));
        c2 += __builtin_popcountll(get_unaligned_cpu64(p + i + 40));
        c3 += __builtin_popcountll(get_unaligned_cpu64(p + i + 48));
        c4 += __builtin_popcountll(get_unaligned_cpu64(p + i + 56));
    }
    for (; i + 8 <= n; i += 8)
        c1 += __builtin_popcountll(get_unaligned_cpu64(p + i));
#else
    for (; i + 32 <= n; i += 32) {
        c1 += __builtin_popcountll(get_unaligned_cpu32(p + i +  0));
        c2 += __builtin_popcountll(get_unaligned_cpu32(p + i +  4));
        c3 += __builtin_popcountll(get_unaligned_cpu32(p + i +  8));
        c4 += __builtin_popcountll(get_unaligned_cpu32(p + i + 12));
        c1 += __builtin_popcountll(get_unaligned_cpu32(p + i + 16));
        c2 += __builtin_popcountll(get_unaligned_cpu32(p + i + 20));
        c3 += __builtin_popcountll(get_unaligned_cpu32(p + i + 24));
        c4 += __builtin_popcountll(get_unaligned_cpu32(p + i + 28));
    }
#endif
    for (; i + 4 <= n; i += 4)
        c1 += __builtin_popcount(get_unaligned_cpu32(p + i));
    for (; i < n; i++)
        c1 += bitcount8(p[i]);
    return c1 + c2 + c3 + c4;
}

__attribute__((target("ssse3")))
static size_t membitcount_ssse3(const void *ptr, size_t n)
{
    static __v16qi const lo_4bits = {
        0xf, 0xf, 0xf, 0xf, 0xf, 0xf, 0xf, 0xf,
        0xf, 0xf, 0xf, 0xf, 0xf, 0xf, 0xf, 0xf,
    };
    static __v16qi const popcnt_4bits = {
        0, 1, 1, 2, 1, 2, 2, 3, 1, 2, 2, 3, 2, 3, 3, 4,
    };
    size_t c = 0;
    size_t i = 0;
    const char *p = ptr;
    __v2di res = { 0, };

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
#define step(Ptr) \
    ({  __v16qi xmm_lo = __builtin_ia32_loaddqu(Ptr);                          \
        __v16qi xmm_hi = (__v16qi)__builtin_ia32_psrlwi128((__v8hi)xmm_lo, 4); \
                                                                               \
        xmm_lo &= lo_4bits;                                                    \
        xmm_hi &= lo_4bits;                                                    \
        xmm_lo  = __builtin_ia32_pshufb128(popcnt_4bits, xmm_lo);              \
        xmm_hi  = __builtin_ia32_pshufb128(popcnt_4bits, xmm_hi);              \
        xmm_lo + xmm_hi;                                                       \
    })

    for (; i + 64 <= n; i += 64) {
        __v16qi acc = { 0, };

        acc += step(p + i +  0);
        acc += step(p + i + 16);
        acc += step(p + i + 32);
        acc += step(p + i + 48);
        res += __builtin_ia32_psadbw128(acc, (__v16qi){ 0, });
    }
    for (; i + 16 <= n; i += 16) {
        res += __builtin_ia32_psadbw128(step(p + i), (__v16qi){ 0, });
    }
    res += (__v2di)__builtin_ia32_movhlps((__v4sf)res, (__v4sf)res);
    for (; i + 4 <= n; i += 4)
        c += bitcount32(get_unaligned_cpu32(p + i));
    for (; i < n; i++)
        c += bitcount8(p[i]);
    return c + res[0];

#undef step
}

#endif

static void (*membitcount_resolve(void))(void)
{
#if defined(__x86_64__) || defined(__i386__)
    int eax, ebx, ecx, edx;

    __cpuid(1, eax, ebx, ecx, edx);
    if (ecx & bit_POPCNT)
        return (void (*)(void))&membitcount_popcnt;
    if (ecx & bit_SSSE3)
        return (void (*)(void))&membitcount_ssse3;
#endif
    return (void (*)(void))&membitcount_c;
}

size_t membitcount(const void *ptr, size_t n)
    __attribute__((ifunc("membitcount_resolve")));

#else

size_t membitcount(const void *ptr, size_t n)
    __attribute__((alias("membitcount_c")));

#endif

Z_GROUP_EXPORT(membitcount)
{
#define N (1U << 20)

    Z_TEST(popcnt, "") {
#if __GNUC_PREREQ(4, 6) || __has_attribute(ifunc)
#if defined(__x86_64__) || defined(__i386__)
        int eax, ebx, ecx, edx;

        __cpuid(1, eax, ebx, ecx, edx);
        if (ecx & bit_POPCNT) {
            t_scope;
            char *v = t_new_raw(char, N);

            for (size_t i = 0; i < N; i++)
                v[i] = i;
            Z_ASSERT_EQ(membitcount_c(v, N), membitcount_popcnt(v, N));
        } else {
            Z_SKIP("your CPU doesn't support popcnt");
        }
#else
        Z_SKIP("neither amd64 nor i386");
#endif
#else
        Z_SKIP("unsupported compiler");
#endif
    } Z_TEST_END;

    Z_TEST(ssse3, "") {
#if __GNUC_PREREQ(4, 6) || __has_attribute(ifunc)
#if defined(__x86_64__) || defined(__i386__)
        int eax, ebx, ecx, edx;

        __cpuid(1, eax, ebx, ecx, edx);
        if (ecx & bit_SSSE3) {
            t_scope;
            char *v = t_new_raw(char, N);

            for (size_t i = 0; i < N; i++)
                v[i] = i;
            Z_ASSERT_EQ(membitcount_c(v, N), membitcount_ssse3(v, N));
        } else {
            Z_SKIP("your CPU doesn't support ssse3");
        }
#else
        Z_SKIP("neither amd64 nor i386");
#endif
#else
        Z_SKIP("unsupported compiler");
#endif
    } Z_TEST_END;
} Z_GROUP_END
