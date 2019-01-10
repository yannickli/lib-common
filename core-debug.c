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
#include "container.h"
#include "unix.h"
#include "thr.h"

struct trace_spec_t {
    const char *path;
    const char *func;
    const char *name;
    int level;
};
qvector_t(spec, struct trace_spec_t);

static struct {
    qv_t(spec)  specs;

    int verbosity_level, max_level;

    int maxlen, rows, cols;
    bool fancy;
} _G = {
    .verbosity_level = 0,
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

    /* XXX This string is "leaked" because we could need debug information
     *     written in it *ANYTIME* (including at shutdown).
     *
     *     If related valgrind error is boring you, please add
     *         --suppressions=<path-to-lib-common>/lib-common.supp
     *     to your valgrind command line.
     */
    p = p_strdup(skipspaces(p));

    /*
     * parses blank separated <specs>
     * <specs> ::= [<path-pattern>][@<funcname>][+<featurename>][:<level>]
     */
    while (*p) {
        struct trace_spec_t spec = {
            .path = NULL,
            .func = NULL,
            .name = NULL,
            .level = INT_MAX,
        };
        int len;

        len = strcspn(p, "@+: \t\r\n\v\f");
        if (len)
            spec.path = p;
        p += len;
        if (*p == '@') {
            *p++ = '\0';
            spec.func = p;
            p += strcspn(p, "+: \t\r\n\v\f");
        }
        if (*p == '+') {
            *p++ = '\0';
            spec.name = p;
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

int e_is_traced_(int lvl, const char *modname, const char *func,
                 const char *name)
{
    int level = _G.verbosity_level;

    if (lvl > _G.max_level)
        return -1;

    for (int i = 0; i < _G.specs.len; i++) {
        struct trace_spec_t *spec = &_G.specs.tab[i];

        if (spec->path && fnmatch(spec->path, modname, FNM_PATHNAME) != 0)
            continue;
        if (spec->func && fnmatch(spec->func, func, 0) != 0)
            continue;
        if (spec->name && (name == NULL || fnmatch(spec->name, name, FNM_PATHNAME | FNM_LEADING_DIR) != 0))
            continue;
        level = spec->level;
    }
    if (lvl > level)
        return -1;
    return 1;
}

static void e_trace_put_fancy(int level, const char *name,
                              const char *module, int lno, const char *func)
{
    char escapes[BUFSIZ];
    int len, cols = _G.cols;

    sb_setf(&tmpbuf_g, "%s:%d:%s", module, lno,
            program_invocation_short_name);
    if (tmpbuf_g.len > cols - 2)
        sb_clip(&tmpbuf_g, cols - 2);
    len = snprintf(escapes, sizeof(escapes), "\r\e[%dC\e[7m ", cols - 2 - tmpbuf_g.len);
    sb_splice(&tmpbuf_g, 0, 0, escapes, len);
    sb_adds(&tmpbuf_g, " \e[0m\r");

#define FUN_WIDTH 20
    if (strlen(func) < FUN_WIDTH) {
        sb_addf(&tmpbuf_g, "\e[33m%*s\e[0m ", FUN_WIDTH, func);
    } else {
        sb_addf(&tmpbuf_g, "\e[33m%*pM...\e[0m ", FUN_WIDTH - 3, func);
    }
    if (name)
        sb_addf(&tmpbuf_g, "\e[1;30m{%s}\e[0m ", name);
}

static void e_trace_put_normal(int level, const char *name,
                               const char *module, int lno, const char *func)
{
    if (name) {
        sb_setf(&tmpbuf_g, "%d %s:%d:%s:{%s} ", level, module, lno, func, name);
    } else {
        sb_setf(&tmpbuf_g, "%d %s:%d:%s: ", level, module, lno, func);
    }
}

void e_trace_put_(int level, const char *module, int lno,
                  const char *func, const char *name, const char *fmt, ...)
{
    const char *p;
    va_list ap;

    va_start(ap, fmt);
    sb_addvf(&buf_g, fmt, ap);
    va_end(ap);

    while ((p = memchr(buf_g.data, '\n', buf_g.len))) {
        if (_G.fancy) {
            e_trace_put_fancy(level, name, module, lno, func);
        } else {
            e_trace_put_normal(level, name, module, lno, func);
        }
        sb_add(&tmpbuf_g, buf_g.data, p + 1 - buf_g.data);
        sb_skip_upto(&buf_g, p + 1);
        IGNORE(xwrite(STDERR_FILENO, tmpbuf_g.data, tmpbuf_g.len));
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
