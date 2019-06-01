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

#include <endian.h>
#include <math.h>

#include "core.h"
#include "z.h"

Z_GROUP_EXPORT(iprintf) {
    char buffer[128];

    Z_TEST(double, "") {
        isprintf(buffer, "%g", -INFINITY);
        Z_ASSERT_STREQUAL(buffer, "-Inf");
        isprintf(buffer, "%g", INFINITY);
        Z_ASSERT_STREQUAL(buffer, "Inf");
        isprintf(buffer, "%+g", INFINITY);
        Z_ASSERT_STREQUAL(buffer, "+Inf");
    } Z_TEST_END;

    Z_TEST(pM, "") {
        isprintf(buffer, "%*pM", 3, "1234");
        Z_ASSERT_STREQUAL(buffer, "123", "");
        isprintf(buffer, "%*pM;toto", 3, "123");
        Z_ASSERT_STREQUAL(buffer, "123;toto", "");
        isprintf(buffer, "%*pMtrailing", 3, "123");
        Z_ASSERT_STREQUAL(buffer, "123trailing", "");
    } Z_TEST_END

    Z_TEST(pX, "") {
        isprintf(buffer, "%*pX", 4, "1234");
        Z_ASSERT_STREQUAL(buffer, "31323334");
        isprintf(buffer, "%*pX world!", 5, "Hello");
        Z_ASSERT_STREQUAL(buffer, "48656C6C6F world!");
        isprintf(buffer, "%*pXworld!", 5, "Hello");
        Z_ASSERT_STREQUAL(buffer, "48656C6C6Fworld!");
    } Z_TEST_END;

    Z_TEST(px, "") {
        isprintf(buffer, "%*px", 4, "1234");
        Z_ASSERT_STREQUAL(buffer, "31323334");
        isprintf(buffer, "%*px world!", 5, "Hello");
        Z_ASSERT_STREQUAL(buffer, "48656c6c6f world!");
        isprintf(buffer, "%*pxworld!", 5, "Hello");
        Z_ASSERT_STREQUAL(buffer, "48656c6c6fworld!");
    } Z_TEST_END;

    Z_TEST(pL, "") {
        const lstr_t str = LSTR_IMMED("1234");
        SB_1k(sb);

        isprintf(buffer, "%pL", &str);
        Z_ASSERT_STREQUAL(buffer, "1234");

        isprintf(buffer, "%pL;toto", &str);
        Z_ASSERT_STREQUAL(buffer, "1234;toto");
        isprintf(buffer, "%pLtrailing", &str);
        Z_ASSERT_STREQUAL(buffer, "1234trailing");

        /* works for sb_t variables too */
        sb_set_lstr(&sb, str);

        isprintf(buffer, "%pL", &sb);
        Z_ASSERT_STREQUAL(buffer, "1234");

        isprintf(buffer, "%pL;toto", &sb);
        Z_ASSERT_STREQUAL(buffer, "1234;toto");
        isprintf(buffer, "%pLtrailing", &sb);
        Z_ASSERT_STREQUAL(buffer, "1234trailing");
    } Z_TEST_END

    Z_TEST(ivasprintf, "") {
        char *formatted = iasprintf("%*pM", 4, "1234");
        int len = 2 * BUFSIZ;
        char big[len + 1];

        Z_ASSERT_STREQUAL(formatted, "1234");
        p_delete(&formatted);

        memset(big, 'a', len);
        big[2*BUFSIZ] = 0;
        formatted = iasprintf("%*pM", len, big);
        Z_ASSERT_STREQUAL(formatted, big);
        p_delete(&formatted);
    } Z_TEST_END;
} Z_GROUP_END

/* LCOV_EXCL_STOP */
