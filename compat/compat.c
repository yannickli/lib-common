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

#define _ATFILE_SOURCE

#if defined(__MINGW) || defined(__MINGW32__)

#include <lib-common/mmappedfile.h>
#include <lib-common/unix.h>
#include <sys/mman.h>

#include <windows.h>
#include <stdlib.h>
#include <string.h>
#include <fcntl.h>
#include <errno.h>

__attribute__((constructor))
static void intersec_initialize(void)
{
    /* Force open by default in binary mode */
    _fmode = _O_BINARY;
}

void *mmap(void *__addr, size_t __len, int __prot,
           int __flags, int __fd, off_t __offset)
{
    return NULL;
}

int munmap(void *__addr, size_t __len)
{
    return -1;
}

int msync(void *__addr, size_t __len, int __flags)
{
    return -1;
}

void *mremap(void *__addr, size_t __old_len, size_t __new_len,
             int __flags, ...)
{
    return NULL;
}

#  if __MINGW32_MAJOR_VERSION < 3 || \
     (__MINGW32_MAJOR_VERSION == 3 && __MINGW32_MINOR_VERSION < 12)
/* Windows API do not have gettimeofday support */
/* OG: should define a simpler API, and implement it in a compatibility
 * module for linux and ming appropriately
 */
void gettimeofday(struct timeval *p, void *tz)
{
    union {
        long long ns100; /* Time since 1 Jan 1601 in 100ns units */
        FILETIME ft;
    } now;

    GetSystemTimeAsFileTime(&now.ft);

    p->tv_usec = (long)((now.ns100 / 10LL) % 1000000LL);
    p->tv_sec  = (long)((now.ns100 - (116444736000000000LL)) / 10000000LL);
}
#endif

char *asctime_r(const struct tm *tm, char *buf)
{
    return strcpy(buf, asctime(tm));
}

char *ctime_r(const time_t *timep, char *buf)
{
    return strcpy(buf, ctime(timep));
}

struct tm *gmtime_r(const time_t *timep, struct tm *result)
{
    *result = *gmtime(timep);
    return result;
}

struct tm *localtime_r(const time_t *timep, struct tm *result)
{
    *result = *localtime(timep);
    return result;
}

int usleep(unsigned long usec)
{
    Sleep(usec / 1000);
    return 0;
}

int fnmatch(const char *pattern, const char *string, int flags)
{
    /* TODO: implement from scu */
    return FNM_NOMATCH;
}

int glob(const char *pattern, int flags,
         int errfunc(const char *epath, int eerrno), glob_t *pglob)
{
    return GLOB_NOSYS;
}

void globfree(glob_t *pglob)
{
}

int pid_get_starttime(pid_t pid, struct timeval *tv)
{
    tv->tv_sec = tv->tv_usec = 0;
    return 0;
}

#elif defined(__CYGWIN__)

int pid_get_starttime(pid_t pid, struct timeval *tv)
{
    tv->tv_sec = tv->tv_usec = 0;
    return 0;
}

#endif

#include <errno.h>

#include <stdlib.h>
#ifdef __is_need_posix_fallocate
int posix_fallocate(int fd, off_t offset, off_t len)
{
    errno = ENOSYS;
    return -1;
}
#endif

#include <sys/mman.h>
#ifdef __is_need_mremap
void * mremap(void *old_address, size_t old_size , size_t new_size, int
                     flags)
{
    if (!(flags & MREMAP_MAYMOVE)) {
    }
}
#endif

#include <dirent.h>
#ifdef __is_need_dirfd
int dirfd(DIR *dir)
{
    return dir->d_fd;
}
#endif

#include <sys/stat.h>
#ifdef __is_need_at_replacement
#include <fcntl.h>
#include <unistd.h>

int fstatat(int dir_fd, const char *pathname, struct stat *buf,
            int flags)
{
    int cwd_fd, err, ret;

    cwd_fd = open(".", O_RDONLY);
    if (!cwd_fd)
        return -1;

    if (fchdir(dir_fd)) {
        err = errno;
        close(cwd_fd);
        errno = err;
        return -1;
    }

    ret = flags & AT_SYMLINK_NOFOLLOW ? lstat(pathname, buf)
                                      :  stat(pathname, buf);
    err = errno;

    /* This won't fail because we kept cwd_fd open */
    fchdir(cwd_fd);
    close(cwd_fd);
    errno = err;
    return ret;
}
#endif
