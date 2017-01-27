/**************************************************************************/
/*                                                                        */
/*  Copyright (C) 2004-2017 INTERSEC SA                                   */
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
#if defined(__clang__) && __clang_major__ >= 4
#pragma GCC diagnostic ignored "-Wnullability-completeness-on-arrays"
#endif
#endif

SWIFT_ENUM(popt_kind) {
    OPTION_END __swift_name__("end"),
    OPTION_FLAG __swift_name__("flag"),
    OPTION_INT __swift_name__("int"),
    OPTION_UINT __swift_name__("uint"),
    OPTION_STR __swift_name__("str"),
    OPTION_GROUP __swift_name__("group"),
    OPTION_CHAR __swift_name__("char"),
    OPTION_VERSION __swift_name__("version"),
};

SWIFT_OPTIONS(popt_options) {
    POPT_STOP_AT_NONARG = (1 << 0),
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

#if __has_feature(nullability)
#pragma GCC diagnostic pop
#endif

#endif
