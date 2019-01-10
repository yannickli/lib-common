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

#ifdef MEM_BENCH
#include "core-mem-bench.h"
#endif

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

typedef struct buffer_instance_t {
    qv_t(log_buffer) vec_buffer;
    flag_t use_handler : 1;
    int buffer_log_level;
} buffer_instance_t;

qvector_t(spec, struct trace_spec_t);
qvector_t(buffer_instance, buffer_instance_t);

#ifndef NDEBUG
#define LOG_DEFAULT  LOG_TRACE
#else
#define LOG_DEFAULT  LOG_DEBUG
#endif

static log_handler_f log_stderr_raw_handler;

_MODULE_ADD_DECLS(log);

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
    spinlock_t update_lock;
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
    .pending_levels = QM_INIT(level, _G.pending_levels),
    .handler        = &log_stderr_raw_handler,
};

static __thread struct {
    bool inited;
    sb_t log;
    sb_t buf;

    log_ctx_t ml_ctx;

    /* log buffer */
    qv_t(buffer_instance) vec_buff_stack;
    mem_stack_pool_t mp_stack;
    int nb_buffer_started;
} log_thr_g;

__thread log_thr_ml_t log_thr_ml_g;

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

    if (!dlist_is_empty(&logger->siblings) && logger->siblings.next) {
        dlist_remove(&logger->siblings);
    }
    lstr_wipe(&logger->name);
    lstr_wipe(&logger->full_name);
}

