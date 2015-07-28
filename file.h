/**************************************************************************/
/*                                                                        */
/*  Copyright (C) 2004-2015 INTERSEC SA                                   */
/*                                                                        */
/*  Should you receive a copy of this source code, you must check you     */
/*  have a proper, written authorization of INTERSEC to hold it. If you   */
/*  don't have such an authorization, you must DELETE all source code     */
/*  files in your possession, and inform INTERSEC of the fact you obtain  */
/*  these files. Should you not comply to these terms, you can be         */
/*  prosecuted in the extent permitted by applicable law.                 */
/*                                                                        */
/**************************************************************************/

#ifndef IS_LIB_COMMON_FILE_H
#define IS_LIB_COMMON_FILE_H

#include "core.h"

/* simplified stdio */

enum file_flags {
    /*--- opening mode ---*/
    FILE_RDONLY = 0x01,
    FILE_WRONLY = 0x02,
    FILE_RDWR   = 0x03,
    FILE_OPEN_MODE_MASK = 0x03,

    /*--- opening option ---*/
    FILE_CREATE = 0x04,            /* O_CREAT */
    FILE_EXCL   = 0x08,            /* O_EXCL  */
    FILE_TRUNC  = 0x10,            /* O_TRUNC */
};

typedef struct file_t {
    unsigned flags; /* Combination of enum file_flags. */
    int fd;
    off_t wpos;
    sb_t obuf;
} file_t;

/*----- open/close -----*/
__must_check__ file_t *file_open_at(int dfd, const char *path,
                                    unsigned flags, mode_t mode);
__must_check__ file_t *file_open(const char *path,
                                 unsigned flags, mode_t mode);
__must_check__ int file_flush(file_t *);
__must_check__ int file_close(file_t **);

/*----- seek/tell -----*/
__must_check__ off_t file_seek(file_t *f, off_t offset, int whence);
__must_check__ off_t file_tell(file_t *f);
static inline __must_check__ off_t file_rewind(file_t *f) {
    return file_seek(f, 0, SEEK_SET);
}

/*----- writing -----*/
int file_putc(file_t *f, int c);
ssize_t file_writev(file_t *f, const struct iovec *iov, size_t iovcnt);
ssize_t file_write(file_t *f, const void *data, size_t len);
static inline ssize_t file_puts(file_t *f, const char *s) {
    return file_write(f, s, strlen(s));
}

__attr_printf__(2, 0)
ssize_t file_writevf(file_t *f, const char *fmt, va_list ap);

__attr_printf__(2, 3)
static inline ssize_t file_writef(file_t *f, const char *fmt, ...) {
    int res;
    va_list ap;
    va_start(ap, fmt);
    res = file_writevf(f, fmt, ap);
    va_end(ap);
    return res;
}
int file_truncate(file_t *f, off_t len);

#endif /* IS_LIB_COMMON_FILE_H */
