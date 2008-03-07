/**************************************************************************/
/*                                                                        */
/*  Copyright (C) 2004-2008 INTERSEC SAS                                  */
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

#include <time.h>
#include <limits.h>

#include "mem.h"

/**
 *  TODO: Should support a symlink to the last opened log file
 */

/* This module provides auto rotating log files: log files are rotated
 * automatically depending on file size or data, or both.
 *
 */

typedef struct log_file_t {
    char prefix[PATH_MAX];
    int max_size;
    int max_files;
    time_t open_date;
    time_t rotate_date;
    time_t rotate_delay;
    FILE *_internal;
} log_file_t;

log_file_t *log_file_open(const char *prefix);
void log_file_close(log_file_t **log_file);

void log_file_set_maxsize(log_file_t *file, int max);
void log_file_set_rotate_delay(log_file_t *file, time_t delay);
void log_file_set_maxfiles(log_file_t *file, int maxfiles);

size_t log_fwrite(const void *buf, size_t size, size_t nmemb,
                  log_file_t *log_file);

int log_fprintf(log_file_t *log_file, const char *format, ...)
    __attr_printf__(2, 3)  __attr_nonnull__((1, 2));

int log_file_flush(log_file_t *log_file);

#endif /* IS_LIB_COMMON_LOG_FILE_H */
