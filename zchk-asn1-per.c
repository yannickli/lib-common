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
} Z_GROUP_END
