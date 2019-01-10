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

#include <sys/mman.h>

#include "unix.h"
#include "mmappedfile.h"

mmfile *mmfile_open(const char *path, int flags, int oflags, off_t minsize)
{
    int mflags = MAP_SHARED, prot = PROT_READ;
    struct stat st;
    mmfile *mf = p_new(mmfile, 1);

    mf->fd     = -1;

#ifdef MAP_POPULATE
    if (oflags & MMO_POPULATE) {
        mflags |= MAP_POPULATE;
    }
#endif

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
        if (p_lockf(mf->fd, flags, F_LOCK, 0, minsize) < 0)
            goto error;
        mf->locked = true;
    } else
    if (oflags & MMO_TLOCK) {
        if (p_lockf(mf->fd, flags, F_TLOCK, 0, minsize) < 0)
            goto error;
        mf->locked = true;
    }

    if (fstat(mf->fd, &st) < 0)
        goto error;

    if (prot & PROT_WRITE && st.st_size < minsize) {
        if (xftruncate(mf->fd, minsize))
            goto error;
        if (posix_fallocate(mf->fd, 0, minsize) && errno != ENOSYS)
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
        p_close(&mf->fd);
    }
    mf->path   = p_strdup(path);
    mf->refcnt = 1;

    if (oflags & MMO_RANDOM) {
        madvise(mf->area, mf->size, MADV_RANDOM);
    }

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

    RETHROW(close(mf->fd));
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

    p_close(&mf->fd);

    if (mf->area) {
        if (mf->writeable)
            msync(mf->area, mf->size, MS_SYNC);
        munmap(mf->area, mf->size);
        mf->area = NULL;
    }

    /* OG: redundant ??? */
    p_close(&mf->fd);

    mmfile_unlock(mf);
    p_delete(&mf->path);
    /* *mfp was already reset to NULL */
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
    int fd = -1;

    if (length == mf->size)
        return 0;

    if (length > mf->size) {
        fd = open(mf->path, O_RDWR);

        if (fd < 0) {
            errno = EBADF;
            return -1;
        }

        if (ftruncate(fd, length)) {
            p_close(&fd);
            return -1;
        }

        if (posix_fallocate(fd, mf->size, length - mf->size)) {
            if (errno != ENOSYS) {
                p_close(&fd);
                return -1;
            }
        }
    }

#ifdef MREMAP_MAYMOVE
    if (fd >= 0) {
        p_close(&fd);
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
#else
    {
        void *new_map;

        if (fd < 0) {
            fd = open(mf->path, O_RDWR);

            if (fd < 0) {
                errno = EBADF;
                return -1;
            }
        }

        new_map = mmap(NULL, length, PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0);
        p_close(&fd);
        if (new_map == MAP_FAILED) {
            e_trace(0, "Could not remap: %s", strerror(errno));
            errno = ENOMEM;
            return -1;
        }

        munmap(mf->area, mf->size);

        mf->area = new_map;

    }
#endif

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
