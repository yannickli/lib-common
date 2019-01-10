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

#include <fnmatch.h>
#include "unix.h"
#include "thr.h"
#include "datetime.h"

#include "log.h"

uint32_t log_conf_gen_g = 1;
log_handler_f *log_stderr_handler_g;
int log_stderr_handler_teefd_g = -1;

struct level {
    int level;
    unsigned flags;
};
qm_kvec_t(level, lstr_t, struct level, qhash_lstr_hash, qhash_lstr_equal);


struct trace_spec_t {
    const char *path;
    const char *func;
    const char *name;
    int level;
};
qvector_t(spec, struct trace_spec_t);


#ifndef NDEBUG
#define LOG_DEFAULT  LOG_TRACE
#else
#define LOG_DEFAULT  LOG_DEBUG
#endif

static log_handler_f log_stderr_raw_handler;

static struct {
    logger_t root_logger;
    e_handler_f   *e_handler;
    log_handler_f *handler;

    char          *is_debug;
    qm_t(level)    pending_levels;

    bool fancy;
    char fancy_prefix[64];
    int fancy_len;

    qv_t(spec)  specs;
    int maxlen, rows, cols;
    int pid;
} log_g = {
#define _G  log_g
    .root_logger = {
        .level         = LOG_DEFAULT,
        .defined_level = LOG_UNDEFINED,
        .default_level = LOG_DEFAULT,
        .name          = LSTR_EMPTY,
        .full_name     = LSTR_EMPTY,
        .parent        = NULL,
        .children      = DLIST_INIT(_G.root_logger.children),
        .siblings      = DLIST_INIT(_G.root_logger.siblings)
    },
    .pending_levels = QM_INIT(level, _G.pending_levels, false),
    .handler        = &log_stderr_raw_handler,
};

static __thread struct {
    bool inited;
    sb_t log;
    sb_t buf;
} log_thr_g;

/* Configuration {{{ */

logger_t *logger_init(logger_t *logger, logger_t *parent, lstr_t name,
                      int default_level, unsigned level_flags)
{
    p_clear(logger, 1);
    logger->level = LOG_UNDEFINED;
    logger->defined_level = LOG_UNDEFINED;
    logger->default_level = default_level;
    logger->level_flags         = level_flags;
    logger->default_level_flags = level_flags;
    dlist_init(&logger->siblings);
    dlist_init(&logger->children);

    logger->parent = parent;
    logger->name   = lstr_dupc(name);
    __logger_refresh(logger);

    return logger;
}

logger_t *logger_new(logger_t *parent, lstr_t name, int default_level,
                     unsigned level_flags)
{
    return logger_init(p_new_raw(logger_t, 1), parent, name, default_level,
                       level_flags);
}

/* Suppose the parent is locked */
static void logger_wipe_child(logger_t *logger)
{
    spin_lock(&logger->children_lock);
    if (!dlist_is_empty(&logger->children) && logger->children.next) {
        logger_t *child;

        dlist_for_each_entry_safe(child, &logger->children, siblings) {
            if (child->is_static) {
                logger_wipe_child(child);
            } else {
#ifndef NDEBUG
                logger_panic(&_G.root_logger,
                             "leaked logger %*pM, cannot wipe %*pM",
                             LSTR_FMT_ARG(child->full_name),
                             LSTR_FMT_ARG(logger->full_name));
#endif
            }
        }
    }
    spin_unlock(&logger->children_lock);

    if (!dlist_is_empty(&logger->siblings) && logger->siblings.next) {
        dlist_remove(&logger->siblings);
    }
    lstr_wipe(&logger->name);
    lstr_wipe(&logger->full_name);
}

void logger_wipe(logger_t *logger)
{
    if (logger->parent) {
        spin_lock(&logger->parent->children_lock);
    }
    logger_wipe_child(logger);
    if (logger->parent) {
        spin_unlock(&logger->parent->children_lock);
    }
}

