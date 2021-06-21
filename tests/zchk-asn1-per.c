/***************************************************************************/
/*                                                                         */
/* Copyright 2021 INTERSEC SA                                              */
/*                                                                         */
/* Licensed under the Apache License, Version 2.0 (the "License");         */
/* you may not use this file except in compliance with the License.        */
/* You may obtain a copy of the License at                                 */
/*                                                                         */
/*     http://www.apache.org/licenses/LICENSE-2.0                          */
/*                                                                         */
/* Unless required by applicable law or agreed to in writing, software     */
/* distributed under the License is distributed on an "AS IS" BASIS,       */
/* WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.*/
/* See the License for the specific language governing permissions and     */
/* limitations under the License.                                          */
/*                                                                         */
/***************************************************************************/

#include <lib-common/asn1-per.h>
#include <lib-common/z.h>
#include <lib-common/iop.h>

#include "iop/tstiop.iop.h"

/* {{{ Choice */

typedef struct {
    uint16_t iop_tag;
    union {
        int i;
    };
} choice1_t;

static __ASN1_IOP_CHOICE_DESC_BEGIN(choice1);
    asn1_reg_scalar(choice1, i, 0);
    asn1_set_int_min_max(choice1, 2, 15);
ASN1_CHOICE_DESC_END(choice1);

/* }}} */
/* {{{ Extended choice. */

static __ASN1_IOP_CHOICE_DESC_BEGIN(tstiop__asn1_ext_choice_);
    asn1_reg_scalar(tstiop__asn1_ext_choice_, i, 0);
    asn1_set_int_min_max(tstiop__asn1_ext_choice_, 42, 666);
    asn1_reg_extension(tstiop__asn1_ext_choice_);
    asn1_reg_string(tstiop__asn1_ext_choice_, ext_s, 1);
    asn1_reg_scalar(tstiop__asn1_ext_choice_, ext_i, 2);
    asn1_set_int_min_max(tstiop__asn1_ext_choice_, 666, 1234567);
ASN1_CHOICE_DESC_END(tstiop__asn1_ext_choice_);

/* }}} */
/* {{{ Extended sequence. */

typedef struct sequence1_t {
#define SEQ_EXT_ROOT_FIELDS                                                  \
    opt_i8_t root1;                                                          \
    int root2

#define SEQ_EXT_PARTIAL_FIELDS                                               \
    SEQ_EXT_ROOT_FIELDS;                                                     \
    lstr_t ext1;

#define SEQ_EXT_FIELDS                                                       \
    SEQ_EXT_PARTIAL_FIELDS;                                                  \
    opt_i32_t ext2;                                                          \
    opt_u8_t ext3

    SEQ_EXT_FIELDS;
} sequence1_t;

static ASN1_SEQUENCE_DESC_BEGIN(sequence1);
#define SEQ_EXT_ROOT_FIELDS_DESC(pfx)                                        \
    asn1_reg_scalar(pfx, root1, 0);                                          \
    asn1_set_int_min_max(pfx, 1, 16);                                        \
                                                                             \
    asn1_reg_scalar(pfx, root2, 0);                                          \
    asn1_set_int_min(pfx, -42);                                              \
                                                                             \
    asn1_reg_extension(pfx)

#define SEQ_EXT_PARTIAL_FIELDS_DESC(pfx)                                     \
    SEQ_EXT_ROOT_FIELDS_DESC(pfx);                                           \
    asn1_reg_opt_string(pfx, ext1, 0)

#define SEQ_EXT_FIELDS_DESC(pfx)                                             \
    SEQ_EXT_PARTIAL_FIELDS_DESC(pfx);                                        \
    asn1_reg_scalar(pfx, ext2, 0);                                           \
    asn1_set_int_min_max(pfx, -100000, 100000);                              \
                                                                             \
    asn1_reg_scalar(pfx, ext3, 0);                                           \
    asn1_set_int_min_max(pfx, 0, 256)

    SEQ_EXT_FIELDS_DESC(sequence1);
ASN1_SEQUENCE_DESC_END(sequence1);

/* Same without extension. */
typedef struct sequence1_root_t {
    SEQ_EXT_ROOT_FIELDS;
} sequence1_root_t;

static ASN1_SEQUENCE_DESC_BEGIN(sequence1_root);
    SEQ_EXT_ROOT_FIELDS_DESC(sequence1_root);
