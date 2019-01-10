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

#include <sys/ioctl.h>
#include <sys/resource.h>
#include <termios.h>
#include <sys/wait.h>
#include "unix.h"
#include "time.h"
#include "z.h"

/** Create a directory path as mkdir -p
 *
 * @param dir   directory to create
 * @param mode  initial mode of directory, and parent directories if required
 *
 * @return 0 if directory exists,
 * @return 1 if directory was created
 * @return -1 in case an error occurred.
 */
int mkdir_p(const char *dir, mode_t mode)
{
    char path[PATH_MAX + 1], *p;
    int atoms = 0, res;

    pstrcpy(path, sizeof(path) - 1, dir);
    path_simplify(path);
    p = path + strlen(path);

    for (;;) {
        struct stat st;

        p = memrchr(path, '/', p - path) ?: path;
        if (stat(path, &st) == 0) {
            if (S_ISDIR(st.st_mode)) {
                if (atoms == 0)
                    return 0;
                break;
            }
            errno = ENOTDIR;
            return -1;
        }
        if (errno != ENOENT)
            return -1;
        atoms++;
        if (p == path)
            goto make_everything;
        *p = '\0';
    }

    assert (atoms);
    for (;;) {
        p += strlen(p);
        *p = '/';
      make_everything:
        if (mkdir(path, mode) < 0) {
            if (errno != EEXIST)
                return -1;
            res = 0;
        } else {
            res = 1;
        }
        if (--atoms == 0)
            return res;
    }
}

/** Retrieve time of last modification
 *
 * @param filename  relative or absolute path
 * @param t         time is returned in this pointer
 *
 * @return 0 if successful
 * @return -1 on error (errno is positioned according to stat)
 */
int get_mtime(const char *filename, time_t *t)
{
    struct stat st;
    if (!t || stat(filename, &st)) {
        return 1;
    }
    *t = st.st_mtime;
    return 0;
}

static off_t fcopy(int fdin, const struct stat *stin, int fdout)
{
#define BUF_SIZE  (8 << 20) /* 8MB */
    t_scope;
    int nread;
    byte *buf = t_new_raw(byte, BUF_SIZE);
    off_t total = 0;

    for (;;) {
        nread = read(fdin, buf, BUF_SIZE);
        if (nread == 0)
            break;
        if (nread < 0) {
            if (ERR_RW_RETRIABLE(errno))
                continue;
            goto error;
        }
        if (xwrite(fdout, buf, nread) < 0) {
            goto error;
        }
        total += nread;
    }
#undef BUF_SIZE

    if (unlikely(total != stin->st_size)) {
        assert (false);
        errno = EIO;
        goto error;
    }

#if defined(__linux__)
    /* copying file times */
    /* OG: should copy full precision times if possible.
       Since kernel 2.5.48, the stat structure supports nanosecond  resolution
       for the three file timestamp fields.  Glibc exposes the nanosecond com-
       ponent of each field using names either of the form st_atim.tv_nsec, if
       the  _BSD_SOURCE  or  _SVID_SOURCE feature test macro is defined, or of
       the form st_atimensec, if neither of these macros is defined.  On  file
       systems  that  do  not  support sub-second timestamps, these nanosecond
       fields are returned with the value 0.
     */
    {
        struct timeval tvp[2];

        tvp[0] = (struct timeval) { .tv_sec = stin->st_atime,
                                    .tv_usec = stin->st_atimensec / 1000 };
        tvp[1] = (struct timeval) { .tv_sec = stin->st_mtime,
                                    .tv_usec = stin->st_mtimensec / 1000 };
        futimes(fdout, tvp);
    }
#endif

    return total;

  error:
    return -1;
}

/** Copy file pathin to pathout. If pathout already exists, it will
 * be overwritten.
 *
 * Note: Use the same mode bits as the input file.
 * @param  pathin  file to copy
 * @param  pathout destination (created if not exist, else overwritten)
 *
 * @return -1 on error
 * @return n  number of bytes copied
 */
off_t filecopy(const char *pathin, const char *pathout)
{
/* OG: since this function returns the number of bytes copied, the
 * return type should be off_t.
 */
    int fdin = -1, fdout = -1;
    struct stat st;
    off_t total = -1;

    fdin = open(pathin, O_RDONLY | O_BINARY);
    if (fdin < 0) {
        goto end;
    }
    if (fstat(fdin, &st)) {
        goto end;
    }

    /* OG: this will not work if the source file is not writeable ;-) */
    /* OG: should test if source and destination files are the same
     * file before truncating destination file ;-) */
    fdout = open(pathout, O_WRONLY | O_BINARY | O_CREAT | O_TRUNC, st.st_mode);
    if (fdout < 0) {
        goto end;
    }

    total = fcopy(fdin, &st, fdout);

  end:
    PROTECT_ERRNO(p_close(&fdin));
    PROTECT_ERRNO(p_close(&fdout));

    return total;
}

