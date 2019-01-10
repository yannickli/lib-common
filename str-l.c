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
        mem_pool_t *mp = mp_ipool(sb->mp);

        if ((mp_ipool(mp)->mem_pool & MEM_BY_FRAME)) {
            if (sb->skip) {
                memmove(sb->data - sb->skip, sb->data, sb->len + 1);
            }
        }
        mp_lstr_copy_(mp, dst, sb->data, sb->len);
        sb_init(sb);
    } else {
        lstr_wipe(dst);
        dst->v = sb_detach(sb, &dst->len);
        dst->mem_pool = MEM_LIBC;
    }
}

int lstr_dlevenshtein(const lstr_t cs1, const lstr_t cs2, int max_dist)
{
    t_scope;
    lstr_t s1 = cs1;
    lstr_t s2 = cs2;
    int *cur, *prev, *prev2;

    if (s2.len > s1.len) {
        SWAP(lstr_t, s1, s2);
    }
    if (max_dist < 0) {
        max_dist = INT32_MAX;
    } else {
        THROW_ERR_IF(s1.len - s2.len > max_dist);
    }
    if (unlikely(!s2.len)) {
        return (s1.len <= max_dist) ? s1.len : -1;
    }

    cur   = t_new_raw(int, 3 * (s2.len + 1));
    prev  = cur + 1 * (s2.len + 1);
    prev2 = cur + 2 * (s2.len + 1);

    for (int j = 0; j <= s2.len; j++) {
        cur[j] = j;
    }

    for (int i = 0; i < s1.len; i++) {
        int  min_dist;
        int *tmp = prev2;

        prev2 = prev;
        prev  = cur;
        cur   = tmp;

        cur[0] = min_dist = i + 1;

        for (int j = 0; j < s2.len; j++) {
            int cost              = (s1.s[i] == s2.s[j]) ? 0 : 1;
            int deletion_cost     = prev[j + 1] + 1;
            int insertion_cost    =  cur[j    ] + 1;
            int substitution_cost = prev[j    ] + cost;

            cur[j + 1] = MIN3(deletion_cost, insertion_cost,
                              substitution_cost);

            if (i > 0 && j > 0
            &&  (s1.s[i    ] == s2.s[j - 1])
            &&  (s1.s[i - 1] == s2.s[j    ]))
            {
                int transposition_cost = prev2[j - 1] + cost;

                cur[j + 1] = MIN(cur[j + 1], transposition_cost);
            }

            min_dist = MIN(min_dist, cur[j + 1]);
        }

        THROW_ERR_IF(min_dist > max_dist);
    }

    return cur[s2.len];
}

int lstr_utf8_iendswith(const lstr_t *s1, const lstr_t *s2)
{
    SB_1k(sb1);
    SB_1k(sb2);

    RETHROW(sb_normalize_utf8(&sb1, s1->s, s1->len, true));
    RETHROW(sb_normalize_utf8(&sb2, s2->s, s2->len, true));

    return lstr_endswith(LSTR_SB_V(&sb1), LSTR_SB_V(&sb2));
}

int lstr_utf8_endswith(const lstr_t *s1, const lstr_t *s2)
{
    SB_1k(sb1);
    SB_1k(sb2);

    RETHROW(sb_normalize_utf8(&sb1, s1->s, s1->len, false));
    RETHROW(sb_normalize_utf8(&sb2, s2->s, s2->len, false));

    return lstr_endswith(LSTR_SB_V(&sb1), LSTR_SB_V(&sb2));
}
