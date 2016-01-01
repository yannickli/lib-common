/**************************************************************************/
/*                                                                        */
/*  Copyright (C) 2004-2016 INTERSEC SA                                   */
/*                                                                        */
/*  Should you receive a copy of this source code, you must check you     */
/*  have a proper, written authorization of INTERSEC to hold it. If you   */
/*  don't have such an authorization, you must DELETE all source code     */
/*  files in your possession, and inform INTERSEC of the fact you obtain  */
/*  these files. Should you not comply to these terms, you can be         */
/*  prosecuted in the extent permitted by applicable law.                 */
/*                                                                        */
/**************************************************************************/

#ifndef IS_LIB_COMMON_TIME_H
#define IS_LIB_COMMON_TIME_H

#ifndef OS_WINDOWS
#include <sys/resource.h>
#define PROCTIMER_USE_RUSAGE  0
#else
#define PROCTIMER_USE_RUSAGE  0
#endif

#include "core.h"

#define TIME_T_ERROR  ((time_t)-1)

#if defined _BSD_SOURCE || defined _SVID_SOURCE || defined _DEFAULT_SOURCE
/* nanosecond precision on file times from struct stat */
#define st_atimensec  st_atim.tv_nsec   /* Backward compatibility.  */
#define st_mtimensec  st_mtim.tv_nsec
#define st_ctimensec  st_ctim.tv_nsec
#endif

unsigned long hardclock(void);

/***************************************************************************/
/* low precision time() and gettimeofday() replacements                    */
/***************************************************************************/

const char *lp_getsec_str(void);
time_t lp_getsec(void);
void lp_gettv(struct timeval *);
uint64_t lp_getmsec(void);
uint64_t lp_getcsec(void);

/***************************************************************************/
/* miscellaneous                                                           */
/***************************************************************************/

/** Count the number of leap years since 1900.
 *
 * When the provided year is a leap year, it is part of the returned count.
 * For example, the number of leap years since 1900 for 2016 is 29.
 */
static inline int nb_leap_years_since_1900(int year)
{
    static const int nb_leap_years_1900 = 460;

    return (year / 4) - (year / 100) + (year / 400) - nb_leap_years_1900;
}

/***************************************************************************/
/* time.h wrappers                                                         */
/***************************************************************************/

/** Return timestamp of the start of the minute which contains
 * the timestamp \p date.
 *
 * If date == 0, 'date' is interpreted as 'now'
 */
time_t localtime_curminute(time_t date);

/** Return timestamp of the start of the minute that follows the one which
 * contains the timestamp \p date.
 *
 * If date == 0, 'date' is interpreted as 'now'
 */
time_t localtime_nextminute(time_t date);

/** Return timestamp of the start of the hour which contains
 * the timestamp \p date.
 *
 * If date == 0, 'date' is interpreted as 'now'
 */
time_t localtime_curhour(time_t date);

/** Return timestamp of the start of the hour that follows the one which
 * contains the timestamp \p date.
 *
 * If date == 0, 'date' is interpreted as 'now'
 */
time_t localtime_nexthour(time_t date);

/** Return timestamp of the start of the day which contains
 * the timestamp \p date.
 *
 * If date == 0, 'date' is interpreted as 'now'
 */
time_t localtime_curday(time_t date);

/** Return timestamp of the start of the day that follows the one which
 * contains the timestamp \p date.
 *
 * If date == 0, 'date' is interpreted as 'now'
 */
time_t localtime_nextday(time_t date);

/** Return timestamp of the start of the n'th day that follow the one which
 * contains the timestamp \p date.
 */
time_t localtime_addday(time_t date, int n);

/** Return timestamp of the start of the week which contains
 * the timestamp \p date.
 *
 * If date == 0, 'date' is interpreted as 'now'.
 * \param[in] first_day_of_week 0 for sunday, 1 for monday, ... 6 for saturday
 */
time_t localtime_curweek(time_t date, int first_day_of_week);

/** Return timestamp of the start of the week that follows the one which
 * contains the timestamp \p date.
 *
 * If date == 0, 'date' is interpreted as 'now'
 * \param[in] first_day_of_week 0 for sunday, 1 for monday, ... 6 for saturday
 */
time_t localtime_nextweek(time_t date, int first_day_of_week);

/** Return timestamp of the start of the month which contains
 * the timestamp \p date.
 *
 * If date == 0, 'date' is interpreted as 'now'
 */
