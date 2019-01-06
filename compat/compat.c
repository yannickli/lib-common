/**************************************************************************/
/*                                                                        */
/*  Copyright (C) 2004-2019 INTERSEC SA                                   */
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

#include <errno.h>

#include <stdlib.h>
#include <fcntl.h>
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

#include <sys/mman.h>
#ifdef __is_need_mremap_diverted
#include <valgrind.h>
#include <memcheck.h>

void *
mremap_diverted(void *old_address, size_t old_size, size_t new_size, int flags)
{
    void *mres;
    mres = (mremap)(old_address, old_size, new_size, flags);

    if (mres != MAP_FAILED) {
        (void)VALGRIND_MAKE_MEM_NOACCESS(old_address, old_size);
        (void)VALGRIND_MAKE_MEM_DEFINED(mres, new_size);
    }

    return mres;
}
#endif