static void logger_compute_fullname(logger_t *logger)
{
    /* The name of a logger must be a non-empty printable string
     * without any '/' or '!'
     */
    assert (memchr(logger->name.s, '/', logger->name.len) == NULL);
    assert (memchr(logger->name.s, '!', logger->name.len) == NULL);
    assert (logger->name.len);
    for (int i = 0; i < logger->name.len; i++) {
        assert (isprint((unsigned char)logger->name.s[i]));
    }

    if (logger->parent->full_name.len) {
        lstr_t name = logger->name;
        lstr_t full_name;

        full_name = lstr_fmt("%*pM/%*pM",
                             LSTR_FMT_ARG(logger->parent->full_name),
                             LSTR_FMT_ARG(logger->name));

        logger->full_name = full_name;
        logger->name = LSTR_INIT_V(full_name.s + full_name.len - name.len,
                                   name.len);
        lstr_wipe(&name);
    } else
    if (logger->name.len) {
        logger->full_name = lstr_dup(logger->name);
        logger->name      = lstr_dupc(logger->full_name);
    }
}

void __logger_refresh(logger_t *logger)
{
    if (logger->conf_gen == log_conf_gen_g) {
        return;
    }

    logger->conf_gen     = log_conf_gen_g;
    logger->level_flags &= ~LOG_FORCED;

    if (logger->parent == NULL && logger != &_G.root_logger) {
        logger->parent = &_G.root_logger;
    }
    if (logger->parent) {
        __logger_refresh(logger->parent);
    }

    if (!logger->full_name.s) {
        int pos = 0;
        logger_t *sibbling;

        logger_compute_fullname(logger);

        assert (logger->level >= LOG_UNDEFINED);
        assert (logger->default_level >= LOG_INHERITS);
        assert (logger->defined_level >= LOG_UNDEFINED);

        spin_lock(&logger->parent->children_lock);
        dlist_for_each_entry(sibbling, &logger->parent->children, siblings) {
            assert (!lstr_equal2(sibbling->name, logger->name));
        }
        dlist_add(&logger->parent->children, &logger->siblings);
        dlist_init(&logger->children);
        spin_unlock(&logger->parent->children_lock);

        pos = qm_del_key(level, &_G.pending_levels, &logger->full_name);
        if (pos >= 0) {
            logger->defined_level = _G.pending_levels.values[pos].level;
            logger->level_flags   = _G.pending_levels.values[pos].flags;
            lstr_wipe(&_G.pending_levels.keys[pos]);
        }
    }

    logger->level = logger->default_level;

    if (logger->defined_level >= 0) {
        logger->level = logger->defined_level;
    } else
    if (logger->parent) {
        if (logger->parent->level_flags & (LOG_FORCED | LOG_RECURSIVE)) {
            logger->level = logger->parent->level;
            logger->level_flags |= LOG_FORCED;
        } else
        if (logger->level == LOG_INHERITS) {
            logger->level = logger->parent->level;
        }
    }

    assert (logger->level >= 0);
}

static logger_t *logger_get_by_name(lstr_t name)
{
    pstream_t ps = ps_initlstr(&name);
    logger_t *logger = &log_g.root_logger;

    while (!ps_done(&ps)) {
        logger_t *child = NULL;
        logger_t *next  = NULL;
        pstream_t n;

        if (ps_get_ps_chr_and_skip(&ps, '/', &n) < 0) {
            n = ps;
            ps = ps_init(NULL, 0);
        }

        spin_lock(&logger->children_lock);
        dlist_for_each_entry(child, &logger->children, siblings) {
            if (lstr_equal2(child->name, LSTR_PS_V(&n))) {
                next = child;
                break;
            }
        }
        spin_unlock(&logger->children_lock);

        RETHROW_P(next);
        logger = next;
    }

    return logger;
}

