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

#define _GNU_SOURCE
#include <assert.h>
#include <unistd.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <errno.h>

#include <lib-common/mem.h>

#include "mmappedfile.h"

GENERIC_INIT(mmfile, mmfile);
GENERIC_NEW(mmfile, mmfile);

mmfile *mmfile_open(const char *path, int flags)
{
    int mflags = MAP_SHARED;
    int fd = -1, prot = PROT_READ;
    struct stat st;
    mmfile *mf = mmfile_new();

    /* Kludge for MAP_POPULATE */
    if (flags & MMAP_O_PRELOAD) {
        mflags |= MAP_POPULATE;
        flags &= ~MMAP_O_PRELOAD;
    }

    fd = open(path, flags, 0644);
    if (fd < 0)
        goto error;

    if (fstat(fd, &st))
        goto error;

    switch (flags & (O_RDONLY | O_WRONLY | O_RDWR)) {
      case O_RDONLY:
        mf->ro = true;
        break;
      case O_WRONLY:
      case O_RDWR:
        prot |= PROT_WRITE;
        mf->ro = false;
        break;
      default:
        errno = EINVAL;
        goto error;
    }

    mf->size = st.st_size;
    mf->area = mmap(NULL, mf->size, prot, mflags, fd, 0);
    if (mf->area == MAP_FAILED) {
        mf->area = NULL;
        goto error;
    }

    close(fd);
    mf->path  = p_strdup(path);
    return mf;

  error:
    if (fd >= 0) {
        close(fd);
    }
    mmfile_close(&mf);
    return NULL;
}

mmfile *mmfile_creat(const char *path, off_t initialsize)
{
    int fd = -1;
    mmfile *mf = mmfile_new();

    fd = open(path, O_CREAT | O_TRUNC | O_RDWR, 0664);
    if (fd < 0)
        goto error;

    if (ftruncate(fd, initialsize) || posix_fallocate(fd, 0, initialsize))
        goto error;

    mf->size = initialsize;

    mf->area = mmap(NULL, mf->size, PROT_READ | PROT_WRITE, MAP_SHARED,
                    fd, 0);
    if (mf->area == MAP_FAILED) {
        mf->area = NULL;
        goto error;
    }

    close(fd);
    mf->path = p_strdup(path);
    mf->ro   = false;
    return mf;

  error:
    if (fd >= 0) {
        close(fd);
    }
    mmfile_close(&mf);
    return NULL;
}

mmfile *mmfile_open_or_creat(const char *path, int flags,
                             off_t initialsize, bool *created)
{
    int mflags = MAP_SHARED;
    int fd = -1, prot = PROT_READ;
    struct stat st;
    mmfile *mf = mmfile_new();

    /* Kludge for MAP_POPULATE */
    if (flags & MMAP_O_PRELOAD) {
        mflags |= MAP_POPULATE;
        flags &= ~MMAP_O_PRELOAD;
    }

    fd = open(path, flags | O_CREAT, 0644);
    if (fd < 0)
        goto error;

    if (fstat(fd, &st))
        goto error;

    switch (flags & (O_RDONLY | O_WRONLY | O_RDWR)) {
      case O_RDONLY:
        mf->ro = true;
        break;
      case O_WRONLY:
      case O_RDWR:
        prot |= PROT_WRITE;
        mf->ro = false;
        break;
      default:
        errno = EINVAL;
        goto error;
    }

    if (created) {
        *created = (st.st_size == 0);
    }

    if (st.st_size < initialsize) {
        if (ftruncate(fd, initialsize) || posix_fallocate(fd, 0, initialsize)) {
            goto error;
        }
        mf->size = initialsize;
    } else {
        mf->size = st.st_size;
    }
    mf->area = mmap(NULL, mf->size, prot, mflags, fd, 0);
    if (mf->area == MAP_FAILED) {
        mf->area = NULL;
        goto error;
    }

    close(fd);
    mf->path = p_strdup(path);
    return mf;

  error:
    if (fd >= 0) {
        close(fd);
    }
    mmfile_close(&mf);
    return NULL;
}

static inline void mmfile_wipe(mmfile *mf)
{
    p_delete(&mf->path);
    if (mf->area) {
        munmap(mf->area, mf->size);
        mf->area = NULL;
    }
}

void mmfile_close(mmfile **mf)
{
    if (*mf) {
        if (!(*mf)->ro) {
            msync((*mf)->area, (*mf)->size, MS_SYNC);
        }
        mmfile_wipe(*mf);
        p_delete(mf);
    }
}

int mmfile_truncate(mmfile *mf, off_t length, int (*lock)(void*),
                    int (*unlock)(void*), void *mut)
{
    int res;

    assert (!lock == !unlock);

    if (length == mf->size)
        return 0;

    if (length > mf->size) {
        int fd;

        fd = open(mf->path, O_RDWR);
        if (fd < 0) {
            errno = EBADF;
            return -1;
        }

        if ((res = ftruncate(fd, length)) != 0
        ||  (res = posix_fallocate(fd, mf->size, length - mf->size)) != 0)
        {
            close(fd);
            return res;
        }
        close(fd);
    }

    /* stage 1: try to remap at the same place */
    if (mremap(mf->area, mf->size, length, 0) == MAP_FAILED) {
        void *map;

        /* stage 2: lock, and remap for real */
        if (lock && (*lock)(mut) < 0) {
            return -1;
        }
        map = mremap(mf->area, mf->size, length, MREMAP_MAYMOVE);
        if (map != MAP_FAILED) {
            mf->area = map;
        }
        if (unlock) {
            (*unlock)(mut);
        }

        if (map == MAP_FAILED) {
            return -1;
        }
    }

    mf->size = length;
    return length > mf->size ? 0 : truncate(mf->path, length);
}
