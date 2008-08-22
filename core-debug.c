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
#include "container.h"
#include "str.h"
#include "blob.h"
#include "unix.h"

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
    blob_t      buf, tmpbuf;

    int verbosity_level, max_level;

    int maxlen, rows, cols;
    bool fancy;
} _G = {
    .verbosity_level = 1,
};

static void on_sigwinch(int signo)
{
    term_get_size(&_G.cols, &_G.rows);
}


__attribute__((constructor))
static void e_debug_initialize(void)
{
    char *p;

    trace_htbl_init(&_G.cache);
    spec_vector_init(&_G.specs);
    blob_init(&_G.buf);
    blob_init(&_G.tmpbuf);
    _G.fancy = is_fancy_fd(STDERR_FILENO);
    if (_G.fancy) {
        term_get_size(&_G.cols, &_G.rows);
        signal(SIGWINCH, &on_sigwinch);
    }

    p = getenv("IS_DEBUG");
    if (!p)
        return;
    p = p_strdup(skipspaces(p));

    /*
     * parses blank separated <specs>
     * <specs> ::= [<path-pattern>][@<funcname>][:<level>]
     */
    while (*p) {
        struct trace_spec_t spec = {
            .path = NULL,
            .func = NULL,
            .level = INT_MAX,
        };
        int len;

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
        }
        if (*p)
            *p++ = '\0';

        _G.max_level = MAX(_G.max_level, spec.level);
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
    static int spin;
    struct trace_record_t tr, *trp;
    uint64_t uuid;
    bool result;

    if (level > _G.max_level)
        return false;

    spin_lock(&spin);
    uuid = e_trace_uuid(modname, func);
    trp  = trace_htbl_find(&_G.cache, uuid);
    if (unlikely(trp == NULL)) {
        tr.uuid  = uuid;
        tr.level = e_find_level(modname, func);
        trace_htbl_insert(&_G.cache, tr);
        trp = &tr;
    }
    result = level <= trp->level;
    spin_unlock(&spin);
    return result;
}

static void e_trace_put_fancy(int level, const char *module, int lno, const char *func)
{
    char escapes[BUFSIZ];
    int len, cols = _G.cols;

    blob_set_fmt(&_G.tmpbuf, "%s:%d", module, lno);
    if (_G.tmpbuf.len > cols - 2)
        blob_kill_last(&_G.tmpbuf, _G.tmpbuf.len - cols - 2);
    len = snprintf(escapes, sizeof(escapes), "\r\e[%dC\e[7m ", cols - 2 - _G.tmpbuf.len);
    blob_insert_data(&_G.tmpbuf, 0, escapes, len);
    blob_append_cstr(&_G.tmpbuf, " \e[0m\r");

    len = strlen(func);
#define FUN_WIDTH 20
    if (strlen(func) < FUN_WIDTH) {
        blob_append_fmt(&_G.tmpbuf, "\e[33m%*s\e[0m ", FUN_WIDTH, func);
    } else {
        blob_append_fmt(&_G.tmpbuf, "\e[33m%.*s...\e[0m ", FUN_WIDTH - 3, func);
    }
}

static void e_trace_put_normal(int level, const char *module, int lno, const char *func)
{
    blob_set_fmt(&_G.tmpbuf, "%d %s:%d:%s: ", level, module, lno, func);
}

void e_trace_put(int level, const char *module, int lno,
                 const char *func, const char *fmt, ...)
{
    static int spin;
    const char *p;
    va_list ap;

    spin_lock(&spin);

    if (e_is_traced_real(level, module, func)) {
        va_start(ap, fmt);
        blob_append_vfmt(&_G.buf, fmt, ap);
        va_end(ap);

        while ((p = memchr(_G.buf.data, '\n', _G.buf.len))) {
            if (_G.fancy) {
                e_trace_put_fancy(level, module, lno, func);
            } else {
                e_trace_put_normal(level, module, lno, func);
            }
            blob_append_data(&_G.tmpbuf, _G.buf.data, p + 1 - blob_get_cstr(&_G.buf));
            blob_kill_at(&_G.buf, p + 1);
            IGNORE(xwrite(STDERR_FILENO, _G.tmpbuf.data, _G.tmpbuf.len));
        }
    }

    spin_unlock(&spin);
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