ASN1_SEQUENCE_DESC_END(sequence1_root);

/* Same with less fields in extension. */
typedef struct sequence1_partial_t {
    SEQ_EXT_PARTIAL_FIELDS;
} sequence1_partial_t;

static ASN1_SEQUENCE_DESC_BEGIN(sequence1_partial);
    SEQ_EXT_PARTIAL_FIELDS_DESC(sequence1_partial);
ASN1_SEQUENCE_DESC_END(sequence1_partial);

static int z_test_seq_ext(const sequence1_t *in, lstr_t exp_encoding)
{
    SB_1k(buf);
    pstream_t ps;
    sequence1_t out;
    sequence1_root_t out_root;
    sequence1_partial_t out_partial;

    Z_ASSERT_N(aper_encode(&buf, sequence1, in), "encoding failure");
    /* TODO switch to bits */
    Z_ASSERT_LSTREQUAL(exp_encoding, LSTR_SB_V(&buf),
                       "unexpected encoding value");

    memset(&out, 0xff, sizeof(out));
    ps = ps_initsb(&buf);
    Z_ASSERT_N(t_aper_decode(&ps, sequence1, false, &out),
               "decoding failure (full sequence)");
    Z_ASSERT_OPT_EQ(out.root1, in->root1);
    Z_ASSERT_EQ(out.root2, in->root2);
    Z_ASSERT_LSTREQUAL(out.ext1, in->ext1);
    Z_ASSERT_OPT_EQ(out.ext2, in->ext2);
    Z_ASSERT_OPT_EQ(out.ext3, in->ext3);

    memset(&out_root, 0xff, sizeof(out_root));
    ps = ps_initsb(&buf);
    Z_ASSERT_N(t_aper_decode(&ps, sequence1_root, false, &out_root),
               "decoding failure (root sequence)");
    Z_ASSERT_OPT_EQ(out_root.root1, in->root1);
    Z_ASSERT_EQ(out_root.root2, in->root2);

    memset(&out_partial, 0xff, sizeof(out_partial));
    ps = ps_initsb(&buf);
    Z_ASSERT_N(t_aper_decode(&ps, sequence1_partial, false, &out_partial),
               "decoding failure (partial sequence)");
    Z_ASSERT_OPT_EQ(out_partial.root1, in->root1);
    Z_ASSERT_EQ(out_partial.root2, in->root2);
    Z_ASSERT_LSTREQUAL(out_partial.ext1, in->ext1);

    Z_HELPER_END;
}

/* }}} */
/* {{{ Enumerated type. */

typedef enum enum1 {
    FOO,
    BAR,
} enum1_t;

static ASN1_ENUM_BEGIN(enum1)
    asn1_enum_reg_val(FOO);
    asn1_enum_reg_val(BAR);
ASN1_ENUM_END();

typedef struct {
    enum1_t e1;
} struct1_t;

static ASN1_SEQUENCE_DESC_BEGIN(struct1);
    asn1_reg_enum(struct1, enum1, e1, 0);
    asn1_set_enum_info(struct1, enum1);
ASN1_SEQUENCE_DESC_END(struct1);

/* }}} */
/* {{{ Integers overflows checks. */

typedef struct ints_seq_t {
    int8_t i8;
    uint8_t u8;
    int16_t i16;
    uint16_t u16;
    int32_t i32;
    uint32_t u32;
    int64_t i64;
    uint64_t u64;
    int64_t i64_bis;
    uint64_t u64_bis;
} ints_seq_t;

typedef struct ints_seq_base_t {
    int64_t i8;
    int64_t u8;
    int64_t i16;
    int64_t u16;
    int64_t i32;
    int64_t u32;
    int64_t i64;
    uint64_t u64;
    uint64_t i64_bis;
    int64_t u64_bis;
} ints_seq_base_t;

#define INTS_SEQ_FIELDS_DESC(pfx)                                            \
    asn1_reg_scalar(pfx, i8, 0);                                             \
    asn1_reg_scalar(pfx, u8, 1);                                             \
    asn1_reg_scalar(pfx, i16, 2);                                            \
    asn1_reg_scalar(pfx, u16, 3);                                            \
    asn1_reg_scalar(pfx, i32, 4);                                            \
    asn1_reg_scalar(pfx, u32, 5);                                            \
    asn1_reg_scalar(pfx, i64, 6);                                            \
    asn1_reg_scalar(pfx, u64, 7);                                            \
    asn1_reg_scalar(pfx, i64_bis, 8);                                        \
    asn1_reg_scalar(pfx, u64_bis, 9)

