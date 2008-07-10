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

#include "str.h"

#ifndef NDEBUG /* disable that module if debug is disabled */

static int  verbosity_level = 0;

typedef struct trace_entry {
    struct trace_entry *next;

    int level;

    char *funcname;
    char modname[];
} trace_entry;

static trace_entry *e_watches;

__attribute__((constructor))
static void e_debug_initialize(void)
{
    const char *p;

    p = getenv("IS_DEBUG");
    if (!p)
        return;

    /*
     * parses blank separated <specs>
     * <specs> ::= <modulename>[@<funcname>][:<level>]
     */
    while (*p) {
        const char *q;
        trace_entry *e;

        q = p = skipspaces(p);

        while (*q && !isspace((unsigned char)*q) && *q != ':') {
            q++;
        }

        e = p_new_extra(trace_entry, q - p + 1);
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
        p = q;
    }
}

bool e_is_traced_real(int level, const char *modname, const char *func)
{
    trace_entry *e;

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
}

void e_incr_verbosity(void)
{
    verbosity_level++;
}

#endif
