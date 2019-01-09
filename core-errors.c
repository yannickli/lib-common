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

#include <syslog.h>
#include "core.h"

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

#define E_FUNCTION(name, level, action)   \
    int name(const char *fmt, ...) {      \
        va_list ap;                       \
        va_start(ap, fmt);                \
        (*handler_g)(level, fmt, ap);     \
        va_end(ap);                       \
        action;                           \
    }

/* Error reporting functions */

E_FUNCTION(e_panic,   LOG_CRIT,    abort());
E_FUNCTION(e_fatal,   LOG_CRIT,    exit(127));
E_FUNCTION(e_error,   LOG_ERR,     return -1);
E_FUNCTION(e_warning, LOG_WARNING, return -1);
E_FUNCTION(e_notice,  LOG_NOTICE,  return  0);
E_FUNCTION(e_info,    LOG_INFO,    return  0);
E_FUNCTION(e_debug,   LOG_DEBUG,   return  0);

void e_set_handler(e_handler_f *handler)
{
    handler_g = handler;
}

void e_init_stderr(void)
{
    handler_g = &stderr_handler;
}
