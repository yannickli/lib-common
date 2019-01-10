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

#if !defined(IS_LIB_COMMON_CORE_H) || defined(IS_LIB_COMMON_CORE_ERRORS_H)
#  error "you must include <lib-common/core.h> instead"
#else
#define IS_LIB_COMMON_CORE_ERRORS_H

typedef void (e_handler_f)(int, const char *, va_list) __attr_printf__(2, 0);
typedef int (error_f)(const char *, ...) __attr_printf__(1, 2);

#define E_PREFIX(fmt) \
    ("%s:%d:%s: " fmt), __FILE__, __LINE__, __func__

#define E_UNIXERR(funcname)  funcname ": %m"

/* These functions are meant to correspond to the syslog levels.  */
error_f e_fatal  __attr_noreturn__;
error_f e_panic  __attr_noreturn__;
error_f e_error;
error_f e_warning;
error_f e_notice;
error_f e_info;
error_f e_debug;

void e_init_stderr(void);
void e_set_handler(e_handler_f *handler);

/**************************************************************************/
/* Debug part                                                             */
/**************************************************************************/

#ifdef NDEBUG

#  define e_trace(...)           (void)0
#  define e_trace_hex(...)       (void)0
#  define e_trace_start(...)     (void)0
#  define e_trace_cont(...)      (void)0
#  define e_trace_end(...)       (void)0

#  define e_set_verbosity(...)   (void)0
#  define e_incr_verbosity(...)  (void)0
#  define e_is_traced_real(...)  false
#  define e_is_traced(...)       false

#else

void e_set_verbosity(int max_debug_level);
void e_incr_verbosity(void);

bool e_is_traced_real(int level, const char *fname, const char *func);

void e_trace_put(int lvl, const char *fname, int lno, const char *func,
                 const char *fmt, ...)
                 __attr_printf__(5, 6);

#define e_is_traced(lvl)              e_is_traced_real(lvl, __FILE__, __func__)

#define e_trace_start(lvl, fmt, ...)                                         \
    e_trace_put(lvl, __FILE__, __LINE__, __func__, fmt, ##__VA_ARGS__)
#define e_trace_cont(lvl, fmt, ...)   e_trace_start(lvl, fmt, ##__VA_ARGS__)
#define e_trace_end(lvl, fmt, ...)    e_trace_start(lvl, fmt "\n", ##__VA_ARGS__)
#define e_trace(lvl, fmt, ...)        e_trace_start(lvl, fmt "\n", ##__VA_ARGS__)

#define e_trace_hex(lvl, str, buf, len)                                      \
    do {                                                                     \
        if (e_is_traced(lvl)) {                                              \
            e_trace(lvl, "--%s (%d)--\n", str, len);                         \
            ifputs_hex(stderr, buf, len);                                    \
        }                                                                    \
    } while (0)

#endif

#endif
