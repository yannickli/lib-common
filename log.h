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

#ifndef IS_LIB_COMMON_LOG_H
#define IS_LIB_COMMON_LOG_H

#include "container-qvector.h"
#include "core.h"
#include "core.iop.h"
#include "iop-rpc.h"

/** \defgroup log Logging facility.
 * \ingroup log
 * \brief Logging Facility.
 *
 * \{
 *
 * \section log_intro Loggers
 *
 * This module provides a hierarchical logging facility. This is done through
 * a tree of \ref logger_t structures. Each logger comes with it's own logging
 * level (no log with a level greater to that one will be printed), that can
 * be inherited from its parent.
 *
 * Each \ref logger_t also comes with a name, that name can be printed in the
 * logs, allowing more contextual informations to be provided.
 *
 * When a log is rejected it costs near to 0 since only an integer comparison
 * is performed. This let you put logging even in critical paths with low
 * impact on global performances. Arguments of the logging functions are
 * evaluated only in case the log is emitted. As a consequence, you must take
 * care only passing arguments that have no side effects.
 *
 *
 * Every logger is initialized with a default logging level. That level can be
 * either an inheritance mark, in which case the logger will always inherits
 * its level from its parent (unless explicitely set for that particular
 * logger).
 *
 * You can manage the logging of every single logger independently from each
 * other or choose to update the level of the whole tree of loggers at once.
 * This gives you (and the user) a fine-grained control on what should be
 * logged.
 *
 *
 * \section log_hander Handlers
 *
 * When a log is accepted, it is pushed to a handler that performs the output.
 * The handler can be changed by a call to \ref log_set_handler. A default
 * handler that prints on stderr is provided.
 */

/* Logger {{{ */

extern uint32_t log_conf_gen_g;

/** Minimum level for tracing.
 *
 * You can use any level equal or greater to that value as tracing.
 */
#define LOG_TRACE        (LOG_DEBUG + 1)

/** Log level is inherited from the parent logger.
 */
#define LOG_INHERITS     (-1)

/** No level defined from the logger.
 *
 * This is a private value, used only internally.
 * Note that -2 is not used because this is the value for DEFAULT in the IOP
 * enum LogLevel.
 */
#define LOG_UNDEFINED    (-3)

enum {
    LOG_RECURSIVE = 1 << 0, /**< Force level to be set recursively. */
    LOG_FORCED    = 1 << 1, /**< Level has been recursively forced. internal */
    LOG_SILENT    = 1 << 2, /**< Log handler is called, but default one does
                                 nothing. */
};
#define LOG_MK_FLAGS(_recursive, _silent)  (_recursive << 0 | _silent << 2)

/** Logger structure.
 *
 * That structure bears the name of the logging module and the level it
 * accepts. It can be either allocated (see \ref logger_init, \ref logger_new)
 * or static (see \ref LOGGER_INIT).
 *
 * You should not touch the fields of that structure. It is provided only for
 * the sake of inlining.
 */
typedef struct logger_t {
    atomic_uint conf_gen;
    flag_t   is_static : 1;

    int level;
    int defined_level;
    int default_level;
    unsigned level_flags;
    unsigned default_level_flags;

    lstr_t name;
    lstr_t full_name;
    struct logger_t *parent;
    dlist_t children;
    dlist_t siblings;
} logger_t;

/** Initialize a logger with a default \p LogLevel.
 *
 * Note that you don't have to wipe a logger initialized with LOGGER_INIT and
 * derivates.
 *
 * \param[in] Parent    The parent logger (NULL if parent is the root logger)
 * \param[in] Name      The name of the logger
 * \param[in] LogLevel  The maximum log level activated by default for that
 *                      logger. (can be LOG_INHERITS).
 */
#define LOGGER_INIT(Parent, Name, LogLevel)  {                               \
        .is_static     = true,                                               \
        .parent        = (Parent),                                           \
        .name          = LSTR_IMMED(Name),                                   \
        .level         = LOG_UNDEFINED,                                      \
        .defined_level = LOG_UNDEFINED,                                      \
        .default_level = (LogLevel),                                         \
    }

/** Initialize a silent logger with a default \p LogLevel.
 *
 * Silent loggers are loggers for which the default log handler does nothing.
 * This can be used if you want the log handler to be called even if you don't
 * want to print anything.
 *
 * \see LOGGER_INIT
 */
#define LOGGER_INIT_SILENT(Parent, Name, LogLevel)  {                        \
        .is_static           = true,                                         \
        .parent              = (Parent),                                     \
        .name                = LSTR_IMMED(Name),                             \
        .level               = LOG_UNDEFINED,                                \
        .defined_level       = LOG_UNDEFINED,                                \
        .default_level       = (LogLevel),                                   \
        .level_flags         = LOG_SILENT,                                   \
        .default_level_flags = LOG_SILENT,                                   \
    }

/** Initialize a logger that inherits it's parent level.
 *
 * \see LOGGER_INIT
 */
#define LOGGER_INIT_INHERITS(Parent, Name)                                   \
    LOGGER_INIT(Parent, Name, LOG_INHERITS)

