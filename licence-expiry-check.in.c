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

/** Converter of licence date from lstr_t to time_t.
 *
 * The value of out will be the timestamp corresponding to the given date
 * parameter, at midnight.
 *
 * \param[in]   s    String describing a date, for example `07-may-2014`.
 * \param[out]  out  The timestamp,
 *                   for example the timestamp of 07-05-2014 00:00:00.
 */
ATTRS
static int F(strtotime)(lstr_t s, time_t *out)
{
    struct tm t;

    RETHROW_PN(s.s);

    p_clear(&t, 1);
    t.tm_isdst = -1;
    RETHROW(strtotm(s.s, &t));

    *out = RETHROW(mktime(&t));

    return 0;
}

ATTRS
#ifdef ALL_STATIC
static
#endif
licence_expiry_t F(licence_check_iop_expiry)(const core__licence__t *licence)
{
    time_t expires_soon_ts, soft_expiration_ts, hard_expiration_ts;
    time_t now = lp_getsec();

    if (F(strtotime)(licence->expiration_date, &soft_expiration_ts) < 0) {
#ifndef NO_LOG
        e_error("invalid soft expiration date");
#endif
        return LICENCE_INVALID_EXPIRATION;
    }

    expires_soon_ts = soft_expiration_ts - licence->expiration_warning_delay;

    if (licence->expiration_hard_date.s) {
        if (F(strtotime)(licence->expiration_hard_date,
                      &hard_expiration_ts) < 0)
        {
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
