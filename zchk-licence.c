/***************************************************************************/
/*                                                                         */
/* Copyright 2019 INTERSEC SA                                              */
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

#include "z.h"
#include "licence.h"
#include "iop-json.h"
#include "datetime.h"
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

        /* Licence expired. */
        Z_LOAD_LICENCE("samples/licence-iop-exp-ko.cf");
        Z_ASSERT_N(iop_check_signature(&core__licence__s, lic.licence,
                                       lic.signature, flags));
        Z_ASSERT_EQ(licence_check_iop_expiry(&lic), LICENCE_HARD_EXPIRED);

        /* Invalid expiration date : "02-aaa-2012" */
        Z_LOAD_LICENCE("samples/licence-iop-exp-invalid.cf");
        Z_ASSERT_EQ(licence_check_iop_expiry(&lic),
                    LICENCE_INVALID_EXPIRATION);

        /* Invalid expiration date : soft expiration > hard expiration */
        t_ts_to_lstr(time(NULL)+(3 * 24 * 3600), &lic.licence->expiration_date);
        t_ts_to_lstr(time(NULL)-(3 * 24 * 3600),
                     &lic.licence->expiration_hard_date);
        Z_ASSERT_EQ(licence_check_iop_expiry(&lic),
                    LICENCE_INVALID_EXPIRATION);

        t_ts_to_lstr(time(NULL)+(3 * 24 * 3600), &lic.licence->expiration_date);
        t_ts_to_lstr(time(NULL)+(3 * 24 * 3600),
                     &lic.licence->expiration_hard_date);
        lic.licence->expiration_warning_delay = 7 * 24 * 3600;
        Z_ASSERT_EQ(licence_check_iop_expiry(&lic), LICENCE_EXPIRES_SOON);

        t_ts_to_lstr(time(NULL)-(3 * 24 * 3600), &lic.licence->expiration_date);
        t_ts_to_lstr(time(NULL)+(3 * 24 * 3600),
                   &lic.licence->expiration_hard_date);
        Z_ASSERT_EQ(licence_check_iop_expiry(&lic), LICENCE_SOFT_EXPIRED);

        t_ts_to_lstr(time(NULL)-(3 * 24 * 3600), &lic.licence->expiration_date);
        t_ts_to_lstr(time(NULL)-(3 * 24 * 3600),
                   &lic.licence->expiration_hard_date);
        Z_ASSERT_EQ(licence_check_iop_expiry(&lic), LICENCE_HARD_EXPIRED);

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

