/**************************************************************************/
/*                                                                        */
/*  Copyright (C) 2004-2007 INTERSEC SAS                                  */
/*                                                                        */
/*  Should you receive a copy of this source code, you must check you     */
/*  have a proper, written authorization of INTERSEC to hold it. If you   */
/*  don't have such an authorization, you must DELETE all source code     */
/*  files in your possession, and inform INTERSEC of the fact you obtain  */
/*  these files. Should you not comply to these terms, you can be         */
/*  prosecuted in the extent permitted by applicable law.                 */
/*                                                                        */
/**************************************************************************/

#include <stdio.h>
#include <sys/time.h>
#include <time.h>
#include <ctype.h>

#include "mem.h"
#include "err_report.h"
#include "timeval.h"
#include "string_is.h"

/* Arithmetics on timeval assume both members of timeval are signed.
 * We keep timeval structures in normalized form:
 * - tv_usec is always in the range [0 .. 999999]
 * - tv_sec is negative iff timeval is negative
 * This choice is consistent with sys/time.h macros,
 * but it makes it a little bit harder to print timevals.
 */

/* Return reference to static buf for immediate printing */
const char *timeval_format(struct timeval tv, bool as_duration)
{
    // FIXME: assuming 32 bit timeval resolution, bug in 2034
    static char buf[32];
    int pos, d, h, m, s, sec, usec;

    usec = tv.tv_usec;

    if (as_duration) {
        pos = 0;
        sec = tv.tv_sec;
        if (sec < 0) {
            buf[pos++] = '-';
            sec = -sec;
            if (usec != 0) {
                usec = 1000 * 1000 - usec;
                sec -= 1;
            }
        }
        s = sec % 60;
        sec /= 60;
        m = sec % 60;
        sec /= 60;
        h = sec % 24;
        sec /= 24;
        d = sec;
        snprintf(buf + pos, sizeof(buf) - pos,
                 "%d %2d:%02d:%02d.%06d", d, h, m, s, usec);
    } else {
        struct tm t;

        localtime_r(&tv.tv_sec, &t);
        snprintf(buf, sizeof(buf),
                 "%02d/%02d/%04d %2d:%02d:%02d.%06d",
                 t.tm_mday, t.tm_mon + 1, t.tm_year + 1900,
                 t.tm_hour, t.tm_min, t.tm_sec, usec);
    }
    return buf;
}

struct timeval timeval_add(struct timeval a, struct timeval b)
{
    struct timeval res;

    res.tv_sec = a.tv_sec + b.tv_sec;
    res.tv_usec = a.tv_usec + b.tv_usec;
    if (res.tv_usec >= 1000 * 1000) {
        res.tv_sec += 1;
        res.tv_usec -= 1000 * 1000;
    }
    return res;
}

struct timeval timeval_sub(struct timeval a, struct timeval b)
{
    struct timeval res;
    int usec;

    res.tv_sec = a.tv_sec - b.tv_sec;
    usec = a.tv_usec - b.tv_usec;
    if (usec < 0) {
        res.tv_sec -= 1;
        usec += 1000 * 1000;
    }
    res.tv_usec = usec;
    return res;
}

struct timeval timeval_mul(struct timeval tv, int k)
{
    struct timeval res;
    int64_t usecs;

    /* Casting to int64_t is necessary because of potential overflow */
    /* Grouping 1000 * 1000 may yield better code */
    usecs  = (int64_t)tv.tv_sec * (1000 * 1000) + tv.tv_usec;
    usecs *= k;

    res.tv_sec  = usecs / (1000 * 1000);
    res.tv_usec = usecs - (int64_t)res.tv_sec * (1000 * 1000);
    if (res.tv_usec < 0) {
        res.tv_usec += 1000 * 1000;
        res.tv_sec  -= 1;
    }

    return res;
}

struct timeval timeval_div(struct timeval tv, int k)
{
    struct timeval res;
    int64_t usecs;

    usecs  = (int64_t)tv.tv_sec * (1000 * 1000) + tv.tv_usec;
    usecs /= k;

    res.tv_sec  = usecs / (1000 * 1000);
    res.tv_usec = usecs - (int64_t)res.tv_sec * (1000 * 1000);
    if (res.tv_usec < 0) {
        res.tv_usec += 1000 * 1000;
        res.tv_sec  -= 1;
    }

    return res;
}

/* Test if timer has expired: if now is provided, test if date is past
 * now, else test if date is past current time of day.  compute
 * available time left and return it if left is not NULL.
 */
bool is_expired(const struct timeval *date,
                const struct timeval *now,
                struct timeval *left)
{
    struct timeval local_now;
    struct timeval local_left;

    if (!now) {
        now = &local_now;
        gettimeofday(&local_now, NULL);
    }
    if (!left) {
        left = &local_left;
    }
    e_trace(3, "is_expired({%s}) ?", timeval_format(*date, false));
    *left = timeval_sub(*date, *now);
    if ((left->tv_sec < 0) || (left->tv_sec == 0 && left->tv_usec == 0)) {
        return true;
    }
    return false;
}