static ASN1_SEQUENCE_DESC_BEGIN(ints_seq);
    INTS_SEQ_FIELDS_DESC(ints_seq);
ASN1_SEQUENCE_DESC_END(ints_seq);

static ASN1_SEQUENCE_DESC_BEGIN(ints_seq_base);
    INTS_SEQ_FIELDS_DESC(ints_seq_base);
ASN1_SEQUENCE_DESC_END(ints_seq_base);

static int z_assert_ints_seq_equals_base(const ints_seq_t *seq,
                                         const ints_seq_base_t *base)
{
#define ASSERT_FIELD_EQUALS(field)                                           \
    Z_ASSERT_EQ(seq->field, base->field);

    ASSERT_FIELD_EQUALS(i8);
    ASSERT_FIELD_EQUALS(u8);
    ASSERT_FIELD_EQUALS(i16);
    ASSERT_FIELD_EQUALS(u16);
    ASSERT_FIELD_EQUALS(i32);
    ASSERT_FIELD_EQUALS(u32);

#undef ASSERT_FIELD_EQUALS

    Z_HELPER_END;
}

#undef INTS_SEQ_FIELDS_DESC

static int z_translate_ints_seq(const ints_seq_base_t *base,
                                bool expect_error)
{
    t_scope;
    SB_1k(sb);
    ints_seq_t ints;
    pstream_t ps;

    p_clear(&ints, 1);
    Z_ASSERT_N(aper_encode(&sb, ints_seq_base, base));
    ps = ps_initsb(&sb);

    if (expect_error) {
        Z_ASSERT_NEG(t_aper_decode(&ps, ints_seq, false, &ints));
    } else {
        Z_ASSERT_N(t_aper_decode(&ps, ints_seq, false, &ints));
        Z_HELPER_RUN(z_assert_ints_seq_equals_base(&ints, base));
    }

    Z_HELPER_END;
}

/* }}} */
/* {{{ Octet string. */

typedef struct {
    lstr_t str;
} z_octet_string_t;

static ASN1_SEQUENCE_DESC_BEGIN(z_octet_string);
    asn1_reg_string(z_octet_string, str, 0);
ASN1_SEQUENCE_DESC_END(z_octet_string);

/* }}} */
/* {{{ Open type. */

typedef struct {
    z_octet_string_t os;
} z_open_type_t;

static ASN1_SEQUENCE_DESC_BEGIN(z_open_type);
    asn1_reg_sequence(z_open_type, z_octet_string, os,
                      ASN1_TAG_SEQUENCE_C);
    asn1_set_open_type(z_open_type, (64 << 10));
ASN1_SEQUENCE_DESC_END(z_open_type);

/* }}} */
/* {{{ Sequence of. */

typedef struct {
    ASN1_VECTOR_TYPE(int8) seqof;
} z_seqof_i8_t;

static ASN1_SEQ_OF_DESC_BEGIN(z_seqof_i8);
    asn1_reg_scalar(z_seqof_i8, seqof, 0);
    asn1_set_int_min_max(z_seqof_i8, -3, 3);
ASN1_SEQ_OF_DESC_END(z_seqof_i8);

typedef struct {
    uint8_t a;
    z_seqof_i8_t s;
} z_seqof_t;

GENERIC_INIT(z_seqof_t, z_seqof);

static ASN1_SEQUENCE_DESC_BEGIN(z_seqof);
    asn1_reg_scalar(z_seqof, a, 0);
    asn1_set_int_min_max(z_seqof, 0, 2);

    asn1_reg_seq_of(z_seqof, z_seqof_i8, s, ASN1_TAG_SEQUENCE_C);
    asn1_set_seq_of_min_max(z_seqof, 0, 1024);
    asn1_set_seq_of_extended_min_max(z_seqof, 0, (256 << 10));
ASN1_SEQUENCE_DESC_END(z_seqof);

/* }}} */
/* {{{ Helpers. */

