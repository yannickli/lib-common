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

mmfile *mmfile_open(const char *path, int flags, int oflags, off_t minsize)
{
    int mflags = MAP_SHARED, prot = PROT_READ;
    struct stat st;
    mmfile *mf = p_new(mmfile, 1);
    mf->fd     = -1;

    if (oflags & MMO_POPULATE) {
        mflags |= MAP_POPULATE;
    }

    switch (flags & (O_RDONLY | O_WRONLY | O_RDWR)) {
      case O_RDONLY:
        mf->writeable = false;
        break;
      case O_WRONLY:
      case O_RDWR:
        prot |= PROT_WRITE;
        mf->writeable = true;
        break;
      default:
        errno = EINVAL;
        goto error;
    }

    mf->fd = open(path, flags, 0644);
    if (mf->fd < 0)
        goto error;

    if (oflags & MMO_LOCK) {
        if (lockf(mf->fd, F_LOCK, minsize) < 0)
            goto error;
        mf->locked = true;
    } else
    if (oflags & MMO_TLOCK) {
        if (lockf(mf->fd, F_TLOCK, minsize) < 0)
            goto error;
        mf->locked = true;
    }

    if (fstat(mf->fd, &st) < 0)
        goto error;

    if (prot & PROT_WRITE && st.st_size < minsize) {
        if (ftruncate(mf->fd, minsize) || posix_fallocate(mf->fd, 0, minsize))
            goto error;
        mf->size = minsize;
    } else {
        if (st.st_size < minsize) {
            errno = EINVAL;
            goto error;
        }
        mf->size = st.st_size;
    }

    mf->area = mmap(NULL, mf->size, prot, mflags, mf->fd, 0);
    if (mf->area == MAP_FAILED) {
        mf->area = NULL;
        goto error;
    }

    if (!(oflags & (MMO_LOCK | MMO_TLOCK))) {
        close(mf->fd);
        mf->fd = -1;
    }
    mf->path   = p_strdup(path);
    mf->refcnt = 1;
    return mf;

  error:
    {
        int save_errno = errno;
        mmfile_close(&mf);
        errno = save_errno;
    }
    return NULL;
}

int mmfile_unlockfile(mmfile *mf)
{
    if (!mf->locked) {
        errno = EINVAL;
        return -1;
    }

    if (close(mf->fd) < 0)
        return -1;

    mf->locked = false;
    mf->fd = -1;
    return 0;
}

void mmfile_close_wlocked(mmfile **mfp)
{
    mmfile *mf = *mfp;

    assert (*mfp);

    *mfp = NULL; /* do it ASAP during lock-safe period */
    if (--mf->refcnt > 0) {
        mmfile_unlock(mf);
        return;
    }

    if (mf->fd >= 0) {
        close(mf->fd);
        mf->fd = -1;
    }

    if (mf->area) {
        if (mf->writeable)
            msync(mf->area, mf->size, MS_SYNC);
        munmap(mf->area, mf->size);
        mf->area = NULL;
    }

    if (mf->fd >= 0) {
        close(mf->fd);
        mf->fd = -1;
    }

    mmfile_unlock(mf);
    p_delete(&mf->path);
    p_delete(&mf);
}

void mmfile_close(mmfile **mfp)
{
    if (*mfp) {
        mmfile *mf = *mfp;

        mmfile_wlock(mf);
        *mfp = NULL; /* do it ASAP during lock-safe period */
        mmfile_close_wlocked(&mf);
    }
}

static int mmfile_truncate_aux(mmfile *mf, off_t length, bool lock)
{
    int res;

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
        if (lock && mmfile_wlock(mf) < 0) {
            return -1;
        }
        map = mremap(mf->area, mf->size, length, MREMAP_MAYMOVE);
        if (map != MAP_FAILED) {
            mf->area = map;
        }
        if (lock)
            mmfile_unlock(mf);

        if (map == MAP_FAILED) {
            return -1;
        }
    }

    mf->size = length;
    return length > mf->size ? 0 : truncate(mf->path, length);
}

int mmfile_truncate(mmfile *mf, off_t length)
{
    return mmfile_truncate_aux(mf, length, true);
}

int mmfile_truncate_unlocked(mmfile *mf, off_t length)
{
    return mmfile_truncate_aux(mf, length, false);
}
