/**************************************************************************/
/*                                                                        */
/*  Copyright (C) 2004-2007 INTERSEC SAS                                  */
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
#include "mmappedfile.h"
#include "unix.h"

#if defined(MINGCC)

#include <windows.h>
#include <stdlib.h>
#include <string.h>
#include <fcntl.h>
#include <errno.h>

void intersec_initialize(void)
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

int posix_fallocate(int fd, off_t offset, off_t len)
{
    return EINVAL;
}

#ifdef NEED_GETTIMEOFDAY
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

void usleep(unsigned long usec)
{
    Sleep(usec / 1000);
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

long int lrand48(void)
{
    unsigned int r, i;
    long int res = 0;
    
    /* OG: This method is incorrect, should use high bits instead of
     * low byte.
     */
    for (i = 0; i < sizeof(long int); i++) {
        r = rand();
        res |= (r & 0xFF) << (i * 8);
    }

    return res;
}

int pid_get_starttime(pid_t pid, struct timeval *tv)
{
    tv->tv_sec = tv->tv_usec = 0;
    return 0;
}

#elif defined(CYGWIN)

int pid_get_starttime(pid_t pid, struct timeval *tv)
{
    tv->tv_sec = tv->tv_usec = 0;
    return 0;
}

#else

void intersec_initialize(void) {}

#endif
