#ifndef IS_ERR_REPORT_H
#define IS_ERR_REPORT_H

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
#ifdef NDEBUG
#  define e_debug(level, fmt, ...)
#else
fatal_f e_debug;
#endif


#define E_PREFIX(fmt) \
    ("%s:%d:%s: " fmt), __FILE__, __LINE__, __func__

#define E_UNIXERR(funcname) \
    E_PREFIX("error: %s: %s"), (funcname), strerror(errno)

#define e_assert(expr)                                          \
    do {                                                        \
        if (!(expr)) {                                          \
            e_panic(E_PREFIX("assertion failed: %s"), #expr);   \
        }                                                       \
    } while (0)

/* callbacks installers */
typedef void e_callback_f(const char *, va_list);

e_callback_f *e_set_fatal_handler   (e_callback_f *) __attribute__((nonnull));
e_callback_f *e_set_error_handler   (e_callback_f *) __attribute__((nonnull));
e_callback_f *e_set_warning_handler (e_callback_f *) __attribute__((nonnull));
e_callback_f *e_set_notice_handler  (e_callback_f *) __attribute__((nonnull));
e_callback_f *e_set_info_handler    (e_callback_f *) __attribute__((nonnull)); 
e_callback_f *e_set_debug_handler   (e_callback_f *) __attribute__((nonnull));

/* useful callbacks */
void e_init_stderr(void);
void e_init_file(const char *ident, const char *filename);
void e_init_syslog(const char *ident, int options, int facility);

#ifdef ERR_REPORT_DEFINE_VERBOSITY
int e_verbosity_level = 0;
#else
extern int e_verbosity_level;
#endif
void e_set_verbosity(int max_debug_level);
void e_incr_verbosity(void);
static inline bool e_verbosity(int level) {
    return (level <= e_verbosity_level);
}

void e_shutdown(void);
#endif
