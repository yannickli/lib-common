/**************************************************************************/
/*                                                                        */
/*  Copyright (C) 2004-2008 INTERSEC SAS                                  */
/*                                                                        */
/*  Should you receive a copy of this source code, you must check you     */
/*  have a proper, written authorization of INTERSEC to hold it. If you   */
/*  don't have such an authorization, you must DELETE all source code     */
/*  files in your possession, and inform INTERSEC of the fact you obtain  */
/*  these files. Should you not comply to these terms, you can be         */
/*  prosecuted in the extent permitted by applicable law.                 */
/*                                                                        */
/**************************************************************************/

#include "net.h"

char __sb_slop[1];

char *sb_detach(sb_t *sb, int *len)
{
    char *s;

    if (len)
        *len = sb->len;
    if (sb->must_free) {
        if (sb->skip)
            memmove(sb->data - sb->skip, sb->data, sb->len + 1);
        s = sb->data - sb->skip;
    } else {
        s = p_dupz(sb->data, sb->len);
    }
    sb_wipe(sb);
    return s;
}

void __sb_rewind_adds(sb_t *sb, const sb_t *orig)
{
    if (!orig->must_free && sb->must_free) {
        mem_free(sb->data - sb->skip);
        *sb = *orig;
        __sb_fixlen(sb, sb->len);
    } else {
        __sb_fixlen(sb, orig->len);
    }
}

void __sb_grow(sb_t *sb, int extra)
{
    int newlen = sb->len + extra;
    int newsz;

    if (unlikely(newlen < 0))
        e_panic("trying to allocate insane amount of memory");

    /* if data fits and skip is worth it, shift it left */
    if (newlen < sb->skip + sb->size && sb->skip > sb->size / 4) {
        memmove(sb->data - sb->skip, sb->data, sb->len + 1);
        sb->data -= sb->skip;
        sb->size += sb->skip;
        sb->skip  = 0;
        return;
    }

    newsz = p_alloc_nr(sb->size + sb->skip);
    if (newsz < newlen + 1)
        newsz = newlen + 1;
    if (sb->must_free && !sb->skip) {
        p_realloc(&sb->data, sb->size = newsz);
    } else {
        char *s = p_new_raw(char, newsz);

        memcpy(s, sb->data, sb->len + 1);
        if (sb->must_free)
            mem_free(sb->data - sb->skip);
        sb_init_full(sb, s, sb->len, newsz, true);
    }
}

void __sb_splice(sb_t *sb, int pos, int len, const void *data, int dlen)
{
    assert (pos >= 0 && len >= 0 && dlen >= 0);
    assert (pos <= sb->len && pos + len <= sb->len);

    if (len >= dlen) {
        p_move(sb->data, pos + dlen, pos + len, sb->len - pos - len);
    } else
    if (len + sb->skip >= dlen) {
        sb->skip -= dlen - len;
        sb->data -= dlen - len;
        sb->size += dlen - len;
        sb->len  += dlen - len;
        p_move(sb->data, 0, dlen - len, pos);
    } else {
        sb_grow(sb, dlen - len);
        p_move(sb->data, pos + dlen, pos + len, sb->len - pos - len);
        __sb_fixlen(sb, sb->len + dlen - len);
    }
    if (data)
        memcpy(sb->data + pos, data, dlen);
}

int sb_getline(sb_t *sb, FILE *f)
{
    sb_t orig = *sb;

    do {
        char *buf = sb_grow(sb, BUFSIZ);

        if (!fgets(buf, sb_avail(sb) + 1, f))
            break;

        sb->len += strlen(buf);
    } while (sb->data[sb->len - 1] == '\n');

    if (ferror(f)) {
        __sb_rewind_adds(sb, &orig);
        return -1;
    }
    return 0;
}

int sb_read(sb_t *sb, int fd, int hint)
{
    sb_t orig = *sb;
    char *buf;
    int   res;

    buf = sb_grow(sb, hint <= 0 ? BUFSIZ : hint);
    res = read(fd, buf, sb_avail(sb));
    if (res < 0) {
        __sb_rewind_adds(sb, &orig);
        return res;
    }
    __sb_fixlen(sb, sb->len + res);
    return res;
}

int sb_recvfrom(sb_t *sb, int fd, int hint, int flags,
                struct sockaddr *addr, socklen_t *alen)
{
    sb_t orig = *sb;
    char *buf;
    int   res;

    buf = sb_grow(sb, hint <= 0 ? BUFSIZ : hint);
    res = recvfrom(fd, buf, sb_avail(sb), flags, addr, alen);
    if (res < 0) {
        __sb_rewind_adds(sb, &orig);
        return res;
    }
    __sb_fixlen(sb, sb->len + res);
    return res;
}

int sb_recv(sb_t *sb, int fd, int hint, int flags)
{
    return sb_recvfrom(sb, fd, hint, flags, NULL, NULL);
}
