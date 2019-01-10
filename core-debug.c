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

#ifndef NDEBUG /* disable that module if debug is disabled */

#include <fnmatch.h>
#include "thr.h"
#include "container.h"
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

qm_k64_t(trace, int);

struct trace_spec_t {
    const char *path;
    const char *func;
    int level;
};
qvector_t(spec, struct trace_spec_t);

static struct {
    qv_t(spec)  specs;
    qm_t(trace) cache;

    int verbosity_level, max_level;

    int maxlen, rows, cols;
    bool fancy;
} _G = {
    .verbosity_level = 0,
    .cache           = QM_INIT(trace, _G.cache, false),
};

static __thread sb_t buf_g;
static __thread sb_t tmpbuf_g;

static void on_sigwinch(int signo)
{
    term_get_size(&_G.cols, &_G.rows);
}

static void e_debug_initialize_thread(void)
{
    sb_init(&buf_g);
    sb_init(&tmpbuf_g);
}
static void e_debug_shutdown_thread(void)
{
    sb_wipe(&buf_g);
    sb_wipe(&tmpbuf_g);
}
thr_hooks(e_debug_initialize_thread, e_debug_shutdown_thread);

__attribute__((constructor))
static void e_debug_initialize(void)
{
    char *p;

    qv_init(spec, &_G.specs);
    _G.fancy = is_fancy_fd(STDERR_FILENO);
    if (_G.fancy) {
        term_get_size(&_G.cols, &_G.rows);
        signal(SIGWINCH, &on_sigwinch);
    }

    e_debug_initialize_thread();

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
        qv_append(spec, &_G.specs, spec);
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
    static spinlock_t spin;
    uint64_t uuid;
    int32_t pos, tr_level;

    if (level > _G.max_level)
        return false;

    uuid = e_trace_uuid(modname, func);
    spin_lock(&spin);
    pos  = qm_find(trace, &_G.cache, uuid);
    if (unlikely(pos < 0)) {
        tr_level = e_find_level(modname, func);
        qm_add(trace, &_G.cache, uuid, tr_level);
    } else {
        tr_level = _G.cache.values[pos];
    }
    spin_unlock(&spin);
    return level <= tr_level;
}

static void e_trace_put_fancy(int level, const char *module, int lno, const char *func)
{
    char escapes[BUFSIZ];
    int len, cols = _G.cols;

    sb_setf(&tmpbuf_g, "%s:%d:%s", module, lno,
            program_invocation_short_name);
    if (tmpbuf_g.len > cols - 2)
        sb_shrink(&tmpbuf_g, tmpbuf_g.len - cols - 2);
    len = snprintf(escapes, sizeof(escapes), "\r\e[%dC\e[7m ", cols - 2 - tmpbuf_g.len);
    sb_splice(&tmpbuf_g, 0, 0, escapes, len);
    sb_adds(&tmpbuf_g, " \e[0m\r");

    len = strlen(func);
#define FUN_WIDTH 20
    if (strlen(func) < FUN_WIDTH) {
        sb_addf(&tmpbuf_g, "\e[33m%*s\e[0m ", FUN_WIDTH, func);
    } else {
        sb_addf(&tmpbuf_g, "\e[33m%.*s...\e[0m ", FUN_WIDTH - 3, func);
    }
}

static void e_trace_put_normal(int level, const char *module, int lno, const char *func)
{
    sb_setf(&tmpbuf_g, "%d %s:%d:%s: ", level, module, lno, func);
}

void e_trace_put(int level, const char *module, int lno,
                 const char *func, const char *fmt, ...)
{
    const char *p;
    va_list ap;

    if (e_is_traced_real(level, module, func)) {
        va_start(ap, fmt);
        sb_addvf(&buf_g, fmt, ap);
        va_end(ap);

        while ((p = memchr(buf_g.data, '\n', buf_g.len))) {
            if (_G.fancy) {
                e_trace_put_fancy(level, module, lno, func);
            } else {
                e_trace_put_normal(level, module, lno, func);
            }
            sb_add(&tmpbuf_g, buf_g.data, p + 1 - buf_g.data);
            sb_skip_upto(&buf_g, p + 1);
            IGNORE(xwrite(STDERR_FILENO, tmpbuf_g.data, tmpbuf_g.len));
        }
    }
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
