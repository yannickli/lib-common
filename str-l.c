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
#include "core.h"

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

int lstr_utf8_iendswith(const lstr_t s1, const lstr_t s2)
{
    SB_1k(sb1);
    SB_1k(sb2);

    RETHROW(sb_normalize_utf8(&sb1, s1.s, s1.len, true));
    RETHROW(sb_normalize_utf8(&sb2, s2.s, s2.len, true));

    return lstr_endswith(LSTR_SB_V(&sb1), LSTR_SB_V(&sb2));
}

int lstr_utf8_endswith(const lstr_t s1, const lstr_t s2)
{
    SB_1k(sb1);
    SB_1k(sb2);

    RETHROW(sb_normalize_utf8(&sb1, s1.s, s1.len, false));
    RETHROW(sb_normalize_utf8(&sb2, s2.s, s2.len, false));

    return lstr_endswith(LSTR_SB_V(&sb1), LSTR_SB_V(&sb2));
}

/* {{{ SQL LIKE pattern matching */

/* XXX: This is copied from QDB, so that other daemons can use the
 * functionality without depending on QDB.
 *
 * XXX The behavior *should not be modified*.
 */

/* This function need only be called if c is in [0xC2...0xF4].
 * It completes the decoding of the current UTF-8 encoded code point
 * from a pstream.
 * - embedded '\0' are handled as normal characters
 * - on correctly encoded UTF-8 stream, it returns the code point and
 *   moves the pstream to the next position
 * - on invalid UTF-8 sequences, it leaves the pstream unchanged and
 * returns the initial byte value.
 */
static inline int ps_utf8_complete(int c, pstream_t *ps)
{
    int c1, c2, c3;

    switch (c) {
        /* 00...7F: US-ASCII */
        /* 80...BF: Non UTF-8 leading byte */
        /* C0...C1: Non canonical 2 byte UTF-8 encoding */
      case 0xC2 ... 0xDF:
        /* 2 byte UTF-8 sequence */
        if (ps_has(ps, 1)
        &&  (unsigned)(c1 = ps->b[0] - 0x80) < 0x40)
        {
            c = ((c & 0x3F) << 6) + c1;
            ps->b += 1;
        }
        break;

      case 0xE0 ... 0xEF:
        /* 3 byte UTF-8 sequence */
        if (ps_has(ps, 2)
        &&  (unsigned)(c1 = ps->b[0] - 0x80) < 0x40
        &&  (unsigned)(c2 = ps->b[1] - 0x80) < 0x40)
        {
            c = ((c & 0x3F) << 12) + (c1 << 6) + c2;
            ps->b += 2;
        }
        break;

      case 0xF0 ... 0xF4:
        /* 3 byte UTF-8 sequence */
        if (ps_has(ps, 3)
        &&  (unsigned)(c1 = ps->b[0] - 0x80) < 0x40
        &&  (unsigned)(c2 = ps->b[1] - 0x80) < 0x40
        &&  (unsigned)(c3 = ps->b[2] - 0x80) < 0x40)
        {
            c = ((c & 0x3F) << 18) + (c1 << 12) + (c2 << 6) + c3;
            ps->b += 3;
        }
        break;
        /* F5..F7: Start of a 4-byte sequence, Restricted by RFC 3629 */
        /* F8..FB: Start of a 5-byte sequence, Restricted by RFC 3629 */
        /* FC..FD: Start of a 6-byte sequence, Restricted by RFC 3629 */
        /* FE..FF: Invalid, non UTF-8 */
    }

    return c;
}

#define COLLATE_MASK      0xffff
#define COLLATE_SHIFT(c)  ((unsigned)(c) >> 16)

