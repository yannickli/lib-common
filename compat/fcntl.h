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

#define O_DIRECT      0x200000

ssize_t fd_get_path(int fd, char buf[], size_t buf_len);


#endif

#ifndef __GLIBC__
#define __is_need_posix_fallocate
int posix_fallocate(int fd, off_t offset, off_t len);

#endif

#ifndef __USE_ATFILE
# ifndef AT_SYMLINK_NOFOLLOW
#  define AT_SYMLINK_NOFOLLOW  0x100  /* Do not follow symbolic links. */
# endif
#endif

#endif /* !IS_COMPAT_FCNTL_H */

