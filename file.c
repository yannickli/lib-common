/**************************************************************************/
/*                                                                        */
/*  Copyright (C) 2004-2009 INTERSEC SAS                                  */
/*                                                                        */
/*  Should you receive a copy of this source code, you must check you     */
/*  have a proper, written authorization of INTERSEC to hold it. If you   */
/*  don't have such an authorization, you must DELETE all source code     */
/*  files in your possession, and inform INTERSEC of the fact you obtain  */
/*  these files. Should you not comply to these terms, you can be         */
/*  prosecuted in the extent permitted by applicable law.                 */
/*                                                                        */
/**************************************************************************/

#include "file.h"

/****************************************************************************/
/* helpers                                                                  */
/****************************************************************************/

#define FAIL_ERRNO(expr, ret)  \
    do { int __save_errno = errno;                                        \
        (expr); errno = __save_errno;                                     \
        return (ret);                                                     \
    } while (0)

static int file_flush_obuf(file_t *f, int len)
{
    sb_t *obuf = &f->obuf;
    int goal = f->obuf.len - len;
    int fd = f->fd;

    assert (f->flags & FILE_WRONLY);
    assert (len <= f->obuf.len);

    while (obuf->len > goal) {
        int nb = write(fd, obuf->data, obuf->len);
        if (nb < 0 && errno != EINTR && errno != EAGAIN)
            return -1;
        sb_skip(obuf, nb);
        f->wpos += nb;
    }
    return 0;
}

/****************************************************************************/
/* open/close                                                               */
/****************************************************************************/

file_t *file_open(const char *path, enum file_flags flags, mode_t mode)
{
    file_t *res;
    int oflags;

    switch (flags & FILE_OPEN_MODE_MASK) {
      case FILE_RDONLY:
        oflags = O_RDONLY;
#if 0
        break;
#else
        errno = ENOSYS;
        return NULL;
#endif
      case FILE_WRONLY:
        oflags = O_WRONLY;
        break;
      case FILE_RDWR:
        oflags = O_RDWR;
#if 0
        break;
#else
        errno = ENOSYS;
        return NULL;
#endif
      default:
        errno = EINVAL;
        return NULL;
    }

    if (flags & FILE_CREATE)
        oflags |= O_CREAT;
    if (flags & FILE_EXCL)
        oflags |= O_EXCL;
    if (flags & FILE_TRUNC)
        oflags |= O_TRUNC;

    res = p_new_raw(file_t, 1);
    res->flags = flags;
    sb_init(&res->obuf);
    res->fd = open(path, oflags, mode);
    if (res->fd < 0)
        FAIL_ERRNO(p_delete(&res), NULL);

    return res;
}

int file_flush(file_t *f)
{
    if (!(f->flags & FILE_WRONLY)) {
        errno = EBADF;
        return -1;
    }
    return file_flush_obuf(f, f->obuf.len);
}

int file_close(file_t **fp)
{
    if (*fp) {
        file_t *f = *fp;
        int res = 0;
        res = file_flush(f) | p_close(&f->fd);
        sb_wipe(&f->obuf);
        p_delete(fp);
        return res;
    }
    return 0;
}

/****************************************************************************/
/* seeking                                                                  */
/****************************************************************************/

off_t file_seek(file_t *f, off_t offset, int whence)
{
    off_t res;

    if (f->flags & FILE_WRONLY) {
        if (file_flush(f))
            return -1;
    }
    res = lseek(f->fd, offset, whence);
    if (res != (off_t)-1) {
        f->wpos = res;
    }
    return res;
}

off_t file_tell(file_t *f)
{
    return f->wpos + f->obuf.len;
}

/****************************************************************************/
/* writing                                                                  */
/****************************************************************************/

int file_putc(file_t *f, int c)
{
    if (!(f->flags & FILE_WRONLY)) {
        errno = EBADF;
        return -1;
    }
    sb_addc(&f->obuf, c);
    if (c == '\n' || f->obuf.len > BUFSIZ) {
        if (file_flush(f))
            return -1;
    }
    return 0;
}

off_t file_write(file_t *f, const void *_data, off_t len)
{
    const byte *data = _data;
    off_t pos;
    int fd = f->fd;

    if (!(f->flags & FILE_WRONLY)) {
        errno = EBADF;
        return -1;
    }
    if (len < 0) {
        errno = EINVAL;
        return -1;
    }

    if (f->obuf.len + len < BUFSIZ) {
        sb_add(&f->obuf, data, len);
        return len;
    }

    pos = BUFSIZ - f->obuf.len;
    sb_add(&f->obuf, data, BUFSIZ - f->obuf.len);
    if (file_flush(f))
        return pos;

    while (len - pos > BUFSIZ) {
        int nb = write(fd, data + pos, len - pos);
        if (nb < 0 && errno != EINTR && errno != EAGAIN)
            return pos;
        pos += nb;
        f->wpos += nb;
    }

    sb_set(&f->obuf, data + pos, len - pos);
    return len;
}

int file_writevf(file_t *f, const char *fmt, va_list ap)
{
    sb_addvf(&f->obuf, fmt, ap);
    if (f->obuf.len > BUFSIZ) {
        if (file_flush(f))
            return -1;
    }
    return 0;
}