/* Skip N characters on two pstreams and check that they are the same. */
static int z_ps_skip_and_check_eq(pstream_t *ps1, pstream_t *ps2, int len)
{
    Z_ASSERT(ps_has(ps1, len));
    Z_ASSERT(ps_has(ps2, len));

    for (int i = 0; i < len; i++) {
        int c1 = __ps_getc(ps1);
        int c2 = __ps_getc(ps2);

        Z_ASSERT_EQ(c1, c2, "[%d] %x != %x", i, c1, c2);
    }

    Z_HELPER_END;
}

/* }}} */

Z_GROUP_EXPORT(asn1_aper) {
    /* {{{ Choice. */

    Z_TEST(choice, "choice") {
        t_scope;
        SB_1k(buf);

        for (int i = 2; i <= 15; i++) {
            pstream_t ps;
            choice1_t in;
            choice1_t out;

            sb_reset(&buf);
            p_clear(&in, 1);
            in.iop_tag = 1;
            in.i = i;

            Z_ASSERT_N(aper_encode(&buf, choice1, &in));
            ps = ps_initsb(&buf);
            Z_ASSERT_EQ(ps_len(&ps), 1u);
            Z_ASSERT_EQ(*ps.b, (i - 2) << 4);
            Z_ASSERT_N(t_aper_decode(&ps, choice1, false, &out));

            Z_ASSERT_EQ(in.iop_tag, out.iop_tag);
            Z_ASSERT_EQ(in.i, out.i);
        }
    } Z_TEST_END;

    /* }}} */
    /* {{{ Extended choice. */

    Z_TEST(extended_choice, "extended choice") {
        t_scope;
        struct {
            tstiop__asn1_ext_choice__t in;
            lstr_t aper_bytes;
        } tests[3];

        tstiop__asn1_ext_choice__t out;
        SB(buf, 42);
        pstream_t ps;

        tests[0].in = IOP_UNION(tstiop__asn1_ext_choice, i, 192);
        tests[0].aper_bytes = LSTR_IMMED_V("\x00\x00\x96");
        tests[1].in = IOP_UNION(tstiop__asn1_ext_choice, ext_s, LSTR("test"));
        tests[1].aper_bytes = LSTR_IMMED_V("\x80\x05\x04\x74\x65\x73\x74");
        tests[2].in = IOP_UNION(tstiop__asn1_ext_choice, ext_i, 667);
        tests[2].aper_bytes = LSTR_IMMED_V("\x81\x02\x00\x01");

        carray_for_each_ptr(t, tests) {
            sb_reset(&buf);
            Z_ASSERT_N(aper_encode(&buf, tstiop__asn1_ext_choice_, &t->in));
            Z_ASSERT_LSTREQUAL(t->aper_bytes, LSTR_SB_V(&buf));
            ps = ps_initsb(&buf);
            Z_ASSERT_N(t_aper_decode(&ps, tstiop__asn1_ext_choice_, false,
                                     &out));
            Z_ASSERT_IOPEQUAL(tstiop__asn1_ext_choice, &t->in, &out);
        }
    } Z_TEST_END;

    /* }}} */
    /* {{{ Extended sequence. */

    Z_TEST(extended_sequence, "extended sequence") {
        struct {
            const char *title;
            sequence1_t in;
            lstr_t encoding;
        } tests[] = { {
            "no extension",
            { OPT(10), -20, LSTR_NULL, OPT_NONE, OPT_NONE },
            LSTR_IMMED("\x64\x01\x16"),
        }, {
            "one extension",
            { OPT(10), -20, LSTR_IMMED("toto"), OPT_NONE, OPT_NONE },
            LSTR_IMMED("\xE4\x01\x16\x05\x00\x05\x04toto"),
        }, {
            "more extensions",
            { OPT(10), -20, LSTR_NULL, OPT(-90000), OPT(42) },
            LSTR_IMMED("\xE4\x01\x16\x04\xC0\x03\x40\x27\x10\x02\x00\x2A"),
        } };

        carray_for_each_ptr(t, tests) {
            Z_HELPER_RUN(z_test_seq_ext(&t->in, t->encoding),
                         "test failure for `%s`", t->title);
        }
    } Z_TEST_END;

    /* }}} */
    /* {{{ Integers overflow. */

    Z_TEST(ints_overflows, "integers overflows") {
        ints_seq_base_t base_min = {
            INT8_MIN, 0, INT16_MIN, 0, INT32_MIN, 0, INT64_MIN, 0, 0, 0,
        };
        ints_seq_base_t base_max = {
            INT8_MAX, UINT8_MAX, INT16_MAX, UINT16_MAX,
            INT32_MAX, UINT32_MAX, INT64_MAX, UINT64_MAX,
            0, 0,
        };
        ints_seq_base_t base;

        struct {
            const char *title;
            int64_t v;
            int64_t *base_field;
        } err_cases[] = {
#define TEST(x)                                                              \
    { "i" #x ", min - 1", (int64_t)INT##x##_MIN - 1, &base.i##x },           \
    { "i" #x ", max + 1", (int64_t)INT##x##_MAX + 1, &base.i##x },           \
    { "u" #x ", min - 1", (int64_t)-1, &base.u##x },                         \
    { "u" #x ", max + 1", (int64_t)UINT##x##_MAX + 1, &base.u##x }

            TEST(8),
            TEST(16),
            TEST(32),

#undef TEST

            /* XXX INT64_MIN - 1 is untestable this way */
            { "i64, max + 1", (uint64_t)INT64_MAX + 1,
                (int64_t *)&base.i64_bis },
            { "u64, min + 1", -1, &base.u64_bis },
            /* XXX UINT64_MAX + 1 is untestable this way */
        };

        Z_HELPER_RUN(z_translate_ints_seq(&base_min, false),
                     "unexected error on minimum values");
        Z_HELPER_RUN(z_translate_ints_seq(&base_max, false),
                     "unexected error on maximum values");

        p_clear(&base, 1);
        Z_HELPER_RUN(z_translate_ints_seq(&base, false),
                     "unexected error on zeros");

        carray_for_each_ptr(t, err_cases) {
            p_clear(&base, 1);
            *t->base_field = t->v;
            Z_HELPER_RUN(z_translate_ints_seq(&base, true),
                         "test `%s`: no overflow detection", t->title);
        }
    } Z_TEST_END;

    /* }}} */
    /* {{{ Enumerated. */

    Z_TEST(enumerated, "enumerated type check (mostly for auto-wipe)") {
        t_scope;
        SB_1k(buf);
        pstream_t ps;
        struct1_t s1[2];
        lstr_t expected_encoding = LSTR_IMMED("\x80");

        p_clear(&s1[0], 1);
        s1[0].e1 = BAR;

        Z_ASSERT_N(aper_encode(&buf, struct1, &s1[0]), "encoding failure");
        Z_ASSERT_LSTREQUAL(LSTR_SB_V(&buf), expected_encoding, "%*pX",
                           SB_FMT_ARG(&buf));
        ps = ps_initsb(&buf);
        Z_ASSERT_N(t_aper_decode(&ps, struct1, false, &s1[1]),
                   "decoding failure");
        Z_ASSERT_EQ(s1[1].e1, s1[0].e1);
    } Z_TEST_END;

    /* }}} */
    /* {{{ Fragmented octet string. */

    Z_TEST(fragmented_octet_string, "") {
        t_scope;
        sb_t buf;
        sb_t str;
        pstream_t str_ps;
        z_octet_string_t os_before;
        z_octet_string_t os_after;
        pstream_t ps;

        sb_init(&buf);
        sb_init(&str);
        for (int i = 0; i < 123456; i++) {
            sb_addc(&str, (char)i);
        }

        p_clear(&os_before, 1);
        os_before.str = LSTR_SB_V(&str);
        Z_ASSERT_N(aper_encode(&buf, z_octet_string, &os_before),
                   "unexpected failure");

        /* Check the fragmented encoding. */
        str_ps = ps_initsb(&str);
        ps = ps_initsb(&buf);
        /* First fragment: 4 x 16k. */
        Z_ASSERT_EQ(ps_getc(&ps), 0xc4);
        Z_HELPER_RUN(z_ps_skip_and_check_eq(&ps, &str_ps, (64 << 10)));
        /* Second fragment: 3 x 16k. */
        Z_ASSERT_EQ(ps_getc(&ps), 0xc3);
        Z_HELPER_RUN(z_ps_skip_and_check_eq(&ps, &str_ps, (48 << 10)));
        /* Remainder. */
        Z_ASSERT(ps_has(&ps, 2));
        Z_ASSERT_EQ(ps_len(&ps), ps_len(&str_ps) + 2);
        Z_ASSERT_EQ(get_unaligned_be16(ps.b), (0x8000 | ps_len(&str_ps)));
        __ps_skip(&ps, 2);
        Z_HELPER_RUN(z_ps_skip_and_check_eq(&ps, &str_ps, ps_len(&str_ps)));
        ps = ps_initsb(&buf);
        Z_ASSERT_N(t_aper_decode(&ps, z_octet_string, false, &os_after));
        Z_ASSERT_LSTREQUAL(os_before.str, os_after.str);

        /* Special case: all the data is in the fragments
         * (a single 16k fragment in this case). */
        os_before.str.len = (16 << 10);
        sb_reset(&buf);
        Z_ASSERT_N(aper_encode(&buf, z_octet_string, &os_before),
                   "unexpected failure");
        str_ps = ps_initlstr(&os_before.str);
        ps = ps_initsb(&buf);
        /* Fragment: 1 x 16k. */
        Z_ASSERT_EQ(ps_getc(&ps), 0xc1);
        Z_HELPER_RUN(z_ps_skip_and_check_eq(&ps, &str_ps, (16 << 10)));
        /* No remainder: just a octet with value == zero. */
        Z_ASSERT_EQ(ps_getc(&ps), 0x00);

        sb_wipe(&str);
        sb_wipe(&buf);
    } Z_TEST_END;

    /* }}} */
    /* {{{ Fragmented open type. */

    Z_TEST(fragmented_open_type, "") {
        t_scope;
        sb_t str;
        sb_t buf;
        sb_t os_buf;
        lstr_t motif = LSTR("OPEN TYPE-");
        z_open_type_t ot_before;
        z_open_type_t ot_after;
        pstream_t ps;
        pstream_t exp_ps;

        sb_init(&str);
        for (int i = 0; i < 20000; i++) {
            sb_addc(&str, motif.s[i % motif.len]);
        }

        p_clear(&ot_before, 1);
        ot_before.os.str = LSTR_SB_V(&str);

        sb_init(&buf);
        Z_ASSERT_N(aper_encode(&buf, z_open_type, &ot_before),
                   "unexpected failure");
        ps = ps_initsb(&buf);

        /* Encode the octet string twice: it should be the same as having the
         * octet string in an open type. */
        sb_init(&os_buf);
        sb_addsb(&os_buf, &str);
        for (int i = 0; i < 2; i++) {
            t_scope;
            z_octet_string_t os;

            p_clear(&os, 1);
            os.str = t_lstr_dup(LSTR_SB_V(&os_buf));
            sb_reset(&os_buf);
            Z_ASSERT_N(aper_encode(&os_buf, z_octet_string, &os),
                       "unexpected failure (supposedly already tested with "
                       "fragmented_octet_string)");
        }
        Z_ASSERT_LSTREQUAL(LSTR_SB_V(&buf), LSTR_SB_V(&os_buf));
        exp_ps = ps_initsb(&os_buf);
        Z_HELPER_RUN(z_ps_skip_and_check_eq(&ps, &exp_ps, ps_len(&exp_ps)));
        Z_ASSERT(ps_done(&ps));

        ps = ps_initsb(&buf);
        Z_ASSERT_N(t_aper_decode(&ps, z_open_type, false, &ot_after));
        Z_ASSERT_LSTREQUAL(ot_before.os.str, ot_after.os.str);
        sb_wipe(&str);
        sb_wipe(&buf);
        sb_wipe(&os_buf);
    } Z_TEST_END;

    /* }}} */
    /* {{{ Fragmented sequence of. */

    Z_TEST(fragmented_seq_of, "") {
        t_scope;
        z_seqof_t seq_of_before;
        qv_t(i8) vec;
        int seqof_len = 100000;
        int min = -3;
        int max = 3;
        SB_1k(buf);

        t_qv_init(&vec, seqof_len);
        for (int i = 0; i < seqof_len; i++) {
            qv_append(&vec, min + (i % (max - min + 1)));
        }

        z_seqof_init(&seq_of_before);
        seq_of_before.a = 1;
        seq_of_before.s.seqof = ASN1_VECTOR(ASN1_VECTOR_TYPE(int8),
                                            vec.tab, vec.len);
        Z_ASSERT_NEG(aper_encode(&buf, z_seqof, &seq_of_before));
    } Z_TEST_END;

    /* }}} */
} Z_GROUP_END