time_t localtime_curmonth(time_t date);

/** Return timestamp of the start of the year which contains
 * the timestamp \p date.
 *
 * If date == 0, 'date' is interpreted as 'now'
 */
time_t localtime_curyear(time_t date);

/** Return timestamp of the start of the month that follows the one which
 * contains the timestamp \p date.
 *
 * If date == 0, 'date' is interpreted as 'now'
 */
time_t localtime_nextmonth(time_t date);

/* Fill struct tm t from date in this format:
 * DDyyyy-eYY]YY with MMM the abbreviated month in English
 */
int strtotm(const char *date, struct tm *t);

/* Get the local time of the timezone specified by the tz parameter.
 * If not NULL, the string pointed by this parameter will be used to set
 * the TZ environment variable, otherwise the system local time will be
 * returned.
 *
 * Because this function deals with process environment variables,
 * you should not call it concurrently to another call of it or to a call
 * to the localtime_r function.
 */
struct tm *time_get_localtime(const time_t *p_ts, struct tm *p_tm,
                              const char *tz);

/* Format a timestamp according to given locale and format.
 * locale should be a string similar to the output of the locale (unix)
 * command. If empty the system locale will be used.
 * The format of fmt is the one described in strftime manpage.
 * XXX: This function is costly (it may call setlocale twice) and NOT thread
 *      safe if locale != NULL.
 *
 * Returns the number of bytes placed in the array out (not including the
 * terminating null byte) in case of success, -1 in case of error.
 */
__must_check__
int format_timestamp(const char *fmt, time_t ts, const char *locale,
                     char out[], int out_size)__attr_nonnull__((1));

/** Count the number of days since January 1st, 1900.
 */
static inline int tm_nb_days_since_1900(struct tm *t)
{
    int nb_days = 365 * t->tm_year + t->tm_yday;

    /* do not consider current year for leap years count since the current
     * leap year day, if any, is already added by tm_yday.
     */
    return nb_days + nb_leap_years_since_1900(t->tm_year + 1900 - 1);
}

/** Count the number of days between two dates.
 */
static inline int tm_diff_days(struct tm *from, struct tm *to)
{
    return tm_nb_days_since_1900(to) - tm_nb_days_since_1900(from);
}

/** Count the number of hours between two dates.
 *
 * Daylight saving time and leap seconds are not considered in the count.
 */
static inline int tm_diff_hours(struct tm* from, struct tm *to)
{
    unsigned long nb_days = tm_diff_days(from, to);

    return nb_days * 24 + (to->tm_hour - from->tm_hour);
}

/** Count the number of minutes between two dates.
 *
 * Daylight saving time and leap seconds are not considered in the count.
 */
static inline int tm_diff_minutes(struct tm* from, struct tm *to)
{
    unsigned long nb_hours = tm_diff_hours(from, to);

    return nb_hours * 60 + (to->tm_min - from->tm_min);
}

/***************************************************************************/
/* iso8601                                                                 */
/***************************************************************************/

/* XXX: Array parameter qualifiers are not supported in C++98. */
#ifndef __cplusplus

static inline void time_fmt_iso8601(char buf[static 21], time_t t)
{
    int len;
    struct tm tm;

    if (!gmtime_r(&t, &tm)) {
        e_panic("invalid timestamp: %jd", t);
    }
    len = snprintf(buf, 21, "%04d-%02d-%02dT%02d:%02d:%02dZ",
                   tm.tm_year + 1900, tm.tm_mon + 1, tm.tm_mday,
                   tm.tm_hour, tm.tm_min, tm.tm_sec);
    if (len >= 21) {
        e_panic("invalid timestamp: %jd", t);
    }
}

/* Format a timestamp into a local ISO 8601 time. This function calls the
 * time_get_localtime function, so you should not call it concurrently to
 * another call of it or to a call to the time_get_localtime or to the
 * localtime_r function. See time_get_localtime for more information.
 */
static inline void time_fmt_localtime_iso8601(char buf[static 26], time_t t,
                                              const char *tz)
{
    int len;
    struct tm tm;
    int delta_h, delta_m;

    time_get_localtime(&t, &tm, tz);

    delta_h = tm.tm_gmtoff / 3600;
    delta_m = labs(tm.tm_gmtoff - delta_h * 3600) / 60;

    len = snprintf(buf, 26, "%04d-%02d-%02dT%02d:%02d:%02d%+03d:%02d",
                   tm.tm_year + 1900, tm.tm_mon + 1, tm.tm_mday,
                   tm.tm_hour, tm.tm_min, tm.tm_sec, delta_h, delta_m);
    if (len >= 26) {
        e_panic("invalid timestamp: %jd", t);
    }
}

