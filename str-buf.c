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

#include "net.h"
#include "unix.h"

__thread char __sb_slop[1];

char *sb_detach(sb_t *sb, int *len)
{
    char *s;

    if (len)
        *len = sb->len;
    if (sb->mem_pool != MEM_STATIC) {
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

void sb_reset(sb_t *sb)
{
    if (sb->mem_pool == MEM_LIBC && sb->skip + sb->size > (128 << 10)) {
        sb_wipe(sb);
        /* sb_init(sb) unneeded */
    } else {
        sb_init_full(sb, sb->data - sb->skip, 0, sb->size + sb->skip, sb->mem_pool);
        sb->data[0] = '\0';
    }
}

void sb_wipe(sb_t *sb)
{
    switch (sb->mem_pool & MEM_POOL_MASK) {
      case MEM_STATIC:
      case MEM_STACK:
        sb_reset(sb);
        return;
      default:
        ifree(sb->data - sb->skip, sb->mem_pool);
        sb_init(sb);
        return;
    }
}

/*
 * this function is meant to rewind any change on a sb in a function doing
 * repetitive appends that may fail.
 *
 * It cannot rewind a sb where anything has been skipped between the store and
 * the rewind. It assumes only appends have been performed.
 *
 */
int __sb_rewind_adds(sb_t *sb, const sb_t *orig)
{
    if (orig->mem_pool == MEM_STATIC && sb->mem_pool == MEM_LIBC) {
        sb_t tmp = *sb;
        int save_errno = errno;

        if (orig->skip) {
            sb_init_full(sb, orig->data - orig->skip, orig->len,
                         orig->size + orig->skip, MEM_STATIC);
            memcpy(sb->data, tmp.data, orig->len);
        } else {
            *sb = *orig;
            __sb_fixlen(sb, orig->len);
        }
        ifree(tmp.data - tmp.skip, MEM_LIBC);
        errno = save_errno;
    } else {
        __sb_fixlen(sb, orig->len);
    }
    return -1;
}

static void sb_destroy_skip(sb_t *sb)
{
    if (sb->skip == 0) {
        return;
    }

    memmove(sb->data - sb->skip, sb->data, sb->len + 1);
    sb->data -= sb->skip;
    sb->size += sb->skip;
    sb->skip  = 0;
}

void __sb_grow(sb_t *sb, int extra)
{
    int newlen = sb->len + extra;
    int newsz;

    if (unlikely(newlen < 0))
        e_panic("trying to allocate insane amount of memory");

    /* if data fits and skip is worth it, shift it left */
    if (newlen < sb->skip + sb->size && sb->skip > sb->size / 4) {
        sb_destroy_skip(sb);
        return;
    }

    /* most of our pool have expensive reallocs wrt a typical memcpy,
     * and optimize the last realloc so we don't want to alloc and free
     */
    if (sb->mem_pool != MEM_LIBC) {
        sb_destroy_skip(sb);
        if (newlen < sb->size)
            return;
    }

    newsz = p_alloc_nr(sb->size + sb->skip);
    if (newsz < newlen + 1)
        newsz = newlen + 1;
    if (sb->mem_pool == MEM_STATIC || sb->skip) {
        char *s = p_new_raw(char, newsz);

        memcpy(s, sb->data, sb->len + 1);
        if (sb->mem_pool != MEM_STATIC) {
            /* XXX: If we have sb->skip, then mem_pool == MEM_LIBC */
            libc_free(sb->data - sb->skip, 0);
        }
        sb_init_full(sb, s, sb->len, newsz, MEM_LIBC);
    } else {
        sb->data = irealloc(sb->data, sb->len + 1, newsz,
                            sb->mem_pool | MEM_RAW);
        sb->size = newsz;
    }
}

char *__sb_splice(sb_t *sb, int pos, int len, int dlen)
{
    assert (pos >= 0 && len >= 0 && dlen >= 0);
    assert (pos <= sb->len && pos + len <= sb->len);

    if (len >= dlen) {
        p_move2(sb->data, pos + dlen, pos + len, sb->len - pos - len);
        __sb_fixlen(sb, sb->len + dlen - len);
    } else
    if (len + sb->skip >= dlen) {
        sb->skip -= dlen - len;
        sb->data -= dlen - len;
        sb->size += dlen - len;
        sb->len  += dlen - len;
        p_move2(sb->data, 0, dlen - len, pos);
    } else {
        sb_grow(sb, dlen - len);
        p_move2(sb->data, pos + dlen, pos + len, sb->len - pos - len);
        __sb_fixlen(sb, sb->len + dlen - len);
    }
    return sb->data + pos;
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
    int64_t start = ftell(f);
    sb_t orig = *sb;

    do {
        int64_t end;
        char *buf = sb_grow(sb, BUFSIZ);

        if (!fgets(buf, sb_avail(sb) + 1, f))
            break;

        end = ftell(f);
        if (start != -1 && end != -1) {
            sb->len += (end - start);
        } else {
            sb->len += strlen(buf);
        }
        start = end;
    } while (sb->data[sb->len - 1] != '\n');

    if (ferror(f))
        return __sb_rewind_adds(sb, &orig);
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
    if (res < 0)
        return __sb_rewind_adds(sb, &orig);

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
    int res, fd, err;
    char *buf;

    fd = RETHROW(open(filename, O_RDONLY));
    if (fstat(fd, &st) < 0 || st.st_size <= 0) {
        for (;;) {
            res = sb_read(sb, fd, 0);
            if (res < 0) {
                __sb_rewind_adds(sb, &orig);
                goto end;
            }
            if (res == 0) {
                res = sb->len - orig.len;
                goto end;
            }
        }
    }

    if (st.st_size > INT_MAX) {
        errno = ENOMEM;
        res = -1;
        goto end;
    }

    res = st.st_size;
    buf = sb_growlen(sb, res);
    if (xread(fd, buf, res) < 0) {
        res = __sb_rewind_adds(sb, &orig);
    }

end:
    err = errno;
    close(fd);
    errno = err;
    return res;
}

int sb_write_file(const sb_t *sb, const char *filename)
{
    return xwrite_file(filename, sb->data, sb->len);
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
    if (res < 0)
        return __sb_rewind_adds(sb, &orig);
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
    if (res < 0)
        return __sb_rewind_adds(sb, &orig);
    __sb_fixlen(sb, sb->len + res);
    return res;
}

int sb_recv(sb_t *sb, int fd, int hint, int flags)
{
    return sb_recvfrom(sb, fd, hint, flags, NULL, NULL);
}
