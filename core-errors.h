/**************************************************************************/
/*                                                                        */
/*  Copyright (C) 2004-2011 INTERSEC SAS                                  */
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
#  error "you must include core.h instead"
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

#  define e_trace(...)              (void)0
#  define e_trace_hex(...)          (void)0
#  define e_trace_start(...)        (void)0
#  define e_trace_cont(...)         (void)0
#  define e_trace_end(...)          (void)0

#  define e_named_trace(...)        (void)0
#  define e_named_trace_hex(...)    (void)0
#  define e_named_trace_start(...)  (void)0
#  define e_named_trace_cont(...)   (void)0
#  define e_named_trace_end(...)    (void)0

#  define e_set_verbosity(...)      (void)0
#  define e_incr_verbosity(...)     (void)0
#  define e_is_traced(...)          false
#  define e_name_is_traced(...)     false

#else

void e_set_verbosity(int max_debug_level);
void e_incr_verbosity(void);

int  e_is_traced_(int level, const char *fname, const char *func,
                  const char *name);

#define e_name_is_traced(lvl, name) \
    ({ static int8_t e_traced;                                               \
       if (unlikely(e_traced == 0))                                          \
           e_traced = e_is_traced_(lvl, __FILE__, __func__, name);           \
       likely(e_traced > 0); })
#define e_is_traced(lvl)  e_name_is_traced(lvl, NULL)

void e_trace_put_(int lvl, const char *fname, int lno, const char *func,
                  const char *name, const char *fmt, ...)
                  __attr_printf__(6, 7);

#define e_named_trace_start(lvl, name, fmt, ...) \
    do {                                                                     \
        if (e_name_is_traced(lvl, name))                                     \
            e_trace_put_(lvl, __FILE__, __LINE__, __func__,                  \
                         name, fmt, ##__VA_ARGS__);                          \
    } while (0)
#define e_named_trace_cont(lvl, name, fmt, ...) \
    e_named_trace_start(lvl, name, fmt, ##__VA_ARGS__)
#define e_named_trace_end(lvl, name, fmt, ...) \
    e_named_trace_start(lvl, name, fmt "\n", ##__VA_ARGS__)
#define e_named_trace(lvl, name, fmt, ...) \
    e_named_trace_start(lvl, name, fmt "\n", ##__VA_ARGS__)
#define e_named_trace_hex(lvl, name, str, buf, len)                          \
    do {                                                                     \
        if (e_name_is_traced(lvl, name)) {                                   \
            e_trace(lvl, "--%s (%d)--\n", str, len);                         \
            ifputs_hex(stderr, buf, len);                                    \
        }                                                                    \
    } while (0)

#define e_trace_start(lvl, fmt, ...)  e_named_trace_start(lvl, NULL, fmt, ##__VA_ARGS__)
#define e_trace_cont(lvl, fmt, ...)   e_named_trace_cont(lvl, NULL, fmt, ##__VA_ARGS__)
#define e_trace_end(lvl, fmt, ...)    e_named_trace_end(lvl, NULL, fmt, ##__VA_ARGS__)
#define e_trace(lvl, fmt, ...)        e_named_trace(lvl, NULL, fmt, ##__VA_ARGS__)
#define e_trace_hex(lvl, str, buf, len) \
    e_named_trace_hex(lvl, NULL, str, buf, len)

#endif

#endif
