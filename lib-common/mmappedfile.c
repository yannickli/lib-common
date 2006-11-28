/**************************************************************************/
/*                                                                        */
/*  Copyright (C) 2004-2006 INTERSEC                                      */
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

    if (flags & (O_WRONLY | O_RDWR)) {
        prot |= PROT_WRITE;
    }
    mf->size = st.st_size;
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
    return mf;

  error:
    if (fd >= 0) {
        close(fd);
    }
    mmfile_close(&mf);
    return NULL;
}

mmfile *mmfile_open_or_creat(const char *path, int flags, int mode,
                             int initialsize, bool *created)
{
    int fd = -1, prot = PROT_READ;
    struct stat st;
    mmfile *mf = mmfile_new();

    fd = open(path, flags | O_CREAT, mode);
    if (fd < 0)
        goto error;

    if (fstat(fd, &st))
        goto error;

    if (flags & (O_WRONLY | O_RDWR)) {
        prot |= PROT_WRITE;
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

    fd = open(mf->path, O_RDWR);
    if (fd < 0) {
        errno = EBADF;
        return -1;
    }

    /* FIXME: try and use munmap on tail if shrinking mapping,
     * Try mmap on extension or mmap before munmap if extending
     * Should not wipe, just keep area = NULL.
     */

    if (munmap(mf->area, mf->size) || ftruncate(fd, length)) {
        close(fd);
        return -1;
    }

    mf->size = length;
    mf->area = mmap(NULL, mf->size, PROT_READ | PROT_WRITE, MAP_SHARED,
                    fd, 0);
    close(fd);
    if (mf->area == MAP_FAILED) {
        mf->area = NULL;
        mmfile_wipe(mf);
        return -2;
    }

    return 0;
}
