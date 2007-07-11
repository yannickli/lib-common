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

#ifndef IS_LIB_COMMON_TIMEVAL_H
#define IS_LIB_COMMON_TIMEVAL_H

#include <sys/time.h>
#include <time.h>
#ifndef MINGCC
#include <sys/resource.h>
#endif

#include <lib-common/macros.h>

/* Return reference to static buf for immediate printing */
const char *timeval_format(struct timeval tv, bool as_duration);

struct timeval timeval_add(struct timeval a, struct timeval b);
struct timeval timeval_sub(struct timeval a, struct timeval b);
struct timeval timeval_mul(struct timeval tv, int k);
struct timeval timeval_div(struct timeval tv, int k);
bool is_expired(const struct timeval *date, const struct timeval *now,
                struct timeval *left);

static inline long long timeval_diff64(const struct timeval *tv2,
                                       const struct timeval *tv1) {
    return (tv2->tv_sec - tv1->tv_sec) * 1000000LL +
            (tv2->tv_usec - tv1->tv_usec);
}

static inline int timeval_diff(const struct timeval *tv2,
                               const struct timeval *tv1) {
    return (tv2->tv_sec - tv1->tv_sec) * 1000000 +
            (tv2->tv_usec - tv1->tv_usec);
}

/* Return timestamp of the start of the day which contains
 * the timestamp 'date'.
 * If date == 0, 'date' is interpreted as 'now' */
time_t localtime_curday(time_t date);

/* Return timestamp of the start of the next day which contains
 * the timestamp 'date'.
 * If date == 0, 'date' is interpreted as 'now' */
time_t localtime_nextday(time_t date);

/* Fill struct tm t from date in this format:
 * DD-MMM-[YY]YY with MMM the abbreviated month in English
 */
int strtotm(const char *date, struct tm *t);

/*---------------- timers for benchmarks ----------------*/

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
#ifndef MINGCC
    struct rusage ru, ru1;
#endif
    unsigned int elapsed_real;
    unsigned int elapsed_user;
    unsigned int elapsed_sys;
    unsigned int elapsed_proc;
} proctimer_t;

typedef struct proctimerstat_t {
    unsigned int nb;
    unsigned int real_min;
    unsigned int real_max;
    unsigned int real_tot;

    unsigned int user_min;
    unsigned int user_max;
    unsigned int user_tot;

    unsigned int sys_min;
    unsigned int sys_max;
    unsigned int sys_tot;

    unsigned int proc_min;
    unsigned int proc_max;
    unsigned int proc_tot;
} proctimerstat_t;


static inline void proctimer_start(proctimer_t *tp) {
    gettimeofday(&tp->tv, NULL);
#ifndef MINGCC
    getrusage(RUSAGE_SELF, &tp->ru);
#endif
}

static inline long long proctimer_stop(proctimer_t *tp) {
#ifndef MINGCC
    getrusage(RUSAGE_SELF, &tp->ru1);
#endif
    gettimeofday(&tp->tv1, NULL);
    tp->elapsed_real = timeval_diff(&tp->tv1, &tp->tv);
#ifndef MINGCC
    tp->elapsed_user = timeval_diff(&tp->ru1.ru_utime, &tp->ru.ru_utime);
    tp->elapsed_sys = timeval_diff(&tp->ru1.ru_stime, &tp->ru.ru_stime);
    tp->elapsed_proc = tp->elapsed_user + tp->elapsed_sys;
#else
    tp->elapsed_sys = 0;
    tp->elapsed_proc = tp->elapsed_user = tp->elapsed_real;
#endif
    return tp->elapsed_proc;
}

static inline void proctimerstat_addsample(proctimerstat_t *pts,
                                           proctimer_t *tp) {
#define COUNT(type) \
    if (pts->nb != 0) { \
        pts->type##_min = MIN(tp->elapsed_##type, pts->type##_min); \
        pts->type##_max = MAX(tp->elapsed_##type, pts->type##_max); \
        pts->type##_tot += tp->elapsed_##type; \
    } else { \
        pts->type##_min = tp->elapsed_##type; \
        pts->type##_max = tp->elapsed_##type; \
        pts->type##_tot = tp->elapsed_##type; \
    } \

    COUNT(real);
    COUNT(user);
    COUNT(sys);
    COUNT(proc);
#undef COUNT
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

/*[ CHECK ]::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::{{{*/
#ifdef CHECK
#include <check.h>

Suite *check_make_timeval_suite(void);

#endif
/*:::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::}}}*/
#endif /* IS_LIB_COMMON_TIMEVAL_H */