Z_GROUP_EXPORT(activation_token)
{
    IOP_REGISTER_PACKAGES(&tstiop_licence__pkg, &core__pkg);

    Z_TEST(iop, "") {
        t_scope;
        SB_1k(tmp);
        unsigned flags = IOP_HASH_SKIP_MISSING | IOP_HASH_SKIP_DEFAULT;
        core__signed_licence__t lic;
        core__activation_token__t token;
        core__activation_token__t new_token;
        time_t now = lp_getsec();
        time_t sigt;
        time_t day = 60 * 60 * 24;
        time_t prod_exp, token_exp;

        Z_ASSERT_N(chdir(z_cmddir_g.s));

        Z_LOAD_LICENCE("samples/licence-iop-2016.4-ok.cf");
        sigt = OPT_VAL(lic.licence->signature_ts);

        /* Check activation token formatting and parsing. */
        {
            t_scope;

            Z_ASSERT_N(iop_check_signature(&core__licence__s, lic.licence,
                                           lic.signature, flags));
            Z_ASSERT_EQ(licence_check_iop_expiry(&lic), LICENCE_OK);

            /* Check correctness of format and parse functions. */
            Z_ASSERT_N(t_format_activation_token(&lic, now + 2 * day,
                                                 now + 1 * day, &new_token,
                                                 &tmp));
            Z_ASSERT_N(t_parse_activation_token(new_token.token, lic.licence,
                                                &prod_exp, &token_exp, &token,
                                                NULL));
            Z_ASSERT(iop_equals(core__activation_token, &new_token, &token));
            Z_ASSERT_EQ(localtime_curday(now + 2 * day),
                        localtime_curday(prod_exp));
            Z_ASSERT_EQ(localtime_curday(now + 1 * day),
                        localtime_curday(token_exp));
            Z_ASSERT_EQ(prod_exp, lstrtotime(token.expiration_date));

            /* Tricky cases. */
            /* Check token upper bound (valid). */
            Z_ASSERT_N(t_format_activation_token(&lic,
                                                 localtime_curday(sigt)
                                                 + (64 * 64) * day - 1,
                                                 sigt + 1 * day, &new_token,
                                                 &tmp));
            /* Check token upper bound (invalid). We use 3600 instead of 0
             * because there can be some daylight saving time delay...
             */
            Z_ASSERT_NEG(t_format_activation_token(&lic,
                                                   localtime_curday(sigt)
                                                   + (64 * 64) * day + 3600,
                                                   sigt + 1 * day,
                                                   &new_token, &tmp));
            /* Another token upper bounds. (Think that tokens are
             * midnight-aligned.)
             */
            Z_ASSERT_N(t_format_activation_token(&lic,
                                                 sigt + (64 * 64 - 1) * day,
                                                 sigt + 1 * day, &new_token,
                                                 &tmp));
            Z_ASSERT_NEG(t_format_activation_token(&lic,
                                                   sigt + (64 * 64) * day,
                                                   sigt + 1 * day,
                                                   &new_token, &tmp));

            /* Token too small. */
            Z_ASSERT_NEG(t_parse_activation_token(LSTR("hheehEiR43Knf5Hd2"),
                                                  lic.licence, NULL, NULL,
                                                  NULL, NULL));
            /* Token can be parsed, but hash is not valid. */
            Z_ASSERT_EQ(t_parse_activation_token(LSTR("////hEiR43Knf5HAAAAA"),
                                                 lic.licence, NULL, NULL,
                                                 &token, NULL), -2);
            lic.activation_token = &token;
            Z_ASSERT_EQ(licence_check_iop_expiry(&lic), LICENCE_OK);

            /* Check values around the signature ts of the licence. */
            OPT_SET(lic.licence->signature_ts, now + 10 * day);
            Z_ASSERT_NEG(t_format_activation_token(&lic, now + 10 * day,
                                                   now + 10 * day,
                                                   &new_token, &tmp));
            Z_ASSERT_NEG(t_format_activation_token(&lic, now + 11 * day,
                                                   now + 1 * day,
                                                   &new_token, &tmp));
            Z_ASSERT_NEG(t_format_activation_token(&lic, now + 11 * day,
                                                   now + 10 * day,
                                                   &new_token, &tmp));

        }

        /* reset licence */
        OPT_SET(lic.licence->signature_ts, sigt);

        {
            /* Check expiry with a licence which expires soon. */
            struct {
                time_t les; /* Licence expires soon */
                time_t lse; /* Licence soft expiration */
                time_t lhe; /* Licence hard expiration */
                time_t te;  /* Activation token expiration */
                enum licence_expiry_t expected;
                /* The struct is initilized with "now" as reference,
                   all data expressed in number of days */
            } tests[] = {{  4, 40, 60, 10, LICENCE_OK },
                         {  4, 40, 60,  5, LICENCE_TOKEN_EXPIRES_SOON },
                         {  4, 40, 60, -2, LICENCE_TOKEN_EXPIRED },
                         {-10,  4, 30, 10, LICENCE_EXPIRES_SOON },
                         {-10,  4, 30,  5, LICENCE_EXPIRES_SOON },
                         {-10,  4, 30,  4, LICENCE_TOKEN_EXPIRES_SOON },
                         {-10,  4, 30,  3, LICENCE_TOKEN_EXPIRES_SOON },
                         {-10,  4, 30, -2, LICENCE_TOKEN_EXPIRED },
                         {-30,-10,  4, 10, LICENCE_INVALID_EXPIRATION },
                         {-30,-10,  4,  4, LICENCE_SOFT_EXPIRED },
                         {-30,-10,  4,  3, LICENCE_SOFT_EXPIRED },
                         {-30,-10,  4, -2, LICENCE_TOKEN_EXPIRED },
                         {-50,-30,-10, 10, LICENCE_INVALID_EXPIRATION },
                         {-50,-30,-10,  5, LICENCE_INVALID_EXPIRATION },
                         {-50,-30,-10, -2, LICENCE_INVALID_EXPIRATION },
                         {-50,-30,-10,-20, LICENCE_HARD_EXPIRED },
            };

            /* checks without tokens */
            lic.activation_token = NULL;
            for (int i = 0; i < countof(tests); i++) {
                t_scope;
                time_t les = now + tests[i].les * day;
                time_t lse = now + tests[i].lse * day;
                time_t lhe = now + tests[i].lhe * day;

                lic.licence->expiration_warning_delay = lse - les;
                t_ts_to_lstr(lse, &lic.licence->expiration_date);
                t_ts_to_lstr(lhe, &lic.licence->expiration_hard_date);
                Z_ASSERT_EQ(licence_check_iop_expiry(&lic),
                            lhe < now ? LICENCE_HARD_EXPIRED :
                            lse < now ? LICENCE_SOFT_EXPIRED :
                            les < now ? LICENCE_EXPIRES_SOON :
                            LICENCE_OK,
                            "check without token at index %d {%ld, ..., %ld}",
                            i, tests[i].les, tests[i].te);
            }

            /* checks with tokens */
            for (int i = 0; i < countof(tests); i++) {
                t_scope;
                time_t les = now + tests[i].les * day;
                time_t lse = now + tests[i].lse * day;
                time_t lhe = now + tests[i].lhe * day;
                time_t te  = now + tests[i].te  * day;
                enum licence_expiry_t expected = tests[i].expected;

                lic.licence->expiration_warning_delay = lse - les;
                t_ts_to_lstr(lse, &lic.licence->expiration_date);
                t_ts_to_lstr(lhe, &lic.licence->expiration_hard_date);
                Z_ASSERT_N(t_format_activation_token(&lic, te, te - 1,
                                                     &new_token, &tmp));
                Z_ASSERT_N(t_parse_activation_token(new_token.token,
                                                    lic.licence, NULL, NULL,
                                                    &token, NULL));
                lic.activation_token = &token;
                Z_ASSERT_EQ(licence_check_iop_expiry(&lic), expected,
                            "check with token at index %d {%ld, ..., %ld}",
                            i, tests[i].les, tests[i].te);
            }
        }
    } Z_TEST_END;

} Z_GROUP_END