void logger_wipe(logger_t *logger)
{
    spin_lock(&_G.update_lock);
    logger_wipe_child(logger);
    spin_unlock(&_G.update_lock);
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

static void __logger_do_refresh(logger_t *logger)
{
    if (atomic_load_explicit(&logger->conf_gen, memory_order_acquire)
        == log_conf_gen_g)
    {
        return;
    }

    logger->level_flags &= ~LOG_FORCED;

    if (logger->parent == NULL && logger != &_G.root_logger) {
        logger->parent = &_G.root_logger;
    }
    if (logger->parent) {
        __logger_do_refresh(logger->parent);
    }

    if (!logger->full_name.s) {
        int pos = 0;
        logger_t *sibling;

        logger_compute_fullname(logger);

        assert (logger->level >= LOG_UNDEFINED);
        assert (logger->default_level >= LOG_INHERITS);
        assert (logger->defined_level >= LOG_UNDEFINED);

        dlist_for_each_entry(sibling, &logger->parent->children, siblings) {
            assert (!lstr_equal2(sibling->name, logger->name));
        }
        dlist_add(&logger->parent->children, &logger->siblings);
        dlist_init(&logger->children);

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
    atomic_store_explicit(&logger->conf_gen, log_conf_gen_g,
                          memory_order_release);
}

void __logger_refresh(logger_t *logger)
{
    if (atomic_load_explicit(&logger->conf_gen, memory_order_acquire)
        == log_conf_gen_g)
    {
        return;
    }

    spin_lock(&_G.update_lock);
    __logger_do_refresh(logger);
    spin_unlock(&_G.update_lock);
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

        dlist_for_each_entry(child, &logger->children, siblings) {
            if (lstr_equal2(child->name, LSTR_PS_V(&n))) {
                next = child;
                break;
            }
        }

        RETHROW_P(next);
        logger = next;
    }

    return logger;
}

int logger_set_level(lstr_t name, int level, unsigned flags)
{
    logger_t *logger;

    spin_lock(&_G.update_lock);
    logger = logger_get_by_name(name);

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
        spin_unlock(&_G.update_lock);
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

    spin_unlock(&_G.update_lock);
    return level;
}

int logger_reset_level(lstr_t name)
{
    return logger_set_level(name, LOG_UNDEFINED, 0);
}

/* }}} */
/* Accessors {{{ */

static void
get_configurations_recursive(logger_t *logger, lstr_t prefix,
                             qv_t(logger_conf) *res)
{
    logger_t *child;
    core__logger_configuration__t conf;

    /* called first as it can force the update of several parameters
     * including the full name (calling __logger_refresh) */
    core__logger_configuration__init(&conf);

    /* Don't use logger_get_level since it takes the update_lock */
    __logger_do_refresh(logger);
    conf.level = MAX(logger->level, LOG_CRIT);

    /* check if the first element in the full name is the prefix */
    if (lstr_startswith(logger->full_name, prefix)) {
        conf.full_name = lstr_dupc(logger->full_name);
        conf.force_all = logger->level_flags & LOG_FORCED;
        conf.is_silent = logger->level_flags & LOG_SILENT;

        qv_append(logger_conf, res, conf);

        /* all children will have the same prefix. No need to check it
         * anymore, we set it to null */
        prefix = LSTR_NULL_V;
    }

    dlist_for_each_entry(child, &logger->children, siblings) {
        get_configurations_recursive(child, prefix, res);
    }
}

void logger_get_all_configurations(lstr_t prefix, qv_t(logger_conf) *confs)
{
    spin_lock(&_G.update_lock);
    get_configurations_recursive(&_G.root_logger, prefix, confs);
    spin_unlock(&_G.update_lock);
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

GENERIC_INIT(buffer_instance_t, buffer_instance);

static void buffer_instance_wipe(buffer_instance_t *buffer_instance)
{
    qv_wipe(log_buffer, &buffer_instance->vec_buffer);
}

static void free_last_buffer(void)
{
    if (log_thr_g.vec_buff_stack.len > log_thr_g.nb_buffer_started) {
        assert (log_thr_g.vec_buff_stack.len
            ==  log_thr_g.nb_buffer_started + 1);
        buffer_instance_wipe(qv_last(buffer_instance,
                                     &log_thr_g.vec_buff_stack));
        qv_remove(buffer_instance, &log_thr_g.vec_buff_stack,
                  log_thr_g.vec_buff_stack.len - 1);
        mem_stack_pop(&log_thr_g.mp_stack);
    }
}

void log_start_buffering_filter(bool use_handler, int log_level)
{
    buffer_instance_t *buffer_instance;

    if (log_thr_g.nb_buffer_started) {
        const buffer_instance_t *buff;

        buff = &log_thr_g.vec_buff_stack.tab[log_thr_g.nb_buffer_started - 1];
        if (!buff->use_handler) {
            use_handler = false;
        }
    }
    free_last_buffer();
    buffer_instance = qv_growlen(buffer_instance,
                                 &log_thr_g.vec_buff_stack, 1);

    buffer_instance_init(buffer_instance);
    buffer_instance->use_handler = use_handler;
    buffer_instance->buffer_log_level = log_level;

    mem_stack_push(&log_thr_g.mp_stack);
    log_thr_g.nb_buffer_started++;
}

void log_start_buffering(bool use_handler)
{
    log_start_buffering_filter(use_handler, INT32_MAX);
}

const qv_t(log_buffer) *log_stop_buffering(void)
{
    buffer_instance_t *buffer_instance;

    if (!expect(log_thr_g.nb_buffer_started > 0)) {
        return NULL;
    }
    free_last_buffer();
    buffer_instance = qv_last(buffer_instance, &log_thr_g.vec_buff_stack);
    log_thr_g.nb_buffer_started--;

    return &buffer_instance->vec_buffer;
}

static __attr_printf__(2, 0)
void logger_vsyslog(int level, const char *fmt, va_list va)
{
    SB_1k(sb);

    sb_addvf(&sb, fmt, va);
    syslog(LOG_USER | level, "%s", sb.data);
}

static __attr_printf__(3, 0)
void logger_putv(const log_ctx_t *ctx, bool do_log,
                 const char *fmt, va_list va)
{
    buffer_instance_t *buff;

    if (ctx->level <= LOG_CRIT) {
        va_list cpy;

        syslog_is_critical = true;
        va_copy(cpy, va);
        logger_vsyslog(ctx->level, fmt, cpy);
        va_end(cpy);
    }

    if (!do_log) {
        return;
    }

    if (!log_thr_g.nb_buffer_started) {
        (*_G.handler)(ctx, fmt, va);
        return;
    }

    buff = &log_thr_g.vec_buff_stack.tab[log_thr_g.nb_buffer_started - 1];

    if (ctx->level <= buff->buffer_log_level) {
        int size_fmt;
        log_buffer_t *log_save;
        va_list cpy;
        char *buffer;
        qv_t(log_buffer) *vec_buffer = &buff->vec_buffer;

        if (buff->use_handler) {
            va_copy(cpy, va);
            (*_G.handler)(ctx, fmt, cpy);
            va_end(cpy);
        }

        va_copy(cpy, va);
        size_fmt = vsnprintf(NULL, 0, fmt, cpy) + 1;
        va_end(cpy);

        free_last_buffer();
        buffer = mp_new_raw(&log_thr_g.mp_stack.funcs, char, size_fmt);
        vsnprintf(buffer, size_fmt, fmt, va);

        log_save = qv_growlen(log_buffer, vec_buffer, 1);
        log_save->ctx = *ctx;
        log_save->msg = LSTR_INIT_V(buffer, size_fmt - 1);
    }
}

static __attr_printf__(3, 4)
void logger_put(const log_ctx_t *ctx, bool do_log, const char *fmt, ...)
{
    va_list va;

    va_start(va, fmt);
    logger_putv(ctx, do_log, fmt, va);
    va_end(va);
}

static __attr_printf__(2, 0)
void logger_put_in_buf(const log_ctx_t *ctx, const char *fmt, va_list ap)
{
    const char *p;
    sb_t *buf = &log_thr_g.buf;

    sb_addvf(buf, fmt, ap);

    while ((p = memchr(buf->data, '\n', buf->len))) {
        logger_put(ctx, true, "%*pM", (int)(p - buf->data), buf->data);
        sb_skip_upto(buf, p + 1);
    }
}

static __attr_noreturn__
void logger_do_fatal(void)
{
    if (psinfo_get_tracer_pid(0) > 0) {
        abort();
    }
#ifdef __has_asan
    /* Avoid having ASAN leak reports when calling logger_fatal. */
    _exit(127);
#else
    exit(127);
#endif
}

int logger_vlog(logger_t *logger, int level, const char *prog, int pid,
                const char *file, const char *func, int line,
                const char *fmt, va_list va)
{
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

    assert (atomic_load_explicit(&logger->conf_gen, memory_order_acquire)
            == log_conf_gen_g);
    logger_putv(&ctx, logger_has_level(logger, level) || level >= LOG_TRACE,
                fmt, va);
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
        logger_do_fatal();
    }

    return res;
}

void __logger_panic(logger_t *logger, const char *file, const char *func,
                    int line, const char *fmt, ...)
{
    va_list va;

    __logger_refresh(logger);

    va_start(va, fmt);
    logger_vlog(logger, LOG_CRIT, NULL, -1, file, func, line, fmt, va);
    abort();
}

void __logger_fatal(logger_t *logger, const char *file, const char *func,
                    int line, const char *fmt, ...)
{
    va_list va;

    __logger_refresh(logger);

    va_start(va, fmt);
    logger_vlog(logger, LOG_CRIT, NULL, -1, file, func, line, fmt, va);

    logger_do_fatal();
}

void __logger_exit(logger_t *logger, const char *file, const char *func,
                   int line, const char *fmt, ...)
{
    va_list va;

    __logger_refresh(logger);

    va_start(va, fmt);
    logger_vlog(logger, LOG_ERR, NULL, -1, file, func, line, fmt, va);
    logger_vsyslog(LOG_ERR, fmt, va);
    va_end(va);

    _exit(0);
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

void __logger_start(logger_t *logger, int level, const char *prog, int pid,
                    const char *file, const char *func, int line)
{
    assert (atomic_load_explicit(&logger->conf_gen, memory_order_acquire)
            == log_conf_gen_g);

    log_thr_g.ml_ctx = (log_ctx_t){
        .logger_name = lstr_dupc(logger->full_name),
        .level       = level,
        .file        = file,
        .func        = func,
        .line        = line,
        .pid         = pid < 0 ? _G.pid : pid,
        .prog_name   = prog ?: program_invocation_short_name,
        .is_silent   = !!(logger->level_flags & LOG_SILENT),
    };
}

void __logger_vcont(const char *fmt, va_list ap)
{
    if (log_thr_ml_g.activated) {
        logger_put_in_buf(&log_thr_g.ml_ctx, fmt, ap);
    }
}

void __logger_cont(const char *fmt, ...)
{
    va_list va;

    va_start(va, fmt);
    __logger_vcont(fmt, va);
    va_end(va);
}

void __logger_end(void)
{
    if (log_thr_ml_g.activated) {
        __logger_cont("\n");
    }
}

void __logger_end_panic(void)
{
    __logger_end();
    abort();
}

void __logger_end_fatal(void)
{
    __logger_end();
    logger_do_fatal();
}

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

    __logger_refresh(&_G.root_logger);

    va_start(va, fmt);
    logger_vlog(&_G.root_logger, LOG_CRIT, NULL, -1, NULL, NULL, -1, fmt, va);
    abort();
}

int e_fatal(const char *fmt, ...)
{
    va_list va;

    __logger_refresh(&_G.root_logger);

    va_start(va, fmt);
    logger_vlog(&_G.root_logger, LOG_CRIT, NULL, -1, NULL, NULL, -1, fmt, va);

    if (psinfo_get_tracer_pid(0) > 0) {
        abort();
    }
#ifdef __has_asan
    /* Avoid having ASAN leak reports when calling e_fatal. */
    _exit(127);
#else
    exit(127);
#endif
}

#define E_FUNCTION(Name, Level)                                              \
    int Name(const char *fmt, ...)                                           \
    {                                                                        \
        if (logger_has_level(&_G.root_logger, (Level))) {                    \
            int ret;                                                         \
            va_list va;                                                      \
                                                                             \
            va_start(va, fmt);                                               \
            ret = logger_vlog(&_G.root_logger, (Level), NULL, -1, NULL,      \
                              NULL, -1, fmt, va);                            \
            va_end(va);                                                      \
            return ret;                                                      \
        }                                                                    \
        return (Level) <= LOG_WARNING ? -1 : 0;                              \
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

    spin_lock(&_G.update_lock);
    logger = logger_get_by_name(LSTR_OPT(name)) ?: &log_g.root_logger;
    spin_unlock(&_G.update_lock);
    return __logger_is_traced(logger, lvl, modname, func, name);
}

void e_trace_put_(int level, const char *module, int lno,
                  const char *func, const char *name, const char *fmt, ...)
{
    va_list ap;
    log_ctx_t ctx = {
        .logger_name = LSTR_OPT(name),
        .level       = LOG_TRACE + level,
        .file        = module,
        .func        = func,
        .line        = lno,
        .pid         = _G.pid,
        .prog_name   = program_invocation_short_name,
    };

    va_start(ap, fmt);
    logger_put_in_buf(&ctx, fmt, ap);
    va_end(ap);
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

        mem_stack_pool_init(&log_thr_g.mp_stack, 16 << 10);
        qv_init(buffer_instance, &log_thr_g.vec_buff_stack);

        log_thr_g.inited = true;
    }
}

