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

#include "macros.h"
#include "unix.h"
#include "str.h"

/* Returns 0 if directory exists,
 * Returns 1 if directory was created
 * Returns -1 in case an error occurred.
 */
int mkdir_p(const char *dir, mode_t mode)
{
    char *p;
    struct stat buf;
    char dir2[PATH_MAX];
    bool needmkdir = 0;

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
        needmkdir = 1;
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

/**
 * Get time of last modification.
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

/**
 * Copy file pathin to pathout. If pathout already exists, it will
 * be overwritten.
 *
 * Note: Use the same mode bits as the input file.
 * OG: since this function returns the number of bytes copied, the
 * return type should be off_t.
 */
int filecopy(const char *pathin, const char *pathout)
{
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
    /* OG: copying the file times might be useful too */
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

    close(fdin);
    close(fdout);

    return total;

  error:
    p_close(&fdin);
    p_close(&fdout);

    /* OG: destination file should be removed upon error ? */
    return -1;
}

int p_lockf(int fd, int mode, int cmd, off_t start, off_t len)
{
    struct flock lock;
    int res;

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

      case F_ULOCK:
        cmd = F_UNLCK;
        break;

      default:
        errno = EINVAL;
        return -1;
    }

    if (O_ISWRITE(mode)) {
        lock.l_type = F_WRLCK;
    } else {
        lock.l_type = F_RDLCK;
    }
    lock.l_whence = SEEK_CUR;
    lock.l_start  = start;
    lock.l_len    = len;
    if (cmd == F_GETLK) {
        lock.l_pid    = getpid();
    }

    res = fcntl(fd, cmd, &lock);

    if (res < 0)
        return res;
    if (cmd == F_GETLK) {
        if (lock.l_type == F_UNLCK) {
            return 0;
        }
        errno = EAGAIN;
        return -1;
    }

    return 0;
}

int tmpfd(void)
{
    char path[PATH_MAX] = "/tmp/XXXXXX";
    mode_t mask = umask(0177);
    int fd = mkstemp(path);
    umask(mask);
    if (fd < 0)
        return fd;
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

#ifndef LINUX
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
