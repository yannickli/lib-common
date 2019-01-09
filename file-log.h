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

#ifndef IS_LIB_COMMON_LOG_FILE_H
#define IS_LIB_COMMON_LOG_FILE_H

#include "file.h"

/* This module provides auto rotating log files: log files are rotated
 * automatically depending on file size or data, or both.
 */

enum log_file_flags {
    LOG_FILE_USE_LAST = (1 << 0),
};

typedef struct log_file_t {
    flag_t use_last : 1;
    char prefix[PATH_MAX];
    char ext[32];
    int max_size;
    int max_files;
    time_t open_date;
    time_t rotate_date;
    time_t rotate_delay;
    file_t *_internal;
} log_file_t;

__must_check__ log_file_t *log_file_open(const char *nametpl, int flags);
__must_check__ int log_file_close(log_file_t **log_file);
__must_check__ int log_file_rotate(log_file_t *log_file);

int  log_file_force_rotate(log_file_t *file);
void log_file_set_maxsize(log_file_t *file, int max);
void log_file_set_rotate_delay(log_file_t *file, time_t delay);
void log_file_set_maxfiles(log_file_t *file, int maxfiles);

int log_fwrite(log_file_t *log_file, const void *data, size_t len);
int log_fprintf(log_file_t *log_file, const char *format, ...)
    __attr_printf__(2, 3)  __attr_nonnull__((1, 2));

int log_file_flush(log_file_t *log_file);

#endif /* IS_LIB_COMMON_LOG_FILE_H */
