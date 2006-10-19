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

#include <stdint.h>
#include <stdio.h>
#include <limits.h>
#include <stdlib.h>
#include <ctype.h>

#include "macros.h"
#include "mem.h"
#include "err_report.h"
#include "string_is.h"


#ifndef NDEBUG /* disable that module if debug is disabled */

int e_verbosity_level    = 0;
int e_verbosity_maxwatch = INT_MAX;

typedef struct trace_entry {
    struct trace_entry *next;
    uint32_t hash;
    int level;
    const char data[];
} trace_entry;

static trace_entry *e_watches;

static inline uint32_t hash(uint32_t init, const char *p) {
    uint32_t result = init;

    if (p) {
        while (*p) {
            result  = (result << 3) | (result >> (32-3));
            result += *p++;
        }
    }

    return result;
}

static void e_debug_initialize(void)
{
    static bool initialized = false;
    const char *p;

    if (initialized)
        return;

    e_verbosity_maxwatch = e_verbosity_level;

    p = getenv("IS_DEBUG");

    /*
     * parses blank separated <specs>
     * <specs> ::= <modulename>[/<funcname>][:<level>]
     */
    while (p && *p) {
        const char *q;
        trace_entry *e;

        while (isspace(*p)) {
            p++;
        }

        q = p;

        while (*q && !isspace(*q) && *q != ':') {
            q++;
        }

        e = mem_alloc0(sizeof(trace_entry) + q - p + 1);

        memcpy(&e->data, p, q - p);
        e->hash  = hash(0, e->data);
        e->level = INT_MAX;

        e->next   = e_watches;
        e_watches = e;

        if (*q == ':') {
            q++;
            e->level = strtol(q, &q, 10);
            while (*q && !isspace(*q)) {
                q++;
            }
        }

        e_verbosity_maxwatch = MAX(e_verbosity_maxwatch, e->level);

        p = q;
    }

    initialized = true;
}

bool e_is_traced_real(int level, const char *fname, const char *func)
{
    uint32_t hash1 = hash(0, fname);
    uint32_t hash2 = hash(hash(hash1, "/"), func);
    char buf[PATH_MAX];
    trace_entry *e;

    e_debug_initialize();

    if (level <= e_verbosity_level)
        return true;

    for (e = e_watches; e; e = e->next) {
        if (e->hash == hash1 && strequal(e->data, fname))
            return level <= e->level;
    }

    snprintf(buf, sizeof(buf), "%s/%s", fname, func);
    for (e = e_watches; e; e = e->next) {
        if (e->hash == hash2 && strequal(e->data, buf))
            return level <= e->level;
    }

    return false;
}


void e_set_verbosity(int max_debug_level)
{
    e_verbosity_level = max_debug_level;
    e_verbosity_maxwatch = MAX(e_verbosity_maxwatch, e_verbosity_level);
}

void e_incr_verbosity(void)
{
    e_verbosity_level++;
    e_verbosity_maxwatch = MAX(e_verbosity_maxwatch, e_verbosity_level);
}

#endif