/** Copy file from a directory descriptor to another. If the file already
 * exists in the destination directory, it will be overwritten.
 *
 * Note: Use the same mode bits as the input file.
 * @param  dfd_src    source directory descriptor.
 * @param  name_src   the file to copy.
 * @param  dfd_dst    destination directory descriptor.
 * @param  name_dst   the new resulting file.
 *
 * @return -1 on error
 * @return n  number of bytes copied
 */
off_t filecopyat(int dfd_src, const char* name_src,
                 int dfd_dst, const char* name_dst)
{
    int fd_src = -1, fd_dst = -1;
    struct stat st;
    off_t total;

    fd_src = openat(dfd_src, name_src, O_RDONLY | O_BINARY);
    if (fd_src < 0)
        goto error;

    if (fstat(fd_src, &st))
        goto error;

    fd_dst = openat(dfd_dst, name_dst,
                    O_WRONLY | O_BINARY | O_CREAT | O_TRUNC, st.st_mode);
    if (fd_dst < 0)
        goto error;

    total = fcopy(fd_src, &st, fd_dst);
    close(fd_src);
    close(fd_dst);
    return total;

  error:
    p_close(&fd_src);
    p_close(&fd_dst);

    /* OG: destination file should be removed upon error ? */
    return -1;
}

/** Lock files and test locks
 *
 * Example: p_lockf(fd, O_RDONLY, 0)   Places a read lock on the whole file
 *
 * Note: - to unlock, use p_unlockf
 *       - See the lockf(3) or fcntl(2)
 *
 * @param  fd    opened file
 * @param  mode  lock mode (O_RDONLY, O_RDWR, O_WRONLY)
 * @param  cmd   One of F_LOCK, F_TLOCK or F_TEST  (see fcntl(2))
 * @param  start Start of the region of the file on which the lock applies
 * @param  len   Length of the region of the file on which the lock applies
 *
 * @return On success, zero is returned. On error, -1 is returned, and errno
 *         is set appropriately.
 */
int p_lockf(int fd, int mode, int cmd, off_t start, off_t len)
{
    struct flock lock = {
        .l_type   = O_ISWRITE(mode) ? F_WRLCK : F_RDLCK,
        .l_whence = SEEK_SET,
        .l_start  = start,
        .l_len    = len,
    };

    switch (cmd) {
      case F_LOCK:
        cmd = F_SETLKW;
        break;

      case F_TLOCK:
        cmd = F_SETLK;
        break;

      case F_TEST:
        cmd = F_GETLK;
        break;

      default:
        errno = EINVAL;
        return -1;
    }

    RETHROW(fcntl(fd, cmd, &lock));
    if (cmd == F_GETLK) {
        if (lock.l_type == F_UNLCK) {
            return 0;
        }
        errno = EAGAIN;
        return -1;
    }

    return 0;
}

/** Unlock files
 *
 * Example: p_unlockf(fd, 0, 0)    Unlocks the whole file
 *
 * @param  fd    opened file
 * @param  start Start of the region of the file to unlock
 * @param  len   Length of the region of the file to unlock
 *
 * @return On success, zero is returned. On error, -1 is returned, and errno
 *         is set appropriately.
 */
int p_unlockf(int fd, off_t start, off_t len)
{
    struct flock lock = {
        .l_type   = F_UNLCK,
        .l_whence = SEEK_SET,
        .l_start  = start,
        .l_len    = len,
    };
    return fcntl(fd, F_SETLK, &lock);
}

int lockdir(int dfd, dir_lock_t *dlock)
{
    *dlock        = DIR_LOCK_INIT_V;
    dlock->lockfd = RETHROW(openat(dfd, ".lock", O_WRONLY | O_CREAT,
                                   S_IRUSR | S_IWUSR | S_IRGRP | S_IROTH));

    if (p_lockf(dlock->lockfd, O_WRONLY, F_TLOCK, 0, 0) < 0) {
        PROTECT_ERRNO(p_close(&dlock->lockfd));
        return -1;
    }

    dlock->dfd = dup(dfd);
    return 0;
}

