/**************************************************************************/
/*                                                                        */
/*  Copyright (C) 2004-2006 INTERSEC                                      */
/*                                                                        */
/*  Should you receive a copy of this source code, you must check you     */
/*  have a proper, written authorization of INTERSEC to hold it. If you   */
/*  don't have such an authorization, you must DELETE all source code     */
/*  files in your possession, and inform INTERSEC of the fact you obtain  */
/*  these files. Should you not comply to these terms, you can be         */
/*  prosecuted in the extent permitted by applicable law.                 */
/*                                                                        */
/**************************************************************************/

#ifndef IS_LIB_COMMON_MACROS_H
#define IS_LIB_COMMON_MACROS_H

/**************************************************************************/
/* GNU extension wrappers                                                 */
/**************************************************************************/

/*
 * __attr_unused__             => unused vars
 * __attr_noreturn__           => functions that perform abord()/exit()
 * __attr_printf__(pos1, pos2) => printf formats
 */

#if !defined(__doxygen_mode__)
#  if (!defined(__GNUC__) || __GNUC__ < 3) && !defined(__attribute__)
#    define __attribute__(attr)
#  endif

#  define __unused__             __attribute__((unused))
#  define __attr_noreturn__      __attribute__((noreturn))
#  define __attr_nonnull__(l)    __attribute__((nonnull l))
#  define __attr_printf__(a, b)  __attribute__((format(printf, a, b)))
#endif

#ifdef __GNUC__
#  undef EXPORT
#  define EXPORT    __attribute__((visibility("default")))
#endif

/**************************************************************************/
/* TYPES                                                                  */
/**************************************************************************/

#if !defined(__cplusplus)
#ifndef bool
typedef int bool;
#endif

#ifndef false
#define false  (0)
#define true   (!false)
#endif
#endif

typedef unsigned char byte;

/**************************************************************************/
/* Misc                                                                   */
/**************************************************************************/

#define countof(table)  ((ssize_t)(sizeof(table) / sizeof((table)[0])))
#define ssizeof(foo)    ((ssize_t)sizeof(foo))

#ifndef MAX
#define MAX(a,b)  (((a) > (b)) ? (a) : (b))
#endif

#ifndef MIN
#define MIN(a,b)  (((a) > (b)) ? (b) : (a))
#endif

enum sign {
    POSITIVE = 1,
    ZERO     = 0,
    NEGATIVE = -1,
};

#ifdef CMP
#error CMP already defined
#endif
#ifdef SIGN
#error SIGN already defined
#endif

#define CMP(x, y)  ((enum sign)(((y) > (x)) - ((y) < (x))))
#define SIGN(x)    CMP(0, x)

/**************************************************************************/
/* Type safe conversion functions                                         */
/**************************************************************************/
#define CONVERSION_FUNCTIONS(type1, type2) \
    static inline type2 *type1##_to_##type2(type1 *p) \
    { \
        return (type2 *)(p); \
    } \
    static inline type1 *type2##_to_##type1(type2 *p) \
    { \
        return (type1 *)(p); \
    } \
    static inline const type2 *type1##_to_##type2##_const(const type1 *p) \
    { \
        return (const type2 *)(p); \
    } \
    static inline const type1 *type2##_to_##type1##_const(const type2 *p) \
    { \
        return (const type1 *)(p); \
    } \
    static inline type1 **type2##_to_##type1##_p(type2 **p) \
    { \
        return (type1 **)(p); \
    } \
    static inline type2 **type1##_to_##type2##_p(type1 **p) \
    { \
        return (type2 **)(p); \
    }

/**************************************************************************/
/* License control                                                        */
/**************************************************************************/

int show_flags(const char *arg, int flags);
int show_licence(const char *arg);
int set_licence(const char *arg, const char *licence_data);
void check_strace(void);

#ifdef EXPIRATION_DATE

#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <unistd.h>
#include <sys/time.h>
#include <time.h>
#include <sys/epoll.h>

static inline void check_licence(const struct timeval *tv) {
    if (tv->tv_sec > EXPIRATION_DATE) {
        fputs("Licence expired\n", stderr);
        exit(127);
    }
}

extern int strace_next_check;
extern const char *strace_msg;

#define STRACE_CHECK_INTERVAL 2

static inline void check_trace(const struct timeval *tv) {
    if (tv->tv_sec >= strace_next_check) {
        strace_next_check = tv->tv_sec + STRACE_CHECK_INTERVAL;
        if (strace_msg) {
            fputs(strace_msg, stderr);
            exit(124);
        }
        check_strace();
    }
}

static inline int gettimeofday_check(struct timeval *tv, struct timezone *tz) {
    int res = (gettimeofday)(tv, tz);
    check_licence(tv);
    check_trace(tv);
    return res;
}

#define gettimeofday(tv, tz)  gettimeofday_check(tv, tz)

static struct timeval now_strace_check;
static inline int epoll_wait_check(int epfd, struct epoll_event * events, int maxevents, int timeout)
{
    int res = (epoll_wait)(epfd, events, maxevents, timeout);
    gettimeofday_check(&now_strace_check, NULL);
    return res;
}

#define epoll_wait(epfd, events, maxevents, timeout) \
    epoll_wait_check(epfd, events, maxevents, timeout)

static inline int getopt_check(int argc, char * const argv[],
			       const char *optstring)
{
    struct timeval tv;

    (gettimeofday)(&tv, NULL);

    if (optind <= 1) {
        if (argv[1]) {
            if (!strcmp(argv[1], "--show-licence")
            ||  !strcmp(argv[1], "--check")) {
                exit(show_licence(argv[0]));
            }
            if (!strcmp(argv[1], "--set-licence"))
                exit(set_licence(argv[0], argv[2]));
            if (!strcmp(argv[1], "--flags")) {
                sleep(5);
                exit(show_flags(argv[0], 1));
            }
            check_licence(&tv);
            //if (show_flags(argv[0], 0)) {
            //    exit(42);
            //}
        }
    }
    check_licence(&tv);
    return (getopt)(argc, argv, optstring);
}
#define getopt(argc, argv, optstring)  getopt_check(argc, argv, optstring)

#endif

/**************************************************************************/
/* Defensive programming                                                  */
/**************************************************************************/

#include <stdio.h>

#undef sprintf
#define sprintf(...)  NEVER_USE_sprintf(__VA_ARGS__)
#undef strtok
#define strtok(...)   NEVER_USE_strtok(__VA_ARGS__)
#undef strncpy
#define strncpy(...)  NEVER_USE_strncpy(__VA_ARGS__)
#undef strncat
#define strncat(...)  NEVER_USE_strncat(__VA_ARGS__)

__attr_nonnull__((1))
static inline int p_fclose(FILE **fpp) {
    if (*fpp) {
        FILE *fp = *fpp;
        *fpp = NULL;
        return fclose(fp);
    } else {
        return 0;
    }
}

#endif /* IS_LIB_COMMON_MACROS_H */
