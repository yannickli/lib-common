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

#ifndef NDEBUG /* disable that module if debug is disabled */

#include "str.h"
#include "htbl.h"

/*
 * The debug module uses a brutally-memoize-answers approach.
 *
 *   (1) There aren't that many [__FILE__,__func__] pairs,
 *   (2) Those points into our binary that is less than gigabyte big.
 *
 *   Hence the lower 32bits from the __FILE__ string and the lower 32 bits
 *   from the __func__ one combined are a unique id we can use in a hashtable,
 *   to remember the answer we previously gave.
 */

static int verbosity_level;

typedef struct trace_entry {
    struct trace_entry *next;

    int level;

    char *funcname;
    char modname[];
} trace_entry;

struct trace_record_t {
    uint64_t uuid;
    int minlevel;
};
DO_HTBL_KEY(struct trace_record_t, uint64_t, trace, uuid);

static struct {
    struct trace_entry *e_watches;
    trace_htbl cache;
} _G;

__attribute__((constructor))
static void e_debug_initialize(void)
{
    const char *p;

    p = getenv("IS_DEBUG");
    if (!p)
        return;

    trace_htbl_init(&_G.cache);

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

        e->next   = _G.e_watches;
        _G.e_watches = e;

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

static int e_find_minlevel(const char *modname, const char *func)
{
    for (trace_entry *e = _G.e_watches; e; e = e->next) {
        if (strequal(modname, e->modname)
        && (!e->funcname[0] || strequal(func, e->funcname))) {
            return e->level;
        }
    }
    return INT_MIN;
}

static uint64_t e_trace_uuid(const char *modname, const char *func)
{
    uint32_t mod_lo = (uintptr_t)modname;
    uint32_t fun_lo = (uintptr_t)func;
    return MAKE64(mod_lo, fun_lo);
}

bool e_is_traced_real(int level, const char *modname, const char *func)
{
    struct trace_record_t tr, *trp;
    uint64_t uuid;

    if (level <= verbosity_level)
        return true;

    uuid = e_trace_uuid(modname, func);
    trp  = trace_htbl_find(&_G.cache, uuid);
    if (unlikely(trp == NULL)) {
        tr.uuid     = uuid;
        tr.minlevel = e_find_minlevel(modname, func);
        trace_htbl_insert(&_G.cache, tr);
        trp = &tr;
    }
    return level <= trp->minlevel;
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
