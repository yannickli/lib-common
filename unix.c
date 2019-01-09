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
#include <termios.h>
#include "unix.h"
#include "time.h"

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
    char *p;
    struct stat buf;
    char dir2[PATH_MAX];
    bool needmkdir = false;

    if (strlen(dir) + 1 > PATH_MAX) {
        return -1;
    }
    pstrcpy(dir2, sizeof(dir2), dir);

    /* Creating "/a/b/c/d" where "/a/b" exists but not "/a/b/c".
     * First find that "/a/b" exists, replacing slashes by \0 (see below)
     */
    while (stat(dir2, &buf) != 0) {
        if (errno != ENOENT) {
            return -1;
        }
        needmkdir = true;
        p = strrchr(dir2, '/');
        if (p == NULL) {
            goto creation;
        }
        /* OG: should check if p == dir2 */
        *p = '\0';
    }
    if (!S_ISDIR(buf.st_mode)) {
        errno = ENOTDIR;
        return -1;
    }
    if (!needmkdir) {
        /* Directory already exists */
        return 0;
    }

  creation:
    /* Then, create /a/b/c and /a/b/c/d : we just have to put '/' where
     * we put \0 in the previous loop. */
    for (;;) {
        if (mkdir(dir2, mode) != 0) {

            /* if dir = "/a/../b", then we do a mkdir("/a/..") => EEXIST,
             * but we want to continue walking the path to create /b !
             */
            if (errno != EEXIST) {
                // XXX: We might have created only a part of the
                // path, and fail now...
                return -1;
            }
        }
        if (!strcmp(dir, dir2)) {
            break;
        }
        p = dir2 + strlen(dir2);
        *p = '/';
    }

    return 1;
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
int filecopy(const char *pathin, const char *pathout)
{
/* OG: since this function returns the number of bytes copied, the
 * return type should be off_t.
 */
    int fdin = -1, fdout = -1;
    struct stat st;
    char buf[BUFSIZ];
    const char *p;
    int nread, nwrite, total;

    fdin = open(pathin, O_RDONLY | O_BINARY);
    if (fdin < 0)
        goto error;

    if (fstat(fdin, &st))
        goto error;

    /* OG: this will not work if the source file is not writeable ;-) */
    /* OG: should test if source and destination files are the same
     * file before truncating destination file ;-) */
    fdout = open(pathout, O_WRONLY | O_BINARY | O_CREAT | O_TRUNC, st.st_mode);
    if (fdout < 0)
        goto error;

    total = 0;
    for (;;) {
        nread = read(fdin, buf, sizeof(buf));
        if (nread == 0)
            break;
        if (nread < 0) {
            if (errno == EINTR || errno == EAGAIN)
                continue;
            goto error;
        }
        for (p = buf; p - buf < nread; ) {
            nwrite = write(fdout, p, nread - (p - buf));
            if (nwrite == 0) {
                goto error;
            }
            if (nwrite < 0) {
                if (errno == EINTR || errno == EAGAIN)
                    continue;
                goto error;
            }
            p += nwrite;
        }
        total += nread;
    }

    /* OG: total should be an off_t */
    if (total != st.st_size) {
        /* This should not happen... But who knows ? */
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

        tvp[0] = (struct timeval) { .tv_sec = st.st_atime,
                                    .tv_usec = st.st_atimensec / 1000 };
        tvp[1] = (struct timeval) { .tv_sec = st.st_mtime,
                                    .tv_usec = st.st_mtimensec / 1000 };
        futimes(fdout, tvp);
    }
#endif

    close(fdin);
    close(fdout);

    return total;

  error:
    p_close(&fdin);
    p_close(&fdout);

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
        if (nb < 0 && errno != EINTR && errno != EAGAIN)
            return -1;
        if (nb > 0) {
            s += nb;
            len -= nb;
        }
    }
    return 0;
}

int xwritev(int fd, struct iovec *iov, int iovcnt)
{
    while (iovcnt) {
        ssize_t nb = writev(fd, iov, iovcnt);

        if (nb < 0) {
            if (errno == EINTR || errno == EAGAIN)
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
    }
    return 0;
}

int xread(int fd, void *data, ssize_t len)
{
    char *s = data;
    while (len > 0) {
        ssize_t nb = read(fd, s, len);
        if (nb < 0 && errno != EINTR && errno != EAGAIN)
            return -1;
        if (nb == 0)
            return -1;
        if (nb > 0) {
            s += nb;
            len -= nb;
        }
    }
    return 0;
}

int xftruncate(int fd, off_t offs)
{
    for (;;) {
        int res = ftruncate(fd, offs);

        if (res < 0 && (errno == EINTR || errno == EAGAIN))
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

__attribute__((constructor))
static void unix_initialize(void)
{
    struct timeval tm;

    gettimeofday(&tm, NULL);
    srand(tm.tv_sec + tm.tv_usec + getpid());
    ha_srand();

    if (!is_fd_open(STDIN_FILENO)) {
        devnull_dup(STDIN_FILENO);
    }
    if (!is_fd_open(STDOUT_FILENO)) {
        devnull_dup(STDOUT_FILENO);
    }
    if (!is_fd_open(STDERR_FILENO)) {
        dup2(STDOUT_FILENO, STDERR_FILENO);
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
    if (flags & O_NONBLOCK) {
#ifndef OS_WINDOWS
        int res = fcntl(fd, F_GETFL);
        if (res < 0)
            return e_error("fcntl failed.");
        if (fcntl(fd, F_SETFL, res | O_NONBLOCK))
            return e_error("fcntl failed.");
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
            return e_error("fcntl failed.");
        if (fcntl(fd, F_SETFD, res | FD_CLOEXEC))
            return e_error("fcntl failed.");
    }
    return 0;
}

int fd_unset_features(int fd, int flags)
{
    if (flags & O_NONBLOCK) {
#ifndef OS_WINDOWS
        int res = fcntl(fd, F_GETFL);
        if (res < 0)
            return e_error("fcntl failed.");
        if (fcntl(fd, F_SETFL, res & ~O_NONBLOCK))
            return e_error("fcntl failed.");
#else
        unsigned long flags = 0;
        int res = ioctlsocket(fd, FIONBIO, &flags);

        if (res == SOCKET_ERROR)
            return e_error("ioctlsocket failed: %d", errno);
#endif
    }
    if (flags & O_CLOEXEC) {
        int res = fcntl(fd, F_GETFD);
        if (res < 0)
            return e_error("fcntl failed.");
        if (fcntl(fd, F_SETFD, res & ~FD_CLOEXEC))
            return e_error("fcntl failed.");
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