static void log_shutdown_thread(void)
{
    if (log_thr_g.inited) {
        sb_wipe(&log_thr_g.buf);
        sb_wipe(&log_thr_g.log);

        qv_deep_wipe(buffer_instance, &log_thr_g.vec_buff_stack,
                     buffer_instance_wipe);
        if (log_thr_g.vec_buff_stack.len > 0) {
            mem_stack_pop(&log_thr_g.mp_stack);
        }
        mem_stack_pool_wipe(&log_thr_g.mp_stack);

        log_thr_g.inited = false;
    }
}
thr_hooks(log_initialize_thread, log_shutdown_thread);

static void log_atfork(void)
{
    _G.pid = getpid();
}

/** Parse the content of the IS_DEBUG environment variable.
 *
 * It is composed of a series of blank separated <specs>:
 *
 *  <specs> ::= [<path-pattern>][@<funcname>][+<featurename>][:<level>]
 */
static void log_parse_specs(char *p, qv_t(spec) *out)
{
    pstream_t ps = ps_initstr(p);
    ctype_desc_t ctype;

    ps_trim(&ps);

    ctype_desc_build(&ctype, "@+:");

    while (!ps_done(&ps)) {
        struct trace_spec_t spec = {
            .path = NULL,
            .func = NULL,
            .name = NULL,
            .level = INT_MAX,
        };
        pstream_t spec_ps;
        int c = '\0';

        spec_ps = ps_get_cspan(&ps, &ctype_isspace);
        ps_skip_span(&ps, &ctype_isspace);

#define GET_ELEM(_dst)  \
        do {                                                                 \
            pstream_t elem = ps_get_cspan(&spec_ps, &ctype);                 \
                                                                             \
            if (ps_len(&elem)) {                                             \
                _dst = elem.s;                                               \
            }                                                                \
            c = *spec_ps.s;                                                  \
            *(char *)spec_ps.s = '\0';                                       \
            ps_skip(&spec_ps, 1);                                            \
        } while (0)

        GET_ELEM(spec.path);
        if (c == '@') {
            GET_ELEM(spec.func);
        }
        if (c == '+') {
            GET_ELEM(spec.name);
        }
        if (c == ':') {
            const char *level = NULL;

            GET_ELEM(level);
            if (level) {
                spec.level = LOG_TRACE + atoi(level);
            }
        }
#undef GET_ELEM

        qv_append(spec, out, spec);
    }
}

static int log_initialize(void* args)
{
    char *is_debug = getenv("IS_DEBUG");

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

    if (is_debug) {
        _G.is_debug = p_strdup(is_debug);
        log_parse_specs(_G.is_debug, &_G.specs);

        qv_for_each_ptr(spec, spec, &_G.specs) {
            if (!spec->func && !spec->path) {
                logger_set_level(LSTR_OPT(spec->name), spec->level, 0);
            }
        }
    }

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
    thr_hooks_register();
    iop_module_register();
    log_module = module_register(LSTR("log"), &log_module,
                                 &log_initialize, &log_shutdown, NULL, 0);
    module_add_dep(log_module, LSTR("log"),  LSTR("thr_hooks"),
                   &MODULE(thr_hooks));
    module_implement_method(log_module, &at_fork_on_child_method,
                            &log_atfork);
    MODULE_REQUIRE(log);
    MODULE_REQUIRE(iop);

#ifdef MEM_BENCH
    mem_bench_require();
#endif
}

/* }}} */
/* Tests {{{ */

#include <lib-common/z.h>

static bool z_handler_was_used_g;

__attr_printf__(2, 0)
static void z_log_buffer_handler(const log_ctx_t *ctx, const char *fmt,
                                 va_list va)
{
    log_stderr_handler_g(ctx, fmt, va);
    z_handler_was_used_g = true;
}

typedef struct z_logger_init_job_t {
    thr_job_t job;

    logger_t  *loggers;
    thr_evc_t *ec;
    int        count;
} z_logger_init_job_t;

static void z_logger_init_job(thr_job_t *tjob, thr_syn_t *syn)
{
    z_logger_init_job_t *job = container_of(tjob, z_logger_init_job_t, job);

    for (int i = 0; i < job->count; i++) {
        logger_t *logger = &job->loggers[i];
        uint64_t key = thr_ec_get(job->ec);

        thr_ec_wait(job->ec, key);
        logger_notice(logger, "coucou");
    }
}

