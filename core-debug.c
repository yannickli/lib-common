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

#include <fnmatch.h>
#include "array.h"
#include "htbl.h"
#include "str.h"

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

struct trace_record_t {
    uint64_t uuid;
    int level;
};
DO_HTBL_KEY(struct trace_record_t, uint64_t, trace, uuid);

struct trace_spec_t {
    const char *path;
    const char *func;
    int level;
};
DO_VECTOR(struct trace_spec_t, spec);

static struct {
    spec_vector specs;
    trace_htbl  cache;
    int verbosity_level;
} _G = {
    .verbosity_level = 1,
};

__attribute__((constructor))
static void e_debug_initialize(void)
{
    char *p;

    trace_htbl_init(&_G.cache);
    spec_vector_init(&_G.specs);

    p = getenv("IS_DEBUG");
    if (!p)
        return;
    p = p_strdup(skipspaces(p));

    /*
     * parses blank separated <specs>
     * <specs> ::= [<path-pattern>][@<funcname>][:<level>]
     */
    while (*p) {
        struct trace_spec_t spec;
        int len;

        p_clear(&spec, 1);

        len = strcspn(p, "@: \t\r\n\v\f");
        if (len)
            spec.path = p;
        p += len;
        if (*p == '@') {
            *p++ = '\0';
            spec.func = p;
            p += strcspn(p, ": \t\r\n\v\f");
        }
        if (*p == ':') {
            *p++ = '\0';
            spec.level = vstrtoip(p, &p);
            p = vstrnextspace(p);
        } else {
            spec.level = INT_MAX;
        }
        if (*p)
            *p++ = '\0';

        spec_vector_append(&_G.specs, spec);
    }
}

static int e_find_level(const char *modname, const char *func)
{
    int level = _G.verbosity_level;

    for (int i = 0; i < _G.specs.len; i++) {
        struct trace_spec_t *spec = &_G.specs.tab[i];

        if (spec->path && fnmatch(spec->path, modname, FNM_PATHNAME) != 0)
            continue;
        if (spec->func && fnmatch(spec->func, func, 0) != 0)
            continue;
        level = spec->level;
    }
    return level;
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

    uuid = e_trace_uuid(modname, func);
    trp  = trace_htbl_find(&_G.cache, uuid);
    if (unlikely(trp == NULL)) {
        tr.uuid  = uuid;
        tr.level = e_find_level(modname, func);
        trace_htbl_insert(&_G.cache, tr);
        trp = &tr;
    }
    return level <= trp->level;
}

void e_set_verbosity(int max_debug_level)
{
    _G.verbosity_level = max_debug_level;
}

void e_incr_verbosity(void)
{
    _G.verbosity_level++;
}

#endif