static inline void time_fmt_iso8601_msec(char buf[static 25], time_t t,
                                         int msec)
{
    int len;
    struct tm tm;

    /* XXX %03d gives a minimum width but not a maximum one, so we need to be
     * careful and not overflow the buffer of size 25 with invalid inputs.
     */
    if (msec < 0 || msec >= 1000) {
        e_panic("invalid msec: %d", msec);
    }
    if (!gmtime_r(&t, &tm)) {
        e_panic("invalid timestamp: %jd", t);
    }
    len = snprintf(buf, 25, "%04d-%02d-%02dT%02d:%02d:%02d.%03dZ",
                   tm.tm_year + 1900, tm.tm_mon + 1, tm.tm_mday,
                   tm.tm_hour, tm.tm_min, tm.tm_sec, msec);
    if (len >= 25) {
        e_panic("invalid timestamp: %jd", t);
    }
}

/* Format a timestamp into a local ISO 8601 time. This function calls the
 * time_get_localtime function, so you should not call it concurrently to
 * another call of it or to a call to the time_get_localtime or to the
 * localtime_r function. See time_get_localtime for more information.
 */
static inline
void time_fmt_localtime_iso8601_msec(char buf[static 30], time_t t,
                                     int msec, const char *tz)
{
    int len;
    struct tm tm;
    int delta_h, delta_m;

    time_get_localtime(&t, &tm, tz);

    delta_h = tm.tm_gmtoff / 3600;
    delta_m = labs(tm.tm_gmtoff - delta_h * 3600) / 60;

    len = snprintf(buf, 30, "%04d-%02d-%02dT%02d:%02d:%02d.%03d%+03d:%02d",
                   tm.tm_year + 1900, tm.tm_mon + 1, tm.tm_mday,
                   tm.tm_hour, tm.tm_min, tm.tm_sec, msec, delta_h, delta_m);
    if (len >= 30) {
        e_panic("invalid timestamp: %jd", t);
    }
}

static inline void sb_add_time_iso8601(sb_t *sb, time_t t)
{
    time_fmt_iso8601(sb_growlen(sb, 20), t);
}

/* Add an ISO 8601 local time into a sb. This function calls the
 * time_get_localtime function, so you should not call it concurrently to
 * another call of it or to a call to the time_get_localtime or to the
 * localtime_r function. See time_get_localtime for more information.
 */
static inline
void sb_add_localtime_iso8601(sb_t *sb, time_t t, const char *tz)
{
    time_fmt_localtime_iso8601(sb_growlen(sb, 25), t, tz);
}

static inline void sb_add_time_iso8601_msec(sb_t *sb, time_t t, int msec)
{
    time_fmt_iso8601_msec(sb_growlen(sb, 24), t, msec);
}

/* Add an ISO 8601 local time into a sb. This function calls the
 * time_get_localtime function, so you should not call it concurrently to
 * another call of it or to a call to the time_get_localtime or to the
 * localtime_r function. See time_get_localtime for more information.
 */
static inline
void sb_add_localtime_iso8601_msec(sb_t *sb, time_t t,
                                   int msec, const char *tz)
{
    time_fmt_localtime_iso8601_msec(sb_growlen(sb, 29), t, msec, tz);
}

/** Flags to use with time_parse_iso8601_flags. */
enum iso8601_flags {
    /** Allow the parsing of our syslog date format.
     *
     * If set, then time formated like in our syslog files are accepted.
     * Format is: YYYY-MM-DD hh:mm:ss +zzzz.
     */
    ISO8601_ALLOW_SYSLOG_FORMAT = (1U << 1),
};

int time_parse_iso8601_flags(pstream_t *ps, time_t *res, unsigned flags);
static inline int time_parse_iso8601(pstream_t *ps, time_t *res) {
    return time_parse_iso8601_flags(ps, res, 0);
}

static inline int time_parse_iso8601s(const char *s, time_t *res)
{
    pstream_t ps = ps_initstr(s);

    /* Trim the ps_stream before getting the date */
    ps_trim(&ps);

    /* FIXME: do we want to err if !ps_done(&ps) at the end ? */
    return time_parse_iso8601(&ps, res);
}

