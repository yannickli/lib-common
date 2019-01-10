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
#include "core.iop.h"

/* This module provides auto rotating log files: log files are rotated
 * automatically depending on file size or data, or both.
 */

#define LOG_FILE_DATE_FMT  "%04d%02d%02d_%02d%02d%02d"
#define LOG_FILE_DATE_FMT_ARG(tm)  \
    (tm).tm_year + 1900, (tm).tm_mon + 1, (tm).tm_mday, (tm).tm_hour,        \
    (tm).tm_min, (tm).tm_sec


enum log_file_flags {
    LOG_FILE_USE_LAST  = (1U << 0),
    LOG_FILE_COMPRESS  = (1U << 1), /* Use gzip on results */
    LOG_FILE_UTCSTAMP  = (1U << 2),
    LOG_FILE_NOSYMLINK = (1U << 3),
};

enum log_file_event {
    LOG_FILE_CREATE, /* just after a new file creation */
    LOG_FILE_CLOSE,  /* called before file_close is called */
    LOG_FILE_DELETE, /* called after a log file is deleted. */
#if 0
    LOG_FILE_ROTATE, /* TODO: just before rotation, to allow writing a
                        footer or similar */
#endif
};

struct log_file_t;

/* Log file event callback. fpath is built using the prefix passed during
 * the log_file initialization. So, fpath will be absolute if the log_file
 * was constructed with an absolute prefix, or else, fpath is relative
 * to the current working directory at log_file_new() time. */
typedef void (log_file_cb_f)(struct log_file_t *file,
                             enum log_file_event event,
                             const char *fpath, void *priv);

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

    /* Event callback */
    log_file_cb_f *on_event;
    void          *priv_cb;
} log_file_t;

log_file_t *log_file_init(log_file_t *, const char *nametpl, int flags);
log_file_t *log_file_new(const char *nametpl, int flags);
__must_check__ int log_file_open(log_file_t *log_file);
__must_check__ int log_file_close(log_file_t **log_file);
__must_check__ int log_file_rotate(log_file_t *log_file);

/** Creates a log file from a configuration object, and opens it. */
log_file_t *
log_file_create_from_iop(const char *nametpl,
                         const core__log_file_configuration__t *conf,
                         int flags, log_file_cb_f *on_event, void *priv);

void log_file_set_maxsize(log_file_t *file, int max);
void log_file_set_rotate_delay(log_file_t *file, time_t delay);
void log_file_set_maxfiles(log_file_t *file, int maxfiles);
void log_file_set_maxtotalsize(log_file_t *file, int maxtotalsize);
void
log_file_set_file_cb(log_file_t *file, log_file_cb_f *on_event,
                     void *priv);

int log_fwrite(log_file_t *log_file, const void *data, size_t len);
int log_fwritev(log_file_t *log_file, struct iovec *iov, size_t iovlen);
int log_fprintf(log_file_t *log_file, const char *format, ...)
    __attr_printf__(2, 3)  __attr_nonnull__((1, 2));

int log_file_flush(log_file_t *log_file);

/** Get the creation timestamp of a log file from his full path.
 *
 * \param[in]  file  The log_file_t.
 * \param[in]  path  The full path of the log file we want the date.
 * \param[out] out   Pointer to the creation date timestamp to fill.
 *
 * \return 0 on success, a negative value on failure.
 */
int log_file_get_file_stamp(const log_file_t *file, const char *path,
                            time_t *out);

static inline off_t log_file_tell(log_file_t *log_file)
{
    if (log_file->_internal)
        return file_tell(log_file->_internal);
    errno = EBADF;
    return -1;
}
static inline int log_file_truncate(log_file_t *log_file, off_t len)
{
    if (log_file->_internal)
        return file_truncate(log_file->_internal, len);
    errno = EBADF;
    return -1;
}
static inline bool log_file_isopen(log_file_t *log_file)
{
    return (log_file->_internal != NULL);
}

#endif /* IS_LIB_COMMON_LOG_FILE_H */
