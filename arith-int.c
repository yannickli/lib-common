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
#include "z.h"

uint64_t const powerof10[16] = {
    1LL,
    10LL,
    100LL,
    1000LL,
    10000LL,
    100000LL,
    1000000LL,
    10000000LL,
    100000000LL,
    1000000000LL,
    10000000000LL,
    100000000000LL,
    1000000000000LL,
    10000000000000LL,
    100000000000000LL,
    1000000000000000LL,
};

/* Note about GCD algorithms:
 *
 * Stein's algorithm is significantly better than Euclid's one for lower
 * values (the switch is located around 1M on a 2009 quad core). For greater
 * values, Euclid's algorithm seems to take advantage of the low-level
 * optimization of modulo operator.
 *
 * We suppose that most basic usages of GCD will be alright with Stein's
 * algorithm.
 *
 */

uint32_t gcd_euclid(uint32_t a, uint32_t b)
{
    if (a < b) {
        SWAP(uint32_t, a, b);
    }

    while (b) {
        a = a % b;
        SWAP(uint32_t, a, b);
    }

    return a;
}

uint32_t gcd_stein(uint32_t a, uint32_t b)
{
    uint8_t za;
    uint8_t zb;

    if (!a)
        return b;
    if (!b)
        return a;

    za = bsf32(a);
    a >>= za;
    zb = bsf32(b);
    b >>= zb;

    while (a != b) {
        if (a > b) {
            a -= b;
            a >>= bsf32(a);
        } else {
            b -= a;
            b >>= bsf32(b);
        }
    }

    return a << MIN(za, zb);
}

uint32_t gcd(uint32_t a, uint32_t b)
{
    return gcd_stein(a, b);
}

Z_GROUP_EXPORT(arithint)
{
    Z_TEST(gcd, "gcd: Euclid's algorithm") {
        struct {
            uint32_t i;
            uint32_t j;
            uint32_t gcd;
        } t[] = {
            { 5,  0,   5  },
            { 0,  7,   7  },
            { 4,  1,   1  },
            { 1,  15,  1  },
            { 17, 999, 1  },
            { 15, 18,  3  },
            { 18, 15,  3  },
            { 60, 84,  12 },
        };

        for (int i = 0; i < countof(t); i++) {
            Z_ASSERT_EQ(t[i].gcd, gcd_euclid(t[i].i, t[i].j),
                        "EUCLID: GCD(%u, %u)", t[i].i, t[i].j);
            Z_ASSERT_EQ(t[i].gcd, gcd_stein(t[i].i, t[i].j),
                        "STEIN: GCD(%u, %u)", t[i].i, t[i].j);
        }
    } Z_TEST_END
} Z_GROUP_END
