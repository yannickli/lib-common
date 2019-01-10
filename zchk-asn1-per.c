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

#include "asn1-per.h"
#include "z.h"
#include "iop.h"
#include "iop/tstiop.iop.h"

typedef struct {
    uint16_t iop_tag;
    union {
        int i;
    };
} choice1_t;

static __ASN1_IOP_CHOICE_DESC_BEGIN(desc, choice1);
    asn1_reg_scalar(desc, choice1, i, 0);
    asn1_set_int_min_max(desc, 2, 15);
ASN1_CHOICE_DESC_END(desc);

static __ASN1_IOP_CHOICE_DESC_BEGIN(desc, tstiop__asn1_ext_choice_);
    asn1_reg_scalar(desc, tstiop__asn1_ext_choice_, i, 0);
    asn1_set_int_min_max(desc, 42, 666);
    asn1_reg_extension(desc);
    asn1_reg_string(desc, tstiop__asn1_ext_choice_, ext_s, 1);
    asn1_reg_scalar(desc, tstiop__asn1_ext_choice_, ext_i, 2);
    asn1_set_int_min_max(desc, 666, 1234567);
ASN1_CHOICE_DESC_END(desc);

enum test_enum {
    A,
    B,
    C,
};

static ASN1_ENUM_BEGIN(test_enum);
    asn1_enum_reg_val(A);
    asn1_enum_reg_val(B);
    asn1_enum_reg_val(C);
ASN1_ENUM_END();

typedef struct {
    enum test_enum e;
} seq1_t;

static ASN1_SEQUENCE_DESC_BEGIN(desc, seq1);
    asn1_reg_enum(desc, seq1, test_enum, e, 0);
    asn1_set_enum_info(desc, test_enum);
ASN1_SEQUENCE_DESC_END(desc);

Z_GROUP_EXPORT(asn1_aper) {
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

    Z_TEST(extended_choice, "extended choice") {
        t_scope;
        struct {
            tstiop__asn1_ext_choice__t in;
            lstr_t aper_bytes;
        } tests[3];

        tstiop__asn1_ext_choice__t out;
        SB_1k(buf);
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

    Z_TEST(enum, "enum") {
        t_scope;
        SB_1k(buf);
        seq1_t seq = { .e = B };
        seq1_t out;
        lstr_t expected_encoding = LSTR_IMMED("\x40");
        pstream_t ps;

        Z_ASSERT_N(aper_encode(&buf, seq1, &seq));
        Z_ASSERT_LSTREQUAL(LSTR_SB_V(&buf), expected_encoding, "%*pX",
                           SB_FMT_ARG(&buf));
        ps = ps_initsb(&buf);
        Z_ASSERT_N(t_aper_decode(&ps, seq1, false, &out));
        Z_ASSERT_EQ(seq.e, out.e);
    } Z_TEST_END;
} Z_GROUP_END
