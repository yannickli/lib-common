/**************************************************************************/
/*                                                                        */
/*  Copyright (C) 2004-2019 INTERSEC SA                                   */
/*                                                                        */
/*  Should you receive a copy of this source code, you must check you     */
/*  have a proper, written authorization of INTERSEC to hold it. If you   */
/*  don't have such an authorization, you must DELETE all source code     */
/*  files in your possession, and inform INTERSEC of the fact you obtain  */
/*  these files. Should you not comply to these terms, you can be         */
/*  prosecuted in the extent permitted by applicable law.                 */
/*                                                                        */
/**************************************************************************/

ATTRS
#ifdef ALL_STATIC
static
#endif
licence_expiry_t
F(licence_check_iop_expiry)(const core__signed_licence__t *slicence)
{
    const core__licence__t *licence = slicence->licence;
    const core__activation_token__t *token = slicence->activation_token;
    time_t expires_soon_ts, soft_expiration_ts, hard_expiration_ts;
    time_t activation_exp_ts, activation_exp_soon_ts;
    time_t now = lp_getsec();
    licence_expiry_t result = LICENCE_OK;
#ifndef NO_LOG
    SB_1k(log);
# define LOGIF(s)  sb_adds(&log, s)
#else
# define LOGIF(s)  (void)0
#endif

    soft_expiration_ts = lstrtotime(licence->expiration_date);
    if (soft_expiration_ts < 0) {
        LOGIF("invalid soft expiration date");
        result = LICENCE_INVALID_EXPIRATION;
        goto end;
    }

    expires_soon_ts = soft_expiration_ts - licence->expiration_warning_delay;

    if (licence->expiration_hard_date.s) {
        hard_expiration_ts = lstrtotime(licence->expiration_hard_date);
        if (hard_expiration_ts < 0) {
            LOGIF("invalid hard expiration date");
            result = LICENCE_INVALID_EXPIRATION;
            goto end;
        }
    } else {
        hard_expiration_ts = soft_expiration_ts;
    }

    if (token) {
        t_scope;

        if (t_parse_activation_token(token->token, licence,
                                     &activation_exp_ts, NULL, NULL,
                                     NULL) == -1)
        {
            LOGIF("invalid activation expiration date");
            result = LICENCE_INVALID_EXPIRATION;
            goto end;
        }
        activation_exp_soon_ts = localtime_addday(activation_exp_ts, -7);
    } else {
        activation_exp_ts = hard_expiration_ts;
        activation_exp_soon_ts = hard_expiration_ts;
    }

    if (soft_expiration_ts > hard_expiration_ts) {
        LOGIF("invalid expiration dates, expiration date must "
              "be before hard expiration date");
        result = LICENCE_INVALID_EXPIRATION;
        goto end;
    }

    if (activation_exp_ts > hard_expiration_ts) {
        LOGIF("invalid expiration dates, activation expiration date must "
              "be before hard expiration date");
        result = LICENCE_INVALID_EXPIRATION;
        goto end;
    }

    if (hard_expiration_ts < now) {
        LOGIF("licence expired");
        result = LICENCE_HARD_EXPIRED;
    } else
    if (activation_exp_ts < now) {
        LOGIF("activation token expired");
        result = LICENCE_TOKEN_EXPIRED;
    } else
    if (soft_expiration_ts < now) {
        LOGIF("licence expired, your product is going to be disabled soon");
        if (activation_exp_soon_ts < now) {
            LOGIF(" and ");
            if (activation_exp_ts < hard_expiration_ts) {
                LOGIF("(URGENT) ");
            }
            LOGIF("activation token expires soon");
        }
        result = LICENCE_SOFT_EXPIRED;
    } else
    if (expires_soon_ts < now) {
        LOGIF("licence expires soon");
        if (activation_exp_soon_ts < now) {
            LOGIF(", activation token too");
            result = (soft_expiration_ts < activation_exp_ts) ?
                LICENCE_EXPIRES_SOON : LICENCE_TOKEN_EXPIRES_SOON;
        } else {
            result = LICENCE_EXPIRES_SOON;
        }
    } else {
        if (activation_exp_soon_ts < now) {
            LOGIF("activation token expires soon");
            result = LICENCE_TOKEN_EXPIRES_SOON;
        }
    }

  end:
#ifndef NO_LOG
    if (result > 0) {
        e_warning("%s", log.data);
    } else
    if (result < 0) {
        e_error("%s", log.data);
    }
#endif
    return result;
}
