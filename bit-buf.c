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

#include "bit.h"

void bb_wipe(bb_t *bb)
{
    switch (bb->mem_pool & MEM_POOL_MASK) {
      case MEM_STATIC:
      case MEM_STACK:
        return;
      default:
        ifree(bb->data, bb->mem_pool);
        break;
    }
}

void bb_reset(bb_t *bb)
{
    if (bb->mem_pool == MEM_LIBC && bb->size > (16 << 10)) {
        bb_wipe(bb);
        bb_init(bb);
    } else {
        bzero(bb->data, DIV_ROUND_UP(bb->len, 8));
        bb->len = 0;
    }
}

void bb_init_sb(bb_t *bb, sb_t *sb)
{
    sb_grow(sb, ROUND_UP(sb->len, 8) - sb->len);
    bb_init_full(bb, sb->data, sb->len * 8, DIV_ROUND_UP(sb->size, 8),
                 sb->mem_pool);

    /* We took ownership of the memory so ensure clear the sb */
    sb_init(sb);
}


void bb_transfer_to_sb(bb_t *bb, sb_t *sb)
{
    sb_wipe(sb);
    bb_grow(bb, 8);
    sb_init_full(sb, bb->data, DIV_ROUND_UP(bb->len, 8),
                 bb->size * 8, bb->mem_pool);
    bb_init(bb);
}

void __bb_grow(bb_t *bb, size_t extra)
{
    size_t newlen = DIV_ROUND_UP(bb->len + extra, 64);
    size_t newsz;

    newsz = p_alloc_nr(bb->size);
    if (newsz < newlen) {
        newsz = newlen;
    }
    if (bb->mem_pool == MEM_STATIC) {
        uint64_t *d = p_new_raw(uint64_t, newsz);

        p_copy(d, bb->data, bb->size);
        bzero(d + bb->size, (newsz - bb->size) * 8);
        bb_init_full(bb, d, bb->len, newsz, MEM_LIBC);
    } else {
        bb->data = irealloc(bb->data, bb->size * 8, newsz * 8,
                            bb->mem_pool | MEM_RAW);
        bzero(bb->data + bb->size, (newsz - bb->size) * 8);
        bb->size = newsz;
    }
}

void bb_add_bs(bb_t *bb, const bit_stream_t *b)
{
    bit_stream_t bs = *b;
    pstream_t ps;

    while (!bs_done(&bs) && !bs_is_aligned(&bs)) {
        bb_add_bit(bb, __bs_get_bit(&bs));
    }

    ps = __bs_get_bytes(&bs, bs_len(&bs) / 8);
    bb_add_bytes(bb, ps.b, ps_len(&ps));

    while (!bs_done(&bs)) {
        bb_add_bit(bb, __bs_get_bit(&bs));
    }
}

void bb_be_add_bs(bb_t *bb, const bit_stream_t *b)
{
    bit_stream_t bs = *b;
    pstream_t ps;

    while (!bs_done(&bs) && !bs_is_aligned(&bs)) {
        bb_be_add_bit(bb, __bs_be_get_bit(&bs));
    }

    ps = __bs_get_bytes(&bs, bs_len(&bs) / 8);
    bb_be_add_bytes(bb, ps.b, ps_len(&ps));

    while (!bs_done(&bs)) {
        bb_be_add_bit(bb, __bs_be_get_bit(&bs));
    }
}

char *t_print_bits(uint8_t bits, uint8_t bstart, uint8_t blen)
{
    char *str = t_new(char, blen + 1);
    char *w   = str;

    for (int i = bstart; i < blen; i++) {
        *w++ = (bits & (1 << i)) ? '1' : '0';
    }

    *w = '\0';

    return str;
}

char *t_print_be_bb(const bb_t *bb, size_t *len)
{
    bit_stream_t bs = bs_init_bb(bb);

    return t_print_be_bs(bs, len);
}

char *t_print_bb(const bb_t *bb, size_t *len)
{
    bit_stream_t bs = bs_init_bb(bb);

    return t_print_bs(bs, len);
}

/* Tests {{{ */

#include "z.h"

