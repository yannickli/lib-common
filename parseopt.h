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

enum popt_kind {
    OPTION_END,
    OPTION_FLAG,
    OPTION_INT,
    OPTION_STR,
    OPTION_GROUP,
};

enum popt_options {
    POPT_STOP_AT_NONARG = (1 << 0),
};

typedef struct popt_t {
    enum popt_kind kind;
    int shrt;
    const char *lng;
    void *value;
    intptr_t init;
    const char *help;
} popt_t;

#define OPT_FLAG(s, l, v, h)   { OPTION_FLAG, (s), (l), (v), 0, (h) }
#define OPT_STR(s, l, v, h)    { OPTION_STR, (s), (l), (v), 0, (h) }
#define OPT_INT(s, l, v, h)    { OPTION_INT, (s), (l), (v), 0, (h) }
#define OPT_GROUP(h)           { OPTION_GROUP, 0, NULL, NULL, 0, (h) }
#define OPT_END()              { OPTION_END, 0, NULL, NULL, 0, NULL }

int parseopt(int argc, char **argv, popt_t *opts, int flags);
__attribute__((noreturn))
void makeusage(int ret, const char *arg0, const char *usage,
               const char * const text[], popt_t *opts);

#endif