/** Initialize a silent logger that inherits it's parent level.
 *
 * \see LOGGER_INIT_SILENT
 */
#define LOGGER_INIT_SILENT_INHERITS(Parent, Name)                            \
    LOGGER_INIT_SILENT(Parent, Name, LOG_INHERITS)

logger_t *logger_init(logger_t *logger, logger_t *parent, lstr_t name,
                      int default_level, unsigned level_flags) __leaf;
logger_t *logger_new(logger_t *parent, lstr_t name, int default_level,
                     unsigned level_flags) __leaf;

void logger_wipe(logger_t *logger) __leaf;
GENERIC_DELETE(logger_t, logger)

/* }}} */
/* Private functions {{{ */

void __logger_refresh(logger_t *logger) __leaf __cold;

static ALWAYS_INLINE
int logger_get_level(logger_t *logger)
{
    if (atomic_load_explicit(&logger->conf_gen, memory_order_acquire)
        != log_conf_gen_g)
    {
        __logger_refresh(logger);
    }
    return logger->level;
}

static ALWAYS_INLINE
bool logger_has_level(logger_t *logger, int level)
{
    return logger_get_level(logger) >= level;
}

static ALWAYS_INLINE __cold
void __logger_cold(void)
{
    /* This function is just a marker for error cases */
}

/* }}} */
/* Logging {{{ */

__attr_printf__(8, 0)
int logger_vlog(logger_t *logger, int level, const char *prog, int pid,
                const char *file, const char *func, int line,
                const char *fmt, va_list va);

__attr_printf__(8, 9)
int __logger_log(logger_t *logger, int level, const char *prog, int pid,
                 const char *file, const char *func, int line,
                 const char *fmt, ...);

__attr_printf__(5, 6) __attr_noreturn__ __cold
void __logger_panic(logger_t *logger, const char *file, const char *func,
                    int line, const char *fmt, ...);

__attr_printf__(5, 6) __attr_noreturn__ __cold
void __logger_fatal(logger_t *logger, const char *file, const char *func,
                    int line, const char *fmt, ...);


