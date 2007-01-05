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
#include <stdint.h>

#include <lib-common/err_report.h>
#include <lib-common/timeval.h>

/* Arithmetics on timeval assume both members of timeval are signed.
 * We keep timeval structures in normalized form:
 * - tv_usec is always in the range [0 .. 999999]
 * - tv_sec is negative iff timeval is negative
 * This choice is consistent with sys/time.h macros,
 * but it makes it a little bit harder to print timevals.
 */

/* Return reference to static buf for immediate printing */
const char *timeval_format(struct timeval tv)
{
    // FIXME: assuming 32 bit timeval resolution, bug in 2034
    static char buf[32];
    int pos, d, h, m, s, sec, usec;

    pos = 0;
    usec = tv.tv_usec;
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
    e_trace(3, "is_expired({%s}) ?", timeval_format(*date));
    *left = timeval_sub(*date, *now);
    if ((left->tv_sec < 0) || (left->tv_sec == 0 && left->tv_usec == 0)) {
        return true;
    }
    return false;
}