int logger_set_level(lstr_t name, int level, unsigned flags)
{
    logger_t *logger = logger_get_by_name(name);

    assert (level >= LOG_UNDEFINED);
    assert ((flags & (LOG_RECURSIVE | LOG_SILENT)) == flags);
    assert (!(flags & LOG_RECURSIVE) || level >= 0);

    if (level == LOG_LEVEL_DEFAULT) {
        level = LOG_DEFAULT;
    }

    if (!logger) {
        int pos;

        if (level == LOG_UNDEFINED) {
            pos = qm_del_key(level, &_G.pending_levels, &name);
            if (pos >= 0) {
                lstr_wipe(&_G.pending_levels.keys[pos]);
            }
        } else {
            struct level l = { .level = level, .flags = flags };

            pos = qm_put(level, &_G.pending_levels, &name, l, QHASH_OVERWRITE);
            if (!(pos & QHASH_COLLISION)) {
                _G.pending_levels.keys[pos] = lstr_dup(name);
            }
        }
        return LOG_UNDEFINED;
    }

    if (level == LOG_UNDEFINED) {
        logger->level_flags = logger->default_level_flags;
    } else {
        logger->level_flags = flags;
    }
    SWAP(int, logger->level, level);
    logger->defined_level = logger->level;
    log_conf_gen_g += 2;
    return level;
}

int logger_reset_level(lstr_t name)
{
    return logger_set_level(name, LOG_UNDEFINED, 0);
}

/* }}} */
/* Logging {{{ */

/* syslog_is_critical allows you to know if a LOG_CRIT, LOG_ALERT or LOG_EMERG
 * logger has been called.
 *
 * It is mainly usefull for destructor function in order to skip some code
 * that shouldn't be run when the system is critical
 */
bool syslog_is_critical;

int logger_vlog(logger_t *logger, int level, const char *prog, int pid,
                const char *file, const char *func, int line,
                const char *fmt, va_list va)
{
    if (level <= LOG_CRIT) {
        va_list cpy;

        syslog_is_critical = true;
        va_copy(cpy, va);
        vsyslog(LOG_USER | level, fmt, cpy);
        va_end(cpy);
    }

    if (logger_has_level(logger, level) || level >= LOG_TRACE) {
        log_ctx_t ctx = {
            .logger_name = lstr_dupc(logger->full_name),
            .level       = level,
            .file        = file,
            .func        = func,
            .line        = line,
            .pid         = pid < 0 ? _G.pid : pid,
            .prog_name   = prog ?: program_invocation_short_name,
            .is_silent   = !!(logger->level_flags & LOG_SILENT),
        };

        (*_G.handler)(&ctx, fmt, va);
    }
    return level <= LOG_WARNING ? -1 : 0;
}

int __logger_log(logger_t *logger, int level, const char *prog, int pid,
                 const char *file, const char *func, int line,
                 const char *fmt, ...)
{
    int res;
    va_list va;

#ifndef NDEBUG
    if (unlikely(level >= LOG_TRACE
              && !logger_is_traced(logger, level - LOG_TRACE)))
    {
        return 0;
    }
#endif

    va_start(va, fmt);
    res = logger_vlog(logger, level, prog, pid, file, func, line, fmt, va);
    va_end(va);

    if (unlikely(level <= LOG_CRIT)) {
        if (psinfo_get_tracer_pid(0) > 0) {
            abort();
        }
        exit(127);
    }

    return res;
}

void __logger_panic(logger_t *logger, const char *file, const char *func,
                    int line, const char *fmt, ...)
{
    va_list va;

    va_start(va, fmt);
    logger_vlog(logger, LOG_CRIT, NULL, -1, file, func, line, fmt, va);
    abort();
}

void __logger_fatal(logger_t *logger, const char *file, const char *func,
                    int line, const char *fmt, ...)
{
    va_list va;

    va_start(va, fmt);
    logger_vlog(logger, LOG_CRIT, NULL, -1, file, func, line, fmt, va);

    if (psinfo_get_tracer_pid(0) > 0) {
        abort();
    }
    exit(127);
}

