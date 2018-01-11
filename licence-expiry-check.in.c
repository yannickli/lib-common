/**************************************************************************/
/*                                                                        */
/*  Copyright (C) 2004-2017 INTERSEC SA                                   */
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
licence_expiry_t F(licence_check_iop_expiry)(const core__licence__t *licence)
{
    time_t expires_soon_ts, soft_expiration_ts, hard_expiration_ts;
    time_t now = lp_getsec();

    soft_expiration_ts = lstrtotime(licence->expiration_date);
    if (soft_expiration_ts < 0) {
#ifndef NO_LOG
        e_error("invalid soft expiration date");
#endif
        return LICENCE_INVALID_EXPIRATION;
    }

    expires_soon_ts = soft_expiration_ts - licence->expiration_warning_delay;

    if (licence->expiration_hard_date.s) {
        hard_expiration_ts = lstrtotime(licence->expiration_hard_date);
        if (hard_expiration_ts < 0) {
#ifndef NO_LOG
            e_error("invalid hard expiration date");
#endif
            return LICENCE_INVALID_EXPIRATION;
        }
    } else {
        hard_expiration_ts = soft_expiration_ts;
    }

    if (soft_expiration_ts > hard_expiration_ts) {
#ifndef NO_LOG
        e_error("invalid expiration dates, expiration date must "
                "be before hard expiration date");
#endif
        return LICENCE_INVALID_EXPIRATION;
    }

    if (hard_expiration_ts < now) {
#ifndef NO_LOG
        e_error("licence expired");
#endif
        return LICENCE_HARD_EXPIRED;
    } else
    if (soft_expiration_ts < now) {
#ifndef NO_LOG
        e_error("licence expired, your product is going to be disabled soon");
#endif
        return LICENCE_SOFT_EXPIRED;
    } else
    if (expires_soon_ts < now) {
#ifndef NO_LOG
        e_warning("licence expires soon");
#endif
        return LICENCE_EXPIRES_SOON;
    } else {
        return LICENCE_OK;
    }
}