/** Parse a string as a date and builds the corresponding unix timestamp.
 *
 * That function parses a string from one of those three formats:
 * - ISO8601: YYYY-MM-DDThh:mm:ss[TZ]
 * - RFC822: [Day, ]D month YYYY hh:mm:ss[ TZ]
 * - Unix timestamp in decimal
 *
 * The parsing is done case insensitively. The timezone is optional, when
 * omitted, the local timezone is used. When present it must have one of the
 * following format:
 * - literal name (e.g. DST, CEST, UTC, Z, GMT, ...)
 * - +/-hh:mm
 * - +/-hhmm
 * - +/-hh
 *
 * \param[in,out] ps the stream from which the date should be parsed.
 * \param[out]    res the timestamp.
 * \return a negative value in case of error.
 */
int time_parse(pstream_t *ps, time_t *res);

static inline int time_parse_str(const char *s, time_t *res)
{
    pstream_t ps = ps_initstr(s);

    ps_trim(&ps);
    return time_parse(&ps, res);
}

#endif

/***************************************************************************/
/* timeval operations                                                      */
/***************************************************************************/

/* Return reference to static buf for immediate printing */
const char *timeval_format(struct timeval tv, bool as_duration);

static inline struct timeval timeval_add(struct timeval a, struct timeval b)
{
    struct timeval res;

    res.tv_sec  = a.tv_sec + b.tv_sec;
    res.tv_usec = a.tv_usec + b.tv_usec;
    if (res.tv_usec >= 1000 * 1000) {
        res.tv_sec += 1;
        res.tv_usec -= 1000 * 1000;
    }
    return res;
}

static inline struct timeval timeval_addmsec(struct timeval a, int64_t msec)
{
    struct timeval res;

    res.tv_sec  = a.tv_sec + msec / 1000;
    res.tv_usec = a.tv_usec + ((msec % 1000) * 1000);
    if (res.tv_usec >= 1000 * 1000) {
        res.tv_sec += 1;
        res.tv_usec -= 1000 * 1000;
    }
    return res;
}

static inline struct timeval timeval_sub(struct timeval a, struct timeval b)
{
    struct timeval res;

    res.tv_sec = a.tv_sec - b.tv_sec;
    res.tv_usec = a.tv_usec - b.tv_usec;
    /* OG: assuming tv_usec is signed or compiler warns about test */
    if (res.tv_usec < 0) {
        res.tv_sec -= 1;
        res.tv_usec += 1000 * 1000;
    }
    return res;
}
struct timeval timeval_mul(struct timeval tv, int k);
struct timeval timeval_div(struct timeval tv, int k);

/* Compute diff between two timeval in microseconds (µs) */
static inline long long timeval_diff64(const struct timeval *tv2,
                                       const struct timeval *tv1) {
    return (tv2->tv_sec - tv1->tv_sec) * 1000000LL +
            (tv2->tv_usec - tv1->tv_usec);
}

/* Compute diff between two timeval in microseconds (µs)
 * use with care as it's not safe for intervals larger than 4000s */
static inline int timeval_diff(const struct timeval *tv2,
                               const struct timeval *tv1) {
    return (tv2->tv_sec - tv1->tv_sec) * 1000000 +
            (tv2->tv_usec - tv1->tv_usec);
}

/* Compute diff between two timeval in milliseconds (ms) */
static inline int64_t timeval_diffmsec(const struct timeval *tv2,
                                       const struct timeval *tv1)
{
    int64_t delta = tv2->tv_sec - tv1->tv_sec;

    return delta * 1000 + (tv2->tv_usec - tv1->tv_usec) / 1000;
}

static inline bool timeval_is_lt0(const struct timeval t) {
    return  t.tv_sec < 0;
}
static inline bool timeval_is_le0(const struct timeval t) {
    return  t.tv_sec < 0 || (t.tv_sec == 0 && t.tv_usec == 0);
}
static inline bool timeval_is_gt0(const struct timeval t) {
    return !timeval_is_le0(t);
}
static inline bool timeval_is_ge0(const struct timeval t) {
    return !timeval_is_lt0(t);
}

static inline int64_t timeval_to_msec(const struct timeval tv)
{
    return tv.tv_sec * 1000 + (tv.tv_usec / 1000);
}

bool is_expired(const struct timeval *date, const struct timeval *now,
                struct timeval *left);