time_t localtime_curday(time_t date)
{
    struct tm t;

    if (date == 0) {
        date = time(NULL);
    }

    if (!localtime_r(&date, &t)) {
        return -1;
    }

    t.tm_sec = 0;
    t.tm_min = 0;
    t.tm_hour = 0;

    return mktime(&t);
}

time_t localtime_nextday(time_t date)
{
    struct tm t;
    
    if (date == 0) {
        date = time(NULL);
    }

    if (!localtime_r(&date, &t)) {
        return -1;
    }

    /* To avoid complex calculations, we rely on mktime() to normalize
     * the tm structure as specified in the man page:
     *
     * "The mktime() function converts a broken-down time structure,
     * expressed as local time, to calendar time representation.  The
     * function ignores the specified contents of the structure members
     * tm_wday and tm_yday and recomputes them from the other
     * information in the broken-down time structure.  If structure
     * members are outside their legal interval, they will be
     * normalized (so that, e.g., 40 October is changed into 9 Novem-
     * ber).  Calling mktime() also sets the external variable tzname
     * with information about the current time zone.  If the specified
     * broken-down time cannot be represented as calendar time (seconds
     * since the epoch), mktime() returns a value of (time_t)(-1) and
     * does not alter the tm_wday and tm_yday members of the
     * broken-down time structure."
     */

    t.tm_sec = 0;
    t.tm_min = 0;
    t.tm_hour = 0;
    t.tm_mday += 1;

    return mktime(&t);
}

static const char * const __abbr_months[] = {
    "jan",
    "feb",
    "mar",
    "apr",
    "may",
    "jun",
    "jul",
    "aug",
    "sep",
    "oct",
    "nov",
    "dec",
    NULL
};

static int const __valid_mdays[] = {
    31,
    28,
    31,
    30,
    31,
    30, /* June */
    31,
    31,
    30,
    31, /* October */
    30,
    31
};

/* We currently support only this format: DD-MMM-[YY]YY */
/* WARNING: Only date related tm structure fields are changed, others
 * must be initialized before this call.
 */
/* OG: why not use strptime ? */
int strtotm(const char *date, struct tm *t)
{
    int mday, mon, year;
    const char *p;
    char lower_mon[4];
    size_t len = strlen(date);

    if (len < strlen("DD-MMM-YY"))
        return -1;

    /* Read day */
    mday = cstrtol(date, &p, 10);
    if (mday <= 0)
        return -1;

    /* Read month */
    if (!p || *p != '-' || p - date != 2)
        return -1;
    p++;

    lower_mon[0] = tolower(p[0]);
    lower_mon[1] = tolower(p[1]);
    lower_mon[2] = tolower(p[2]);
    lower_mon[3] = '\0';

    for (mon = 0;; mon++) {
        if (!__abbr_months[mon])
            return -1;
        if (!memcmp(lower_mon, __abbr_months[mon], 4))
            break;
    }
    
    /* Read year */
    p += 3;
    if (*p++ != '-')
        return -1;

    year = atoi(p);
    if (year < 0)
        return -1;

    if (year < 70) {
        year += 2000;
    } else
    if (year < 100) {
        year += 1900;
    }
    if (year < 1970 || year > 2036)
        return -1;

    /* Simplistic leap year check because 1900 < year < 2100 */
#define year_is_leap(year)  (((unsigned)(year) % 4) == 0)

    /* Check mday validity */
    if (mday > __valid_mdays[mon] + (mon == 1 && year_is_leap(year)))
        return -1;

    t->tm_mday = mday;
    t->tm_mon  = mon;
    t->tm_year = year - 1900;
    /* OG: should also update t->tm_wday and t->tm_yday */

    return 0;
}

/*---------------- timers for benchmarks ----------------*/

const char *proctimer_report(proctimer_t *tp, const char *fmt)
{
    static char buf[256];
    int pos;
    int elapsed;
    const char *p;

    if (!fmt) {
#ifdef MINGCC
        fmt = "real %rms";
#else
        fmt = "real %rms, proc %pms, user %ums, sys %sms";
#endif
    }

    for (p = fmt, pos = 0; *p && pos < ssizeof(buf) - 1; p++) {
        if (*p == '%') {
            switch (*++p) {
            case 'r':   /* real */
                elapsed = tp->elapsed_real;
                goto format_elapsed;
            case 'u':   /* user */
                elapsed = tp->elapsed_user;
                goto format_elapsed;
            case 's':   /* sys */
                elapsed = tp->elapsed_sys;
                goto format_elapsed;
            case 'p':   /* process */
                elapsed = tp->elapsed_proc;
            format_elapsed:
                snprintf(buf + pos, sizeof(buf) - pos, "%d.%03d",
                         elapsed / 1000, elapsed % 1000);
                pos += strlen(buf + pos);
                continue;
            case '%':
            default:
                break;
            }
        }
        buf[pos++] = *p;
    }
    buf[pos] = '\0';
    return buf;
}

