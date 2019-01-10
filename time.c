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

/*
 *  Portable interface to the CPU cycle counter
 *
 *  Copyright (C) 2006-2007  Christophe Devine
 *
 *  Redistribution and use in source and binary forms, with or without
 *  modification, are permitted provided that the following conditions
 *  are met:
 *
 *    * Redistributions of source code must retain the above copyright
 *      notice, this list of conditions and the following disclaimer.
 *    * Redistributions in binary form must reproduce the above copyright
 *      notice, this list of conditions and the following disclaimer in the
 *      documentation and/or other materials provided with the distribution.
 *    * Neither the name of XySSL nor the names of its contributors may be
 *      used to endorse or promote products derived from this software
 *      without specific prior written permission.
 *
 *  THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 *  "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 *  LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS
 *  FOR A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 *  OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 *  SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED
 *  TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 *  PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF
 *  LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING
 *  NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
 *  SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 * {{{
 */
#if (defined(_MSC_VER) && defined(_M_IX86)) || defined(__WATCOMC__)

unsigned long hardclock(void)
{
    unsigned long tsc;
    __asm   rdtsc
    __asm   mov  [tsc], eax
    return tsc;
}

#elif defined(__GNUC__)
# if defined(__i386__)

unsigned long hardclock(void)
{
    unsigned long tsc;
    asm("rdtsc" : "=a" (tsc));
    return tsc;
}

# elif defined(__amd64__) || defined(__x86_64__)

unsigned long hardclock(void)
{
    unsigned long lo, hi;
    asm("rdtsc" : "=a" (lo), "=d" (hi));
    return lo | (hi << 32);
}

# elif defined(__powerpc__) || defined(__ppc__)

unsigned long hardclock(void)
{
    unsigned long tbl, tbu0, tbu1;

    do {
        asm("mftbu %0" : "=r" (tbu0));
        asm("mftb  %0" : "=r" (tbl));
        asm("mftbu %0" : "=r" (tbu1));
    } while (tbu0 != tbu1);

    return tbl;
}

# elif defined(__sparc__)

unsigned long hardclock(void)
{
    unsigned long tick;
    asm(".byte 0x83, 0x41, 0x00, 0x00");
    asm("mov   %%g1, %0" : "=r" (tick));
    return tick;
}

# elif defined(__alpha__)

unsigned long hardclock(void)
{
    unsigned long cc;
    asm("rpcc %0" : "=r" (cc));
    return cc & 0xFFFFFFFF;
}

# elif defined(__ia64__)

unsigned long hardclock(void)
{
    unsigned long itc;
    asm("mov %0 = ar.itc" : "=r" (itc));
    return itc;
}
# else
#  error unimplemented for your arch
# endif
#endif

/* }}} */
/***************************************************************************/
/* timeval operations                                                      */
/***************************************************************************/

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

    if (!now) {
        now = &local_now;
        gettimeofday(&local_now, NULL);
    }
    if (left) {
        *left = timeval_sub(*date, *now);
        return timeval_is_le0(*left);
    }
    return timeval_is_le0(timeval_sub(*date, *now));
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

TEST_DECL("time: localtime_{next,cur}day", 0)
{
    int date, res;

    /* date -d "03/06/2007 12:34:13" +"%s" */
    date = 1173180853;

    res = localtime_curday(date);
    /* date -d "03/06/2007 00:00:00" +"%s" -> 1173135600 */
    TEST_FAIL_IF(res != 1173135600,
                 "Invalid current day time: %d != %d", res, 1173135600);

    res = localtime_nextday(date);
    /* date -d "03/07/2007 00:00:00" +"%s" -> 1173222000 */
    TEST_FAIL_IF(res != 1173222000,
                 "Invalid next day time: %d != %d", res, 1173222000);

    /* The following test may fail if we are ***very*** unlucky, call
     * it the midnight bug!
     */
    TEST_FAIL_IF(localtime_curday(0) != localtime_curday(time(NULL)),
                 "Invalid handling of date = 0");
    TEST_FAIL_IF(localtime_nextday(0) != localtime_nextday(time(NULL)),
                 "Invalid handling of date = 0");
    TEST_DONE();
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

    lower_mon[0] = tolower((unsigned char)p[0]);
    lower_mon[1] = tolower((unsigned char)p[1]);
    lower_mon[2] = tolower((unsigned char)p[2]);
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

    year = RETHROW(atoi(p));
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

TEST_DECL("time: strtom", 0)
{
    struct tm t;
    const char *date;

    p_clear(&t, 1);

    date = "23-Jul-97";
    TEST_FAIL_IF(strtotm(date, &t),
                 "strtotm could not parse %s", date);
    TEST_FAIL_IF(t.tm_mday != 23,
                 "strtotm failed to parse mday: %d != %d", t.tm_mday, 23);
    TEST_FAIL_IF(t.tm_mon + 1 != 7,
                 "strtotm failed to parse month: %d != %d", t.tm_mon + 1, 7);
    TEST_FAIL_IF(t.tm_year + 1900 != 1997,
                 "strtotm failed to parse year: %d != %d", t.tm_year + 1900, 1997);

    date = "32-Jul-97";
    TEST_FAIL_IF(!strtotm(date, &t),
                 "strtotm should not have parsed %s", date);
    date = "29-Feb-96";
    TEST_FAIL_IF(strtotm(date, &t),
                 "strtotm should have parsed %s", date);
    date = "29-Feb-2000";
    TEST_FAIL_IF(strtotm(date, &t),
                 "strtotm should have parsed %s", date);
    date = "01-Jun-07";
    TEST_FAIL_IF(strtotm(date, &t),
                 "strtotm should have parsed %s", date);
    date = "31-Jun-07";
    TEST_FAIL_IF(!strtotm(date, &t),
                 "strtotm should not have parsed %s", date);
    TEST_DONE();
}


/***************************************************************************/
/* timers for benchmarks                                                   */
/***************************************************************************/

const char *proctimer_report(proctimer_t *tp, const char *fmt)
{
    static char buf[256];
    int pos;
    int elapsed;
    const char *p;

    if (!fmt) {
#ifdef OS_WINDOWS
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
#ifdef OS_WINDOWS
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