/***************************************************************************/
/* timers for benchmarks                                                   */
/***************************************************************************/

/* we use gettimeofday() and getrusage() for accurate benchmark timings.
 * - clock(); returns process time, but has a precision of only 10ms
 * - clock_gettime() has a better precision: timespec can measure times
 *   down to the nanosecond, but only real time is supported in linux
 *   (CLOCK_REALTIME), CLOCK_PROCESS_CPUTIME_ID is not supported, and
 *   librt must be specified at link time.
 * - times() has a precision of 10ms and units must be retrieved with a
 *   separate call to sysconf(_SC_CLK_TCK).
 */
typedef struct proctimer_t {
    struct timeval tv, tv1;
#if PROCTIMER_USE_RUSAGE
    struct rusage ru, ru1;
#endif
    unsigned long hc, hc1;
    unsigned int  elapsed_real;
    unsigned int  elapsed_user;
    unsigned int  elapsed_sys;
    unsigned int  elapsed_proc;
    unsigned long elapsed_hard;
} proctimer_t;

/* for each time source, the following statistics are computed :
 * - minimal value
 * - maximal value
 * - cumulated sum (tot)
 */
#define PTIMER_STATS(T, type)  \
    T type##_min; \
    T type##_max; \
    T type##_tot

typedef struct proctimerstat_t {
    unsigned int nb;

    PTIMER_STATS(unsigned int,  real);
    PTIMER_STATS(unsigned int,  user);
    PTIMER_STATS(unsigned int,  sys);
    PTIMER_STATS(unsigned int,  proc);
    PTIMER_STATS(unsigned long, hard);
} proctimerstat_t;

#undef PTIMER_STATS

static inline void proctimer_start(proctimer_t *tp) {
    gettimeofday(&tp->tv, NULL);
#if PROCTIMER_USE_RUSAGE
    getrusage(RUSAGE_SELF, &tp->ru);
#endif
    tp->hc = hardclock();
}

static inline long long proctimer_stop(proctimer_t *tp) {
    tp->hc1 = hardclock();
#if PROCTIMER_USE_RUSAGE
    getrusage(RUSAGE_SELF, &tp->ru1);
#endif
    gettimeofday(&tp->tv1, NULL);
    tp->elapsed_real = timeval_diff(&tp->tv1, &tp->tv);
#if PROCTIMER_USE_RUSAGE
    tp->elapsed_user = timeval_diff(&tp->ru1.ru_utime, &tp->ru.ru_utime);
    tp->elapsed_sys = timeval_diff(&tp->ru1.ru_stime, &tp->ru.ru_stime);
    tp->elapsed_proc = tp->elapsed_user + tp->elapsed_sys;
#else
    tp->elapsed_sys = 0;
    tp->elapsed_proc = tp->elapsed_user = tp->elapsed_real;
#endif
    tp->elapsed_hard = tp->hc1 - tp->hc;
    return tp->elapsed_proc;
}

static inline void proctimerstat_addsample(proctimerstat_t *pts,
                                           proctimer_t *tp) {
#define PTIMER_COUNT(type)                                              \
    do {                                                                \
        if (pts->nb != 0) {                                             \
            pts->type##_min = MIN(tp->elapsed_##type, pts->type##_min); \
            pts->type##_max = MAX(tp->elapsed_##type, pts->type##_max); \
            pts->type##_tot += tp->elapsed_##type;                      \
        } else {                                                        \
            pts->type##_min = tp->elapsed_##type;                       \
            pts->type##_max = tp->elapsed_##type;                       \
            pts->type##_tot = tp->elapsed_##type;                       \
        }                                                               \
    } while (0)

    PTIMER_COUNT(real);
    PTIMER_COUNT(user);
    PTIMER_COUNT(sys);
    PTIMER_COUNT(proc);
    PTIMER_COUNT(hard);
#undef PTIMER_COUNT
    pts->nb++;
}

const char *proctimerstat_report(proctimerstat_t *pts, const char *fmt);

/* report timings from proctimer_t using format string fmt:
 * %r -> real time in ms
 * %p -> process time in ms
 * %u -> user time in ms
 * %s -> system time in ms
 * if fmt is NULL, use "real: %r ms, proc:%p ms, user:%u ms, sys: %s ms"
 */
const char *proctimer_report(proctimer_t *tp, const char *fmt);

#endif /* IS_LIB_COMMON_TIMEVAL_H */
