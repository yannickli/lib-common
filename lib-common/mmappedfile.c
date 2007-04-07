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

#include <sys/stat.h>
#include <unistd.h>
#include <errno.h>

#include <lib-common/mem.h>

#include "mmappedfile.h"

GENERIC_INIT(mmfile, mmfile);
GENERIC_NEW(mmfile, mmfile);

mmfile *mmfile_open(const char *path, int flags)
{
    int fd = -1, prot = PROT_READ;
    struct stat st;
    mmfile *mf = mmfile_new();

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
    mf->area = mmap(NULL, mf->size, prot, MAP_SHARED, fd, 0);
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

    if (ftruncate(fd, initialsize))
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
    int fd = -1, prot = PROT_READ;
    struct stat st;
    mmfile *mf = mmfile_new();

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
        if (ftruncate(fd, initialsize)) {
            goto error;
        }
        mf->size = initialsize;
    } else {
        mf->size = st.st_size;
    }
    mf->area = mmap(NULL, mf->size, prot, MAP_SHARED, fd, 0);
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
        mmfile_wipe(*mf);
        p_delete(mf);
    }
}

int mmfile_truncate(mmfile *mf, off_t length)
{
    int fd = -1;
    int res;

    if (length == mf->size)
        return 0;

    fd = open(mf->path, O_RDWR);
    if (fd < 0) {
        errno = EBADF;
        return -1;
    }

    res = ftruncate(fd, length);

    if (length < mf->size) {
        close(fd);
        /* ignore errors: we shrink */
        mf->size = length;
        return 0;
    }

    if (res) {
        close(fd);
        return res;
    }

    munmap(mf->area, mf->size);
    mf->area = mmap(NULL, length, PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0);
    if (mf->area == MAP_FAILED) {
        mf->area = mmap(NULL, mf->size, PROT_READ | PROT_WRITE, MAP_SHARED,
                        fd, 0);
        res = close(fd);

        if (mf->area == MAP_FAILED) {
            mf->area = NULL;
            mf->size = 0;
            return -2;
        }

        return res;
    }

    mf->size = length;
    return close(fd);
}