Z_GROUP_EXPORT(log) {
    Z_TEST(log_level, "log") {
        logger_t a = LOGGER_INIT_INHERITS(NULL, "a");
        logger_t b = LOGGER_INIT_SILENT_INHERITS(&a, "b");
        logger_t c = LOGGER_INIT(&b, "c", LOG_ERR);
        logger_t d;

        logger_init(&d, &c, LSTR("d"), LOG_INHERITS, LOG_SILENT);

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

        logger_set_level(LSTR("a"), LOG_WARNING, 0);
        Z_ASSERT_EQ(LOG_ERR, logger_get_level(&d));
        Z_ASSERT_EQ(LOG_ERR, logger_get_level(&c));
        Z_ASSERT_EQ(LOG_WARNING, logger_get_level(&b));
        Z_ASSERT_EQ(LOG_WARNING, logger_get_level(&a));
        Z_ASSERT_EQ(log_g.root_logger.default_level,
                    logger_get_level(&log_g.root_logger));

        logger_set_level(LSTR("a"), LOG_WARNING, LOG_RECURSIVE);
        Z_ASSERT_EQ(LOG_WARNING, logger_get_level(&d));
        Z_ASSERT_EQ(LOG_WARNING, logger_get_level(&c));
        Z_ASSERT_EQ(LOG_WARNING, logger_get_level(&b));
        Z_ASSERT_EQ(LOG_WARNING, logger_get_level(&a));
        Z_ASSERT_EQ(log_g.root_logger.default_level,
                    logger_get_level(&log_g.root_logger));

        logger_set_level(LSTR("a/b/c"), LOG_TRACE, 0);
        Z_ASSERT_EQ(LOG_TRACE, logger_get_level(&d));
        Z_ASSERT_EQ(LOG_TRACE, logger_get_level(&c));
        Z_ASSERT_EQ(LOG_WARNING, logger_get_level(&b));
        Z_ASSERT_EQ(LOG_WARNING, logger_get_level(&a));
        Z_ASSERT_EQ(log_g.root_logger.default_level,
                    logger_get_level(&log_g.root_logger));

        logger_reset_level(LSTR("a"));
        Z_ASSERT_EQ(LOG_TRACE, logger_get_level(&d));
        Z_ASSERT_EQ(LOG_TRACE, logger_get_level(&c));
        Z_ASSERT_EQ(log_g.root_logger.default_level, logger_get_level(&b));
        Z_ASSERT_EQ(log_g.root_logger.default_level, logger_get_level(&a));
        Z_ASSERT_EQ(log_g.root_logger.default_level,
                    logger_get_level(&log_g.root_logger));

        logger_reset_level(LSTR("a/b/c"));
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

        logger_set_level(LSTR("a/b"), LOG_WARNING, 0);
        Z_ASSERT_EQ(LOG_WARNING, logger_get_level(&b));
        Z_ASSERT_EQ(b.level_flags, 0u);
        logger_reset_level(LSTR("a/b"));
        Z_ASSERT_EQ(log_g.root_logger.default_level, logger_get_level(&b));
        Z_ASSERT_EQ(b.level_flags, (unsigned)LOG_SILENT);

        logger_wipe(&d);
        logger_wipe(&c);
        logger_wipe(&b);
        logger_wipe(&a);
    } Z_TEST_END;

    Z_TEST(get_loggers_confs, "get loggers confs") {
        t_scope;
        logger_t a = LOGGER_INIT_INHERITS(NULL, "ztest_log_conf");
        logger_t b = LOGGER_INIT_SILENT_INHERITS(&a, "son");
        logger_t c = LOGGER_INIT(&b, "gdson", LOG_ERR);
        logger_t d;
        qv_t(logger_conf) confs;

        logger_init(&d, &a, LSTR("sibl"), LOG_INHERITS, LOG_SILENT);

        /* force update of the tree */
        logger_get_level(&a);
        logger_get_level(&b);
        logger_get_level(&c);
        logger_get_level(&d);

        t_qv_init(logger_conf, &confs, 10);

        logger_get_all_configurations(LSTR("ztest_log_conf"), &confs);

        Z_ASSERT_EQ(confs.len, 4);
        Z_ASSERT_LSTREQUAL(confs.tab[0].full_name, LSTR("ztest_log_conf"));
        Z_ASSERT_LSTREQUAL(confs.tab[1].full_name,
                           LSTR("ztest_log_conf/son"));
        Z_ASSERT_LSTREQUAL(confs.tab[2].full_name,
                           LSTR("ztest_log_conf/son/gdson"));
        Z_ASSERT_LSTREQUAL(confs.tab[3].full_name,
                           LSTR("ztest_log_conf/sibl"));

        Z_ASSERT_EQ(confs.tab[0].level, log_g.root_logger.default_level);
        Z_ASSERT_EQ(confs.tab[1].level, log_g.root_logger.default_level);
        Z_ASSERT_EQ(confs.tab[2].level, LOG_ERR);
        Z_ASSERT_EQ(confs.tab[3].level, log_g.root_logger.default_level);

        Z_ASSERT_EQ(confs.tab[0].force_all, false);
        Z_ASSERT_EQ(confs.tab[1].force_all, false);
        Z_ASSERT_EQ(confs.tab[2].force_all, false);
        Z_ASSERT_EQ(confs.tab[3].force_all, false);

        Z_ASSERT_EQ(confs.tab[0].is_silent, false);
        Z_ASSERT_EQ(confs.tab[1].is_silent, true);
        Z_ASSERT_EQ(confs.tab[2].is_silent, false);
        Z_ASSERT_EQ(confs.tab[3].is_silent, true);

        logger_set_level(LSTR("ztest_log_conf"), LOG_WARNING, LOG_RECURSIVE);

        logger_get_all_configurations(LSTR("ztest_log_conf/son"),
                                      &confs);

        Z_ASSERT_EQ(confs.len, 6);
        Z_ASSERT_LSTREQUAL(confs.tab[4].full_name,
                           LSTR("ztest_log_conf/son"));
        Z_ASSERT_LSTREQUAL(confs.tab[5].full_name,
                           LSTR("ztest_log_conf/son/gdson"));

        Z_ASSERT_EQ(confs.tab[4].level, LOG_WARNING);
        Z_ASSERT_EQ(confs.tab[5].level, LOG_WARNING);

        Z_ASSERT_EQ(confs.tab[4].force_all, true);
        Z_ASSERT_EQ(confs.tab[5].force_all, true);

        logger_wipe(&d);
        logger_wipe(&c);
        logger_wipe(&b);
        logger_wipe(&a);
    } Z_TEST_END;

    Z_TEST(log_buffer, "log buffer") {
        log_handler_f *handler = log_set_handler(&z_log_buffer_handler);
        logger_t logger_test = LOGGER_INIT_SILENT(NULL, "logger test",
                                                  LOG_TRACE);
        logger_t logger_test_2 = LOGGER_INIT_SILENT(NULL, "logger test 2",
                                                  LOG_TRACE);
        const qv_t(log_buffer) *vect_buffer;

#define TEST_LOG_ENTRY(_i, _j, _level, _logger)                              \
    do {                                                                     \
        log_buffer_t entry = vect_buffer->tab[_i];                           \
        Z_ASSERT_LSTREQUAL(entry.msg,                                        \
                           LSTR("log message inside buffer " # _j));         \
        Z_ASSERT_LSTREQUAL(entry.ctx.logger_name, _logger.name);             \
        Z_ASSERT_EQ(entry.ctx.level, _level);                                \
    } while (0)

#define TEST_HANDLER(_state)                                                 \
    do {                                                                     \
        Z_ASSERT_EQ(z_handler_was_used_g, _state);                           \
        z_handler_was_used_g = false;                                        \
    } while (0)

        /* Buffer imbrication */

        log_start_buffering(false);
        logger_notice(&logger_test, "log message inside buffer %d", 1);
        logger_error(&logger_test_2, "log message inside buffer %d", 2);
        logger_warning(&logger_test, "log message inside buffer %d", 3);

        log_start_buffering(false);
        logger_notice(&logger_test, "log message inside buffer %d", 1);
        logger_error(&logger_test_2, "log message inside buffer %d", 2);
        logger_warning(&logger_test, "log message inside buffer %d", 3);
        logger_info(&logger_test, "log message inside buffer %d", 4);

        log_start_buffering(false);
        logger_info(&logger_test, "log message inside buffer %d", 4);
        logger_notice(&logger_test_2, "log message inside buffer %d", 5);
        logger_error(&logger_test, "log message inside buffer %d", 6);
        logger_warning(&logger_test_2, "log message inside buffer %d", 7);
        logger_info(&logger_test_2, "log message inside buffer %d", 8);

        log_start_buffering(false);
        logger_notice(&logger_test, "log message inside buffer %d", 1);
        logger_error(&logger_test, "log message inside buffer %d", 2);
        logger_warning(&logger_test, "log message inside buffer %d", 3);
        logger_info(&logger_test, "log message inside buffer %d", 4);
        logger_notice(&logger_test_2, "log message inside buffer %d", 5);
        logger_error(&logger_test_2, "log message inside buffer %d", 6);
        logger_warning(&logger_test_2, "log message inside buffer %d", 7);
        logger_info(&logger_test_2, "log message inside buffer %d", 8);

        vect_buffer = log_stop_buffering();
        Z_ASSERT_EQ(vect_buffer->len, 8);

        TEST_LOG_ENTRY(0, 1, LOG_NOTICE, logger_test);
        TEST_LOG_ENTRY(1, 2, LOG_ERR, logger_test);
        TEST_LOG_ENTRY(2, 3, LOG_WARNING, logger_test);
        TEST_LOG_ENTRY(3, 4, LOG_INFO, logger_test);
        TEST_LOG_ENTRY(4, 5, LOG_NOTICE, logger_test_2);
        TEST_LOG_ENTRY(5, 6, LOG_ERR, logger_test_2);
        TEST_LOG_ENTRY(6, 7, LOG_WARNING, logger_test_2);
        TEST_LOG_ENTRY(7, 8, LOG_INFO, logger_test_2);

        log_start_buffering(false);
        logger_error(&logger_test, "log message inside buffer 2");
        logger_info(&logger_test_2, "log message inside buffer 8");
        logger_warning(&logger_test, "log message inside buffer 3");
        logger_notice(&logger_test, "log message inside buffer 1");
        logger_info(&logger_test, "log message inside buffer 4");
        logger_error(&logger_test_2, "log message inside buffer 6");
        logger_notice(&logger_test_2, "log message inside buffer 5");

        vect_buffer = log_stop_buffering();
        Z_ASSERT_EQ(vect_buffer->len, 7);

        TEST_LOG_ENTRY(0, 2, LOG_ERR, logger_test);
        TEST_LOG_ENTRY(1, 8, LOG_INFO, logger_test_2);
        TEST_LOG_ENTRY(2, 3, LOG_WARNING, logger_test);
        TEST_LOG_ENTRY(3, 1, LOG_NOTICE, logger_test);
        TEST_LOG_ENTRY(4, 4, LOG_INFO, logger_test);
        TEST_LOG_ENTRY(5, 6, LOG_ERR, logger_test_2);
        TEST_LOG_ENTRY(6, 5, LOG_NOTICE, logger_test_2);
        vect_buffer = log_stop_buffering();
        Z_ASSERT_EQ(vect_buffer->len, 5);

        TEST_LOG_ENTRY(0, 4, LOG_INFO, logger_test);
        TEST_LOG_ENTRY(1, 5, LOG_NOTICE, logger_test_2);
        TEST_LOG_ENTRY(2, 6, LOG_ERR, logger_test);
        TEST_LOG_ENTRY(3, 7, LOG_WARNING, logger_test_2);
        TEST_LOG_ENTRY(4, 8, LOG_INFO, logger_test_2);

        logger_notice(&logger_test_2, "log message inside buffer %d", 5);

        vect_buffer = log_stop_buffering();
        Z_ASSERT_EQ(vect_buffer->len, 5);

        TEST_LOG_ENTRY(0, 1, LOG_NOTICE, logger_test);
        TEST_LOG_ENTRY(1, 2, LOG_ERR, logger_test_2);
        TEST_LOG_ENTRY(2, 3, LOG_WARNING, logger_test);
        TEST_LOG_ENTRY(3, 4, LOG_INFO, logger_test);
        TEST_LOG_ENTRY(4, 5, LOG_NOTICE, logger_test_2);

        vect_buffer = log_stop_buffering();
        Z_ASSERT_EQ(vect_buffer->len, 3);

        TEST_LOG_ENTRY(0, 1, LOG_NOTICE, logger_test);
        TEST_LOG_ENTRY(1, 2, LOG_ERR, logger_test_2);
        TEST_LOG_ENTRY(2, 3, LOG_WARNING, logger_test);

        log_start_buffering(false);
        logger_info(&logger_test, "log message inside buffer %d", 4);
        logger_notice(&logger_test_2, "log message inside buffer %d", 5);
        logger_error(&logger_test, "log message inside buffer %d", 6);
        logger_warning(&logger_test_2, "log message inside buffer %d", 7);
        logger_info(&logger_test_2, "log message inside buffer %d", 8);

        vect_buffer = log_stop_buffering();
        Z_ASSERT_EQ(vect_buffer->len, 5);

        TEST_LOG_ENTRY(0, 4, LOG_INFO, logger_test);
        TEST_LOG_ENTRY(1, 5, LOG_NOTICE, logger_test_2);
        TEST_LOG_ENTRY(2, 6, LOG_ERR, logger_test);
        TEST_LOG_ENTRY(3, 7, LOG_WARNING, logger_test_2);
        TEST_LOG_ENTRY(4, 8, LOG_INFO, logger_test_2);

        /* buffer with handler */

        log_start_buffering(true);
        logger_error(&logger_test, "log message inside buffer 2");
        logger_info(&logger_test_2, "log message inside buffer 8");
        logger_warning(&logger_test, "log message inside buffer 3");
        logger_notice(&logger_test, "log message inside buffer 1");
        logger_info(&logger_test, "log message inside buffer 4");
        logger_error(&logger_test_2, "log message inside buffer 6");
        logger_notice(&logger_test_2, "log message inside buffer 5");
        TEST_HANDLER(true);

        log_start_buffering(true);
        logger_info(&logger_test, "log message inside buffer %d", 4);
        logger_notice(&logger_test_2, "log message inside buffer %d", 5);
        logger_error(&logger_test, "log message inside buffer %d", 6);
        logger_warning(&logger_test_2, "log message inside buffer %d", 7);
        TEST_HANDLER(true);

        log_start_buffering(false);
        logger_notice(&logger_test, "log message inside buffer %d", 1);
        logger_error(&logger_test, "log message inside buffer %d", 2);
        logger_warning(&logger_test, "log message inside buffer %d", 3);
        logger_info(&logger_test, "log message inside buffer %d", 4);
        logger_notice(&logger_test_2, "log message inside buffer %d", 5);
        logger_error(&logger_test_2, "log message inside buffer %d", 6);
        logger_warning(&logger_test_2, "log message inside buffer %d", 7);
        logger_info(&logger_test_2, "log message inside buffer %d", 8);
        TEST_HANDLER(false);

        log_start_buffering(true);
        logger_notice(&logger_test, "log message inside buffer %d", 1);
        logger_error(&logger_test, "log message inside buffer %d", 2);
        logger_warning(&logger_test, "log message inside buffer %d", 3);
        logger_info(&logger_test, "log message inside buffer %d", 4);
        logger_notice(&logger_test_2, "log message inside buffer %d", 5);
        logger_error(&logger_test_2, "log message inside buffer %d", 6);
        logger_warning(&logger_test_2, "log message inside buffer %d", 7);
        logger_info(&logger_test_2, "log message inside buffer %d", 8);

        TEST_HANDLER(false);

        vect_buffer = log_stop_buffering();
        Z_ASSERT_EQ(vect_buffer->len, 8);

        TEST_LOG_ENTRY(0, 1, LOG_NOTICE, logger_test);
        TEST_LOG_ENTRY(1, 2, LOG_ERR, logger_test);
        TEST_LOG_ENTRY(2, 3, LOG_WARNING, logger_test);
        TEST_LOG_ENTRY(3, 4, LOG_INFO, logger_test);
        TEST_LOG_ENTRY(4, 5, LOG_NOTICE, logger_test_2);
        TEST_LOG_ENTRY(5, 6, LOG_ERR, logger_test_2);
        TEST_LOG_ENTRY(6, 7, LOG_WARNING, logger_test_2);
        TEST_LOG_ENTRY(7, 8, LOG_INFO, logger_test_2);

        log_start_buffering(true);
        logger_notice(&logger_test, "log message inside buffer %d", 1);
        logger_error(&logger_test, "log message inside buffer %d", 2);
        logger_warning(&logger_test, "log message inside buffer %d", 3);
        logger_info(&logger_test, "log message inside buffer %d", 4);
        logger_notice(&logger_test_2, "log message inside buffer %d", 5);
        logger_error(&logger_test_2, "log message inside buffer %d", 6);
        logger_warning(&logger_test_2, "log message inside buffer %d", 7);
        logger_info(&logger_test_2, "log message inside buffer %d", 8);
        TEST_HANDLER(false);

        vect_buffer = log_stop_buffering();
        Z_ASSERT_EQ(vect_buffer->len, 8);

        TEST_LOG_ENTRY(0, 1, LOG_NOTICE, logger_test);
        TEST_LOG_ENTRY(1, 2, LOG_ERR, logger_test);
        TEST_LOG_ENTRY(2, 3, LOG_WARNING, logger_test);
        TEST_LOG_ENTRY(3, 4, LOG_INFO, logger_test);
        TEST_LOG_ENTRY(4, 5, LOG_NOTICE, logger_test_2);
        TEST_LOG_ENTRY(5, 6, LOG_ERR, logger_test_2);
        TEST_LOG_ENTRY(6, 7, LOG_WARNING, logger_test_2);
        TEST_LOG_ENTRY(7, 8, LOG_INFO, logger_test_2);

        log_start_buffering(true);
        logger_error(&logger_test, "log message inside buffer 2");
        logger_info(&logger_test_2, "log message inside buffer 8");
        logger_warning(&logger_test, "log message inside buffer 3");
        logger_notice(&logger_test, "log message inside buffer 1");
        logger_info(&logger_test, "log message inside buffer 4");
        logger_error(&logger_test_2, "log message inside buffer 6");
        logger_notice(&logger_test_2, "log message inside buffer 5");
        TEST_HANDLER(false);

        vect_buffer = log_stop_buffering();
        Z_ASSERT_EQ(vect_buffer->len, 7);

        vect_buffer = log_stop_buffering();
        Z_ASSERT_EQ(vect_buffer->len, 8);

        TEST_LOG_ENTRY(0, 1, LOG_NOTICE, logger_test);
        TEST_LOG_ENTRY(1, 2, LOG_ERR, logger_test);
        TEST_LOG_ENTRY(2, 3, LOG_WARNING, logger_test);
        TEST_LOG_ENTRY(3, 4, LOG_INFO, logger_test);
        TEST_LOG_ENTRY(4, 5, LOG_NOTICE, logger_test_2);
        TEST_LOG_ENTRY(5, 6, LOG_ERR, logger_test_2);
        TEST_LOG_ENTRY(6, 7, LOG_WARNING, logger_test_2);
        TEST_LOG_ENTRY(7, 8, LOG_INFO, logger_test_2);

        logger_info(&logger_test_2, "log message inside buffer %d", 8);
        TEST_HANDLER(true);

        vect_buffer = log_stop_buffering();
        Z_ASSERT_EQ(vect_buffer->len, 5);

        TEST_LOG_ENTRY(0, 4, LOG_INFO, logger_test);
        TEST_LOG_ENTRY(1, 5, LOG_NOTICE, logger_test_2);
        TEST_LOG_ENTRY(2, 6, LOG_ERR, logger_test);
        TEST_LOG_ENTRY(3, 7, LOG_WARNING, logger_test_2);
        TEST_LOG_ENTRY(4, 8, LOG_INFO, logger_test_2);

        vect_buffer = log_stop_buffering();
        Z_ASSERT_EQ(vect_buffer->len, 7);

        TEST_LOG_ENTRY(0, 2, LOG_ERR, logger_test);
        TEST_LOG_ENTRY(1, 8, LOG_INFO, logger_test_2);
        TEST_LOG_ENTRY(2, 3, LOG_WARNING, logger_test);
        TEST_LOG_ENTRY(3, 1, LOG_NOTICE, logger_test);
        TEST_LOG_ENTRY(4, 4, LOG_INFO, logger_test);
        TEST_LOG_ENTRY(5, 6, LOG_ERR, logger_test_2);
        TEST_LOG_ENTRY(6, 5, LOG_NOTICE, logger_test_2);

        logger_notice(&logger_test_2, "log message inside buffer");
        log_start_buffering(true);
        vect_buffer = log_stop_buffering();
        Z_ASSERT_EQ(vect_buffer->len, 0);

        /* buffer capture different level */

        log_start_buffering_filter(true, LOG_NOTICE);

        logger_error(&logger_test, "log message inside buffer 2");
        logger_info(&logger_test_2, "log message inside buffer 8");
        logger_warning(&logger_test, "log message inside buffer 3");
        logger_notice(&logger_test, "log message inside buffer 1");
        logger_info(&logger_test, "log message inside buffer 4");
        logger_error(&logger_test_2, "log message inside buffer 6");
        logger_notice(&logger_test_2, "log message inside buffer 5");

        vect_buffer = log_stop_buffering();
        Z_ASSERT_EQ(vect_buffer->len, 5);

        TEST_LOG_ENTRY(0, 2, LOG_ERR, logger_test);
        TEST_LOG_ENTRY(1, 3, LOG_WARNING, logger_test);
        TEST_LOG_ENTRY(2, 1, LOG_NOTICE, logger_test);
        TEST_LOG_ENTRY(3, 6, LOG_ERR, logger_test_2);
        TEST_LOG_ENTRY(4, 5, LOG_NOTICE, logger_test_2);

        log_start_buffering_filter(true, LOG_WARNING);

        logger_error(&logger_test, "log message inside buffer 2");
        logger_info(&logger_test_2, "log message inside buffer 8");
        logger_warning(&logger_test, "log message inside buffer 3");

        log_start_buffering_filter(true, LOG_ERR);

        logger_error(&logger_test, "log message inside buffer 2");
        logger_info(&logger_test_2, "log message inside buffer 8");
        logger_warning(&logger_test, "log message inside buffer 3");
        logger_notice(&logger_test, "log message inside buffer 1");
        logger_info(&logger_test, "log message inside buffer 4");
        logger_error(&logger_test_2, "log message inside buffer 6");
        logger_notice(&logger_test_2, "log message inside buffer 5");

        vect_buffer = log_stop_buffering();
        Z_ASSERT_EQ(vect_buffer->len, 2);

        TEST_LOG_ENTRY(0, 2, LOG_ERR, logger_test);
        TEST_LOG_ENTRY(1, 6, LOG_ERR, logger_test_2);

        logger_notice(&logger_test, "log message inside buffer 1");
        logger_info(&logger_test, "log message inside buffer 4");
        logger_error(&logger_test_2, "log message inside buffer 6");
        logger_notice(&logger_test_2, "log message inside buffer 5");

        vect_buffer = log_stop_buffering();
        Z_ASSERT_EQ(vect_buffer->len, 3);

        TEST_LOG_ENTRY(0, 2, LOG_ERR, logger_test);
        TEST_LOG_ENTRY(1, 3, LOG_WARNING, logger_test);
        TEST_LOG_ENTRY(2, 6, LOG_ERR, logger_test_2);

        log_start_buffering_filter(true, LOG_ALERT);

        logger_error(&logger_test, "log message inside buffer 2");
        logger_info(&logger_test_2, "log message inside buffer 8");
        logger_warning(&logger_test, "log message inside buffer 3");
        logger_notice(&logger_test, "log message inside buffer 1");
        logger_info(&logger_test, "log message inside buffer 4");
        logger_error(&logger_test_2, "log message inside buffer 6");
        logger_notice(&logger_test_2, "log message inside buffer 5");

        vect_buffer = log_stop_buffering();
        Z_ASSERT_EQ(vect_buffer->len, 0);

        logger_notice(&logger_test_2, "log message inside buffer");
        log_start_buffering_filter(true, LOG_TRACE);
        vect_buffer = log_stop_buffering();
        Z_ASSERT_EQ(vect_buffer->len, 0);

        logger_wipe(&logger_test);
        logger_wipe(&logger_test_2);

        log_set_handler(handler);
#undef TEST_LOG_ENTRY
    } Z_TEST_END;

    Z_TEST(scope, "scoped logs") {
        logger_t l = LOGGER_INIT_INHERITS(NULL, "blah");

#define _CHECK_RES(Level, Catch)  do {                                       \
        if (Catch) {                                                         \
            Z_ASSERT_EQ(vect_buffer->len, 2);                                \
            Z_ASSERT_EQ(vect_buffer->tab[0].ctx.level, Level);               \
            Z_ASSERT_LSTREQUAL(vect_buffer->tab[0].msg, LSTR("coucou2"));    \
            Z_ASSERT_LSTREQUAL(vect_buffer->tab[0].ctx.logger_name,          \
                               LSTR("blah"));                                \
            Z_ASSERT_EQ(vect_buffer->tab[1].ctx.level, Level);               \
            Z_ASSERT_LSTREQUAL(vect_buffer->tab[1].msg, LSTR("coucou"));     \
            Z_ASSERT_LSTREQUAL(vect_buffer->tab[1].ctx.logger_name,          \
                               LSTR("blah"));                                \
        } else {                                                             \
            Z_ASSERT_ZERO(vect_buffer->len);                                 \
        }                                                                    \
    } while (0)

#define TEST(Scope, Start, End, Level, Catch)  do {                          \
        const qv_t(log_buffer) *vect_buffer;                                 \
                                                                             \
        log_start_buffering(false);                                          \
        Start;                                                               \
        logger_cont("coucou");                                               \
        logger_cont("%d", 2);                                                \
        logger_cont("\ncou");                                                \
        logger_cont("cou");                                                  \
        End;                                                                 \
        vect_buffer = log_stop_buffering();                                  \
        _CHECK_RES(Level, Catch);                                            \
                                                                             \
        log_start_buffering(false);                                          \
        {                                                                    \
            Scope;                                                           \
            logger_cont("coucou");                                           \
            logger_cont("%d", 2);                                            \
            logger_cont("\ncou");                                            \
            logger_cont("cou");                                              \
        }                                                                    \
        vect_buffer = log_stop_buffering();                                  \
        _CHECK_RES(Level, Catch);                                            \
    } while (0)

        TEST(logger_error_scope(&l),
             logger_error_start(&l), logger_end(&l),
             LOG_ERR, true);
        TEST(logger_warning_scope(&l),
             logger_warning_start(&l), logger_end(&l),
             LOG_WARNING, true);
        TEST(logger_notice_scope(&l),
             logger_notice_start(&l), logger_end(&l),
             LOG_NOTICE, true);
        TEST(logger_info_scope(&l),
             logger_info_start(&l), logger_end(&l),
             LOG_INFO, true);
        TEST(logger_debug_scope(&l),
             logger_debug_start(&l), logger_end(&l),
             LOG_DEBUG, true);
        TEST(logger_trace_scope(&l, 0),
             logger_trace_start(&l, 0), logger_end(&l),
             LOG_TRACE, true);
        TEST(logger_trace_scope(&l, 3),
             logger_trace_start(&l, 3), logger_end(&l),
             LOG_TRACE + 3, false);

        logger_set_level(LSTR("blah"), LOG_TRACE + 8, 0);
        TEST(logger_trace_scope(&l, 3),
             logger_trace_start(&l, 3), logger_end(&l),
             LOG_TRACE + 3, true);
#undef TEST

        logger_wipe(&l);
    } Z_TEST_END;

    Z_TEST(thr, "concurrent access to a logger") {
        t_scope;
        thr_syn_t syn;
        thr_evc_t ec;
        logger_t parent0 = LOGGER_INIT_INHERITS(NULL, "thrbase");
        logger_t parent1[2] = {
            LOGGER_INIT_INHERITS(&parent0, "a"),
            LOGGER_INIT_INHERITS(&parent0, "b")
        };
        logger_t parent2[4] = {
            LOGGER_INIT_INHERITS(&parent1[0], "a"),
            LOGGER_INIT_INHERITS(&parent1[0], "b"),
            LOGGER_INIT_INHERITS(&parent1[1], "c"),
            LOGGER_INIT_INHERITS(&parent1[1], "d")
        };
        logger_t children[10] = {
            LOGGER_INIT_INHERITS(&parent2[0], "a"),
            LOGGER_INIT_INHERITS(&parent2[0], "b"),
            LOGGER_INIT_INHERITS(&parent2[1], "c"),
            LOGGER_INIT_INHERITS(&parent2[1], "d"),
            LOGGER_INIT_INHERITS(&parent2[2], "e"),
            LOGGER_INIT_INHERITS(&parent2[2], "f"),
            LOGGER_INIT_INHERITS(&parent2[3], "g"),
            LOGGER_INIT_INHERITS(&parent2[3], "h"),
            LOGGER_INIT_INHERITS(&parent2[3], "i"),
            LOGGER_INIT_INHERITS(&parent2[3], "j")
        };

        MODULE_REQUIRE(thr);

        thr_syn_init(&syn);
        thr_ec_init(&ec);

        for (size_t i = 0; i < thr_parallelism_g - 1; i++) {
            z_logger_init_job_t *job = t_new(z_logger_init_job_t, 1);

            job->job.run = &z_logger_init_job;
            job->count = countof(children);
            job->loggers = children;
            job->ec      = &ec;

            thr_syn_schedule(&syn, &job->job);
        }

        for (int c = 0; c < countof(children); c++) {
            sleep(1);
            thr_ec_broadcast(&ec);
            logger_notice(&children[c], "coucou");
        }

        Z_ASSERT(true);

        thr_syn_wait(&syn);
        thr_syn_wipe(&syn);
        thr_ec_wipe(&ec);

        MODULE_RELEASE(thr);
    } Z_TEST_END;

    Z_TEST(parse_specs, "test parsing of IS_DEBUG environment variable") {
        t_scope;
        qv_t(spec) specs;

        t_qv_init(spec, &specs, 8);

#define TEST(_str, _nb)  \
        do {                                                                 \
            qv_clear(spec, &specs);                                          \
            log_parse_specs(t_strdup(_str), &specs);                         \
            Z_ASSERT_EQ(specs.len, _nb);                                     \
        } while (0)

        TEST("   ", 0);

        TEST("log.c", 1);
        Z_ASSERT_STREQUAL(specs.tab[0].path, "log.c");
        Z_ASSERT_NULL(specs.tab[0].func);
        Z_ASSERT_NULL(specs.tab[0].name);
        Z_ASSERT_EQ(specs.tab[0].level, INT_MAX);

        TEST("@log_parse_specs", 1);
        Z_ASSERT_NULL(specs.tab[0].path);
        Z_ASSERT_STREQUAL(specs.tab[0].func, "log_parse_specs");
        Z_ASSERT_NULL(specs.tab[0].name);
        Z_ASSERT_EQ(specs.tab[0].level, INT_MAX);

        TEST("+log/tracing", 1);
        Z_ASSERT_NULL(specs.tab[0].path);
        Z_ASSERT_NULL(specs.tab[0].func);
        Z_ASSERT_STREQUAL(specs.tab[0].name, "log/tracing");
        Z_ASSERT_EQ(specs.tab[0].level, INT_MAX);

        TEST(":5", 1);
        Z_ASSERT_NULL(specs.tab[0].path);
        Z_ASSERT_NULL(specs.tab[0].func);
        Z_ASSERT_NULL(specs.tab[0].name);
        Z_ASSERT_EQ(specs.tab[0].level, LOG_TRACE + 5);

        TEST(" log.c@log_parse_specs+log/tracing:5  ", 1);
        Z_ASSERT_STREQUAL(specs.tab[0].path, "log.c");
        Z_ASSERT_STREQUAL(specs.tab[0].func, "log_parse_specs");
        Z_ASSERT_STREQUAL(specs.tab[0].name, "log/tracing");
        Z_ASSERT_EQ(specs.tab[0].level, LOG_TRACE + 5);

        TEST(" log.c@log_parse_specs+log/tracing:5  log.c", 2);
        Z_ASSERT_STREQUAL(specs.tab[0].path, "log.c");
        Z_ASSERT_STREQUAL(specs.tab[0].func, "log_parse_specs");
        Z_ASSERT_STREQUAL(specs.tab[0].name, "log/tracing");
        Z_ASSERT_EQ(specs.tab[0].level, LOG_TRACE + 5);
        Z_ASSERT_STREQUAL(specs.tab[1].path, "log.c");
        Z_ASSERT_NULL(specs.tab[1].func);
        Z_ASSERT_NULL(specs.tab[1].name);
        Z_ASSERT_EQ(specs.tab[1].level, INT_MAX);

#undef TEST
    } Z_TEST_END;

} Z_GROUP_END;

/* }}} */
