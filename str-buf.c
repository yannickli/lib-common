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
#include "unix.h"

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
        sb_init(sb);
    } else {
        s = p_dupz(sb->data, sb->len);
        sb_reset(sb);
    }
    return s;
}

/*
 * this function is meant to rewind any change on a sb in a function doing
 * repetitive appends that may fail.
 *
 * It cannot rewind a sb where anything has been skipped between the store and
 * the rewind. It assumes only appends have been performed.
 *
 */
void __sb_rewind_adds(sb_t *sb, const sb_t *orig)
{
    if (!orig->must_free && sb->must_free) {
        sb_t tmp = *sb;
        if (orig->skip) {
            sb_init_full(sb, orig->data - orig->skip, orig->len,
                         orig->size + orig->skip, false);
            memcpy(sb->data, tmp.data, orig->len);
        } else {
            *sb = *orig;
            __sb_fixlen(sb, orig->len);
        }
        mem_free(tmp.data - tmp.skip);
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

    if (newlen < sb->skip + sb->size) {
        /* if data fits and skip is worth it, shift it left */
        if (!sb->must_free || sb->skip > sb->size / 4) {
            memmove(sb->data - sb->skip, sb->data, sb->len + 1);
            sb->data -= sb->skip;
            sb->size += sb->skip;
            sb->skip  = 0;
            return;
        }
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
        __sb_fixlen(sb, sb->len + dlen - len);
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

/**************************************************************************/
/* str/mem-functions wrappers                                             */
/**************************************************************************/

int sb_search(const sb_t *sb, int pos, const void *what, int wlen)
{
    const char *p = memsearch(sb->data + pos, sb->len - pos, what, wlen);
    return p ? p - sb->data : -1;
}


/**************************************************************************/
/* printf function                                                        */
/**************************************************************************/

int sb_addvf(sb_t *sb, const char *fmt, va_list ap)
{
    va_list ap2;
    int len;

    va_copy(ap2, ap);
    len = ivsnprintf(sb_end(sb), sb_avail(sb) + 1, fmt, ap2);
    va_end(ap2);

    if (len < sb_avail(sb)) {
        __sb_fixlen(sb, sb->len + len);
    } else {
        ivsnprintf(sb_growlen(sb, len), len + 1, fmt, ap);
    }
    return len;
}

int sb_addf(sb_t *sb, const char *fmt, ...)
{
    int res;
    va_list args;

    va_start(args, fmt);
    res = sb_addvf(sb, fmt, args);
    va_end(args);

    return res;
}

/**************************************************************************/
/* FILE *                                                                 */
/**************************************************************************/

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

/* OG: returns the number of elements actually appended to the sb,
 * -1 if error
 */
int sb_fread(sb_t *sb, int size, int nmemb, FILE *f)
{
    sb_t orig = *sb;
    int   res = size * nmemb;
    char *buf = sb_grow(sb, size * nmemb);

    if (unlikely(((long long)size * (long long)nmemb) != res))
        e_panic("trying to allocate insane amount of memory");

    res = fread(buf, size, nmemb, f);
    if (res < 0) {
        __sb_rewind_adds(sb, &orig);
        return res;
    }
    __sb_fixlen(sb, sb->len + res * size);
    return res;
}

/* Return the number of bytes appended to the sb, negative value
 * indicates an error.
 * OG: this function insists on reading a complete file.  If the file
 * cannot be read completely, no data is kept in the sb and an error
 * is returned.
 */
int sb_read_file(sb_t *sb, const char *filename)
{
    sb_t orig = *sb;
    struct stat st;
    int fd, origlen = sb->len;
    char *buf;

    origlen = sb->len;

    if ((fd = open(filename, O_RDONLY)) < 0)
        return -1;

    if (fstat(fd, &st) < 0 || st.st_size <= 0) {
        for (;;) {
            int res = sb_read(sb, fd, 0);
            if (res < 0) {
                __sb_rewind_adds(sb, &orig);
                return -1;
            }
            if (res == 0)
                return sb->len - origlen;
        }
    }

    if (st.st_size > INT_MAX) {
        errno = ENOMEM;
        return -1;
    }

    buf = sb_grow(sb, st.st_size);
    if (xread(fd, buf, st.st_size) < 0) {
        __sb_rewind_adds(sb, &orig);
        return -1;
    }
    __sb_fixlen(sb, sb->len + st.st_size);
    return st.st_size;
}

/* Return number of bytes written or -1 on error */
int sb_write_file(const sb_t *sb, const char *filename)
{
    int fd, res;

    if ((fd = open(filename, O_WRONLY | O_CREAT | O_TRUNC, 0644)) < 0)
        return -1;

    res = xwrite(fd, sb->data, sb->len);
    if (res < 0) {
        int save_errno = errno;
        unlink(filename);
        close(fd);
        errno = save_errno;
        return -1;
    }
    close(fd);
    return sb->len;
}


/**************************************************************************/
/* fd and sockets                                                         */
/**************************************************************************/

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
