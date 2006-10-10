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

#ifndef IS_MACROS_H
#define IS_MACROS_H

/**************************************************************************/
/* GNU EXTENSIONS fake                                                    */
/**************************************************************************/

/*
 * __attr_format__(pos1, pos2) => printf formats
 * __attr_unused__             => unused vars
 * __attr_noreturn__           => functions that perform abord()/exit()
 */

#define __attr_format__(format_idx, arg_idx)  \
    __attribute__((format(printf, format_idx, arg_idx)))
#define __unused__        __attribute__((unused))
#define __attr_noreturn__ __attribute__((noreturn))

#if (!defined(__GNUC__) || __GNUC__ < 3) && !defined(__attribute__)
#  define __attribute__(foo)
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
/* Memory functions                                                       */
/**************************************************************************/

#define MEM_ALIGN_SIZE  8
#define MEM_ALIGN(size) \
    (((size) + MEM_ALIGN_SIZE - 1) & ~((ssize_t)MEM_ALIGN_SIZE - 1))

#define FREE(ptr) do {  \
    free(ptr);          \
    (ptr) = NULL;       \
} while(0)

#define countof(table) ((ssize_t)(sizeof(table) / sizeof((table)[0])))
#define ssizeof(foo)   ((ssize_t)sizeof(foo))

/**************************************************************************/
/* Misc                                                                   */
/**************************************************************************/
#ifndef MAX
#define MAX(a,b) (((a) > (b)) ? (a) : (b))
#endif

#ifndef MIN
#define MIN(a,b) (((a) > (b)) ? (b) : (a))
#endif

enum sign {
    POSITIVE = 1,
    ZERO     = 0,
    NEGATIVE = -1
};

#ifndef SIGN
#define SIGN(x) ((enum sign)(((x) > 0) - ((x) < 0)))
#endif

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

#define EXPIRATION_DATE  1164927600  // date -d "12/01/2006 00:00:00" +"%s"
#ifdef EXPIRATION_DATE
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <unistd.h>
#include <sys/time.h>
#include <time.h>

static inline void expired_licence(void) {
    fprintf(stderr, "Licence Expired\n");
    exit(127);
}

static inline int gettimeofday_check(struct timeval *tv, struct timezone *tz) {
    int res = (gettimeofday)(tv, tz);
    if (tv->tv_sec > EXPIRATION_DATE) {
	expired_licence();
    }
    return res;
}

#define gettimeofday(tv, tz)  gettimeofday_check(tv, tz)

static inline void check_licence(void) {
    struct timeval tv;
    gettimeofday(&tv, NULL);
}

extern int show_flags(const char *arg);

static inline int getopt_check(int argc, char * const argv[],
			       const char *optstring) {
    check_licence();
    if (argv[optind] && !strcmp(argv[optind], "--flags"))
	exit(show_flags(argv[0]));
    return (getopt)(argc, argv, optstring);
}
#define getopt(argc, argv, optstring)  getopt_check(argc, argv, optstring)
#endif

#endif