Z_GROUP_EXPORT(bit_buf)
{
    Z_TEST(le_full, "bit-buf/bit-stream: full check") {
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
        Z_ASSERT_EQ(bb.len, 10U);

        bs = bs_init_bb(&bb);
        Z_ASSERT_EQ(bs_len(&bs), 10U, "Check length #1");

        bb_add_bits(&bb, 0x1a, 7); /* 0011010 */
        Z_ASSERT_STREQUAL("0101100", t_print_bits(0x1a, 0, 7));

        Z_ASSERT_EQ(bb.len, 17U);
        bs = bs_init_bb(&bb);
        Z_ASSERT_STREQUAL(".10110001.01010110.0", t_print_bs(bs, NULL));

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

    Z_TEST(le_add_0_1, "") {
        BB_1k(bb);

        for (int i = 1; i < 256; i++) {
            for (int j = 0; j < i; j++) {
                bit_stream_t bs;

                bb_reset(&bb);
                Z_ASSERT_EQ(bb.len, (size_t)0);

                bb_add0s(&bb, j);
                Z_ASSERT_EQ(bb.len, (size_t)j, "bad size 0s %d-%d", i, j);
                bb_add1s(&bb, i - j);
                Z_ASSERT_EQ(bb.len, (size_t)i, "bad size 1s %d-%d", i, j);

                bs = bs_init_bb(&bb);
                for (int k = 0; k < i; k++) {
                    int bit = bs_get_bit(&bs);

                    Z_ASSERT_N(bit, "buffer too short %d-%d-%d", i, j, k);
                    if (k < j) {
                        Z_ASSERT(!bit, "bad bit %d-%d-%d", i, j, k);
                    } else {
                        Z_ASSERT(bit, "bad bit %d-%d-%d", i, j, k);
                    }
                }
                Z_ASSERT_NEG(bs_get_bit(&bs));
            }
        }
    } Z_TEST_END;

    Z_TEST(le_add_bytes, "") {
        BB_1k(bb);
        byte b[32];

        for (int i = 0; i < countof(b); i++) {
            b[i] = i;
        }

        for (int i = 0; i < countof(b) / 2; i++) {
            for (int j = 0; j < countof(b) / 2; j++) {
                for (int k = 0; k < 64; k++) {
                    bit_stream_t bs;
                    bit_stream_t bs2;

                    bb_reset(&bb);

                    bb_add1s(&bb, k);
                    bb_add_bytes(&bb, b + j, i);

                    bs = bs_init_bb(&bb);
                    for (int l = 0; l < k; l++) {
                        int bit = bs_get_bit(&bs);

                        Z_ASSERT_N(bit, "buffer too short %d-%d-%d", i, k, l);
                        Z_ASSERT(bit, "bad bit %d-%d-%d", i, k, l);
                    }

                    bs2 = bs_init(b + j, 0, i * 8);
                    while (!bs_done(&bs2)) {
                        Z_ASSERT_EQ(bs_get_bit(&bs), bs_get_bit(&bs2));
                    }
                    Z_ASSERT(bs_done(&bs));
                }
            }
        }
    } Z_TEST_END;

    Z_TEST(be_full, "bit-buf/bit-stream: full check") {
        t_scope;
        BB_1k(bb);
        bit_stream_t bs;

        bb_be_add_bit(&bb, true);
        bb_be_add_bit(&bb, false);
        bb_be_add_bit(&bb, true);
        bb_be_add_bit(&bb, true);
        bb_be_add_bit(&bb, false);
        bb_be_add_bit(&bb, false);
        bb_be_add_bit(&bb, false);
        bb_be_add_bit(&bb, true);
        bb_be_add_bit(&bb, false);
        bb_be_add_bit(&bb, true);
        Z_ASSERT_EQ(bb.len, 10U);

        bs = bs_init_bb(&bb);
        Z_ASSERT_EQ(bs_len(&bs), 10U, "Check length #1");

        bb_be_add_bits(&bb, 0x1a, 7); /* 0011010 */
        Z_ASSERT_STREQUAL("0101100", t_print_bits(0x1a, 0, 7));

        Z_ASSERT_EQ(bb.len, 17U);
        bs = bs_init_bb(&bb);
        Z_ASSERT_STREQUAL(".10110001.01001101.0", t_print_be_bs(bs, NULL));

        Z_ASSERT_EQ(bs_len(&bs), 17U, "Check length #2");
        Z_ASSERT_EQ(__bs_be_get_bit(&bs), true,  "Check bit #1");
        Z_ASSERT_EQ(__bs_be_get_bit(&bs), false, "Check bit #2");
        Z_ASSERT_EQ(__bs_be_get_bit(&bs), true,  "Check bit #3");
        Z_ASSERT_EQ(__bs_be_get_bit(&bs), true,  "Check bit #4");
        Z_ASSERT_EQ(__bs_be_get_bit(&bs), false, "Check bit #5");
        Z_ASSERT_EQ(__bs_be_get_bit(&bs), false, "Check bit #6");
        Z_ASSERT_EQ(__bs_be_get_bit(&bs), false, "Check bit #7");
        Z_ASSERT_EQ(__bs_be_get_bit(&bs), true,  "Check bit #8");
        Z_ASSERT_EQ(__bs_be_get_bit(&bs), false, "Check bit #9");
        Z_ASSERT_EQ(__bs_be_get_bit(&bs), true,  "Check bit #10");
    } Z_TEST_END;

    Z_TEST(le_bug, "bit-buf: add 64nth bit") {
        t_scope;
        t_BB_1k(bb);

        bb_add0s(&bb, 63);
        bb_add_bit(&bb, true);
        bb_add0s(&bb, 8);
        Z_ASSERT_STREQUAL(".00000000.00000000.00000000.00000000.00000000.00000000.00000000.00000001.00000000", t_print_bb(&bb, NULL));
    } Z_TEST_END;
} Z_GROUP_END;

/* }}} */
