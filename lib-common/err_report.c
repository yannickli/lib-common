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

#include <stdio.h>
#include <stdlib.h>
#include <syslog.h>
#include <string.h>
#include <errno.h>

#include "macros.h"
#include "err_report.h"

/**************************************************************************/
/* private API                                                            */
/**************************************************************************/

struct log_status {
    bool is_open;    /* is the syslog open */
    FILE *fd;        /* file descriptor for the file backend */
    char *ident;     /* prefix for the logs stances */
};

static struct log_status log_state = { false, NULL, NULL };

static void set_log_ident(const char *ident)
{
    if (ident == NULL) {
        return;
    }
    log_state.ident = strdup(ident);
    if (log_state.ident == NULL) {
        e_fatal(FATAL_NOMEM,
                E_PREFIX("not enough memory to allocate new log_state.ident"));
    }
}

static void file_handler(bool eol, const char *format, va_list args)
{
    if (log_state.fd == NULL) {
        log_state.fd = stderr;
    }
    if (log_state.ident != NULL) {
        if (fprintf(log_state.fd, "[%s] ", log_state.ident) < 0) {
            goto error;
        }
    }
    if (vfprintf(log_state.fd, format, args) < 0) {
        goto error;
    }

    if (eol) {
        if (fputc('\n', log_state.fd) == EOF)
            goto error;
    }
    return;

error:
    if (log_state.fd == stderr) {
        exit(FATAL_LOGWRITE);
    }
    log_state.fd = stderr;
    e_fatal(FATAL_LOGWRITE,
            E_PREFIX("cannot write log : %s"), strerror(errno));
}

#define H_ARGS             const char *format, va_list args
#define FILE_HANDLER(eol)  file_handler(eol, format, args)
static void file_fatal_handler     (H_ARGS) { FILE_HANDLER(true); }
static void file_error_handler     (H_ARGS) { FILE_HANDLER(true); }
static void file_warning_handler   (H_ARGS) { FILE_HANDLER(true); }
static void file_notice_handler    (H_ARGS) { FILE_HANDLER(true); }
static void file_info_handler      (H_ARGS) { FILE_HANDLER(true); }

#define SYSLOG(priority)  vsyslog(priority, format, args)
static void syslog_fatal_handler   (H_ARGS) { SYSLOG(LOG_CRIT); }
static void syslog_error_handler   (H_ARGS) { SYSLOG(LOG_ERR); }
static void syslog_warning_handler (H_ARGS) { SYSLOG(LOG_WARNING); }
static void syslog_notice_handler  (H_ARGS) { SYSLOG(LOG_NOTICE); }
static void syslog_info_handler    (H_ARGS) { SYSLOG(LOG_INFO); }

static e_callback_f *fatal_handler   = file_fatal_handler;
static e_callback_f *error_handler   = file_error_handler;
static e_callback_f *warning_handler = file_warning_handler;
static e_callback_f *notice_handler  = file_notice_handler;
static e_callback_f *info_handler    = file_info_handler;

static void init_file(const char *ident, FILE *file)
{
    e_shutdown();
    log_state.fd = (file == NULL) ? stderr : file;
    set_log_ident(ident);
    (void)e_set_fatal_handler(&file_fatal_handler);
    (void)e_set_error_handler(&file_error_handler);
    (void)e_set_warning_handler(&file_warning_handler);
    (void)e_set_notice_handler(&file_notice_handler);
    (void)e_set_info_handler(&file_info_handler);
}

/**************************************************************************/
/* useful macros                                                          */
/**************************************************************************/

#define E_BODY(kind)  do {              \
    va_list args;                       \
    va_start(args, format);             \
    kind##_handler(format, args);       \
    va_end(args);                       \
} while (0)

#define E_SET_BODY(kind)  do {                              \
    e_callback_f *old = kind##_handler;                     \
    kind##_handler = hook ? hook : file_##kind##_handler;   \
    return old;                                             \
} while (0)

/**************************************************************************/
/* public API                                                             */
/**************************************************************************/

/* Error reporting functions */

void e_fatal(int status, const char *format, ...)
{
    E_BODY(fatal);
    exit(status);
}

void e_panic(const char *format, ...)
{
    E_BODY(fatal);
    exit(FATAL_DEFAULT);
}

void e_error   (const char *format, ...) { E_BODY(error);   }
void e_warning (const char *format, ...) { E_BODY(warning); }
void e_notice  (const char *format, ...) { E_BODY(notice);  }
void e_info    (const char *format, ...) { E_BODY(info);    }

/* callback installers */

e_callback_f *e_set_fatal_handler  (e_callback_f *hook) { E_SET_BODY(fatal);  }
e_callback_f *e_set_error_handler  (e_callback_f *hook) { E_SET_BODY(error);  }
e_callback_f *e_set_warning_handler(e_callback_f *hook) { E_SET_BODY(warning);}
e_callback_f *e_set_notice_handler (e_callback_f *hook) { E_SET_BODY(notice); }
e_callback_f *e_set_info_handler   (e_callback_f *hook) { E_SET_BODY(info);   }

/* useful callbacks */

void e_init_stderr(void)
{
    e_init_file(NULL, NULL);
}

void e_init_file(const char *ident, const char *filename)
{
    FILE *fd;

    if (filename == NULL || strcmp(filename, "/dev/stderr") == 0) {
        init_file(NULL, stderr);
    } else {
        fd = fopen(filename, "a");
        if (fd == NULL) {
            e_fatal(FATAL_LOGOPEN,
                    E_PREFIX("can't open log file %s: %s"),
                    filename, strerror(errno));
        }
        init_file(ident, fd);
    }
}

void e_init_syslog(const char *ident, int options, int facility)
{
    e_shutdown();

    openlog(ident, options, facility);
    log_state.is_open = true;
    set_log_ident(ident);

    (void)e_set_fatal_handler(&syslog_fatal_handler);
    (void)e_set_error_handler(&syslog_error_handler);
    (void)e_set_warning_handler(&syslog_warning_handler);
    (void)e_set_notice_handler(&syslog_notice_handler);
    (void)e_set_info_handler(&syslog_info_handler);
}

void e_shutdown()
{
    if (log_state.is_open) {
        closelog();
        log_state.is_open = false;
    }

    if (log_state.fd != stderr) {
        p_fclose(&log_state.fd);
        log_state.fd = stderr;
    }

    if (log_state.ident) {
        free(log_state.ident);
        log_state.ident = NULL;
    }
}
