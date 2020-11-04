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

#ifndef IS_LIB_COMMON_PARSEOPT_H
#define IS_LIB_COMMON_PARSEOPT_H

#include "core.h"

#if __has_feature(nullability)
#pragma GCC diagnostic push
#pragma GCC diagnostic error "-Wnullability-completeness"
#if __has_warning("-Wnullability-completeness-on-arrays")
#pragma GCC diagnostic ignored "-Wnullability-completeness-on-arrays"
#endif
#endif

enum popt_kind {
    OPTION_END,
    OPTION_FLAG,
    OPTION_INT,
    OPTION_UINT,
    OPTION_STR,
    OPTION_GROUP,
    OPTION_CHAR,
    OPTION_VERSION,
};

enum popt_options {
    /** Stop as soon as a non option argument is found. */
    POPT_STOP_AT_NONARG =      (1 << 0),

    /** Ignore all unknown options, they will be left in argv. */
    POPT_IGNORE_UNKNOWN_OPTS = (1 << 1),
};

typedef struct popt_t {
    enum popt_kind kind;
    int shrt;
    const char * nullable lng;
    void * nullable value;
    intptr_t init;
    const char * nullable help;
    size_t int_vsize;
} popt_t;

#define OPT_FLAG(s, l, v, h)   { OPTION_FLAG, (s), (l), (v), 0, (h),         \
                                 sizeof(*(v)) }
#define OPT_STR(s, l, v, h)    { OPTION_STR, (s), (l), (v), 0, (h), 0 }
#define OPT_INT(s, l, v, h)    { OPTION_INT, (s), (l), (v), 0, (h),          \
                                 sizeof(*(v)) }
#define OPT_UINT(s, l, v, h)   { OPTION_UINT, (s), (l), (v), 0, (h),         \
                                 sizeof(*(v)) }
#define OPT_CHAR(s, l, v, h)   { OPTION_CHAR, (s), (l), (v), 0, (h), 0 }
#define OPT_GROUP(h)           { OPTION_GROUP, 0, NULL, NULL, 0, (h), 0 }
#define OPT_END()              { OPTION_END, 0, NULL, NULL, 0, NULL, 0 }

/* If "name" or "f" is NULL, then the core versions are printed
 * (cf. core-stdlib.h). */
#define OPT_VERSION(name, f)   { OPTION_VERSION, 'V', "version",             \
                                 (void *)(name), (intptr_t)(f),              \
                                 "show version information", 0 }

int parseopt(int argc, char * nullable * nonnull argv,
             popt_t * nonnull opts, int flags);
__attribute__((noreturn))
void makeusage(int ret, const char * nonnull arg0, const char * nonnull usage,
               const char * nullable const text[], popt_t * nonnull opts);
__attribute__((noreturn))
void makeversion(int ret, const char * nullable name,
                 const char * nonnull (* nullable get_version)(void));

/** Parse an integer argument (supposedly positional).
 *
 * Use the internal parseopt parsers to read an integer.
 *
 * \example  if (parseopt_geti(NEXTARG(argc, argv), "MYARG", &val) < 0) {
 *               goto usage;
 *           }
 *
 * Note: negative values may be recognized as unknown short-form options by
 * parseopt() (FIXME).
 * In that case, the caller can use the "--" (no more options) marker.
 */
int parseopt_geti(const char *nonnull arg, const char *nonnull param_name,
                  int *nonnull val);

/** Parse an unsigned integer argument (supposedly positional). */
int parseopt_getu(const char *nonnull arg, const char *nonnull param_name,
                  unsigned *nonnull val);

#if __has_feature(nullability)
#pragma GCC diagnostic pop
#endif

#endif