/* XXX: extracted from QDB (was named qdb_strlike), Do not change behavior! */
static bool ps_is_like(pstream_t ps, pstream_t pattern)
{
    pstream_t pattern0;
    int c1, c10, c11, c2, c20, c21;

    for (;;) {
        if (ps_done(&pattern)) {
            return ps_done(&ps);
        }
        c1 = __ps_getc(&pattern);
        if (c1 == '_') {
            if (ps_done(&ps)) {
                return false;
            }
            c2 = __ps_getc(&ps);
            if (c2 >= 0xC2) {
                ps_utf8_complete(c2, &ps);
            }
            continue;
        }
        if (c1 == '%') {
            for (;;) {
                if (ps_done(&pattern)) {
                    return true;
                }

                pattern0 = pattern;

                /* Check for non canonical pattern */
                c1 = __ps_getc(&pattern);
                if (c1 == '_') {
                    if (ps_done(&ps)) {
                        return false;
                    }
                    ps_utf8_complete(__ps_getc(&ps), &ps);
                    continue;
                }
                if (c1 == '%') {
                    continue;
                }
                if (c1 == '\\' && !ps_done(&pattern)) {
                    c1 = __ps_getc(&pattern);
                }

                c1 = ps_utf8_complete(c1, &pattern);
                c10 = c1;
                c11 = 0;
                if (c1 < countof(__str_unicode_general_ci)) {
                    int cc1 = __str_unicode_general_ci[c1];
                    c10 = cc1 & COLLATE_MASK;
                    c11 = COLLATE_SHIFT(cc1);
                }

                /* Simplistic recursive matcher */
                for (;;) {
                    pstream_t ps0 = ps;

                    if (ps_done(&ps)) {
                        return false;
                    }
                    c2 = __ps_getc(&ps);
                    if (c2 >= 0xC2) {
                        c2 = ps_utf8_complete(c2, &ps);
                    }
                    c20 = c2;
                    c21 = 0;
                    if (c2 < countof(__str_unicode_general_ci)) {
                        int cc2 = __str_unicode_general_ci[c2];
                        c20 = cc2 & COLLATE_MASK;
                        c21 = COLLATE_SHIFT(cc2);
                    }
                    if (c10 != c20) {
                        continue;
                    }
                    /* Handle dual collation */
                    if (c11 != c21) {
                        /* identical leading collation chars, but
                         * different dual collation: recurse
                         * without skipping */
                        if (ps_is_like(ps0, pattern0)) {
                            return true;
                        }
                        continue;
                    }
                    /* both large, single or dual and identical */
                    if (ps_is_like(ps, pattern)) {
                        return true;
                    }
                }
            }
        }
        if (c1 == '\\' && !ps_done(&pattern)) {
            c1 = __ps_getc(&pattern);
        }
        c1 = ps_utf8_complete(c1, &pattern);
        if (ps_done(&ps)) {
            return false;
        }
        c2 = __ps_getc(&ps);
        if (c2 >= 0xC2) {
            c2 = ps_utf8_complete(c2, &ps);
        }
        if (c1 == c2) {
            continue;
        }
        if ((c1 | c2) >= countof(__str_unicode_general_ci)) {
            /* large characters require exact match */
            break;
        }
        c1 = __str_unicode_general_ci[c1];
        c2 = __str_unicode_general_ci[c2];

      again:
        if ((c1 & COLLATE_MASK) != (c2 & COLLATE_MASK)) {
            break;
        }
        /* Handle dual collation */
        c1 = COLLATE_SHIFT(c1);
        c2 = COLLATE_SHIFT(c2);
        if ((c1 | c2) == 0) {
            /* both collation chars are single and identical */
            continue;
        }
        if (c1 == 0) {
            /* c2 is non zero */
            if (ps_done(&pattern)) {
                break;
            }
            c1 = __ps_getc(&pattern);
            if (c1 == '_' || c1 == '%' || c1 == '\\') {
                /* wildcards must fall on character boundaries */
                break;
            }
            c1 = ps_utf8_complete(c1, &pattern);
            if (c1 >= countof(__str_unicode_general_ci)) {
                /* large character cannot match c2 */
                break;
            }
            c1 = __str_unicode_general_ci[c1];
            goto again;
        } else
        if (c2 == 0) {
            /* c1 is non zero */
            if (ps_done(&ps)) {
                break;
            }
            c2 = ps_utf8_complete(__ps_getc(&ps), &ps);
            if (c2 >= countof(__str_unicode_general_ci)) {
                /* large character cannot match c1 */
                break;
            }
            c2 = __str_unicode_general_ci[c2];
            goto again;
        } else
        if (c1 == c2) {
            /* both collation chars are dual and identical */
            continue;
        } else {
            /* both are dual and different */
            break;
        }
    }
    return false;
}

#undef COLLATE_MASK
#undef COLLATE_SHIFT

bool lstr_utf8_is_ilike(const lstr_t s, const lstr_t pattern)
{
    return ps_is_like(ps_initlstr(&s), ps_initlstr(&pattern));
}

/* }}} */
