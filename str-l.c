/**************************************************************************/
/*                                                                        */
/*  Copyright (C) 2004-2012 INTERSEC SAS                                  */
/*                                                                        */
/*  Should you receive a copy of this source code, you must check you     */
/*  have a proper, written authorization of INTERSEC to hold it. If you   */
/*  don't have such an authorization, you must DELETE all source code     */
/*  files in your possession, and inform INTERSEC of the fact you obtain  */
/*  these files. Should you not comply to these terms, you can be         */
/*  prosecuted in the extent permitted by applicable law.                 */
/*                                                                        */
/**************************************************************************/

#include "str.h"
#include "unix.h"

int lstr_init_from_file(lstr_t *dst, const char *path, int prot, int flags)
{
    int fd_flags = 0;
    struct stat st;
    int fd = -1;

    if (flags & MAP_ANONYMOUS) {
        assert (false);
        return -1;
    }
    if (prot & PROT_READ) {
        if (prot & PROT_WRITE) {
            fd_flags = O_RDWR;
        } else {
            fd_flags = O_RDONLY;
        }
    } else
    if (prot & PROT_WRITE) {
        fd_flags = O_WRONLY;
    } else {
        assert (false);
        *dst = LSTR_NULL_V;
        return -1;
    }

    fd = RETHROW(open(path, fd_flags));
    if (fstat(fd, &st)) {
        PROTECT_ERRNO(p_close(&fd));
        return -1;
    }

    if (st.st_size == 0) {
        PROTECT_ERRNO(p_close(&fd));
        *dst = LSTR_EMPTY_V;
        return 0;
    }

    *dst = lstr_init_(mmap(NULL, st.st_size, prot, flags, fd, 0),
                      st.st_size, MEM_MMAP);
    PROTECT_ERRNO(p_close(&fd));

    if (dst->v == MAP_FAILED) {
        return -1;
    }
    return 0;
}

void (lstr_munmap)(lstr_t *dst)
{
    if (munmap(dst->v, dst->len) < 0) {
        e_panic("bad munmap: %m");
    }
}
