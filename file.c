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

#include "container.h"
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

int file_writev(file_t *f, const struct iovec *iov, size_t iovcnt)
{
    struct iovec *iov2;
    size_t len = f->obuf.len;
    int fd = f->fd, res = 0;
    size_t ilen;

    if (unlikely(!(f->flags & FILE_WRONLY))) {
        errno = EBADF;
        return -1;
    }
    if (unlikely(iovcnt > IOV_MAX)) {
        errno = EINVAL;
        return -1;
    }

    for (size_t i = 0; i < iovcnt; i++)
        len += iov[i].iov_len;

    t_push();
    iov2 = t_new(struct iovec, ilen = iovcnt + 1);
    iov2[0] = MAKE_IOVEC(f->obuf.data, f->obuf.len);
    memcpy(iov2 + 1, iov, iovcnt);

    while (len >= BUFSIZ) {
        ssize_t resv = writev(fd, iov2, ilen);
        int i;

        if (resv < 0 && errno != EINTR && errno != EAGAIN) {
            res = -1;
            break;
        }

        f->wpos += resv;
        len     -= resv;
        if (len == 0)
            goto done;
        for (i = 0; iov2[i].iov_len <= (size_t)resv; i++)
            resv -= iov2[i].iov_len;
        iov2 += i;
        ilen -= i;
        iov2->iov_base  = (char *)iov2->iov_base + resv;
        iov2->iov_len  -= resv;
    }

    if (ilen == iovcnt) {
        sb_skip_upto(&f->obuf, iov2->iov_base);
    } else {
        sb_set(&f->obuf, iov2->iov_base, iov2->iov_len);
    }
    for (size_t i = 1; i < ilen; i++) {
        sb_add(&f->obuf, iov2[i].iov_base, iov2[i].iov_len);
    }
done:
    t_pop();
    return res;
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
