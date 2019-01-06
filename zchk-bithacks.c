/**************************************************************************/
/*                                                                        */
/*  Copyright (C) 2004-2019 INTERSEC SA                                   */
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

Z_GROUP_EXPORT(bithacks)
{
    Z_TEST(bsr8, "full check of bsr8") {
        for (int i = 1; i < 256; i++) {
            Z_ASSERT_EQ(bsr8(i), bsr16(i), "[i:%d]", i);
        }
    } Z_TEST_END;

    Z_TEST(bsf8, "full check of bsf8") {
        for (int i = 1; i < 256; i++) {
            Z_ASSERT_EQ(bsf8(i), bsf16(i), "[i:%d]", i);
        }
    } Z_TEST_END;

    Z_TEST(bitmasks, "core: BITMASKS") {
        Z_ASSERT_EQ(BITMASK_GE(uint32_t, 0),  0xffffffffU);
        Z_ASSERT_EQ(BITMASK_GE(uint32_t, 31), 0x80000000U);

        Z_ASSERT_EQ(BITMASK_GT(uint32_t, 0),  0xfffffffeU);
        Z_ASSERT_EQ(BITMASK_GT(uint32_t, 31), 0x00000000U);

        Z_ASSERT_EQ(BITMASK_LE(uint32_t, 0),  0x00000001U);
        Z_ASSERT_EQ(BITMASK_LE(uint32_t, 31), 0xffffffffU);

        Z_ASSERT_EQ(BITMASK_LT(uint32_t, 0),  0x00000000U);
        Z_ASSERT_EQ(BITMASK_LT(uint32_t, 31), 0x7fffffffU);
    } Z_TEST_END;
} Z_GROUP_END
