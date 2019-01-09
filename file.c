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

#include "container.h"
#include "file.h"
#include "unix.h"

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

        if (nb < 0) {
            if (errno != EINTR && errno != EAGAIN)
                return -1;
            continue;
        }
        sb_skip(obuf, nb);
        f->wpos += nb;
    }
    return 0;
}

/****************************************************************************/
/* open/close                                                               */
/****************************************************************************/

file_t *file_open_at(int dfd, const char *path,
                     enum file_flags flags, mode_t mode)
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

    res = p_new(file_t, 1);
    res->flags = flags;
    sb_init(&res->obuf);
    res->fd = openat(dfd, path, oflags, mode);
    if (res->fd < 0)
        FAIL_ERRNO(p_delete(&res), NULL);

    return res;
}

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

    res = p_new(file_t, 1);
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

    if (f->flags & FILE_WRONLY)
        RETHROW(file_flush(f));
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
        RETHROW(file_flush(f));
    }
    return 0;
}

int file_writev(file_t *f, const struct iovec *iov, size_t iovcnt)
{
    struct iovec *iov2;
    size_t iov2cnt, len;
    int res = 0;

    if (unlikely(!(f->flags & FILE_WRONLY))) {
        errno = EBADF;
        return -1;
    }
    if (unlikely(iovcnt > IOV_MAX)) {
        errno = EINVAL;
        return -1;
    }

    len = f->obuf.len;
    for (size_t i = 0; i < iovcnt; i++)
        len += iov[i].iov_len;

    t_push();
    if (f->obuf.len == 0) {
        iov2    = t_dup(iov, iovcnt);
        iov2cnt = iovcnt;
    } else {
        iov2    = t_new(struct iovec, iovcnt + 1);
        iov2[0] = MAKE_IOVEC(f->obuf.data, f->obuf.len);
        memcpy(iov2 + 1, iov, sizeof(*iov) * iovcnt);
        iov2cnt = iovcnt + 1;
    }

    while (len >= BUFSIZ) {
        ssize_t resv = writev(f->fd, iov2, iov2cnt);

        if (resv < 0) {
            if (errno != EINTR && errno != EAGAIN) {
                res = -1;
                break;
            }
        } else {
            int cnt;

            f->wpos += resv;
            len     -= resv;
            if (len == 0) {
                iov2cnt = 0;
                break;
            }
            for (cnt = 0; iov2[cnt].iov_len <= (size_t)resv; cnt++)
                resv -= iov2[cnt].iov_len;
            iov2    += cnt;
            iov2cnt -= cnt;
            iov2->iov_base  = (char *)iov2->iov_base + resv;
            iov2->iov_len  -= resv;
        }
    }

    if (iov2cnt == 0) {
        sb_reset(&f->obuf);
    } else {
        if (iov2cnt == iovcnt + 1) {
            sb_skip_upto(&f->obuf, iov2->iov_base);
        } else {
            sb_set(&f->obuf, iov2->iov_base, iov2->iov_len);
        }
        for (size_t i = 1; i < iov2cnt; i++) {
            sb_add(&f->obuf, iov2[i].iov_base, iov2[i].iov_len);
        }
    }
    t_pop();
    return res;
}

int file_writevf(file_t *f, const char *fmt, va_list ap)
{
    sb_addvf(&f->obuf, fmt, ap);
    if (f->obuf.len > BUFSIZ)
        RETHROW(file_flush(f));
    return 0;
}
