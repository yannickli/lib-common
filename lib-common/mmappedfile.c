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

GENERIC_FUNCTIONS(mmfile, mmfile);

mmfile *mmfile_open(const char *path, int flags, off_t minsize)
{
    int mflags = MAP_SHARED, prot = PROT_READ;
    int fd = -1;
    struct stat st;
    mmfile *mf = mmfile_new();

    /* Kludge for MAP_POPULATE */
    if (flags & MMAP_O_PRELOAD) {
        mflags |= MAP_POPULATE;
        flags &= ~MMAP_O_PRELOAD;
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

    fd = open(path, flags, 0644);
    if (fd < 0 || fstat(fd, &st) < 0)
        goto error;

    if (prot & PROT_WRITE && st.st_size < minsize) {
        if (ftruncate(fd, minsize) || posix_fallocate(fd, 0, minsize))
            goto error;
        mf->size = minsize;
    } else {
        if (st.st_size < minsize) {
            errno = EINVAL;
            goto error;
        }
        mf->size = st.st_size;
    }

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
    mmfile_close(&mf, NULL);
    return NULL;
}

void mmfile_close(mmfile **mfp, void *mutex)
{
    if (*mfp) {
        mmfile *mf = *mfp;

        if (mf->area) {
            if (mf->writeable) {
                if (mutex) {
                    (*mf->unlock)(mutex);
                }
                msync(mf->area, mf->size, MS_SYNC);
            }
            munmap(mf->area, mf->size);
            mf->area = NULL;
            if (mf->writeable && mutex) {
                (*mf->unlock)(mutex);
            }
        }
        if (mutex) {
            (*mf->destroy)(mutex);
        }
        p_delete(&mf->path);
        mmfile_delete(mfp);
    }
}

int mmfile_truncate(mmfile *mf, off_t length, void *mutex)
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
        if (mutex && (*mf->lock)(mutex) < 0) {
            return -1;
        }
        map = mremap(mf->area, mf->size, length, MREMAP_MAYMOVE);
        if (map != MAP_FAILED) {
            mf->area = map;
        }
        if (mutex) {
            (*mf->unlock)(mutex);
        }

        if (map == MAP_FAILED) {
            return -1;
        }
    }

    mf->size = length;
    return length > mf->size ? 0 : truncate(mf->path, length);
}
