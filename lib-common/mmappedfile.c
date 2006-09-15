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

#define mmfile_new() mmfile_init(p_new_raw(mmfile, 1))
static inline mmfile *mmfile_init(mmfile *mf)
{
    p_blank(mmfile, mf, 1);
    mf->fd = -1;

    return mf;
}

mmfile *mmfile_open(const char *path, int flags)
{
    struct stat st;
    mmfile *mf = mmfile_new();

    mf->fd = open(path, flags | O_RDWR);
    if (mf->fd < 0)
        goto error;

    if (fstat(mf->fd, &st))
        goto error;

    mf->size = st.st_size;
    mf->area = mmap(NULL, mf->size, PROT_READ | PROT_WRITE, MAP_SHARED,
                    mf->fd, 0);
    if (mf->area == (void *)-1) {
        mf->area = NULL;
        goto error;
    }

    mf->path = strdup(path);
    return mf;

  error:
    mmfile_close(&mf);
    return NULL;
}

mmfile *mmfile_creat(const char *path, off_t initialsize)
{
    mmfile *mf = mmfile_new();

    mf->fd = open(path, O_CREAT | O_TRUNC | O_RDWR, 0664);
    if (mf->fd < 0)
        goto error;

    if (ftruncate(mf->fd, initialsize))
        goto error;

    mf->size = initialsize;

    mf->area = mmap(NULL, mf->size, PROT_READ | PROT_WRITE, MAP_SHARED,
                    mf->fd, 0);
    if (mf->area == (void *)-1) {
        mf->area = NULL;
        goto error;
    }

    mf->path  = strdup(path);
    return mf;

  error:
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
    if (mf->fd >= 0) {
        close(mf->fd);
        mf->fd = -1;
    }
}

void mmfile_close(mmfile **mf)
{
    GENERIC_DELETE(mmfile_wipe, mf);
}

int mmfile_truncate(mmfile *mf, off_t length)
{
    if (mf->fd < 0) {
        errno = EBADF;
        return -1;
    }

    /* FIXME: try and use munmap on tail if shrinking mapping,
     * Try mmap on extension or mmap before munmap if extending
     * Should not wipe, just keep area = NULL.
     */

    if (munmap(mf->area, mf->size) || ftruncate(mf->fd, length))
        return -1;

    mf->size = length;
    mf->area = mmap(NULL, mf->size, PROT_READ | PROT_WRITE, MAP_SHARED,
                    mf->fd, 0);
    if (mf->area == (void *)-1) {
        mf->area = NULL;
        mmfile_wipe(mf);
        return -2;
    }

    return 0;
}
