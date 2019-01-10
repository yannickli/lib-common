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
#include "core.h"

/* TODO restablish after merge into lib-common/master */
#include "z.h"

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
#define Z_GCD(_u1, _u2, _exp)                                                \
        do {                                                                 \
            uint32_t res;                                                    \
                                                                             \
            res = gcd_euclid(_u1, _u2);                                      \
            Z_ASSERT(res == _exp, "EUCLID: GCD("#_u1", "#_u2") = "#_exp      \
                     ", got %u", res);                                       \
            res = gcd_stein(_u1, _u2);                                       \
            Z_ASSERT(res == _exp, "STEIN: GCD("#_u1", "#_u2") = "#_exp       \
                     ", got %u", res);                                       \
        } while (0)

        Z_GCD(5, 0, 5);
        Z_GCD(0, 7, 7);
        Z_GCD(4, 1, 1);
        Z_GCD(1, 15, 1);
        Z_GCD(17, 999, 1);
        Z_GCD(15, 18, 3);
        Z_GCD(18, 15, 3);
        Z_GCD(60, 84, 12);

#undef Z_GCD
    } Z_TEST_END
} Z_GROUP_END
