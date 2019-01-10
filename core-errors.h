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
#  error "you must include core.h instead"
#else
#define IS_LIB_COMMON_CORE_ERRORS_H

#include <syslog.h>

typedef void (e_handler_f)(int, const char *, va_list) __attr_printf__(2, 0);
typedef int (error_f)(const char *, ...) __attr_printf__(1, 2);

#define E_PREFIX(fmt) \
    ("%s:%d:%s: " fmt), __FILE__, __LINE__, __func__

#define E_UNIXERR(funcname)  funcname ": %m"

/* These functions are meant to correspond to the syslog levels.  */
int e_fatal(const char *, ...)
    __leaf __attr_noreturn__ __cold __attr_printf__(1, 2);
int e_panic(const char *, ...)
    __leaf __attr_noreturn__ __cold __attr_printf__(1, 2);
int e_error(const char *, ...)
    __leaf __cold __attr_printf__(1, 2);
int e_warning(const char *, ...)
    __leaf __cold __attr_printf__(1, 2);
int e_notice(const char *, ...)
    __leaf __attr_printf__(1, 2);
int e_info(const char *, ...)
    __leaf __attr_printf__(1, 2);
int e_debug(const char *, ...)
    __leaf __attr_printf__(1, 2);

int e_log(int priority, const char *fmt, ...)
    __leaf __attribute__((format(printf, 2, 3)));

void e_init_stderr(void) __leaf;
void e_set_handler(e_handler_f *handler) __leaf;

/** This macro provides assertions that remain activated in production builds.
 *
 * \param[in] level The syslog level (fatal, panic, error, warning, notice,
 *                  info or debug).
 * \param[in] Cond  The condition to verify
 * \param[in] fmt   The message to log if the condition is not verified.
 */
#define __e_assert(level, Cond, StrCond, fmt, ...)  do {                     \
        if (unlikely(!(Cond))) {                                             \
            e_##level(E_PREFIX("assertion failed: \"%s\": "fmt), StrCond,    \
                      ##__VA_ARGS__);                                        \
        }                                                                    \
    } while (0)

#define e_assert(level, Cond, fmt, ...)  \
    __e_assert(level, (Cond), #Cond, fmt, ##__VA_ARGS__)

#define e_assert_n(level, Expr, fmt, ...)  \
    e_assert(level, (Expr) >= 0, fmt, ##__VA_ARGS__)

#define e_assert_neg(level, Expr, fmt, ...)  \
    e_assert(level, (Expr) < 0, fmt, ##__VA_ARGS__)

#define e_assert_p(level, Expr, fmt, ...)  \
    e_assert(level, (Expr) != NULL, fmt, ##__VA_ARGS__)

#define e_assert_null(level, Expr, fmt, ...)  \
    e_assert(level, (Expr) == NULL, fmt, ##__VA_ARGS__)

/**************************************************************************/
/* Debug part                                                             */
/**************************************************************************/

#ifdef NDEBUG

static ALWAYS_INLINE void e_ignore(int level, ...) { }
#define e_trace_ignore(level, ...) if (false) e_ignore(level, ##__VA_ARGS__)

static ALWAYS_INLINE void assert_ignore(bool cond) { }
#undef  assert
#define assert(Cond)  ({ if (false) assert_ignore(Cond); (void)0; })

#undef  expect
#define expect(Cond)  likely(!!(Cond))

#  define e_trace(level, ...)              e_trace_ignore(level, ##__VA_ARGS__)
#  define e_trace_hex(level, ...)          e_trace_ignore(level, ##__VA_ARGS__)
#  define e_trace_start(level, ...)        e_trace_ignore(level, ##__VA_ARGS__)
#  define e_trace_cont(level, ...)         e_trace_ignore(level, ##__VA_ARGS__)
#  define e_trace_end(level, ...)          e_trace_ignore(level, ##__VA_ARGS__)

#  define e_named_trace(level, ...)        e_trace_ignore(level, ##__VA_ARGS__)
#  define e_named_trace_hex(level, ...)    e_trace_ignore(level, ##__VA_ARGS__)
#  define e_named_trace_start(level, ...)  e_trace_ignore(level, ##__VA_ARGS__)
#  define e_named_trace_cont(level, ...)   e_trace_ignore(level, ##__VA_ARGS__)
#  define e_named_trace_end(level, ...)    e_trace_ignore(level, ##__VA_ARGS__)

#  define e_set_verbosity(...)      (void)0
#  define e_incr_verbosity(...)     (void)0
#  define e_is_traced(...)          false
#  define e_name_is_traced(...)     false

#else

void e_set_verbosity(int max_debug_level) __leaf;
void e_incr_verbosity(void) __leaf;

int  e_is_traced_(int level, const char *fname, const char *func,
                  const char *name) __leaf;

#define e_name_is_traced(lvl, name) \
    ({                                                                       \
       int8_t e_res;                                                         \
                                                                             \
       if (__builtin_constant_p(lvl) && __builtin_constant_p(name)) {        \
           static int8_t e_traced;                                           \
                                                                             \
           if (unlikely(e_traced == 0)) {                                    \
               e_traced = e_is_traced_(lvl, __FILE__, __func__, name);       \
           }                                                                 \
           e_res = e_traced;                                                 \
       } else {                                                              \
           e_res = e_is_traced_(lvl, __FILE__, __func__, name);              \
       }                                                                     \
       e_res > 0;                                                            \
    })
#define e_is_traced(lvl)  e_name_is_traced(lvl, NULL)

void e_trace_put_(int lvl, const char *fname, int lno, const char *func,
                  const char *name, const char *fmt, ...)
                  __leaf __attr_printf__(6, 7) __cold;

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
            e_trace_put_(lvl, __FILE__, __LINE__, __func__,                  \
                         name, "--%s (%d)--\n", str, len);                   \
            ifputs_hex(stderr, buf, len);                                    \
        }                                                                    \
    } while (0)

#define e_trace_start(lvl, fmt, ...)  e_named_trace_start(lvl, NULL, fmt, ##__VA_ARGS__)
#define e_trace_cont(lvl, fmt, ...)   e_named_trace_cont(lvl, NULL, fmt, ##__VA_ARGS__)
#define e_trace_end(lvl, fmt, ...)    e_named_trace_end(lvl, NULL, fmt, ##__VA_ARGS__)
#define e_trace(lvl, fmt, ...)        e_named_trace(lvl, NULL, fmt, ##__VA_ARGS__)
#define e_trace_hex(lvl, str, buf, len) \
    e_named_trace_hex(lvl, NULL, str, buf, len)


static ALWAYS_INLINE __must_check__
bool e_expect(bool cond, const char *expr, const char *file, int line,
              const char *func)
{
    if (unlikely(!cond)) {
        __assert_fail(expr, file, line, func);
    }
    return true;
}

#define expect(Cond)  \
    e_expect((Cond), TOSTR(Cond), __FILE__, __LINE__, __func__)

#endif

#endif
