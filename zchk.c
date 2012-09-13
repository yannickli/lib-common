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

Z_GROUP(endianess)
{
    Z_TEST(unaligned, "put_unaligned/get_unaligned") {
        byte data[BUFSIZ];
        uint16_t us;
        uint32_t u;
        uint64_t ul;

#define DO_TEST(w, e, x)                                                    \
        ({                                                                  \
            void *v1 = data, *v2;                                           \
            v2 = put_unaligned_##e##w(v1, x);                               \
            put_unaligned_##e##w(v2, x);                                    \
            Z_ASSERT_EQ(get_unaligned_##e##w(v1), x, "check 1 " #w #e);     \
            Z_ASSERT_EQ(get_unaligned_##e##w(v2), x, "check 2 " #w #e);     \
        })
        us = 0x0201;
        DO_TEST(16, cpu, us);
        DO_TEST(16,  be, us);
        DO_TEST(16,  le, us);

        u  = 0x030201;
        DO_TEST(24,  be, u);
        DO_TEST(24,  le, u);

        u  = 0x04030201;
        DO_TEST(32, cpu, u);
        DO_TEST(32,  be, u);
        DO_TEST(32,  le, u);

        ul = 0x060504030201;
        DO_TEST(48,  be, ul);
        DO_TEST(48,  le, ul);

        ul = 0x0807060504030201;
        DO_TEST(64, cpu, ul);
        DO_TEST(64,  be, ul);
        DO_TEST(64,  le, ul);

#undef DO_TEST
    } Z_TEST_END;
} Z_GROUP_END;

int main(int argc, const char **argv)
{
    argc = z_setup(argc, argv);
    z_register_exports("lib-common/");
    z_register_group(z_bit_buf);
    z_register_group(z_endianess);
    return z_run();
}
