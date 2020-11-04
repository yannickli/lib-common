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

#include <math.h>
#include <float.h>

#include "arith.h"
#include "z.h"

double double_round(double val, uint8_t precision)
{
    double val_floor;

    if (isinf(val) || isnan(val)) {
        return val;
    }

    val_floor = floor(val);
    if (!expect(precision < countof(powerof10))) {
        return val;
    }

    val -= val_floor;
    val *= powerof10[precision];
    val  = round(val);
    val /= powerof10[precision];

    return val + val_floor;
}

/* {{{ Tests */

Z_GROUP_EXPORT(arithfloat)
{
    Z_TEST(double_round, "double_round") {
#define T(val, precision, res)  \
    Z_ASSERT_LT(fabs(double_round(val, precision) - res), DBL_EPSILON)

        T(12.1234567, 0, 12.);
        T(12.1234567, 1, 12.1);
        T(12.1234567, 2, 12.12);
        T(12.1234567, 3, 12.123);
        T(12.1234567, 4, 12.1235);
        T(12.1234567, 5, 12.12346);
        T(12.1234567, 6, 12.123457);
        T(12.1234567, 7, 12.1234567);
        T(12.1234567, 8, 12.1234567);
        T(12.12345,   4, 12.1235);

        T(12.6, 0, 13.);

        T(-12.1234567, 0, -12.);
        T(-12.1234567, 1, -12.1);
        T(-12.1234567, 2, -12.12);
        T(-12.1234567, 3, -12.123);
        T(-12.1234567, 4, -12.1235);
        T(-12.1234567, 5, -12.12346);
        T(-12.1234567, 6, -12.123457);
        T(-12.1234567, 7, -12.1234567);
        T(-12.1234567, 8, -12.1234567);
        T(-12.12345,   4, -12.1234);

        T(-12.6, 0, -13.);
#undef T
        Z_ASSERT_NE(isinf(double_round(INFINITY, 3)), 0);
        Z_ASSERT_NE(isinf(double_round(-INFINITY, 3)), 0);
        Z_ASSERT_NE(isnan(double_round(NAN, 3)), 0);
    } Z_TEST_END
} Z_GROUP_END

/* }}} */
