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
#include "bit-stream.h"

Z_GROUP(bit_buf)
{
    Z_TEST(full, "bit-buf/bit-stream: full check") {
        t_scope;
        BB_1k(bb);
        bit_stream_t bs;

        bb_add_bit(&bb, true);
        bb_add_bit(&bb, false);
        bb_add_bit(&bb, true);
        bb_add_bit(&bb, true);
        bb_add_bit(&bb, false);
        bb_add_bit(&bb, false);
        bb_add_bit(&bb, false);
        bb_add_bit(&bb, true);
        bb_add_bit(&bb, false);
        bb_add_bit(&bb, true);
        bs = bs_init_bb(&bb);
        Z_ASSERT_EQ(bs_len(&bs), 10U, "Check length #1");

        bb_add_bits(&bb, 0x1a, 7); /* 0011010 */
        Z_ASSERT_STREQUAL("0101100", t_print_bits(0x1a, 0, 7));

        bs = bs_init_bb(&bb);
        Z_ASSERT_STREQUAL(".10110001.01001101.0", t_print_bs(bs, NULL));

        Z_ASSERT_EQ(bs_len(&bs), 17U, "Check length #2");
        Z_ASSERT_EQ(__bs_get_bit(&bs), true,  "Check bit #1");
        Z_ASSERT_EQ(__bs_get_bit(&bs), false, "Check bit #2");
        Z_ASSERT_EQ(__bs_get_bit(&bs), true,  "Check bit #3");
        Z_ASSERT_EQ(__bs_get_bit(&bs), true,  "Check bit #4");
        Z_ASSERT_EQ(__bs_get_bit(&bs), false, "Check bit #5");
        Z_ASSERT_EQ(__bs_get_bit(&bs), false, "Check bit #6");
        Z_ASSERT_EQ(__bs_get_bit(&bs), false, "Check bit #7");
        Z_ASSERT_EQ(__bs_get_bit(&bs), true,  "Check bit #8");
        Z_ASSERT_EQ(__bs_get_bit(&bs), false, "Check bit #9");
        Z_ASSERT_EQ(__bs_get_bit(&bs), true,  "Check bit #10");
    } Z_TEST_END;
} Z_GROUP_END;

int main(int argc, const char **argv)
{
    argc = z_setup(argc, argv);
    z_register_exports("lib-common/");
    z_register_group(z_bit_buf);
    return z_run();
}