#define logger_log(Logger, Level, Fmt, ...)                                  \
    __logger_log((Logger), (Level), NULL, -1, __FILE__, __func__, __LINE__,  \
                 (Fmt), ##__VA_ARGS__)

#define logger_panic(Logger, Fmt, ...)                                       \
    __logger_panic((Logger), __FILE__, __func__, __LINE__, (Fmt), ##__VA_ARGS__)

#define logger_fatal(Logger, Fmt, ...)                                       \
    __logger_fatal((Logger), __FILE__, __func__, __LINE__, (Fmt), ##__VA_ARGS__)


#define __LOGGER_LOG(Logger, Level, Mark, Fmt, ...)  ({                      \
        const logger_t *__clogger = (Logger);                                \
        logger_t *__logger = (logger_t *)__clogger;                          \
        const int __level = (Level);                                         \
                                                                             \
        Mark;                                                                \
        if (logger_has_level(__logger, __level)) {                           \
            __logger_log(__logger, __level, NULL, -1, __FILE__, __func__,    \
                         __LINE__, Fmt, ##__VA_ARGS__);                      \
        }                                                                    \
        __level <= LOG_WARNING ? -1 : 0;                                     \
    })

#define logger_error(Logger, Fmt, ...)                                       \
    __LOGGER_LOG(Logger, LOG_ERR, __logger_cold(), Fmt, ##__VA_ARGS__)

#define logger_warning(Logger, Fmt, ...)                                     \
    __LOGGER_LOG(Logger, LOG_WARNING, __logger_cold(), Fmt, ##__VA_ARGS__)

#define logger_notice(Logger, Fmt, ...)                                      \
    __LOGGER_LOG(Logger, LOG_NOTICE,, Fmt, ##__VA_ARGS__)

#define logger_info(Logger, Fmt, ...)                                        \
    __LOGGER_LOG(Logger, LOG_INFO,, Fmt, ##__VA_ARGS__)

#define logger_debug(Logger, Fmt, ...)                                       \
    __LOGGER_LOG(Logger, LOG_DEBUG,, Fmt, ##__VA_ARGS__)


#ifndef NDEBUG

int __logger_is_traced(logger_t *logger, int level, const char *file,
                       const char *func, const char *name);

#define logger_is_traced(Logger, Level)  ({                                  \
        static int8_t __traced;                                              \
        static const logger_t *__last_logger = NULL;                         \
        const logger_t *__i_clogger = (Logger);                              \
        logger_t *__i_logger = (logger_t *)__i_clogger;                      \
        const int __i_level = (Level);                                       \
        bool __h_level = logger_has_level(__i_logger, LOG_TRACE + __i_level);\
                                                                             \
        if (!__h_level) {                                                    \
            if (unlikely(!__builtin_constant_p(Level)                        \
                       || __i_clogger != __last_logger))                     \
            {                                                                \
                __traced = __logger_is_traced(__i_logger, __i_level,         \
                                              __FILE__, __func__,            \
                                              __i_logger->full_name.s);      \
                __last_logger = __i_clogger;                                 \
            }                                                                \
        }                                                                    \
        __h_level || __traced > 0;                                           \
    })

#define __LOGGER_TRACE(Logger, Level, Fmt, ...)  ({                          \
        const logger_t *__clogger = (Logger);                                \
        logger_t *__logger = (logger_t *)__clogger;                          \
        const int __level = (Level);                                         \
                                                                             \
        if (logger_is_traced(__logger, __level)) {                           \
            __logger_log(__logger, LOG_TRACE + __level, NULL, -1, __FILE__,  \
                         __func__, __LINE__, Fmt, ##__VA_ARGS__);            \
        }                                                                    \
        0;                                                                   \
    })

#define logger_trace(Logger, Level, Fmt, ...)                                \
    __LOGGER_TRACE(Logger, (Level), Fmt, ##__VA_ARGS__)

#else

#define logger_is_traced(Logger, Level)  ({                                  \
        const logger_t *__i_clogger = (Logger);                              \
        logger_t *__i_logger = (logger_t *)__i_clogger;                      \
                                                                             \
        logger_has_level(__i_logger, (Level) + LOG_TRACE);                   \
    })

#define logger_trace(Logger, Level, Fmt, ...)                                \
    __LOGGER_LOG(Logger, LOG_TRACE + (Level),, Fmt, ##__VA_ARGS__)

#endif

/* }}} */
/* Configuration {{{ */

/** Set the maximum logging level for \p logger.
 *
 * \param[in] name   The full name of the logger that should be updated
 * \param[in] level  The new level (can be LOG_INHERITS)
 * \param[in] flags  The flags to apply. (LOG_RECURSIVE if you want that
 *                   level to all children with no forced level).
 * \return The previous level.
 */
int logger_set_level(lstr_t name, int level, unsigned flags) __leaf;

/** Reset the maximum logging level of \p logger to its default level.
 *
 * \param[in] name  The full name of the logger that should be updated
 * \return The previous level.
 */
int logger_reset_level(lstr_t name) __leaf;


/** Set the configuration of the logging system.
 */
void logger_configure(const core__log_configuration__t *conf);

/** IOP configuration interface.
 */

void IOP_RPC_IMPL(core__core, log, set_root_level);
void IOP_RPC_IMPL(core__core, log, reset_root_level);
void IOP_RPC_IMPL(core__core, log, set_logger_level);
void IOP_RPC_IMPL(core__core, log, reset_logger_level);

/* }}} */
/* Handlers {{{ */

typedef struct log_ctx_t {
    int    level;
    lstr_t logger_name;

    const char *file;
    const char *func;
    int line;

    int pid;
    const char *prog_name;

    flag_t is_silent :  1;
    flag_t padding   : 31;
} log_ctx_t;

typedef void (log_handler_f)(const log_ctx_t *ctx, const char *fmt, va_list va)
    __attr_printf__(2, 0);

/** Default log handler.
 *
 * That log handler prints on stderr.
 */
extern log_handler_f *log_stderr_handler_g;

/** Default log handler tee fd.
 *
 * If set, the default log handler will also print on this fd.
 */
extern int log_stderr_handler_teefd_g;

/** Change the handler.
 *
 * This also returns the previous handler.
 */
log_handler_f *log_set_handler(log_handler_f *handler);

/* }}} */
/* Log buffer {{{ */

/** Logs buffer.
 *
 * This buffer captures all logger messages after it has been started
 * and it stops the capture when it is stopped.
 *
 * Buffer imbrication:
 * However if the buffer contains an other buffer it does not captures the log
 * messages emitted after the second call of \ref log_start_buffering.
 * When the second is stopped the capture restarts.
 *
 * |--->log_start_buffering();
 * |
 * |   log_msg1;
 * |
 * |   |--->log_start_buffering();
 * |   |
 * |   |  log_msg2;
 * |   |  log_msg3;
 * |   |
 * |   |--->log_stop_buffering(); -------> return qv which [msg2, msg3]
 * |
 * |   log_msg3;
 * |
 * |--->log_stop_buffering();------------> return qv which [msg1, msg4]
 *
 */

typedef struct log_buffer_t {
    log_ctx_t ctx;
    lstr_t msg;
} log_buffer_t;

qvector_t(log_buffer, log_buffer_t);

/** Start buffer for logger and filter on log level.
 *
 * \param[in] use_handler    if false, the log handler won't be called for the
 *                           logs emitted until log_stop_buffering is called.
 * \param[in] log_level      Set the maximum log level for captured logs.
 */
void log_start_buffering_filter(bool use_handler, int log_level);

/** Start buffer for logger.
 *
 * \param[in] use_handler    if false, the log handler won't be called for the
 * logs emitted until log_stop_buffering is called.
 */
void log_start_buffering(bool use_handler);

/** Stop buffer previously started for logger.
 *
 * \return the list of the logs that were emitted since the last call to
 * log_start_buffering, in the order or emission.
 */
const qv_t(log_buffer) *log_stop_buffering(void);

/* }}} */
/** \} */

#endif