#ifndef NDEBUG

int __logger_is_traced(logger_t *logger, int lvl, const char *modname,
                       const char *func, const char *name)
{
    int level;

    lvl += LOG_TRACE;
    level = logger->level;

    for (int i = 0; i < _G.specs.len; i++) {
        struct trace_spec_t *spec = &_G.specs.tab[i];

        if (spec->path && fnmatch(spec->path, modname, FNM_PATHNAME) != 0)
            continue;
        if (spec->func && fnmatch(spec->func, func, 0) != 0)
            continue;
        if (spec->name
        &&  (name == NULL
         ||  fnmatch(spec->name, name, FNM_PATHNAME | FNM_LEADING_DIR) != 0))
        {
            continue;
        }
        level = spec->level;
    }
    return lvl > level ? -1 : 1;
}

#endif

/* }}} */
/* Handlers {{{ */

static int log_make_fancy_prefix(const char *progname, int pid,
                                 char fancy[static 64])
{
    static int colors[] = { 1, 2, 4, 5, 6, 9, 12, 14 };
    uint32_t color;
    int len;

    color = mem_hash32(progname, strlen(progname));

    color = colors[color % countof(colors)];
    len = snprintf(fancy, 64, "\e[%d;%dm%10s[%d]\e[0m: ", color >= 10 ? 1 : 0,
                   (color >= 10 ? 20 : 30) + color, progname, pid);
    return MIN(len, 63);
}

__attr_printf__(2, 0)
static void log_stderr_fancy_handler(const log_ctx_t *ctx, const char *fmt,
                                     va_list va)
{
    sb_t *sb  = &log_thr_g.log;
    int max_len = _G.cols - 2;

    if (ctx->is_silent) {
        return;
    }

    if (ctx->level >= LOG_TRACE) {
        int len;
        char escapes[BUFSIZ];

        sb_setf(sb, "%s:%d:%s[%d]", ctx->file, ctx->line,
                ctx->prog_name, ctx->pid);
        if (sb->len > max_len) {
            sb_clip(sb, max_len);
        }
        len = snprintf(escapes, sizeof(escapes), "\r\e[%dC\e[7m ",
                       max_len - sb->len);
        sb_splice(sb, 0, 0, escapes, len);
        sb_adds(sb, " \e[0m\r");
    }

    if (ctx->level >= LOG_TRACE) {
        sb_adds(sb, "\e[33m");
        if (strlen(ctx->func) < 17) {
            sb_addf(sb, "%*s: ", 17, ctx->func);
        } else {
            sb_addf(sb, "%*pM...: ", 14, ctx->func);
        }
    } else {
        if (ctx->prog_name == program_invocation_short_name) {
            sb_add(sb, _G.fancy_prefix, _G.fancy_len);
        } else {
            char fancy[64];

            sb_add(sb, fancy, log_make_fancy_prefix(ctx->prog_name, ctx->pid,
                                                    fancy));
        }
    }
    if (ctx->logger_name.len) {
        sb_addf(sb, "\e[1;30m{%*pM} ", LSTR_FMT_ARG(ctx->logger_name));
    }
    switch (ctx->level) {
      case LOG_DEBUG:
      case LOG_INFO:
        sb_adds(sb, "\e[39;3m");
        break;
      case LOG_WARNING:
        sb_adds(sb, "\e[33;1m");
        break;
      case LOG_ERR:
        sb_adds(sb, "\e[31;1m");
        break;
      case LOG_CRIT:
      case LOG_ALERT:
      case LOG_EMERG:
        sb_adds(sb, "\e[41;37;1m");
        break;
      default:
        sb_adds(sb, "\e[0m");
        break;
    }
    sb_addvf(sb, fmt, va);
    sb_adds(sb, "\e[0m\n");

    fputs(sb->data, stderr);
    if (log_stderr_handler_teefd_g >= 0) {
        IGNORE(xwrite(log_stderr_handler_teefd_g, sb->data, sb->len));
    }
    sb_reset(sb);
}

