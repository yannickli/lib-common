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

#ifndef IS_LIB_COMMON_LOG_FILE_H
#define IS_LIB_COMMON_LOG_FILE_H

#include "file.h"

/* This module provides auto rotating log files: log files are rotated
 * automatically depending on file size or data, or both.
 */

enum log_file_flags {
    LOG_FILE_USE_LAST = (1U << 0),
    LOG_FILE_COMPRESS = (1U << 1), /* Use gzip on results */
    LOG_FILE_UTCSTAMP = (1U << 2),
};

enum log_file_event {
    LOG_FILE_CREATE, /* just after a new file creation */
    LOG_FILE_CLOSE,  /* called before file_close is called */
#if 0
    LOG_FILE_ROTATE, /* TODO: just before rotation, to allow writing a
                        footer or similar */
#endif
};

typedef struct log_file_t {
    uint32_t flags;
    int      max_size;
    int      max_files;
    int      max_total_size; /* in Mo */
    time_t   open_date;
    time_t   rotate_delay;
    file_t  *_internal;
    char     prefix[PATH_MAX];
    char     ext[8];
    void     (*log_file_cb_f)(struct log_file_t *file, int event, void *priv);
    void     *priv_cb;
} log_file_t;

log_file_t *log_file_init(log_file_t *, const char *nametpl, int flags);
log_file_t *log_file_new(const char *nametpl, int flags);
__must_check__ int log_file_open(log_file_t *log_file);
__must_check__ int log_file_close(log_file_t **log_file);
__must_check__ int log_file_rotate(log_file_t *log_file);

void log_file_set_maxsize(log_file_t *file, int max);
void log_file_set_rotate_delay(log_file_t *file, time_t delay);
void log_file_set_maxfiles(log_file_t *file, int maxfiles);
void log_file_set_maxtotalsize(log_file_t *file, int maxtotalsize);
void
log_file_set_file_cb(log_file_t *file,
                     void (*file_cb)(log_file_t*, int event, void* priv),
                     void *priv);

int log_fwrite(log_file_t *log_file, const void *data, size_t len);
int log_fprintf(log_file_t *log_file, const char *format, ...)
    __attr_printf__(2, 3)  __attr_nonnull__((1, 2));

int log_file_flush(log_file_t *log_file);

#endif /* IS_LIB_COMMON_LOG_FILE_H */
