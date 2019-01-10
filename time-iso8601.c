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

#include "time.h"

static int time_parse_iso8601_tok(pstream_t *ps, int *nb, int *type)
{
    *nb = ps_geti(ps);
    if (!ps_has(ps, 1)) {
        e_debug("invalid date tok");
        return -1;
    }
    *type = __ps_getc(ps);
    return 0;
}

int time_parse_iso8601(pstream_t *ps, time_t *res)
{
    struct tm t;
    bool local = false;
    int sign, tz_h, tz_m, i;

    if (!ps_has(ps, 1)) {
        e_debug("invalid date: empty");
        return -1;
    }

    if (*ps->s == 'P') {
        /* Relative date */
        int nb, tok;
        time_t now = lp_getsec();

        localtime_r(&now, &t);

        __ps_skip(ps, 1);
        if (ps_done(ps))
            goto end_rel;
        RETHROW(time_parse_iso8601_tok(ps, &nb, &tok));
        if (tok == 'Y') {
            t.tm_year += nb;
            if (ps_done(ps))
                goto end_rel;
            RETHROW(time_parse_iso8601_tok(ps, &nb, &tok));
        }
        if (tok == 'M') {
            t.tm_mon += nb;
            if (ps_done(ps))
                goto end_rel;
            RETHROW(time_parse_iso8601_tok(ps, &nb, &tok));
        }
        if (tok == 'D') {
            t.tm_mday += nb;
            if (ps_done(ps))
                goto end_rel;
            RETHROW(time_parse_iso8601_tok(ps, &nb, &tok));
        }
        if (tok != 'T') {
            e_debug("missing 'T' time mark");
            return -1;
        }
        RETHROW(time_parse_iso8601_tok(ps, &nb, &tok));
        if (tok == 'H') {
            t.tm_hour += nb;
            if (ps_done(ps))
                goto end_rel;
            RETHROW(time_parse_iso8601_tok(ps, &nb, &tok));
        }
        if (tok == 'M') {
            t.tm_min += nb;
            if (ps_done(ps))
                goto end_rel;
            RETHROW(time_parse_iso8601_tok(ps, &nb, &tok));
        }
        if (tok == 'S') {
            t.tm_sec += nb;
        }

      end_rel:
        t.tm_isdst = -1;
        *res = mktime(&t);
        return 0;
    }

    p_clear(&t, 1);

    t.tm_year = ps_geti(ps) - 1900;
    if (t.tm_year <= 0 || t.tm_year > 200) {
        e_debug("invalid year in date");
        return -1;
    }
    if (ps_getc(ps) != '-') {
        e_debug("invalid date format, missing '-' after year");
        return -1;
    }

    t.tm_mon = ps_geti(ps) - 1;
    if (t.tm_mon < 0 || t.tm_mon > 11) {
        e_debug("invalid month in date");
        return -1;
    }
    if (ps_getc(ps) != '-') {
        e_debug("invalid date format, missing '-' after month");
        return -1;
    }

    t.tm_mday = ps_geti(ps);
    if (t.tm_mday <= 0 || t.tm_mday > 31) {
        e_debug("invalid day in date");
        return -1;
    }
    if (ps_getc(ps) != 'T') {
        e_debug("invalid date format, missing 'T' after day");
        return -1;
    }

    t.tm_hour = ps_geti(ps);
    switch (ps_getc(ps)) {
      case 'L':
        local = true;
        break;

      case ':':
        break;

      default:
        e_debug("invalid date format, invalid char after hour");
        return -1;
    }

    t.tm_min = ps_geti(ps);
    if (ps_getc(ps) != ':') {
        e_debug("invalid date format, missing ':' after minutes");
        return -1;
    }

    t.tm_sec = ps_geti(ps);
    if (ps_startswith(ps, ".", 1)) { /* Ignore it */
        ps_getc(ps);
        ps_geti(ps);
    }

    if (local) {
        t.tm_isdst = -1;
        *res = mktime(&t);
        return 0;
    }
    switch ((i = ps_getc(ps))) {
      case 'Z':
        t.tm_isdst = 0;
        *res = timegm(&t);
        return 0;

      case '-':
        sign = -1;
        break;

      case '+':
        sign = 1;
        break;

      case -1:
        t.tm_isdst = -1;
        *res = mktime(&t);
        return 0;

      default:
        e_debug("invalid date format, invalid timezone char %i", i);
        return -1;
    }

    tz_h = ps_geti(ps);
    if (ps_getc(ps) != ':') {
        e_debug("invalid date format, missing ':' after TZ hour");
        return -1;
    }
    tz_m = ps_geti(ps);

    /* subtract the offset from the local time to get UTC time */
    t.tm_hour -= sign * tz_h;
    t.tm_min  -= sign * tz_m;
    t.tm_isdst = 0;
    *res = timegm(&t);
    return 0;
}
