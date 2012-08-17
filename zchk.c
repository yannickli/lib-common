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

Z_GROUP_EXPORT(bit_buf)
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
} Z_GROUP_END;

Z_GROUP_EXPORT(endianess)
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

Z_GROUP_EXPORT(bit_stream)
{
    bit_stream_t bs;
    bit_stream_t n;
    const byte data[128];

    /* Multiple of 64 in the range 
        0 64 128 192 256
        320 384 448 512
        576 640 704 768
        832 896 960 1024
    */

#define Z_CHECK_LENGTH(Stream, Len, ...)  do {                               \
        const bit_stream_t __bs = Stream;                                     \
        int         __len;                                                   \
        Z_ASSERT_EQ((__len = bs_len(&__bs)), Len, ##__VA_ARGS__);            \
        if (__len == 0) {                                                    \
            Z_ASSERT(bs_done(&__bs));                                        \
        } else {                                                             \
            Z_ASSERT(!bs_done(&__bs));                                       \
        }                                                                    \
        for (int i = __len; i-- > 0;) {                                      \
            Z_ASSERT(bs_has(&__bs, i));                                      \
        }                                                                    \
        for (int i = __len + 1; i < __len * 2 + 2; i++) {                    \
            Z_ASSERT(!bs_has(&__bs, i));                                     \
        }                                                                    \
    } while (0)

#define Z_CHECK_BOUNDS(Stream, From, To)  do {                               \
        const bit_stream_t __bs2 = Stream;                                   \
        const bit_stream_t __bds = bs_init_ptroff(data, From, data, To);     \
                                                                             \
        Z_ASSERT(__bds.s.p == __bs2.s.p);                                    \
        Z_ASSERT_EQ(__bds.s.offset, __bs2.s.offset);                         \
        Z_ASSERT(__bds.e.p == __bs2.e.p);                                    \
        Z_ASSERT_EQ(__bds.e.offset, __bs2.e.offset);                         \
        Z_CHECK_LENGTH(__bs2, To - From);                                    \
    } while (0)

    Z_TEST(len, "bit_stream: check length") {
        Z_CHECK_LENGTH(bs_init_ptr(data, data), 0);
        Z_CHECK_LENGTH(bs_init_ptr(&data[1], &data[1]), 0);
        Z_CHECK_LENGTH(bs_init_ptr(&data[2], &data[2]), 0);
        Z_CHECK_LENGTH(bs_init_ptr(&data[3], &data[3]), 0);
        Z_CHECK_LENGTH(bs_init_ptr(&data[4], &data[4]), 0);
        Z_CHECK_LENGTH(bs_init_ptr(&data[5], &data[5]), 0);

        Z_CHECK_LENGTH(bs_init_ptroff(data, 0, data, 0), 0);
        Z_CHECK_LENGTH(bs_init_ptroff(data, 8, &data[1], 0), 0);
        Z_CHECK_LENGTH(bs_init_ptroff(data, 19, &data[2], 3), 0);
        Z_CHECK_LENGTH(bs_init_ptroff(data, 138, &data[16], 10), 0);
        Z_CHECK_LENGTH(bs_init_ptroff(data, 138, &data[17], 2), 0);

        Z_CHECK_LENGTH(bs_init_ptr(data, &data[1]), 8);
        Z_CHECK_LENGTH(bs_init_ptr(data, &data[2]), 16);
        Z_CHECK_LENGTH(bs_init_ptr(data, &data[3]), 24);
        Z_CHECK_LENGTH(bs_init_ptr(data, &data[4]), 32);
        Z_CHECK_LENGTH(bs_init_ptr(data, &data[8]), 64);
        Z_CHECK_LENGTH(bs_init_ptr(&data[3], &data[7]), 32);
        Z_CHECK_LENGTH(bs_init_ptr(&data[3], &data[19]), 128);
        Z_CHECK_LENGTH(bs_init_ptr(data, &data[128]), 1024);

        Z_CHECK_LENGTH(bs_init_ptroff(data, 0, data, 1), 1);
        Z_CHECK_LENGTH(bs_init_ptroff(data, 3, data, 4), 1);
        Z_CHECK_LENGTH(bs_init_ptroff(data, 7, data, 8), 1);
        Z_CHECK_LENGTH(bs_init_ptroff(data, 63, data, 64), 1);
        Z_CHECK_LENGTH(bs_init_ptroff(data, 0, data, 128), 128);
        Z_CHECK_LENGTH(bs_init_ptroff(data, 19, data, 147), 128);
        Z_CHECK_LENGTH(bs_init_ptroff(data, 63, data, 191), 128);

    } Z_TEST_END;

    Z_TEST(skip, "bit_stream: bs_skip") {
        bs = bs_init_ptr(data, &data[128]);

        Z_ASSERT_NEG(bs_skip(&bs, 1025));
        Z_ASSERT_EQ(bs_skip(&bs, 1024), 1024);
        Z_CHECK_BOUNDS(bs, 1024, 1024);

        bs = bs_init_ptr(data, &data[128]);
        Z_ASSERT_EQ(bs_skip(&bs, 0), 0);
        Z_CHECK_BOUNDS(bs, 0, 1024);

        Z_ASSERT_EQ(bs_skip(&bs, 13), 13);
        Z_CHECK_BOUNDS(bs, 13, 1024);

        Z_ASSERT_EQ(bs_skip(&bs, 51), 51);
        Z_CHECK_BOUNDS(bs, 64, 1024);

        Z_ASSERT_EQ(bs_skip(&bs, 70), 70);
        Z_CHECK_BOUNDS(bs, 134, 1024);

        Z_ASSERT_EQ(bs_skip(&bs, 2), 2);
        Z_CHECK_BOUNDS(bs, 136, 1024);

        Z_ASSERT_EQ(bs_skip(&bs, 128), 128);
        Z_CHECK_BOUNDS(bs, 264, 1024);
    } Z_TEST_END;

    Z_TEST(shrink, "bit_stream: bs_shrink") {
        bs = bs_init_ptr(data, &data[128]);

        Z_ASSERT_NEG(bs_shrink(&bs, 1025));
        Z_ASSERT_EQ(bs_shrink(&bs, 1024), 1024);
        Z_CHECK_BOUNDS(bs, 0, 0);

        bs = bs_init_ptr(data, &data[128]);
        Z_ASSERT_EQ(bs_shrink(&bs, 0), 0);
        Z_CHECK_BOUNDS(bs, 0, 1024);

        Z_ASSERT_EQ(bs_shrink(&bs, 13), 13);
        Z_CHECK_BOUNDS(bs, 0, 1011);

        Z_ASSERT_EQ(bs_shrink(&bs, 51), 51);
        Z_CHECK_BOUNDS(bs, 0, 960);

        Z_ASSERT_EQ(bs_shrink(&bs, 70), 70);
        Z_CHECK_BOUNDS(bs, 0, 890);

        Z_ASSERT_EQ(bs_shrink(&bs, 2), 2);
        Z_CHECK_BOUNDS(bs, 0, 888);

        Z_ASSERT_EQ(bs_shrink(&bs, 128), 128);
        Z_CHECK_BOUNDS(bs, 0, 760);
    } Z_TEST_END;

    Z_TEST(skip_upto, "bit_stream: bs_skip_upto") {
        bs = bs_init_ptr(data, &data[128]);

        Z_ASSERT_NEG(bs_skip_upto(&bs, data, 1025));
        Z_ASSERT_EQ(bs_skip_upto(&bs, data, 1024), 1024);
        Z_CHECK_BOUNDS(bs, 1024, 1024);

        bs = bs_init_ptr(data, &data[128]);
        Z_ASSERT_EQ(bs_skip_upto(&bs, data, 0), 0);
        Z_CHECK_BOUNDS(bs, 0, 1024);

        Z_ASSERT_EQ(bs_skip_upto(&bs, data, 13), 13);
        Z_CHECK_BOUNDS(bs, 13, 1024);

        Z_ASSERT_EQ(bs_skip_upto(&bs, data, 64), 51);
        Z_CHECK_BOUNDS(bs, 64, 1024);

        Z_ASSERT_EQ(bs_skip_upto(&bs, data, 134), 70);
        Z_CHECK_BOUNDS(bs, 134, 1024);

        Z_ASSERT_EQ(bs_skip_upto(&bs, data, 136), 2);
        Z_CHECK_BOUNDS(bs, 136, 1024);

        Z_ASSERT_EQ(bs_skip_upto(&bs, data, 264), 128);
        Z_CHECK_BOUNDS(bs, 264, 1024);
    } Z_TEST_END;

    Z_TEST(clip_at, "bit_stream: bs_clip_at") {
        bs = bs_init_ptr(data, &data[128]);

        Z_ASSERT_NEG(bs_clip_at(&bs, data, 1025));
        Z_ASSERT_N(bs_clip_at(&bs, data, 0));
        Z_CHECK_BOUNDS(bs, 0, 0);

        bs = bs_init_ptr(data, &data[128]);
        Z_ASSERT_N(bs_clip_at(&bs, data, 1024));
        Z_CHECK_BOUNDS(bs, 0, 1024);

        Z_ASSERT_N(bs_clip_at(&bs, data, 1011));
        Z_CHECK_BOUNDS(bs, 0, 1011);

        Z_ASSERT_N(bs_clip_at(&bs, data, 960));
        Z_CHECK_BOUNDS(bs, 0, 960);

        Z_ASSERT_N(bs_clip_at(&bs, data, 890));
        Z_CHECK_BOUNDS(bs, 0, 890);

        Z_ASSERT_N(bs_clip_at(&bs, data, 888));
        Z_CHECK_BOUNDS(bs, 0, 888);

        Z_ASSERT_N(bs_clip_at(&bs, data, 760));
        Z_CHECK_BOUNDS(bs, 0, 760);
    } Z_TEST_END;

    Z_TEST(extract_after, "bit_stream: bs_extract_after") {
        bs = bs_init_ptr(data, &data[128]);

        Z_ASSERT_NEG(bs_extract_after(&bs, data, 1025, &n));
        Z_ASSERT_N(bs_extract_after(&bs, data, 0, &n));
        Z_CHECK_BOUNDS(bs, 0, 1024);
        Z_CHECK_BOUNDS(n, 0, 1024);

        bs = n;
        Z_ASSERT_N(bs_extract_after(&bs, data, 1024, &n));
        Z_CHECK_BOUNDS(bs, 0, 1024);
        Z_CHECK_BOUNDS(n, 1024, 1024);

        Z_ASSERT_N(bs_extract_after(&bs, data, 13, &n));
        Z_CHECK_BOUNDS(bs, 0, 1024);
        Z_CHECK_BOUNDS(n, 13, 1024);

        bs = n;
        Z_ASSERT_N(bs_extract_after(&bs, data, 64, &n));
        Z_CHECK_BOUNDS(bs, 13, 1024);
        Z_CHECK_BOUNDS(n, 64, 1024);

        bs = n;
        Z_ASSERT_N(bs_extract_after(&bs, data, 134, &n));
        Z_CHECK_BOUNDS(bs, 64, 1024);
        Z_CHECK_BOUNDS(n, 134, 1024);

        bs = n;
        Z_ASSERT_N(bs_extract_after(&bs, data, 136, &n));
        Z_CHECK_BOUNDS(bs, 134, 1024);
        Z_CHECK_BOUNDS(n, 136, 1024);

        bs = n;
        Z_ASSERT_N(bs_extract_after(&bs, data, 264, &n));
        Z_CHECK_BOUNDS(bs, 136, 1024);
        Z_CHECK_BOUNDS(n, 264, 1024);
    } Z_TEST_END;


    Z_TEST(get_bs_upto, "bit_stream: bs_get_bs_upto") {
        bs = bs_init_ptr(data, &data[128]);

        Z_ASSERT_NEG(bs_get_bs_upto(&bs, data, 1025, &n));
        Z_ASSERT_N(bs_get_bs_upto(&bs, data, 1024, &n));
        Z_CHECK_BOUNDS(bs, 1024, 1024);
        Z_CHECK_BOUNDS(n, 0, 1024);

        bs = n;
        Z_ASSERT_N(bs_get_bs_upto(&bs, data, 0, &n));
        Z_CHECK_BOUNDS(bs, 0, 1024);
        Z_CHECK_BOUNDS(n, 0, 0);

        Z_ASSERT_N(bs_get_bs_upto(&bs, data, 13, &n));
        Z_CHECK_BOUNDS(bs, 13, 1024);
        Z_CHECK_BOUNDS(n, 0, 13);

        Z_ASSERT_N(bs_get_bs_upto(&bs, data, 64, &n));
        Z_CHECK_BOUNDS(bs, 64, 1024);
        Z_CHECK_BOUNDS(n, 13, 64);

        Z_ASSERT_N(bs_get_bs_upto(&bs, data, 134, &n));
        Z_CHECK_BOUNDS(bs, 134, 1024);
        Z_CHECK_BOUNDS(n, 64, 134);

        Z_ASSERT_N(bs_get_bs_upto(&bs, data, 136, &n));
        Z_CHECK_BOUNDS(bs, 136, 1024);
        Z_CHECK_BOUNDS(n, 134, 136);

        Z_ASSERT_N(bs_get_bs_upto(&bs, data, 264, &n));
        Z_CHECK_BOUNDS(bs, 264, 1024);
        Z_CHECK_BOUNDS(n, 136, 264);
    } Z_TEST_END;

    Z_TEST(get_bs, "bit_stream: bs_get_bs") {
        bs = bs_init_ptr(data, &data[128]);

        Z_ASSERT_NEG(bs_get_bs(&bs, 1025, &n));
        Z_ASSERT_N(bs_get_bs(&bs, 1024, &n));
        Z_CHECK_BOUNDS(bs, 1024, 1024);
        Z_CHECK_BOUNDS(n, 0, 1024);

        bs = n;
        Z_ASSERT_N(bs_get_bs(&bs, 0, &n));
        Z_CHECK_BOUNDS(bs, 0, 1024);
        Z_CHECK_BOUNDS(n, 0, 0);

        Z_ASSERT_N(bs_get_bs(&bs, 13, &n));
        Z_CHECK_BOUNDS(bs, 13, 1024);
        Z_CHECK_BOUNDS(n, 0, 13);

        Z_ASSERT_N(bs_get_bs(&bs, 51, &n));
        Z_CHECK_BOUNDS(bs, 64, 1024);
        Z_CHECK_BOUNDS(n, 13, 64);

        Z_ASSERT_N(bs_get_bs(&bs, 70, &n));
        Z_CHECK_BOUNDS(bs, 134, 1024);
        Z_CHECK_BOUNDS(n, 64, 134);

        Z_ASSERT_N(bs_get_bs(&bs, 2, &n));
        Z_CHECK_BOUNDS(bs, 136, 1024);
        Z_CHECK_BOUNDS(n, 134, 136);

        Z_ASSERT_N(bs_get_bs(&bs, 128, &n));
        Z_CHECK_BOUNDS(bs, 264, 1024);
        Z_CHECK_BOUNDS(n, 136, 264);
    } Z_TEST_END;
} Z_GROUP_END;

int main(int argc, const char **argv)
{
    argc = z_setup(argc, argv);
    z_register_exports("lib-common/");
    return z_run();
}
