/**************************************************************************/
/*                                                                        */
/*  Copyright (C) 2004-2006 INTERSEC                                      */
/*                                                                        */
/*  Should you receive a copy of this source code, you must check you     */
/*  have a proper, written authorization of INTERSEC to hold it. If you   */
/*  don't have such an authorization, you must DELETE all source code     */
/*  files in your possession, and inform INTERSEC of the fact you obtain  */
/*  these files. Should you not comply to these terms, you can be         */
/*  prosecuted in the extent permitted by applicable law.                 */
/*                                                                        */
/**************************************************************************/

#ifndef IS_LIB_COMMON_ERR_REPORT_H
#define IS_LIB_COMMON_ERR_REPORT_H

#include <stdarg.h>

#include "macros.h"

/* Useful exit codes */
enum fatal_exit_codes {
    FATAL_NOMEM    = 0x80, /* out of memory */
    FATAL_LOGOPEN  = 0x81, /* can't open log file */
    FATAL_LOGWRITE = 0x82, /* can't write log */
    FATAL_DEFAULT  = 0xff,  /* Default exit code */
};

/* error reporting functions */
typedef void fatal_f(int, const char *, ...) __attr_format__(2, 3);
typedef void error_f(const char *, ...) __attr_format__(1, 2);

/*
 * These functions are meant to correspond to the syslog levels.
 *
 * e_fatal/e_panic exit the program.
 * e_debug does not add a terminating '\n' whereas all others do
 *
 */
fatal_f e_fatal __attribute__((noreturn));
error_f e_panic __attribute__((noreturn));
error_f e_error;
error_f e_warning;
error_f e_notice;
error_f e_info;


#define E_PREFIX(fmt) \
    ("%s:%d:%s: " fmt), __FILE__, __LINE__, __func__

#define E_UNIXERR(funcname)  funcname ": %m"

/* callback installers */
typedef void e_callback_f(const char *, va_list);

e_callback_f *e_set_fatal_handler   (e_callback_f *) __attribute__((nonnull));
e_callback_f *e_set_error_handler   (e_callback_f *) __attribute__((nonnull));
e_callback_f *e_set_warning_handler (e_callback_f *) __attribute__((nonnull));
e_callback_f *e_set_notice_handler  (e_callback_f *) __attribute__((nonnull));
e_callback_f *e_set_info_handler    (e_callback_f *) __attribute__((nonnull)); 

/* useful callbacks */
void e_init_stderr(void);
void e_init_file(const char *ident, const char *filename);
void e_init_syslog(const char *ident, int options, int facility);

void e_shutdown(void);

/**************************************************************************/
/* Debug part                                                             */
/**************************************************************************/

#ifdef NDEBUG

#  define e_debug(...)
#  define e_trace(...)
#  define e_trace_start(...)
#  define e_trace_cont(...)
#  define e_trace_end(...)

#  define e_verbosity_maxwatch   INT_MIN
#  define e_set_verbosity(...)
#  define e_incr_verbosity(...)
#  define e_is_traced_real(...)  false
#  define e_is_traced(...)       false

#else

extern int e_verbosity_maxwatch;

void e_set_verbosity(int max_debug_level);
void e_incr_verbosity(void);

int e_is_traced_real(int level, const char *fname, const char *func);

#define e_is_traced(lvl)                                 \
        (lvl <= e_verbosity_maxwatch                     \
         && e_is_traced_real(lvl, __FILE__, __func__))

#define e_debug(lvl, fmt, ...)                                               \
    do {                                                                     \
        if (e_is_traced(lvl)) {                                              \
            fprintf(stderr, fmt, ##__VA_ARGS__);                             \
        }                                                                    \
    } while (0)

#define e_trace_start(lvl, fmt, ...)  e_debug(lvl, E_PREFIX(fmt), ##__VA_ARGS__)
#define e_trace_cont(lvl, fmt, ...)   e_debug(lvl, fmt, ##__VA_ARGS__)
#define e_trace_end(lvl, fmt, ...)    e_debug(lvl, fmt "\n", ##__VA_ARGS__)

#define e_trace(lvl, fmt, ...)        e_trace_start(lvl, fmt "\n", ##__VA_ARGS__)

#endif

#endif /* IS_LIB_COMMON_ERR_REPORT_H */
