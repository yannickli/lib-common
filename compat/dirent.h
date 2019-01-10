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

#ifndef IS_COMPAT_DIRENT_H
#define IS_COMPAT_DIRENT_H

#include_next <dirent.h>

#if defined(__APPLE__)

#include <sys/syslimits.h>
#include <fcntl.h>

static inline
DIR *fdopendir(int dirfd)
{
    char path[PATH_MAX];
    DIR *dir;

    if (fd_get_path(dirfd, path, PATH_MAX) < 0) {
        return NULL;
    }
    dir = opendir(path);
    if (dir == NULL) {
        return NULL;
    }
    close(dir->__dd_fd);
    dir->__dd_fd = dirfd;
    return dir;
}

#elif !defined(__GLIBC__)
#define __is_need_dirfd
int dirfd(DIR *dir);

#endif

#endif /* !IS_COMPAT_DIRENT_H */