static void log_initialize_thread(void);

__attr_printf__(2, 0)
static void log_stderr_raw_handler(const log_ctx_t *ctx, const char *fmt,
                                   va_list va)
{
    static char const *prefixes[] = {
        [LOG_CRIT]     = "fatal: ",
        [LOG_ERR]      = "error: ",
        [LOG_WARNING]  = "warn:  ",
        [LOG_NOTICE]   = "note:  ",
        [LOG_INFO]     = "info:  ",
        [LOG_DEBUG]    = "debug: ",
        [LOG_TRACE]    = "trace: ",
    };

    sb_t *sb = &log_thr_g.log;

    if (!log_thr_g.inited) {
        log_initialize_thread();
    }

    if (ctx->is_silent) {
        return;
    }

    sb_addf(sb, "%s[%d]: ", ctx->prog_name, ctx->pid);
    if (ctx->level >= LOG_TRACE && ctx->func) {
        sb_addf(sb, "%s:%d:%s: ", ctx->file, ctx->line, ctx->func);
    } else {
        sb_adds(sb, prefixes[MIN(LOG_TRACE, ctx->level)]);
    }
    if (ctx->logger_name.len) {
        sb_addf(sb, "{%*pM} ", LSTR_FMT_ARG(ctx->logger_name));
    }
    sb_addvf(sb, fmt, va);
    sb_addc(sb, '\n');

    fputs(sb->data, stderr);
    if (log_stderr_handler_teefd_g >= 0) {
        IGNORE(xwrite(log_stderr_handler_teefd_g, sb->data, sb->len));
    }
    sb_reset(sb);
}

log_handler_f *log_set_handler(log_handler_f *handler)
{
    SWAP(log_handler_f *, _G.handler, handler);
    return handler;
}

/* }}} */
/* Backward compatibility e_*() {{{ */

int e_log(int priority, const char *fmt, ...)
{
    int ret;
    va_list va;

    va_start(va, fmt);
    ret = logger_vlog(&_G.root_logger, priority, NULL, -1, NULL, NULL, -1,
                      fmt, va);
    va_end(va);
    return ret;
}

int e_panic(const char *fmt, ...)
{
    va_list va;

    va_start(va, fmt);
    logger_vlog(&_G.root_logger, LOG_CRIT, NULL, -1, NULL, NULL, -1, fmt, va);
    abort();
}

int e_fatal(const char *fmt, ...)
{
    va_list va;

    va_start(va, fmt);
    logger_vlog(&_G.root_logger, LOG_CRIT, NULL, -1, NULL, NULL, -1, fmt, va);

    if (psinfo_get_tracer_pid(0) > 0) {
        abort();
    }
    exit(127);
}

#define E_FUNCTION(Name, Level)                                              \
    int Name(const char *fmt, ...)                                           \
    {                                                                        \
        int ret;                                                             \
        va_list va;                                                          \
                                                                             \
        va_start(va, fmt);                                                   \
        ret = logger_vlog(&_G.root_logger, (Level), NULL, -1, NULL, NULL, -1,\
                          fmt, va);                                          \
        va_end(va);                                                          \
        return ret;                                                          \
    }
E_FUNCTION(e_error,   LOG_ERR)
E_FUNCTION(e_warning, LOG_WARNING)
E_FUNCTION(e_notice,  LOG_NOTICE)
E_FUNCTION(e_info,    LOG_INFO)
E_FUNCTION(e_debug,   LOG_DEBUG)

#undef E_FUNCTION

__attr_printf__(2, 0)
static void e_handler(const log_ctx_t *ctx, const char *fmt, va_list va)
{
    if (ctx->level >= LOG_TRACE) {
        (*log_stderr_handler_g)(ctx, fmt, va);
    } else {
        (*_G.e_handler)(ctx->level, fmt, va);
    }
}

