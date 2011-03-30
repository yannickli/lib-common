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

#include "bit-stream.h"

/* Tests {{{ */
TEST_DECL("bit-buf/bit-stream: full check", 0)
{
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
    TEST_FAIL_UNLESS(bs_len(&bs) == 10, "Check length #1");
    bb_add_bits(&bb, 0x1a, 7); /* 0011010 */
    t_push();
    TEST_FAIL_IF(strcmp("0101100", t_print_bits(0x1a, 0, 7)), "t_print_bits");
    t_pop();

    bs = bs_init_bb(&bb);
    t_push();
    TEST_FAIL_IF(strcmp(".10110001.01001101.0",
                        t_print_bs(bs, NULL)), "t_print_bs");
    t_pop();

    TEST_FAIL_UNLESS(bs_len(&bs) == 17, "Check length #2");
    TEST_FAIL_UNLESS(__bs_get_bit(&bs) == true,  "Check bit #1");
    TEST_FAIL_UNLESS(__bs_get_bit(&bs) == false, "Check bit #2");
    TEST_FAIL_UNLESS(__bs_get_bit(&bs) == true,  "Check bit #3");
    TEST_FAIL_UNLESS(__bs_get_bit(&bs) == true,  "Check bit #4");
    TEST_FAIL_UNLESS(__bs_get_bit(&bs) == false, "Check bit #5");
    TEST_FAIL_UNLESS(__bs_get_bit(&bs) == false, "Check bit #6");
    TEST_FAIL_UNLESS(__bs_get_bit(&bs) == false, "Check bit #7");
    TEST_FAIL_UNLESS(__bs_get_bit(&bs) == true,  "Check bit #8");
    TEST_FAIL_UNLESS(__bs_get_bit(&bs) == false, "Check bit #9");
    TEST_FAIL_UNLESS(__bs_get_bit(&bs) == true,  "Check bit #10");

    TEST_DONE();
}

/* }}} */