void unlockdir(dir_lock_t *dlock)
{
    unlinkat(dlock->dfd, ".lock", 0);

    /* XXX: The file gets unlocked after close() */
    p_close(&dlock->lockfd);
    p_close(&dlock->dfd);
}

int tmpfd(void)
{
    char path[PATH_MAX] = "/tmp/XXXXXX";
    mode_t mask = umask(0177);
    int fd = mkstemp(path);
    umask(mask);
    RETHROW(fd);
    unlink(path);
    return fd;
}

/****************************************************************************/
/* file descriptor related                                                  */
/****************************************************************************/

int xwrite(int fd, const void *data, ssize_t len)
{
    const char *s = data;

    while (len > 0) {
        ssize_t nb = write(fd, s, len);

        if (nb < 0) {
            if (ERR_RW_RETRIABLE(errno))
                continue;
            return -1;
        }
        s   += nb;
        len -= nb;
    }
    return 0;
}

int xwrite_file(const char *path, const void *data, ssize_t dlen)
{
    int fd, res;

    fd = RETHROW(open(path, O_WRONLY | O_CREAT | O_TRUNC, 0644));
    res = xwrite(fd, data, dlen);
    if (res < 0) {
        int save_errno = errno;
        unlink(path);
        close(fd);
        errno = save_errno;
        return -1;
    }
    close(fd);
    return dlen;
}

int xwritev(int fd, struct iovec *iov, int iovcnt)
{
    while (iovcnt) {
        ssize_t nb = writev(fd, iov, iovcnt);

        if (nb < 0) {
            if (ERR_RW_RETRIABLE(errno))
                continue;
            return -1;
        }
        while (nb) {
            if ((size_t)nb >= iov->iov_len) {
                nb -= iov->iov_len;
                iovcnt--;
                iov++;
            } else {
                iov->iov_len  -= nb;
                iov->iov_base  = (char *)iov->iov_base + nb;
                break;
            }
        }
        while (iovcnt && iov[0].iov_len == 0) {
            iovcnt--;
            iov++;
        }
    }
    return 0;
}

int xpwrite(int fd, const void *data, ssize_t len, off_t offset)
{
    const char *s = data;

    while (len > 0) {
        ssize_t nb = pwrite(fd, s, len, offset);

        if (nb < 0) {
            if (ERR_RW_RETRIABLE(errno))
                continue;
            return -1;
        }
        s      += nb;
        len    -= nb;
        offset += nb;
    }
    return 0;
}

int xread(int fd, void *data, ssize_t len)
{
    char *s = data;
    while (len > 0) {
        ssize_t nb = read(fd, s, len);

        if (nb < 0) {
            if (ERR_RW_RETRIABLE(errno))
                continue;
            return -1;
        }
        if (nb == 0)
            return -1;
        s   += nb;
        len -= nb;
    }
    return 0;
}

int xpread(int fd, void *data, ssize_t len, off_t offset)
{
    char *s = data;
    while (len > 0) {
        ssize_t nb = pread(fd, s, len, offset);

        if (nb < 0) {
            if (ERR_RW_RETRIABLE(errno))
                continue;
            return -1;
        }
        if (nb == 0)
            return -1;
        s      += nb;
        len    -= nb;
        offset += nb;
    }
    return 0;
}

int xftruncate(int fd, off_t offs)
{
    for (;;) {
        int res = ftruncate(fd, offs);

        if (res < 0 && ERR_RW_RETRIABLE(errno))
            continue;
        return res;
    }
}

bool is_fd_open(int fd)
{
    return fcntl(fd, F_GETFD) != -1 || errno != EBADF;
}

void devnull_dup(int fd)
{
    int nullfd;

    close(fd);

    nullfd = open("/dev/null", O_RDWR);
    assert (nullfd >= 0);
    if (fd != nullfd) {
        dup2(nullfd, fd);
        close(nullfd);
    }
}

bool is_fancy_fd(int fd)
{
    const char *term;

    if (!isatty(fd))
        return false;
    term = getenv("TERM");
    return term && *term && !strequal(term, "dumb");
}

void term_get_size(int *cols, int *rows)
{
    struct winsize w;
    int fd;

    if ((fd = open("/dev/tty", O_RDONLY)) != -1) {
        if (ioctl(fd, TIOCGWINSZ, &w) != -1) {
            *rows = w.ws_row;
            *cols = w.ws_col;
        }
        close(fd);
    }
    if (*rows <= 0) {
        *rows = atoi(getenv("LINES") ?: "24");
    }
    if (*cols <= 0) {
        *cols = atoi(getenv("COLUMNS") ?: "80");
    }
}