void e_set_handler(e_handler_f *handler)
{
    _G.e_handler = handler;
    log_set_handler(&e_handler);
}

void e_init_stderr(void)
{
    _G.handler = log_stderr_handler_g;
}

#ifndef NDEBUG

void e_set_verbosity(int max_debug_level)
{
    logger_set_level(LSTR_EMPTY_V, LOG_TRACE + max_debug_level, 0);
}

void e_incr_verbosity(void)
{
    logger_set_level(LSTR_EMPTY_V, log_g.root_logger.level + 1, 0);
}

int e_is_traced_(int lvl, const char *modname, const char *func,
                 const char *name)
{
    logger_t *logger;

    logger = logger_get_by_name(LSTR_OPT_STR_V(name)) ?: &log_g.root_logger;
    return __logger_is_traced(logger, lvl, modname, func, name);
}

__attr_printf__(2, 3)
static void e_trace_vput_(const log_ctx_t *ctx, const char *fmt, ...)
{
    va_list va;

    va_start(va, fmt);
    (*_G.handler)(ctx, fmt, va);
    va_end(va);
}

void e_trace_put_(int level, const char *module, int lno,
                  const char *func, const char *name, const char *fmt, ...)
{
    const char *p;
    va_list ap;
    sb_t *buf = &log_thr_g.buf;
    log_ctx_t ctx = {
        .logger_name = LSTR_OPT_STR_V(name),
        .level       = LOG_TRACE + level,
        .file        = module,
        .func        = func,
        .line        = lno,
        .pid         = _G.pid,
        .prog_name   = program_invocation_short_name,
    };

    va_start(ap, fmt);
    sb_addvf(buf, fmt, ap);
    va_end(ap);

    while ((p = memchr(buf->data, '\n', buf->len))) {
        e_trace_vput_(&ctx, "%*pM", (int)(p - buf->data), buf->data);
        sb_skip_upto(buf, p + 1);
    }
}

#endif

/* }}} */
/* Module {{{ */

static void on_sigwinch(int signo)
{
    term_get_size(&_G.cols, &_G.rows);
}

static void log_initialize_thread(void)
{
    if (!log_thr_g.inited) {
        sb_init(&log_thr_g.log);
        sb_init(&log_thr_g.buf);
        log_thr_g.inited = true;
    }
}

static void log_shutdown_thread(void)
{
    if (log_thr_g.inited) {
        sb_wipe(&log_thr_g.buf);
        sb_wipe(&log_thr_g.log);
        log_thr_g.inited = false;
    }
}
thr_hooks(log_initialize_thread, log_shutdown_thread);

static void log_atfork(void)
{
    _G.pid = getpid();
}

static int log_initialize(void* args)
{
    qv_init(spec, &_G.specs);
    _G.fancy = is_fancy_fd(STDERR_FILENO);
    _G.pid   = getpid();
    log_stderr_handler_g = &log_stderr_raw_handler;
    if (_G.fancy) {
        term_get_size(&_G.cols, &_G.rows);
        signal(SIGWINCH, &on_sigwinch);
        log_stderr_handler_g = &log_stderr_fancy_handler;
        _G.fancy_len = log_make_fancy_prefix(program_invocation_short_name,
                                             _G.pid, _G.fancy_prefix);
    }

    _G.handler = log_stderr_handler_g;

    log_initialize_thread();

#ifndef NDEBUG
    {
        char *p = getenv("IS_DEBUG");

        if (!p) {
            return 0;
        }

        _G.is_debug = p = p_strdup(skipspaces(p));

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
                spec.level = LOG_TRACE + vstrtoip(p, &p);
                p = vstrnextspace(p);
            }
            if (*p)
                *p++ = '\0';

            if (!spec.func && !spec.path) {
                logger_set_level(LSTR_OPT_STR_V(spec.name), spec.level, 0);
            }
            qv_append(spec, &_G.specs, spec);
        }
    }
