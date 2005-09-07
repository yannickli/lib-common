#ifndef IS_ERR_REPORT_H
#define IS_ERR_REPORT_H

#include <stdarg.h>

#include "macros.h"

/* Useful exit codes */
enum fatal_exit_codes {
    FATAL_NOMEM    = 0x80, /* out of memory */
    FATAL_LOGOPEN  = 0x81, /* can't open log file */
    FATAL_LOGWRITE = 0x82, /* can't write log */
    FATAL_DEFAULT  = 0xff  /* Default exit code */
};

/* error reporting functions */
typedef void fatal_t (int, const char *, ...) __attr_format__(2, 3);
typedef void error_t (const char *, ...) __attr_format__(1, 2);

fatal_t e_fatal;
error_t e_panic;
error_t e_error;
error_t e_warning;
error_t e_notice;
error_t e_info;
error_t e_debug;

#define E_PREFIX(fmt) \
    ("file %s: line %d (%s): " fmt), __FILE__, __LINE__, __func__

#define e_assert(expr)                                          \
    do {                                                        \
        if(!(expr)) {                                           \
            e_panic(E_PREFIX("assertion failed: %s"), #expr);   \
        }                                                       \
    } while(0)

/* callbacks installers */
typedef void e_callback_t (const char *, va_list);

e_callback_t * e_set_fatal_handler   (e_callback_t *);
e_callback_t * e_set_error_handler   (e_callback_t *);
e_callback_t * e_set_warning_handler (e_callback_t *);
e_callback_t * e_set_notice_handler  (e_callback_t *);
e_callback_t * e_set_info_handler    (e_callback_t *); 
e_callback_t * e_set_debug_handler   (e_callback_t *);

/* useful callbacks */
void e_init_stderr(void);
void e_init_file(const char * ident, const char * filename);
void e_init_syslog(const char * ident, int options, int facility);

void e_deinit(void);
#endif
