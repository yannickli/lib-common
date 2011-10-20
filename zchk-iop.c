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
} Z_GROUP_END