#endif
    return 0;
}

static int log_shutdown(void)
{
    logger_wipe(&_G.root_logger);
    qm_deep_wipe(level, &_G.pending_levels, lstr_wipe, IGNORE);
    qv_wipe(spec, &_G.specs);
    p_delete(&_G.is_debug);
    return 0;
}

__attribute__((constructor))
static void log_module_register(void)
{
    static module_t *log_module;

    thr_hooks_register();
    log_module = module_register(LSTR_IMMED_V("log"), &log_module,
                                 &log_initialize, &log_shutdown, NULL, 0);
    module_add_dep(log_module, LSTR_IMMED_V("thr_hooks"), &thr_hooks_module);
    module_implement_method(log_module, &at_fork_on_child_method,
                            &log_atfork);
    MODULE_REQUIRE(log);
}

/* }}} */
/* Tests {{{ */

#include <lib-common/z.h>

Z_GROUP_EXPORT(log) {
    Z_TEST(log_level, "log") {
        logger_t a = LOGGER_INIT_INHERITS(NULL, "a");
        logger_t b = LOGGER_INIT_SILENT_INHERITS(&a, "b");
        logger_t c = LOGGER_INIT(&b, "c", LOG_ERR);
        logger_t d;

        logger_init(&d, &c, LSTR_IMMED_V("d"), LOG_INHERITS, LOG_SILENT);

        Z_ASSERT_EQ(LOG_ERR, logger_get_level(&d));
        Z_ASSERT_EQ(LOG_ERR, logger_get_level(&c));
        Z_ASSERT_EQ(log_g.root_logger.default_level, logger_get_level(&b));
        Z_ASSERT_EQ(log_g.root_logger.default_level, logger_get_level(&a));
        Z_ASSERT_EQ(log_g.root_logger.default_level,
                    logger_get_level(&log_g.root_logger));

        logger_set_level(LSTR_EMPTY_V, LOG_WARNING, 0);
        Z_ASSERT_EQ(LOG_ERR, logger_get_level(&d));
        Z_ASSERT_EQ(LOG_ERR, logger_get_level(&c));
        Z_ASSERT_EQ(LOG_WARNING, logger_get_level(&b));
        Z_ASSERT_EQ(LOG_WARNING, logger_get_level(&a));
        Z_ASSERT_EQ(LOG_WARNING, logger_get_level(&log_g.root_logger));

        logger_reset_level(LSTR_EMPTY_V);
        Z_ASSERT_EQ(LOG_ERR, logger_get_level(&d));
        Z_ASSERT_EQ(LOG_ERR, logger_get_level(&c));
        Z_ASSERT_EQ(log_g.root_logger.default_level, logger_get_level(&b));
        Z_ASSERT_EQ(log_g.root_logger.default_level, logger_get_level(&a));
        Z_ASSERT_EQ(log_g.root_logger.default_level,
                    logger_get_level(&log_g.root_logger));

        logger_set_level(LSTR_EMPTY_V, LOG_WARNING, LOG_RECURSIVE);
        Z_ASSERT_EQ(LOG_WARNING, logger_get_level(&d));
        Z_ASSERT_EQ(LOG_WARNING, logger_get_level(&c));
        Z_ASSERT_EQ(LOG_WARNING, logger_get_level(&b));
        Z_ASSERT_EQ(LOG_WARNING, logger_get_level(&a));
        Z_ASSERT_EQ(LOG_WARNING, logger_get_level(&log_g.root_logger));

        logger_reset_level(LSTR_EMPTY_V);
        Z_ASSERT_EQ(LOG_ERR, logger_get_level(&d));
        Z_ASSERT_EQ(LOG_ERR, logger_get_level(&c));
        Z_ASSERT_EQ(log_g.root_logger.default_level, logger_get_level(&b));
        Z_ASSERT_EQ(log_g.root_logger.default_level, logger_get_level(&a));
        Z_ASSERT_EQ(log_g.root_logger.default_level,
                    logger_get_level(&log_g.root_logger));

        logger_set_level(LSTR_IMMED_V("a"), LOG_WARNING, 0);
        Z_ASSERT_EQ(LOG_ERR, logger_get_level(&d));
        Z_ASSERT_EQ(LOG_ERR, logger_get_level(&c));
        Z_ASSERT_EQ(LOG_WARNING, logger_get_level(&b));
        Z_ASSERT_EQ(LOG_WARNING, logger_get_level(&a));
        Z_ASSERT_EQ(log_g.root_logger.default_level,
                    logger_get_level(&log_g.root_logger));

        logger_set_level(LSTR_IMMED_V("a"), LOG_WARNING, LOG_RECURSIVE);
        Z_ASSERT_EQ(LOG_WARNING, logger_get_level(&d));
        Z_ASSERT_EQ(LOG_WARNING, logger_get_level(&c));
        Z_ASSERT_EQ(LOG_WARNING, logger_get_level(&b));
        Z_ASSERT_EQ(LOG_WARNING, logger_get_level(&a));
        Z_ASSERT_EQ(log_g.root_logger.default_level,
                    logger_get_level(&log_g.root_logger));

        logger_set_level(LSTR_IMMED_V("a/b/c"), LOG_TRACE, 0);
        Z_ASSERT_EQ(LOG_TRACE, logger_get_level(&d));
        Z_ASSERT_EQ(LOG_TRACE, logger_get_level(&c));
        Z_ASSERT_EQ(LOG_WARNING, logger_get_level(&b));
        Z_ASSERT_EQ(LOG_WARNING, logger_get_level(&a));
        Z_ASSERT_EQ(log_g.root_logger.default_level,
                    logger_get_level(&log_g.root_logger));

        logger_reset_level(LSTR_IMMED_V("a"));
        Z_ASSERT_EQ(LOG_TRACE, logger_get_level(&d));
        Z_ASSERT_EQ(LOG_TRACE, logger_get_level(&c));
        Z_ASSERT_EQ(log_g.root_logger.default_level, logger_get_level(&b));
        Z_ASSERT_EQ(log_g.root_logger.default_level, logger_get_level(&a));
        Z_ASSERT_EQ(log_g.root_logger.default_level,
                    logger_get_level(&log_g.root_logger));

        logger_reset_level(LSTR_IMMED_V("a/b/c"));
        Z_ASSERT_EQ(LOG_ERR, logger_get_level(&d));
        Z_ASSERT_EQ(LOG_ERR, logger_get_level(&c));
        Z_ASSERT_EQ(log_g.root_logger.default_level, logger_get_level(&b));
        Z_ASSERT_EQ(log_g.root_logger.default_level, logger_get_level(&a));
        Z_ASSERT_EQ(log_g.root_logger.default_level,
                    logger_get_level(&log_g.root_logger));


        /* Checks on silent flag */
        Z_ASSERT_EQ(a.level_flags, 0u);
        Z_ASSERT_EQ(b.level_flags, (unsigned)LOG_SILENT);
        Z_ASSERT_EQ(c.level_flags, 0u);
        Z_ASSERT_EQ(d.level_flags, (unsigned)LOG_SILENT);

        logger_set_level(LSTR_IMMED_V("a/b"), LOG_WARNING, 0);
        Z_ASSERT_EQ(LOG_WARNING, logger_get_level(&b));
        Z_ASSERT_EQ(b.level_flags, 0u);
        logger_reset_level(LSTR_IMMED_V("a/b"));
        Z_ASSERT_EQ(log_g.root_logger.default_level, logger_get_level(&b));
        Z_ASSERT_EQ(b.level_flags, (unsigned)LOG_SILENT);

        logger_wipe(&d);
        logger_wipe(&c);
        logger_wipe(&b);
        logger_wipe(&a);
    } Z_TEST_END;
} Z_GROUP_END;

/* }}} */