int fd_set_features(int fd, int flags)
{
    if (flags & (O_NONBLOCK | O_DIRECT)) {
#ifndef OS_WINDOWS
        int res = fcntl(fd, F_GETFL);
        if (res < 0)
            return e_error("fcntl failed: %m");
        if (fcntl(fd, F_SETFL, res | (flags & (O_NONBLOCK | O_DIRECT))))
            return e_error("fcntl failed: %m");
#else
        unsigned long flags = 1;
        int res = ioctlsocket(fd, FIONBIO, &flags);

        if (res == SOCKET_ERROR)
            return e_error("ioctlsocket failed: %d", errno);
#endif
    }
    if (flags & O_CLOEXEC) {
        int res = fcntl(fd, F_GETFD);
        if (res < 0)
            return e_error("fcntl failed: %m");
        if (fcntl(fd, F_SETFD, res | FD_CLOEXEC))
            return e_error("fcntl failed: %m");
    }
    return 0;
}

int fd_unset_features(int fd, int flags)
{
    if (flags & (O_NONBLOCK | O_DIRECT)) {
#ifndef OS_WINDOWS
        int res = fcntl(fd, F_GETFL);
        if (res < 0)
            return e_error("fcntl failed: %m");
        if (fcntl(fd, F_SETFL, res & ~(flags & (O_NONBLOCK | O_DIRECT))))
            return e_error("fcntl failed: %m");
#else
        unsigned long flags = 0;
        int res = ioctlsocket(fd, FIONBIO, &flags);

        if (res == SOCKET_ERROR)
            return e_error("ioctlsocket failed: %d", error);
#endif
    }
    if (flags & O_CLOEXEC) {
        int res = fcntl(fd, F_GETFD);
        if (res < 0)
            return e_error("fcntl failed: %m");
        if (fcntl(fd, F_SETFD, res & ~FD_CLOEXEC))
            return e_error("fcntl failed: %m");
    }
    return 0;
}


#ifndef __linux__
int close_fds_higher_than(int fd)
{
    int maxfd = sysconf(_SC_OPEN_MAX);

    if (maxfd == -1)
        maxfd = 1024;

    while (++fd < maxfd) {
        close(fd);
    }
    return 0;
}
#endif

int iovec_vector_kill_first(qv_t(iovec) *iovs, ssize_t len)
{
    int i = 0;

    while (i < iovs->len && len >= (ssize_t)iovs->tab[i].iov_len) {
        len -= iovs->tab[i++].iov_len;
    }
    qv_splice(iovec, iovs, 0, i, NULL, 0);
    if (iovs->len > 0 && len) {
        iovs->tab[0].iov_base = (byte *)iovs->tab[0].iov_base + len;
        iovs->tab[0].iov_len  -= len;
    }
    return i;
}

/* {{{ Tests */

Z_GROUP_EXPORT(Unix) {
    Z_TEST(lockdir, "directory locking/unlocking") {
        dir_lock_t dlock1 = DIR_LOCK_INIT;
        int fd;
        pid_t pid;
        int res;

        /* A takes the lock */
        fd = open(".", O_RDONLY);
        res = lockdir(fd, &dlock1);
        p_close(&fd);
        Z_ASSERT_N(res);

        pid = fork();
        if (pid == 0) {
            dir_lock_t dlock2 = DIR_LOCK_INIT;

            /* B tries to take the lock */
            fd = open(".", O_RDONLY);
            res = lockdir(fd, &dlock2);
            p_close(&fd);
            unlockdir(&dlock2);

            /* Failure is expected */
            _exit(res < 0 ? 0 : 1);
        }

        waitpid(pid, &res, 0);
        Z_ASSERT_EQ(res, 0);

        /* A releases the lock */
        unlockdir(&dlock1);

        pid = fork();
        if (pid == 0) {
            dir_lock_t dlock2 = DIR_LOCK_INIT;

            /* B tries to take the lock */
            fd = open(".", O_RDONLY);
            res = lockdir(fd, &dlock2);
            p_close(&fd);
            unlockdir(&dlock2);

            /* Success is expected */
            _exit(res < 0 ? 1 : 0);
        }

        waitpid(pid, &res, 0);
        Z_ASSERT_EQ(res, 0);
    } Z_TEST_END;
} Z_GROUP_END;

/* }}} */
