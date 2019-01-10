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

#include "core.h"
#include "unix.h"

static void stderr_handler(int priority, const char *format, va_list args)
    __attr_printf__(2,0);

static e_handler_f *handler_g = &stderr_handler;

static void stderr_handler(int priority, const char *format, va_list args)
{
    static char const *prefixes[] = {
        [LOG_CRIT]     = "fatal: ",
        [LOG_ERR]      = "error: ",
        [LOG_WARNING]  = "warn:  ",
        [LOG_NOTICE]   = "note:  ",
        [LOG_INFO]     = "info:  ",
        [LOG_DEBUG]    = "debug: ",
    };
    fputs(prefixes[priority], stderr);
    vfprintf(stderr, format, args);
    fputc('\n', stderr);
}

/**************************************************************************/
/* public API                                                             */
/**************************************************************************/

#define E_FUNCTION_VF(name, level, action)                 \
    __attr_printf__(1,0)                                   \
    static int name##_vf(const char *fmt, va_list args) {  \
        (*handler_g)(level, fmt, args);                    \
        action;                                            \
    }

#define E_FUNCTION(name, level, action)                    \
    int name(const char *fmt, ...) {                       \
        va_list ap;                                        \
        va_start(ap, fmt);                                 \
        (*handler_g)(level, fmt, ap);                      \
        va_end(ap);                                        \
        action;                                            \
    }

#define E_FUNCTIONS(name, level, action)                   \
    E_FUNCTION_VF(name, level, action)                     \
    E_FUNCTION(name, level, action)


#define E_FUNCTION_SYSLOG_VF(name, level, action)          \
    __attr_printf__(1,0)                                   \
    static int name##_vf(const char *fmt, va_list args) {  \
        va_list ap;                                        \
        va_copy(ap, args);                                 \
        (*handler_g)(level, fmt, args);                    \
        vsyslog(LOG_USER | level, fmt, ap);                \
        va_end(ap);                                        \
        action;                                            \
    }

#define E_FUNCTION_SYSLOG(name, level, action)             \
    int name(const char *fmt, ...) {                       \
        va_list ap;                                        \
        va_start(ap, fmt);                                 \
        (*handler_g)(level, fmt, ap);                      \
        va_end(ap);                                        \
        va_start(ap, fmt);                                 \
        vsyslog(LOG_USER | level, fmt, ap);                \
        va_end(ap);                                        \
        action;                                            \
    }

#define E_FUNCTIONS_SYSLOG(name, level, action)            \
    E_FUNCTION_SYSLOG_VF(name, level, action)              \
    E_FUNCTION_SYSLOG(name, level, action)

/* Error reporting functions */

__attr_noreturn__
static void fatality(void)
{
    if (psinfo_get_tracer_pid(0) > 0) {
        abort();
    }
    exit(127);
}

E_FUNCTION_SYSLOG (e_panic,   LOG_CRIT,    abort());
E_FUNCTIONS_SYSLOG(e_fatal,   LOG_CRIT,    fatality());
E_FUNCTIONS       (e_error,   LOG_ERR,     return -1);
E_FUNCTIONS       (e_warning, LOG_WARNING, return -1);
E_FUNCTIONS       (e_notice,  LOG_NOTICE,  return  0);
E_FUNCTIONS       (e_info,    LOG_INFO,    return  0);
E_FUNCTIONS       (e_debug,   LOG_DEBUG,   return  0);

typedef int (error_vf_f)(const char *, va_list) __attr_printf__(1,0);

__attribute__((format(printf, 2, 3)))
int e_log(int priority, const char *fmt, ...)
{
    static error_vf_f *functions[] = {
        [LOG_CRIT]    = e_fatal_vf,
        [LOG_ERR]     = e_error_vf,
        [LOG_WARNING] = e_warning_vf,
        [LOG_NOTICE]  = e_notice_vf,
        [LOG_INFO]    = e_info_vf,
        [LOG_DEBUG]   = e_debug_vf,
    };
    int res;
    va_list args;

    if ((unsigned)priority >= countof(functions) || !functions[priority])
        priority = LOG_ERR;

    va_start(args, fmt);
    res = (*functions[priority])(fmt, args);
    va_end(args);

    return res;
}

void e_set_handler(e_handler_f *handler)
{
    handler_g = handler;
}

void e_init_stderr(void)
{
    handler_g = &stderr_handler;
}