const char *proctimerstat_report(proctimerstat_t *pts, const char *fmt)
{
    static char buf[1024];
    int pos;
    unsigned int min, max, tot, mean;
    const char *p;

    if (!fmt) {
#ifdef MINGCC
        fmt = "real: %r";
#else
        fmt = "%n samples\nreal: %r\nproc: %p\nuser: %u\nsys : %s";
#endif
    }

    for (p = fmt, pos = 0; *p && pos < ssizeof(buf) - 1; p++) {
        if (*p == '%') {
            switch (*++p) {
            case 'n':   /* nb samples */
                snprintf(buf + pos, sizeof(buf) - pos, "%d", pts->nb);
                pos += strlen(buf + pos);
                continue;
            case 'r':   /* real */
                min = pts->real_min;
                max = pts->real_max;
                tot = pts->real_tot;
                goto format;
            case 'u':   /* user */
                min = pts->user_min;
                max = pts->user_max;
                tot = pts->user_tot;
                goto format;
            case 's':   /* sys */
                min = pts->sys_min;
                max = pts->sys_max;
                tot = pts->sys_tot;
                goto format;
            case 'p':   /* process */
                min = pts->proc_min;
                max = pts->proc_max;
                tot = pts->proc_tot;
                goto format;
            format:
                mean = tot / pts->nb;
                snprintf(buf + pos, sizeof(buf) - pos,
                         "min=%d.%03dms "
                         "max=%d.%03dms "
                         "mean=%d.%03dms",
                         min / 1000, min % 1000,
                         max / 1000, max % 1000,
                         mean / 1000, mean % 1000
                         );
                pos += strlen(buf + pos);
                continue;
            case '%':
            default:
                break;
            }
        }
        buf[pos++] = *p;
    }
    buf[pos] = '\0';
    return buf;
}

/*[ CHECK ]::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::{{{*/
#ifdef CHECK
/* tests legacy functions                                              {{{*/

START_TEST(check_localtime)
{
    int date, res;

    /* date -d "03/06/2007 12:34:13" +"%s" */
    date = 1173180853;

    res = localtime_curday(date);
    /* date -d "03/06/2007 00:00:00" +"%s" -> 1173135600 */
    fail_if(res != 1173135600,
            "Invalid current day time: %d != %d", res, 1173135600);

    res = localtime_nextday(date);
    /* date -d "03/07/2007 00:00:00" +"%s" -> 1173222000 */
    fail_if(res != 1173222000,
            "Invalid next day time: %d != %d", res, 1173222000);

    /* The following test may fail if we are ***very*** unlucky, call
     * it the midnight bug!
     */
    fail_if(localtime_curday(0) != localtime_curday(time(NULL)),
            "Invalid handling of date = 0");
    fail_if(localtime_nextday(0) != localtime_nextday(time(NULL)),
            "Invalid handling of date = 0");
}
END_TEST

START_TEST(check_strtotm)
{
    struct tm t;
    const char *date;

    p_clear(&t, 1);

    date = "23-Jul-97";
    fail_if(strtotm(date, &t),
            "strtotm could not parse %s", date);
    fail_if(t.tm_mday != 23,
            "strtotm failed to parse mday: %d != %d", t.tm_mday, 23);
    fail_if(t.tm_mon + 1 != 7,
            "strtotm failed to parse month: %d != %d", t.tm_mon + 1, 7);
    fail_if(t.tm_year + 1900 != 1997,
            "strtotm failed to parse year: %d != %d", t.tm_year + 1900, 1997);

    date = "32-Jul-97";
    fail_if(!strtotm(date, &t),
            "strtotm should not have parsed %s", date);
    date = "29-Feb-96";
    fail_if(strtotm(date, &t),
            "strtotm should have parsed %s", date);
    date = "29-Feb-2000";
    fail_if(strtotm(date, &t),
            "strtotm should have parsed %s", date);
    date = "01-Jun-07";
    fail_if(strtotm(date, &t),
            "strtotm should have parsed %s", date);
    date = "31-Jun-07";
    fail_if(!strtotm(date, &t),
            "strtotm should not have parsed %s", date);
}
END_TEST

/*.....................................................................}}}*/
/* public testing API                                                  {{{*/

Suite *check_make_timeval_suite(void)
{
    Suite *s  = suite_create("Timeval");
    TCase *tc = tcase_create("Core");

    suite_add_tcase(s, tc);
    tcase_add_test(tc, check_localtime);
    tcase_add_test(tc, check_strtotm);

    return s;
}

/*.....................................................................}}}*/
#endif
/*:::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::}}}*/
