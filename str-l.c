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

#include "unix.h"

int lstr_init_from_fd(lstr_t *dst, int fd, int prot, int flags)
{
    struct stat st;

    if (unlikely(fstat(fd, &st)) < 0) {
        return -2;
    }

    if (st.st_size <= 0) {
        SB_8k(sb);

        if (sb_read_fd(&sb, fd) < 0) {
            return -3;
        }

        *dst = LSTR_EMPTY_V;
        if (sb.len == 0) {
            return 0;
        }
        lstr_transfer_sb(dst, &sb, false);
        return 0;
    }

    if (st.st_size == 0) {
        *dst = LSTR_EMPTY_V;
        return 0;
    }

    if (st.st_size > INT_MAX) {
        errno = ERANGE;
        return -3;
    }

    *dst = lstr_init_(mmap(NULL, st.st_size, prot, flags, fd, 0),
                      st.st_size, MEM_MMAP);

    if (dst->v == MAP_FAILED) {
        *dst = LSTR_NULL_V;
        return -3;
    }
    return 0;

}

int lstr_init_from_file(lstr_t *dst, const char *path, int prot, int flags)
{
    int fd_flags = 0;
    int fd = -1;
    int ret = 0;

    if (flags & MAP_ANONYMOUS) {
        assert (false);
        errno = EINVAL;
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
        errno = EINVAL;
        return -1;
    }

    fd = RETHROW(open(path, fd_flags));
    ret = lstr_init_from_fd(dst, fd, prot, flags);
    PROTECT_ERRNO(p_close(&fd));
    return ret;
}

void (lstr_munmap)(lstr_t *dst)
{
    if (munmap(dst->v, dst->len) < 0) {
        e_panic("bad munmap: %m");
    }
}

void lstr_transfer_sb(lstr_t *dst, sb_t *sb, bool keep_pool)
{
    if (keep_pool) {
        if (sb->mem_pool != MEM_STATIC) {
            if (sb->skip) {
                memmove(sb->data - sb->skip, sb->data, sb->len + 1);
            }
        }
        lstr_copy_(NULL, dst, sb->data, sb->len, sb->mem_pool);
        sb_init(sb);
    } else {
        dst->v = sb_detach(sb, &dst->len);
        dst->mem_pool = MEM_LIBC;
    }
}
