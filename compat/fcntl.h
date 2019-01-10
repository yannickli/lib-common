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

#ifndef IS_COMPAT_FCNTL_H
#define IS_COMPAT_FCNTL_H

#include <sys/types.h>

#include_next <fcntl.h>

#ifndef O_BINARY
#define O_BINARY 0
#endif

#if defined(__MINGW) || defined(__MINGW32__)

#ifndef O_NONBLOCK
#define O_NONBLOCK 00004000
#endif

#ifndef __USE_FILE_OFFSET64
# define F_GETLK        5       /* Get record locking info.  */
# define F_SETLK        6       /* Set record locking info (non-blocking).  */
# define F_SETLKW       7       /* Set record locking info (blocking).  */
#else
# define F_GETLK        F_GETLK64  /* Get record locking info.  */
# define F_SETLK        F_SETLK64  /* Set record locking info (non-blocking).*/
# define F_SETLKW       F_SETLKW64 /* Set record locking info (blocking).  */
#endif
#define F_GETLK64       12      /* Get record locking info.  */
#define F_SETLK64       13      /* Set record locking info (non-blocking).  */
#define F_SETLKW64      14      /* Set record locking info (blocking).  */

/* For posix fcntl() and `l_type' field of a `struct flock' for lockf().  */
#define F_RDLCK         0       /* Read lock.  */
#define F_WRLCK         1       /* Write lock.  */
#define F_UNLCK         2       /* Remove lock.  */

/* NOTE: These declarations also appear in <unistd.h>; be sure to keep both
   files consistent.  Some systems have them there and some here, and some
   software depends on the macros being defined without including both.  */

/* `lockf' is a simpler interface to the locking facilities of `fcntl'.
   LEN is always relative to the current file position.
   The CMD argument is one of the following.  */

# define F_ULOCK 0      /* Unlock a previously locked region.  */
# define F_LOCK  1      /* Lock a region for exclusive use.  */
# define F_TLOCK 2      /* Test and lock a region for exclusive use.  */
# define F_TEST  3      /* Test a region for other processes locks.  */

struct flock
  {
    short int l_type;   /* Type of lock: F_RDLCK, F_WRLCK, or F_UNLCK.  */
    short int l_whence; /* Where `l_start' is relative to (like `lseek').  */
#ifndef __USE_FILE_OFFSET64
    __off_t l_start;    /* Offset where the lock begins.  */
    __off_t l_len;      /* Size of the locked area; zero means until EOF.  */
#else
    __off64_t l_start;  /* Offset where the lock begins.  */
    __off64_t l_len;    /* Size of the locked area; zero means until EOF.  */
#endif
    __pid_t l_pid;      /* Process holding the lock.  */
  };

#ifdef __USE_LARGEFILE64
struct flock64
  {
    short int l_type;   /* Type of lock: F_RDLCK, F_WRLCK, or F_UNLCK.  */
    short int l_whence; /* Where `l_start' is relative to (like `lseek').  */
    __off64_t l_start;  /* Offset where the lock begins.  */
    __off64_t l_len;    /* Size of the locked area; zero means until EOF.  */
    __pid_t l_pid;      /* Process holding the lock.  */
  };
#endif

/* fcntl does not seem implemented in Mingw32... */
static inline int fcntl (int __fd, int __cmd, ...)
{
    /* Not implemented */
    return -1;
}

#elif defined(__APPLE__)

#include <sys/syslimits.h>
#include <sys/stat.h>
#include <string.h>
#include <stdarg.h>
#include <stdio.h>
#include <unistd.h>
#include <errno.h>

#define AT_FDCWD      -100
#define O_DIRECT      0x200000
#define AT_REMOVEDIR  1

ssize_t fd_get_path(int fd, char buf[], size_t buf_len);

static int get_path_at(int dirfd, const char *pathname,
                       char buf[], size_t buf_len)
{
    if (*pathname == '/') {
        buf[0] = '\0';
        pathname++;
    } else
    if (dirfd == AT_FDCWD) {
        getcwd(buf, buf_len);
    } else {
        if (fd_get_path(dirfd, buf, buf_len) < 0) {
            return -1;
        }
    }

    if (strlcat(buf, "/", buf_len) > buf_len) {
        errno = ENAMETOOLONG;
        return -1;
    }
    if (strlcat(buf, pathname, buf_len) > buf_len) {
        errno = ENAMETOOLONG;
        return -1;
    }

    return 0;
}

static inline
int openat(int dirfd, const char *pathname, int flags, ...)
{
    int mode = 0;
    char path[PATH_MAX];

    if (get_path_at(dirfd, pathname, path, PATH_MAX) < 0) {
        return -1;
    }

    if (flags & O_CREAT) {
        va_list arg;

        va_start(arg, flags);
        mode = va_arg(arg, int);
        va_end(arg);
    }
    return open(path, flags, mode);
}

static inline
int unlinkat(int dirfd, const char *pathname, int flags)
{
    char path[PATH_MAX];

    if (get_path_at(dirfd, pathname, path, PATH_MAX) < 0) {
        return -1;
    }
    if (flags & AT_REMOVEDIR) {
        return rmdir(path);
    } else {
        return unlink(path);
    }
}

static inline
int renameat(int olddirfd, const char *oldpath,
             int newdirfd, const char *newpath)
{
    char oldp[PATH_MAX];
    char newp[PATH_MAX];

    if (get_path_at(olddirfd, oldpath, oldp, PATH_MAX) < 0
    ||  get_path_at(newdirfd, newpath, newp, PATH_MAX) < 0)
    {
        return -1;
    }
    return rename(oldp, newp);
}

static inline
int linkat(int olddirfd, const char *oldpath,
           int newdirfd, const char *newpath, int flags)
{
    char oldp[PATH_MAX];
    char newp[PATH_MAX];

    if (get_path_at(olddirfd, oldpath, oldp, PATH_MAX) < 0
    ||  get_path_at(newdirfd, newpath, newp, PATH_MAX) < 0)
    {
        return -1;
    }
    return link(oldp, newp);
}

static inline
int mkdirat(int dirfd, const char *pathname, mode_t mode)
{
    char path[PATH_MAX];

    if (get_path_at(dirfd, pathname, path, PATH_MAX) < 0) {
        return -1;
    }
    return mkdir(path, mode);
}

#endif

#ifndef __GLIBC__
#define __is_need_posix_fallocate
int posix_fallocate(int fd, off_t offset, off_t len);

#endif

#ifndef __USE_ATFILE
#  define AT_SYMLINK_NOFOLLOW  0x100  /* Do not follow symbolic links. */
#endif

#endif /* !IS_COMPAT_FCNTL_H */

