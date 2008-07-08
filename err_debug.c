/**************************************************************************/
/*                                                                        */
/*  Copyright (C) 2004-2008 INTERSEC SAS                                  */
/*                                                                        */
/*  Should you receive a copy of this source code, you must check you     */
/*  have a proper, written authorization of INTERSEC to hold it. If you   */
/*  don't have such an authorization, you must DELETE all source code     */
/*  files in your possession, and inform INTERSEC of the fact you obtain  */
/*  these files. Should you not comply to these terms, you can be         */
/*  prosecuted in the extent permitted by applicable law.                 */
/*                                                                        */
/**************************************************************************/

#include "err_report.h"
#include "str.h"

#ifndef NDEBUG /* disable that module if debug is disabled */

int e_verbosity_maxwatch = INT_MAX;

static int  verbosity_level = 0;
static bool e_initialized     = false;

typedef struct trace_entry {
    struct trace_entry *next;

    int level;

    char *funcname;
    char modname[];
} trace_entry;

static trace_entry *e_watches;

static void e_debug_initialize(void)
{
    const char *p;

    if (e_initialized)
        return;

    e_verbosity_maxwatch = verbosity_level;

    p = getenv("IS_DEBUG");
    if (!p) {
        e_initialized = true;
        return;
    }

    /*
     * parses blank separated <specs>
     * <specs> ::= <modulename>[@<funcname>][:<level>]
     */
    while (*p) {
        const char *q;
        trace_entry *e;

        while (isspace((unsigned char)*p)) {
            p++;
        }

        q = p;

        while (*q && !isspace((unsigned char)*q) && *q != ':') {
            q++;
        }

        e = mem_alloc0(sizeof(trace_entry) + q - p + 1);

        memcpy(&e->modname, p, q - p);
        e->level = INT_MAX;

        e->next   = e_watches;
        e_watches = e;

        e->funcname = strchr(e->modname, '@');
        if (e->funcname) {
            *e->funcname++ = '\0';
        } else {
            e->funcname = e->modname + (q - p);
        }

        if (*q == ':') {
            q++;
            e->level = strtol(q, &q, 10);
            while (*q && !isspace((unsigned char)*q)) {
                q++;
            }
        }

        e_verbosity_maxwatch = MAX(e_verbosity_maxwatch, e->level);

        p = q;
    }

    e_initialized = true;
}

bool e_is_traced_real(int level, const char *modname, const char *func)
{
    trace_entry *e;

    e_debug_initialize();

    if (level <= verbosity_level)
        return true;

    for (e = e_watches; e; e = e->next) {
        if (strequal(modname, e->modname)
        && (!e->funcname[0] || strequal(func, e->funcname))) {
            return level <= e->level;
        }
    }

    return false;
}


void e_set_verbosity(int max_debug_level)
{
    verbosity_level = max_debug_level;
    e_verbosity_maxwatch = MAX(e_verbosity_maxwatch, verbosity_level);
}

void e_incr_verbosity(void)
{
    verbosity_level++;
    e_verbosity_maxwatch = MAX(e_verbosity_maxwatch, verbosity_level);
}

#endif
