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

/* LCOV_EXCL_START */

#include <lib-common/z.h>

Z_GROUP_EXPORT(core_havege) {
    Z_TEST(havege_range, "havege_range") {
        int number1;
        int numbers[10000];
        bool is_different_than_int_min = false;

        /* Test bug that existed in rand_range when INT_MIN was given, the
         * results were always INT_MIN.
         */
        for (int i = 0; i < 10000; i++) {
            numbers[i] = rand_range(INT_MIN, INT_MAX);
        }
        for (int i = 0; i < 10000; i++) {
            if (numbers[i] != INT_MIN) {
                is_different_than_int_min = true;
            }
        }
        Z_ASSERT(is_different_than_int_min);

        number1 = rand_range(INT_MIN + 1, INT_MAX - 1);
        Z_ASSERT(number1 > INT_MIN && number1 < INT_MAX);
        number1 = rand_range(-10, 10);
        Z_ASSERT(number1 >= -10 && number1 <= 10);
    } Z_TEST_END;
} Z_GROUP_END;

/* LCOV_EXCL_STOP */
