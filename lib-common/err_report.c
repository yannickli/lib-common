/**************************************************************************/
/*                                                                        */
/*  Copyright (C) 2004-2007 INTERSEC SAS                                  */
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

#ifndef MINGCC
#include <syslog.h>
#else
#define SIGHUP            1
#define LOG_EMERG       0       /* system is unusable */
#define LOG_ALERT       1       /* action must be taken immediately */
#define LOG_CRIT        2       /* critical conditions */
#define LOG_ERR         3       /* error conditions */
#define LOG_WARNING     4       /* warning conditions */
#define LOG_NOTICE      5       /* normal but significant condition */
#define LOG_INFO        6       /* informational */
#define LOG_DEBUG       7       /* debug-level messages */
#endif

#include <string.h>
#include <errno.h>
#include <signal.h>

#include "macros.h"
#include "mem.h"
#include "err_report.h"

/**************************************************************************/
/* private API                                                            */
/**************************************************************************/

static void file_handler(int priority, const char *format, va_list args)
    __attr_printf__(2,0);

static struct {
    flag_t is_open : 1; /* is the syslog open */
    flag_t sig_hooked : 1;

    FILE *f;
    char *ident;
    char *filename;
    e_handler *handler;
    void (*oldsig)(int);
} log_state = {
    false,
    false,
    NULL,
    NULL,
    NULL,
    &file_handler,
    NULL,
};

static void set_log_ident(const char *ident)
{
    p_delete(&log_state.ident);
    if (ident) {
        log_state.ident = p_strdup(ident);
    }
}

static void log_sighup(int __unused__ signum)
{
    if (log_state.filename && log_state.f) {
        fflush(log_state.f);
        p_fclose(&log_state.f);
        log_state.f = fopen(log_state.filename, "a");
        if (!log_state.f) {
            e_fatal(FATAL_LOGOPEN,
                    E_PREFIX("can't open log file %s: %s"),
                    log_state.filename, strerror(errno));
        }
    }
}

static void siginstall(void)
{
    if (!log_state.sig_hooked) {
        log_state.oldsig = signal(SIGHUP, &log_sighup);
        log_state.sig_hooked = true;
    }
}

static void file_handler(int __unused__ priority,
                         const char *format, va_list args)
{
    FILE *fp = log_state.f ? log_state.f : stderr;

    if (log_state.ident != NULL) {
        if (fprintf(fp, "[%s] ", log_state.ident) < 0) {
            goto error;
        }
    }
    if (vfprintf(fp, format, args) < 0) {
        goto error;
    }

    if (fputc('\n', fp) == EOF)
        goto error;
    return;

error:
    exit(FATAL_LOGWRITE);
}

static void init_file(const char *ident, FILE *file)
{
    e_set_handler(&file_handler);

    siginstall();
    log_state.f = file ? file : stderr;
    set_log_ident(ident);
}

/**************************************************************************/
/* public API                                                             */
/**************************************************************************/

#define E_BODY(prio, format)  do {                                                \
    va_list args;                                                         \
    va_start(args, format);                                               \
    (*log_state.handler)(prio, format, args);                             \
    va_end(args);                                                         \
} while (0)

/* Error reporting functions */

void e_fatal(int status, const char *format, ...)
{
    E_BODY(LOG_CRIT, format);
    exit(status);
}

void e_panic(const char *format, ...)
{
    E_BODY(LOG_CRIT, format);
    exit(FATAL_DEFAULT);
}

void e_error   (const char *format, ...) { E_BODY(LOG_ERR,     format); }
void e_warning (const char *format, ...) { E_BODY(LOG_WARNING, format); }
void e_notice  (const char *format, ...) { E_BODY(LOG_NOTICE,  format); }
void e_info    (const char *format, ...) { E_BODY(LOG_INFO,    format); }

/* useful callbacks */

void e_init_stderr(void)
{
    e_init_file(NULL, NULL);
}

void e_init_file(const char *ident, const char *filename)
{
    FILE *fp;

    p_delete(&log_state.filename);

    if (filename == NULL || !strcmp(filename, "/dev/stderr")) {
        init_file(NULL, stderr);
    } else {
        fp = fopen(filename, "a");
        if (fp == NULL) {
            e_fatal(FATAL_LOGOPEN,
                    E_PREFIX("can't open log file %s: %s"),
                    filename, strerror(errno));
        }
        log_state.filename = p_strdup(filename);
        init_file(ident, fp);
    }
}

#ifndef MINGCC
void e_init_syslog(const char *ident, int options, int facility)
{
    e_set_handler(&vsyslog);

    openlog(ident, options, facility);
    log_state.is_open = true;
    set_log_ident(ident);
}
#endif

void e_set_handler(e_handler *handler)
{
    e_shutdown();
    log_state.handler = handler;
}

void e_shutdown(void)
{
    if (log_state.is_open) {
#ifndef MINGCC
        closelog();
#endif
        log_state.is_open = false;
    }

    if (log_state.f != stderr) {
        p_fclose(&log_state.f);
    }

    p_delete(&log_state.filename);
    p_delete(&log_state.ident);
}
