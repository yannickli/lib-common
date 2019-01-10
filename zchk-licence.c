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

#include "z.h"
#include "licence.h"
#include "iop/tstiop_licence.iop.h"

static void t_ts_to_lstr(time_t ts, lstr_t *out)
{
    char *date;

    date = t_new(char, 20);
    if (strftime(date, 20, "%d-%b-%Y", localtime(&ts)) != 0) {
        *out = LSTR(date);
    } else {
        *out = LSTR_EMPTY_V;
    }
}

Z_GROUP_EXPORT(licence)
{
    IOP_REGISTER_PACKAGES(&tstiop_licence__pkg, &core__pkg);

    Z_TEST(iop, "") {
        t_scope;
        SB_1k(tmp);
        unsigned flags = IOP_HASH_SKIP_MISSING | IOP_HASH_SKIP_DEFAULT;
        core__signed_licence__t lic;

#define Z_LOAD_LICENCE(path) \
    Z_ASSERT_N(t_iop_junpack_file(path, &core__signed_licence__s, &lic, \
                                  0, NULL, &tmp));

        Z_ASSERT_N(chdir(z_cmddir_g.s));
        Z_LOAD_LICENCE("samples/licence-iop-ok.cf");
        Z_ASSERT_N(licence_check_iop(&lic, LSTR("1.0"), flags));
        Z_ASSERT_N(licence_check_iop(&lic, LSTR("2.0"), flags));

        Z_LOAD_LICENCE("samples/licence-iop-sig-ko.cf");
        Z_ASSERT_NEG(licence_check_iop(&lic, LSTR("1.0"), flags));

        /* Licence expired. */
        Z_LOAD_LICENCE("samples/licence-iop-exp-ko.cf");
        Z_ASSERT_N(iop_check_signature(&core__licence__s, lic.licence,
                                       lic.signature, flags));
        Z_ASSERT_EQ(licence_check_iop_expiry(lic.licence),
                    LICENCE_HARD_EXPIRED);

        /* Invalid expiration date : "02-aaa-2012" */
        Z_LOAD_LICENCE("samples/licence-iop-exp-invalid.cf");
        Z_ASSERT_EQ(licence_check_iop_expiry(lic.licence),
                    LICENCE_INVALID_EXPIRATION);

        /* Invalid expiration date : soft expiration > hard expiration */
        t_ts_to_lstr(time(NULL)+(3 * 24 * 3600), &lic.licence->expiration_date);
        t_ts_to_lstr(time(NULL)-(3 * 24 * 3600),
                     &lic.licence->expiration_hard_date);
        Z_ASSERT_EQ(licence_check_iop_expiry(lic.licence),
                    LICENCE_INVALID_EXPIRATION);

        t_ts_to_lstr(time(NULL)+(3 * 24 * 3600), &lic.licence->expiration_date);
        t_ts_to_lstr(time(NULL)+(3 * 24 * 3600),
                     &lic.licence->expiration_hard_date);
        lic.licence->expiration_warning_delay = 7 * 24 * 3600;
        Z_ASSERT_N(licence_check_iop(&lic, LSTR("1.0"), flags));
        Z_ASSERT_EQ(licence_check_iop_expiry(lic.licence),
                    LICENCE_EXPIRES_SOON);

        t_ts_to_lstr(time(NULL)-(3 * 24 * 3600), &lic.licence->expiration_date);
        t_ts_to_lstr(time(NULL)+(3 * 24 * 3600),
                   &lic.licence->expiration_hard_date);
        Z_ASSERT_N(licence_check_iop(&lic, LSTR("1.0"), flags));
        Z_ASSERT_EQ(licence_check_iop_expiry(lic.licence),
                    LICENCE_SOFT_EXPIRED);

        t_ts_to_lstr(time(NULL)-(3 * 24 * 3600), &lic.licence->expiration_date);
        t_ts_to_lstr(time(NULL)-(3 * 24 * 3600),
                   &lic.licence->expiration_hard_date);
        Z_ASSERT_EQ(licence_check_iop_expiry(lic.licence),
                    LICENCE_HARD_EXPIRED);

#define Z_MODULE_ACTIVATION(module, result) \
    Z_ASSERT_EQ(licence_is_module_activated_desc(lic.licence, \
                                                 module), result);

        Z_LOAD_LICENCE("samples/licence-iop-ok-mod1.cf");
        Z_ASSERT_N(licence_check_modules(lic.licence));
        /* Expiration date OK */
        Z_MODULE_ACTIVATION(&tstiop_licence__licence_tst1__s, true);
        /* Expiration date KO */
        Z_MODULE_ACTIVATION(&tstiop_licence__licence_tst2__s, false);
        /* Module not declared in the licence */
        Z_MODULE_ACTIVATION(&tstiop_licence__licence_tst3__s, false);

#define Z_MODULE_TYPE(_lic, _type)                                          \
    do {                                                                    \
        Z_ASSERT_P(licence_get_module(_lic, _type));                        \
        Z_ASSERT(iop_obj_is_a(licence_get_module(_lic, _type), _type));     \
    } while (0)

        Z_MODULE_TYPE(lic.licence, tstiop_licence__licence_tst1);
        Z_MODULE_TYPE(lic.licence, tstiop_licence__licence_tst2);
        Z_ASSERT_NULL(licence_get_module(lic.licence,
                                         tstiop_licence__licence_tst3));

#undef Z_MODULE_TYPE

        lic.licence->modules.tab[0]->expiration_warning_delay = 3 * 24 * 3600;

        t_ts_to_lstr(time(NULL) + (5 * 24 * 3600),
                     &lic.licence->modules.tab[0]->expiration_date);
        Z_ASSERT_EQ(licence_check_module_expiry(lic.licence->modules.tab[0]),
                    LICENCE_OK);

        t_ts_to_lstr(time(NULL) + (2 * 24 * 3600),
                     &lic.licence->modules.tab[0]->expiration_date);
        Z_ASSERT_EQ(licence_check_module_expiry(lic.licence->modules.tab[0]),
                    LICENCE_EXPIRES_SOON);

        t_ts_to_lstr(time(NULL) - (3 * 24 * 3600),
                     &lic.licence->modules.tab[0]->expiration_date);
        Z_ASSERT_EQ(licence_check_module_expiry(lic.licence->modules.tab[0]),
                    LICENCE_HARD_EXPIRED);

        Z_LOAD_LICENCE("samples/licence-iop-ok-mod2.cf");
        Z_ASSERT_N(licence_check_modules(lic.licence));
        /* No expiration date for that module */
        Z_MODULE_ACTIVATION(&tstiop_licence__licence_tst1__s, true);

        Z_LOAD_LICENCE("samples/licence-iop-ko-double-mod.cf");
        Z_ASSERT_NEG(licence_check_modules(lic.licence));

        Z_LOAD_LICENCE("samples/licence-iop-ko-cant-exp.cf");
        Z_ASSERT_NEG(licence_check_modules(lic.licence));
    } Z_TEST_END;

} Z_GROUP_END
