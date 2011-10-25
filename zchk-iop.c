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

#include "z.h"
#include "iop.h"
#include "iop/tstiop.iop.h"

Z_GROUP_EXPORT(iop)
{
    Z_TEST(dso_open, "test wether iop_dso_open works and loads stuff") {
        t_scope;

        iop_dso_t *dso;
        lstr_t path = t_lstr_cat(z_cmddir_g,
                                 LSTR_IMMED_V("zchk-iop-plugin.so"));

        Z_ASSERT(dso = iop_dso_open(path.s));
        Z_ASSERT_N(qm_find(iop_struct, &dso->struct_h,
                           &LSTR_IMMED_V("ic.Hdr")));

        Z_ASSERT_P(iop_dso_find_type(dso, LSTR_IMMED_V("ic.SimpleHdr")));

        iop_dso_close(&dso);
    } Z_TEST_END;

    Z_TEST(hash_sha1, "test wether iop_hash_sha1 is stable wrt ABI change") {
        t_scope;

        struct tstiop__hash_v1__t v1 = {
            .i  = 10,
            .s  = LSTR_IMMED("foo bar baz"),
        };

        struct tstiop__hash_v2__t v2 = {
            .i  = 10,
            .s  = LSTR_IMMED("foo bar baz"),
        };

        struct tstiop__hash_v1__t v1_not_same = {
            .i  = 11,
            .s  = LSTR_IMMED("foo bar baz"),
        };

        const iop_struct_t *stv1;
        const iop_struct_t *stv2;

        iop_dso_t *dso;
        lstr_t path = t_lstr_cat(z_cmddir_g,
                                 LSTR_IMMED_V("zchk-tstiop-plugin.so"));
        uint8_t buf1[20], buf2[20];

        dso = iop_dso_open(path.s);
        if (dso == NULL)
            Z_SKIP("unable to load zchk-tstiop-plugin, TOOLS repo?");

        Z_ASSERT_P(stv1 = iop_dso_find_type(dso, LSTR_IMMED_V("tstiop.HashV1")));
        Z_ASSERT_P(stv2 = iop_dso_find_type(dso, LSTR_IMMED_V("tstiop.HashV2")));

        iop_hash_sha1(stv1, &v1, buf1);
        iop_hash_sha1(stv2, &v2, buf2);
        Z_ASSERT_EQUAL(buf1, sizeof(buf1), buf2, sizeof(buf2));

        iop_hash_sha1(stv1, &v1_not_same, buf2);
        Z_ASSERT(memcmp(buf1, buf2, sizeof(buf1)) != 0);
        iop_dso_close(&dso);
    } Z_TEST_END;
} Z_GROUP_END
