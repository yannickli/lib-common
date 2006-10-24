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

#include <assert.h>
#include <stdlib.h>
#include <errno.h>
#include <sys/param.h>
#include <sys/stat.h>
#include <unistd.h>
#include <stdarg.h>
#include <stdio.h>
#include <fcntl.h>

#include "macros.h"
#include "blob.h"
#include "blob_time.h"
#include "mem.h"
#include "string_is.h"
#include "err_report.h"

#define IPRINTF_HIDE_STDIO 1
#include "iprintf.h"

/**************************************************************************/
/* Blob creation / deletion                                               */
/**************************************************************************/

/* @see strdup(3) */
blob_t *blob_dup(const blob_t *src)
{
    blob_t *dst = p_new_raw(blob_t, 1);

    dst->len = src->len;

    if (src->len < BLOB_INITIAL_SIZE) {
        dst->size = BLOB_INITIAL_SIZE;
        dst->area = NULL;
        dst->data = dst->initial;
    } else {
        dst->size = MEM_ALIGN(src->len + 1);
        dst->area = p_new_raw(byte, dst->size);
        dst->data = dst->area;
    }

    /* Copy the blob data including the trailing '\0' */
    memcpy(dst->data, src->data, src->len + 1);

    return dst;
}

/* FIXME: this function is nasty, and has bad semantics: blob should know if
 * it owns the buffer. This makes the programmer write really horrible code
 * atm ==> NEVER SET THAT PUBLIC
 */
/* Set the payload of a blob to the given buffer of size bufsize.
 * len is the len of the data inside it.
 *
 * The payload MUST be a valid block allocated through malloc or an
 * alias.
 *
 * Should have a extra parameter telling the blob if it owns buf and
 * must free it with p_delete upon resize and wipe.
 */
static
void blob_set_payload(blob_t *blob, ssize_t len, void *buf, ssize_t bufsize)
{
    assert (bufsize >= len + 1);

    p_delete(&blob->area);
    blob->area = blob->data = buf;
    blob->len  = len;
    blob->size = bufsize;
    blob->data[len] = '\0';
}

/*
 * Resize a blob to a new length, preserving its contents upto the
 * smallest of the newlen and oldlen.
 * The byte at newlen is forced to '\0'.
 * If blob is shrunk, memory is not freed back to the system.
 * If blob is extended, its contents between oldlen and newlen is
 * undefined.
 */
void blob_resize_real(blob_t *blob, ssize_t newlen)
{
    ssize_t newsize = MEM_ALIGN(newlen + 1);

    if (blob->data == blob->area) {
        blob->area = mem_realloc(blob->area, newsize);
        blob->data = blob->area;
        blob->size = newsize;
    } else {
        /* Check if data fits in current area */
        byte *area = blob->area ? blob->area : blob->initial;
        ssize_t skip = blob->data - area;

        if (newsize <= skip + blob->size) {
            /* Data fits in the current area, shift it left */
            memmove(blob->data - skip, blob->data, blob->len + 1);
            blob->data -= skip;
            blob->size += skip;
        } else {
            /* Allocate a new area */
            byte *new_area = p_new_raw(byte, newsize);
            /* Copy the blob data including the trailing '\0' */
            memcpy(new_area, blob->data, blob->len + 1);
            p_delete(&blob->area);
            blob->area = new_area;
            blob->data = blob->area;
            blob->size = newsize;
        }
    }
    blob->len = newlen;
    blob->data[blob->len] = '\0';
}

/* Inline version for special case of clearing blob */
static inline void blob_empty(blob_t *blob)
{
    /* Remove initial skip if any */
    if (blob->area) {
        blob->size += (blob->data - blob->area);
        blob->data = blob->area;
    } else {
        blob->size = BLOB_INITIAL_SIZE;
        blob->data = blob->initial;
    }
    blob->len = 0;
    blob->data[0] = '\0';
}

/**************************************************************************/
/* Blob manipulations                                                     */
/**************************************************************************/

/*** private inlines ***/

/* data and blob->data must not overlap! */
static inline void
blob_blit_data_real(blob_t *blob, ssize_t pos, const void *data, ssize_t len)
{
    if (pos < 0) {
        return;
    }
    if (len <= 0) {
        return;
    }
    if (pos > blob->len) {
        pos = blob->len;
    }
    if (pos + len > blob->len) {
        blob_resize(blob, pos + len);
    }
    /* This will fail if data and blob->data overlap */
    memcpy(blob->data + pos, data, len);
}

/* Insert `len' data C bytes into a blob.
 * `pos' gives the position in `blob' where `data' should be inserted.
 *
 * If the given `pos' is greater than the length of the blob data,
 * the data is appended.
 *
 * data and blob->data must not overlap!
 */
static inline void
blob_insert_data_real(blob_t *blob, ssize_t pos, const void *data, ssize_t len)
{
    ssize_t oldlen = blob->len;

    if (pos < 0) {
        pos = 0;
    }
    if (len <= 0) {
        return;
    }
    if (pos > oldlen) {
        pos = oldlen;
    }

    blob_resize(blob, oldlen + len);
    if (pos < oldlen) {
        memmove(blob->data + pos + len, blob->data + pos, oldlen - pos);
    }
    memcpy(blob->data + pos, data, len);
}

void blob_kill_data(blob_t *blob, ssize_t pos, ssize_t len)
{
    if (pos < 0) {
        len += pos;
        pos = 0;
    }
    if (len <= 0) {
        return;
    }
    if (pos >= blob->len) {
        return;
    }

    if (pos + len >= blob->len) {
        /* Simple case: we are truncating the blob at the end */
        blob->len = pos;
        blob->data[blob->len] = '\0';
    } else
    if (pos == 0) {
        /* Simple case: chopping bytes from the beginning:
         * we increase the initial skip.  We already checked that
         * len < blob->len.
         */
        blob->data += len;
        blob->size -= len;
        blob->len  -= len;
    } else {
        /* General case: shift the blob data */
        /* We could improve speed by moving the smaller of the left and
         * right parts, but not as issue for now.
         */
        /* move the blob data including the trailing '\0' */
        memmove(blob->data + pos, blob->data + pos + len,
                blob->len - pos - len + 1);
        blob->len  -= len;
    }
}

/*** set functions ***/

void blob_set(blob_t *blob, const blob_t *src)
{
    blob_empty(blob);
    blob_blit_data_real(blob, 0, src->data, src->len);
}

void blob_set_data(blob_t *blob, const void *data, ssize_t len)
{
    blob_empty(blob);
    blob_blit_data_real(blob, 0, data, len);
}

/*** blit functions ***/

void blob_blit(blob_t *dest, ssize_t pos, const blob_t *src)
{
    blob_blit_data_real(dest, pos, src->data, src->len);
}

void blob_blit_data(blob_t *blob, ssize_t pos, const void *data, ssize_t len)
{
    blob_blit_data_real(blob, pos, data, len);
}

/*** insert functions ***/

void blob_insert(blob_t *dest, ssize_t pos, const blob_t *src)
{
    blob_insert_data_real(dest, pos, src->data, src->len);
}

void blob_insert_data(blob_t *blob, ssize_t pos, const void *data, ssize_t len)
{
    blob_insert_data_real(blob, pos, data, len);
}

void blob_insert_byte(blob_t *blob, byte b)
{
    blob_insert_data_real(blob, 0, &b, 1);
}

/*** append functions ***/

void blob_append(blob_t *dest, const blob_t *src)
{
    blob_insert_data_real(dest, dest->len, src->data, src->len);
}

void blob_append_data(blob_t *blob, const void *data, ssize_t len)
{
    blob_insert_data_real(blob, blob->len, data, len);
}

/**************************************************************************/
/* Blob file functions                                                    */
/**************************************************************************/

/* Return the number of bytes appended to the blob, negative value
 * indicates an error.
 * OG: this function insists on reading a complete file.  If the file
 * size cannot be determined or if the file cannot be read accordingly,
 * no data is kept in the blob and an error is returned
 */
ssize_t blob_append_file_data(blob_t *blob, const char *filename)
{
    const ssize_t origlen = blob->len;

    int fd;
    ssize_t total;
    ssize_t to_read;
    struct stat st;

    if ((fd = open(filename, O_RDONLY)) < 0) {
        goto error;
    }

    if (fstat(fd, &st) < 0) {
        goto error;
    }

    /* Should test and reject huge files */
    to_read = total = (ssize_t)st.st_size;
    if (total < 0) {
        /* OG: size of file is not known: may be a pipe or a device,
         * should not be an error.
         */
        goto error;
    }

    /* force allocation */
    blob_resize(blob, origlen + total);
    blob_resize(blob, origlen);

    while (to_read > 0) {
        ssize_t size;
        char buf[4096];

        if ((size = read(fd, buf, sizeof(buf))) < 0) {
            if (errno == EINTR) {
                continue;
            }
            goto error;
        }

        if (size == 0) {
            /* OG: cannot combine with test above because errno is only
             * set on errors, and end of file is not a read error.
             * Getting an early end of file here is indeed an error.
             */
            goto error;
        }

        blob_append_data(blob, buf, size);
        to_read -= size;
    }

    return total;

  error:
    if (fd >= 0) {
        close(fd);
        /* OG: maybe we should keep data read so far */
        blob_resize(blob, origlen);
    }
    return -1;
}

/* OG: returns the number of elements actually appended to the blob,
 * -1 if error
 */
ssize_t blob_append_fread(blob_t *blob, ssize_t size, ssize_t nmemb, FILE *f)
{
    const ssize_t oldlen = blob->len;
    ssize_t total = size * nmemb;
    ssize_t res;

    blob_resize(blob, oldlen + total);

    res = fread(blob->data + oldlen, size, nmemb, f);
    if (res < 0) {
        blob_resize(blob, oldlen);
        return res;
    }

    if (res < nmemb) {
        blob_resize(blob, oldlen + res * size);
    }

    return res;
}

ssize_t blob_append_read(blob_t *blob, int fd, ssize_t count)
{
    const ssize_t oldlen = blob->len;
    ssize_t res;

    if (count < 0)
        count = BUFSIZ;

    blob_resize(blob, oldlen + count);

    res = read(fd, blob->data + oldlen, count);
    if (res < 0) {
        blob_resize(blob, oldlen);
        return res;
    }

    if (res < count) {
        blob_resize(blob, oldlen + res);
    }

    return res;
}

/**************************************************************************/
/* Blob printf function                                                   */
/**************************************************************************/

ssize_t blob_append_vfmt(blob_t *blob, const char *fmt, va_list ap)
{
    ssize_t pos = blob->len;
    int len;
    int available;
    va_list ap2;

    va_copy(ap2, ap);

    available = blob->size - pos;

    len = vsnprintf((char *)(blob->data + pos), available, fmt, ap);
    if (len < 0) {
        len = 0;
    }
    if (len >= available) {
        /* only move the `pos' first bytes in case of realloc */
        blob->len = pos;
        blob_resize(blob, pos + len);
        available = blob->size - pos;

        len = vsnprintf((char*)(blob->data + pos), available, fmt, ap2);
        if (len >= available) {
            /* Defensive programming (old libc) */
            len = available - 1;
        }
    }
    blob->len = pos + len;
    blob->data[blob->len] = '\0';

    return len;
}

/* returns the number of bytes written.
   note that blob_append_fmt always works (or never returns due to memory
   allocation */
ssize_t blob_append_fmt(blob_t *blob, const char *fmt, ...)
{
    ssize_t res;
    va_list args;

    va_start(args, fmt);
    res = blob_append_vfmt(blob, fmt, args);
    va_end(args);

    return res;
}

ssize_t blob_set_fmt(blob_t *blob, const char *fmt, ...)
{
    ssize_t res;
    va_list args;

    va_start(args, fmt);
    blob_resize(blob, 0);
    res = blob_append_vfmt(blob, fmt, args);
    va_end(args);

    return res;
}

ssize_t blob_set_vfmt(blob_t *blob, const char *fmt, va_list ap)
{
    ssize_t res;

    blob_resize(blob, 0);
    res = blob_append_vfmt(blob, fmt, ap);

    return res;
}

/* Returns the number of bytes written.
 *
 * A negative value indicates an error, without much precision
 * (presumably not enough space in the internal buffer, and such an error
 * is permanent. The 1 KB buffer should suffice.
 */
ssize_t blob_strftime(blob_t *blob, ssize_t pos, const char *fmt,
                      const struct tm *tm)
{
    /* A 1 KB buffer should suffice */
    char buffer[1024];
    size_t res;

    if (pos > blob->len) {
        pos = blob->len;
    }

    /* Detecting overflow in strftime cannot be done reliably:  older
     * versions of the glibc returned bufsize upon overflow, while
     * newer ones follow the norm and return 0.  An unlucky choice
     * since valid time conversions can have 0 length, including non
     * trivial ones such as %p.
     * In case of overflow, we leave the blob unchanged as the contents
     * of the buffer is undefined.
     */
    res = strftime(buffer, sizeof(buffer), fmt, tm);
    if (res > 0 && res < sizeof(buffer)) {
        blob_resize(blob, pos);
        blob_blit_data_real(blob, pos, buffer, res);
        return res;
    }

    return -1;
}

/**************************************************************************/
/* Blob search functions                                                  */
/**************************************************************************/

static inline ssize_t
blob_search_data_real(const blob_t *haystack, ssize_t pos,
                      const void *needle, ssize_t len)
{
    const byte *p;

    /* OG: should validate pos against blob bounds */
    if (pos < 0) {
        pos = 0;
    }
    if (len < 0 || pos + len > haystack->len)
        return -1;

    p = memsearch(haystack->data + pos, haystack->len - pos, needle, len);
    if (!p) {
        return -1;
    }
    return p - haystack->data;
}

/* not very efficent ! */

ssize_t blob_search(const blob_t *haystack, ssize_t pos, const blob_t *needle)
{
    return blob_search_data_real(haystack, pos, needle->data, needle->len);
}

ssize_t blob_search_data(const blob_t *haystack, ssize_t pos,
                         const void *needle, ssize_t len)
{
    return blob_search_data_real(haystack, pos, needle, len);
}

/**************************************************************************/
/* Blob filtering                                                         */
/**************************************************************************/

/* map filter to blob->data[start .. end-1]
   beware that blob->data[end] is not modified !
 */

static inline void
blob_map_range_real(blob_t *blob, ssize_t start, ssize_t end,
                    blob_filter_func_t *filter)
{
    ssize_t i;

    for (i = start; i < end; i++) {
        blob->data[i] = (*filter)(blob->data[i]);
    }
}


void blob_map(blob_t *blob, blob_filter_func_t filter)
{
    blob_map_range_real(blob, 0, blob->len, filter);
}

void blob_map_range(blob_t *blob, ssize_t start, ssize_t end,
                    blob_filter_func_t *filter)
{
    blob_map_range_real(blob, start, end, filter);
}


void blob_ltrim(blob_t *blob)
{
    ssize_t i;

    for (i = 0; i < blob->len; i++) {
        if (!isspace(blob->data[i]))
            break;
    }
    blob_kill_data(blob, 0, i);
}

void blob_rtrim(blob_t *blob)
{
    ssize_t i;

    for (i = blob->len; i > 0; i--) {
        if (!isspace(blob->data[i - 1]))
            break;
    }
    blob_kill_data(blob, i, blob->len - i);
}

void blob_trim(blob_t *blob)
{
    blob_ltrim(blob);
    blob_rtrim(blob);
}

/**************************************************************************/
/* Blob comparisons                                                       */
/**************************************************************************/

/* @see memcmp(3) */
int blob_cmp(const blob_t *blob1, const blob_t *blob2)
{
    ssize_t len = MIN(blob1->len, blob2->len);
    int res = memcmp(blob1->data, blob2->data, len);
    if (res != 0) {
        return res;
    }
    return (blob1->len - blob2->len);
}

int blob_icmp(const blob_t *blob1, const blob_t *blob2)
{
    ssize_t len = MIN(blob1->len, blob2->len);
    ssize_t pos;

    const char *s1 = (const char *)blob1->data;
    const char *s2 = (const char *)blob2->data;

    for (pos = 0; pos < len; pos++) {
        int res = tolower(s1[pos]) - tolower(s2[pos]);
        if (res != 0) {
            return res;
        }
    }
    return (blob1->len - blob2->len);
}

bool blob_is_equal(const blob_t *blob1, const blob_t *blob2)
{
    if (blob1 == blob2)
        return true;

    if (blob1->len != blob2->len) {
        return false;
    }
    return (memcmp(blob1->data, blob2->data, blob1->len) == 0);
}

bool blob_is_iequal(const blob_t *blob1, const blob_t *blob2)
{
    ssize_t i;

    if (blob1 == blob2)
        return true;

    if (blob1->len != blob2->len) {
        return false;
    }

    /* Compare from the end because we deal with a lot of strings with
     * long identical initial portions.  (OG: not a general assumption)
     */
    for (i = blob1->len; --i >= 0; ) {
        if (tolower(blob1->data[i]) != tolower(blob2->data[i])) {
            return false;
        }
    }
    return true;
}

bool blob_cstart(const blob_t *blob, const char *p, const char **pp)
{
    return strstart((const char *)blob->data, p, pp);
}

bool blob_start(const blob_t *blob1, const blob_t *blob2, const byte **pp)
{
    return blob_cstart(blob1, (const char*)blob2->data, (const char **)pp);
}

bool blob_cistart(const blob_t *blob, const char *p, const char **pp)
{
    return stristart((const char *)blob->data, p, pp);
}

bool blob_istart(const blob_t *blob1, const blob_t *blob2, const byte **pp)
{
    return blob_cistart(blob1, (const char*)blob2->data, (const char **)pp);
}

/**************************************************************************/
/* Blob string functions                                                  */
/**************************************************************************/

static inline int hex_to_dec(char c)
{
    if (c >= '0' && c <= '9') {
        return c - '0';
    }
    if (c >= 'a' && c <= 'f') {
        return c + 10 - 'a';
    }
    if (c >= 'A' && c <= 'F') {
        return c + 10 - 'A';
    }
    return -1;
}

void blob_urldecode(blob_t *url)
{
    byte *p, *q;

    /* This function relies on the final NUL at the end of the blob
     * and will stop on any embedded NULs.
     */

    /* Optimize the general case with a quick scan for % */
    if ((p = memchr(url->data, '%', url->len)) == NULL)
        return;

    q = p;
    while ((*q = *p) != '\0') {
        int a, b;
        if (*p == '%'
        &&  (a = hex_to_dec(p[1])) >= 0
        &&  (b = hex_to_dec(p[2])) >= 0)
        {
            *q++ = (a << 4) | b;
            p += 3;
        } else {
            q++;
            p++;
        }
    }
    url->len = q - url->data;
}

static const char
b64[] = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";

static inline ssize_t b64_size(ssize_t oldlen, int nbpackets)
{
    if (nbpackets > 0) {
        ssize_t nb_full_lines = oldlen / (3 * nbpackets);
        ssize_t lastlen = oldlen % (3 * nbpackets);

        if (lastlen % 3) {
            lastlen += 3 - (lastlen % 3);
        }
        lastlen = lastlen * 4 / 3;
        if (lastlen) {
            lastlen += 2; /* crlf */
        }

        return nb_full_lines * (4 * nbpackets + 2) + lastlen;
    } else {
        return 4 * ((oldlen + 2) / 3);
    }
}

void blob_b64encode(blob_t *blob, int nbpackets)
{
    const ssize_t oldlen  = blob->len;
    const ssize_t newlen  = b64_size(oldlen, nbpackets);
    const ssize_t newsize = MEM_ALIGN(newlen + 1);

    int     packs   = nbpackets;
    byte    *buf    = p_new_raw(byte, newsize);
    byte    *dst    = buf;
    const byte *src = blob->data;
    const byte *end = blob->data + blob->len;

    while (src < end) {
        int c1, c2, c3;

        c1       = *(src++);
        *(dst++) = b64[c1 >> 2];

        if (src == end) {
            *(dst++) = b64[((c1 & 0x3) << 4)];
            *(dst++) = '=';
            *(dst++) = '=';
            if (nbpackets > 0) {
                *(dst++) = '\r';
                *(dst++) = '\n';
            }
            break;
        }

        c2       = *(src++);
        *(dst++) = b64[((c1 & 0x3) << 4) | ((c2 & 0xf0) >> 4)];

        if (src == end) {
            *(dst++) = b64[((c2 & 0x0f) << 2)];
            *(dst++) = '=';
            if (nbpackets > 0) {
                *(dst++) = '\r';
                *(dst++) = '\n';
            }
            break;
        }

        c3 = *(src++);
        *(dst++) = b64[((c2 & 0x0f) << 2) | ((c3 & 0xc0) >> 6)];
        *(dst++) = b64[c3 & 0x3f];

        if (nbpackets > 0 && (!--packs || src == end)) {
            packs = nbpackets;
            *(dst++) = '\r';
            *(dst++) = '\n';
        }
    }

#ifdef DEBUG
    assert (dst == buf + newlen);
#endif

    blob_set_payload(blob, newlen, buf, newsize);
}

/**************************************************************************/
/* Blob parsing                                                           */
/**************************************************************************/

/* try to parse a c-string from the current position in the buffer.

   pos will be incremented to the position right after the NUL
   answer is a pointer *in* the blob, so you have to strdup it if needed.

   @returns :

     positive number or 0
       represent the length of the parsed string (not
       including the leading \0)

     PARSE_EPARSE
       no \0 was found before the end of the blob
 */

ssize_t blob_parse_cstr(const blob_t *blob, ssize_t *pos, const char **answer)
{
    ssize_t walk = *pos;

    while (walk < blob->len) {
        if (blob->data[walk] == '\0') {
            ssize_t len = walk - *pos;
            PARSE_SET_RESULT(answer, (char *)(blob->data + *pos));
            *pos = walk + 1;
            return len;
        }
        walk ++;
    }

    return PARSE_EPARSE;
}

/*---------------- blob conversion stuff ----------------*/

#define QP  1
#define XP  2

byte const __str_encode_flags[128 + 256] = {
#define REPEAT16(x) x,x,x,x,x,x,x,x,x,x,x,x,x,x,x,x
    REPEAT16(0), REPEAT16(0), REPEAT16(0), REPEAT16(0),
    REPEAT16(0), REPEAT16(0), REPEAT16(0), REPEAT16(0),
    0,     0,     0,     0,     0,     0,     0,     0,
    0,     0,     QP,    0,     0,     QP,    0,     0,
    REPEAT16(0),
    0,     QP,    XP|QP, QP,    QP,    QP,    XP|QP, XP|QP,
    QP,    QP,    QP,    QP,    QP,    QP,    QP,    QP,
    QP,    QP,    QP,    QP,    QP,    QP,    QP,    QP,
    QP,    QP,    QP,    QP,    XP|QP, 0,     XP|QP, QP,  /* = */
    QP,    QP,    QP,    QP,    QP,    QP,    QP,    QP,
    QP,    QP,    QP,    QP,    QP,    QP,    QP,    QP,
    QP,    QP,    QP,    QP,    QP,    QP,    QP,    QP,
    QP,    QP,    QP,    QP,    QP,    QP,    QP,    QP,
    QP,    QP,    QP,    QP,    QP,    QP,    QP,    QP,
    QP,    QP,    QP,    QP,    QP,    QP,    QP,    QP,
    QP,    QP,    QP,    QP,    QP,    QP,    QP,    QP,
    QP,    QP,    QP,    QP,    QP,    QP,    QP,    0,
    REPEAT16(0), REPEAT16(0), REPEAT16(0), REPEAT16(0),
    REPEAT16(0), REPEAT16(0), REPEAT16(0), REPEAT16(0),
#undef REPEAT16
};

static inline int test_quoted_printable(int x) {
    return __str_encode_flags[x + 128] & QP;
}

static inline int test_xml_printable(int x) {
    return !(__str_encode_flags[x + 128] & XP);
}

#undef QP
#undef XP

int blob_append_xml_escape(blob_t *dst, const byte *src, ssize_t len)
{
    int i, j, c;

    for (i = j = 0; i < len; i++) {
        if (!test_xml_printable(src[i])) {
            if (i > j) {
                blob_append_data(dst, src + j, i - j);
            }
            switch (c = src[i]) {
            case '&':
                blob_append_cstr(dst, "&amp;");
                break;
            case '<':
                blob_append_cstr(dst, "&lt;");
                break;
            case '>':
                blob_append_cstr(dst, "&gt;");
                break;
            case '\'':
                blob_append_cstr(dst, "&#39;");
                break;
            case '"':
                blob_append_cstr(dst, "&#34;");
                break;
            default:
                blob_append_fmt(dst, "&#%d;", c);
                break;
            }
            j = i + 1;
        }
    }
    if (i > j) {
        blob_append_data(dst, src + j, i - j);
    }
    return 0;
}

int blob_append_quoted_printable(blob_t *dst, const byte *src, ssize_t len)
{
    int i, j, c;

    /* TODO:
     * - encode \n as \r\n
     * - clip lines to 76 chars with =\n
     */
    for (i = j = 0; i < len; i++) {
        if (!test_quoted_printable(c = src[i])) {
            /* encode spaces only at end on line */
            if ((c == ' ' || c == '\t')
             && (i < len - 1 && src[i + 1] != '\n')) {
                continue;
            }
            if (i > j) {
                blob_append_data(dst, src + j, i - j);
            }
            blob_append_fmt(dst, "=%02X", c);
            j = i + 1;
        }
    }
    if (i > j) {
        blob_append_data(dst, src + j, i - j);
    }
    return 0;
}

int blob_append_base64(blob_t *dst, const byte *src, ssize_t len, int width)
{
    const byte *end;
    int pos, packs_per_line, pack_num, newlen;
    byte *data;

    if (width < 0) {
        packs_per_line = -1;
    } else
    if (width == 0) {
        packs_per_line = 19; /* 76 characters + \r\n */
    } else {
        packs_per_line = width / 4;
    }

    pos = dst->len;
    newlen = b64_size(len, packs_per_line);
    blob_extend(dst, newlen);
    data = dst->data + pos;
    pack_num = 0;

    end = src + len;

    while (src < end) {
        int c1, c2, c3;

        c1      = *src++;
        *data++ = b64[c1 >> 2];

        if (src == end) {
            *data++ = b64[((c1 & 0x3) << 4)];
            *data++ = '=';
            *data++ = '=';
            if (packs_per_line > 0) {
                *data++ = '\r';
                *data++ = '\n';
            }
            break;
        }

        c2      = *src++;
        *data++ = b64[((c1 & 0x3) << 4) | ((c2 & 0xf0) >> 4)];

        if (src == end) {
            *data++ = b64[((c2 & 0x0f) << 2)];
            *data++ = '=';
            if (packs_per_line > 0) {
                *data++ = '\r';
                *data++ = '\n';
            }
            break;
        }

        c3 = *src++;
        *data++ = b64[((c2 & 0x0f) << 2) | ((c3 & 0xc0) >> 6)];
        *data++ = b64[c3 & 0x3f];

        if (packs_per_line > 0) {
            if (++pack_num >= packs_per_line || src == end) {
                pack_num = 0;
                *data++ = '\r';
                *data++ = '\n';
            }
        }
    }

#ifdef DEBUG
    assert (data == dst->data + dst->len);
#endif
    return 0;
}

#if 0

static char const __str_digits_upper[36] =
    "0123456789ABCDEFGHIJKLMNOPQRSTUVWXYZ";

int blob_append_ira(blob_t *dst, const byte *src, ssize_t len)
{
    int i, pos;
    byte *data;

    pos = dst->len;
    blob_extend(dst, len * 2);
    data = dst->data + pos;
    for (i = 0; i < len; i++) {
        data[0] = __str_digits_upper[(src[i] >> 4) & 0x0F];
        data[1] = __str_digits_upper[(src[i] >> 0) & 0x0F];
        data += 2;
    }
    return 0;
}

#else

static union {
    char buf[2];
    uint16_t us;
} const __str_uph[256] = {
#define HEX(x)    ((x) < 10 ? '0' + (x) : 'A' + (x) - 10)
#define UPH(x)    {{ HEX(((x) >> 4) & 15), HEX(((x) >> 0) & 15) }}
#define UPH4(n)   UPH(n),   UPH((n) + 1),    UPH((n) + 2),    UPH((n) + 3)
#define UPH16(n)  UPH4(n),  UPH4((n) + 4),   UPH4((n) + 8),   UPH4((n) + 12)
#define UPH64(n)  UPH16(n), UPH16((n) + 16), UPH16((n) + 32), UPH16((n) + 48)
    UPH64(0), UPH64(64), UPH64(128), UPH64(196),
#undef UPH64
#undef UPH16
#undef UPH4
#undef UPH
#undef HEX
};

int blob_append_ira(blob_t *blob, const byte *src, ssize_t len)
{
    int pos;
    byte *dst;

    pos = blob->len;
    blob_extend(blob, len * 2);
    dst = blob->data + pos;
    /* Unroll loop 4 times */
    while (len >= 4) {
        *(uint16_t *)(dst + 0) = __str_uph[src[0]].us;
        *(uint16_t *)(dst + 2) = __str_uph[src[1]].us;
        *(uint16_t *)(dst + 4) = __str_uph[src[2]].us;
        *(uint16_t *)(dst + 6) = __str_uph[src[3]].us;
        dst += 8;
        src += 4;
        len -= 4;
    }
    while (len > 0) {
        *(uint16_t *)(dst + 0) = __str_uph[src[0]].us;
        dst += 2;
        src += 1;
        len -= 1;
    }
    return 0;
}

#endif

/*[ CHECK ]::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::{{{*/
#ifdef CHECK
/* inlines (check invariants) + setup/teardowns                        {{{*/

static inline void check_blob_invariants(blob_t *blob)
{
    fail_if(blob->len >= blob->size,
            "a blob must have `len < size'. "
            "this one has `len = %d' and `size = %d'",
            blob->len, blob->size);
    fail_if(blob->data[blob->len] != '\0', \
            "a blob must have data[len] set to `\\0', `%c' found",
            blob->data[blob->len]);
}

static inline void check_setup(blob_t *blob, const char *data)
{
    blob_init(blob);
    blob_set_cstr(blob, data);
}

static inline void check_teardown(blob_t *blob, blob_t **blob2)
{
    blob_wipe(blob);
    if (blob2 && *blob2) {
        blob_delete(blob2);
    }
}

/*.....................................................................}}}*/
/* tests legacy functions                                              {{{*/

START_TEST(check_init_wipe)
{
    blob_t blob;
    blob_init(&blob);
    check_blob_invariants(&blob);

    fail_if(blob.len != 0,
            "initalized blob MUST have `len' = 0, but has `len = %d'",
            blob.len);
    fail_if(blob.data == NULL,
            "initalized blob MUST have a valid `data'");

    blob_wipe(&blob);
    fail_if(blob.data != NULL,
            "wiped blob MUST have `data' set to NULL");
    fail_if(blob.area != NULL,
            "wiped blob MUST have `area' set to NULL");
}
END_TEST

START_TEST(check_blob_new)
{
    blob_t *blob = blob_new();

    check_blob_invariants(blob);
    fail_if(blob == NULL,
            "no blob was allocated");
    fail_if(blob->len != 0,
            "new blob MUST have `len 0', but has `len = %d'", blob->len);
    fail_if(blob->data == NULL,
            "new blob MUST have a valid `data'");

    blob_delete(&blob);
    fail_if(blob != NULL,
            "pointer was not nullified by `blob_delete'");
}
END_TEST

/*.....................................................................}}}*/
/* test set functions                                                  {{{*/

START_TEST(check_set)
{
    blob_t blob, bloub;
    blob_init (&blob);
    blob_init (&bloub);

    /* blob set cstr */
    blob_set_cstr(&blob, "toto");
    check_blob_invariants(&blob);
    fail_if(blob.len != strlen("toto"),
            "blob.len should be %d, but is %d", strlen("toto"), blob.len);
    fail_if(strcmp((const char *)blob.data, "toto") != 0,
            "blob is not set to `%s'", "toto");

    /* blob set data */
    blob_set_data(&blob, "tutu", strlen("tutu"));
    check_blob_invariants(&blob);
    fail_if(blob.len != strlen("tutu"),
            "blob.len should be %d, but is %d", strlen("tutu"), blob.len);
    fail_if(strcmp((const char *)blob.data, "tutu") != 0,
            "blob is not set to `%s'", "tutu");

    /* blob set */
    blob_set(&bloub, &blob);
    check_blob_invariants(&bloub);
    fail_if(bloub.len != strlen("tutu"),
            "blob.len should be %d, but is %d", strlen("tutu"), bloub.len);
    fail_if(strcmp((const char *)bloub.data, "tutu") != 0,
            "blob is not set to `%s'", "tutu");

    blob_wipe(&blob);
    blob_wipe(&bloub);
}
END_TEST

/*.....................................................................}}}*/
/* test blob_dup / blob_resize                                         {{{*/

START_TEST(check_dup)
{
    blob_t blob;
    blob_t * bdup;

    check_setup(&blob, "toto string");
    bdup = blob_dup(&blob);
    check_blob_invariants(bdup);

    fail_if(bdup->len != blob.len, "duped blob *must* have same len");
    if (memcmp(bdup->data, blob.data, blob.len) != 0) {
        fail("original and dupped blob don't have the same content");
    }

    check_teardown(&blob, &bdup);
}
END_TEST

START_TEST(check_resize)
{
    blob_t b1;
    check_setup(&b1, "tototutu");

    blob_resize(&b1, 4);
    check_blob_invariants(&b1);
    fail_if (b1.len != 4,
             "blob_resized blob should have len 4, but has %d", b1.len);

    check_teardown(&b1, NULL);
}
END_TEST

/*.....................................................................}}}*/
/* test blit functions                                                 {{{*/

START_TEST(check_blit)
{
    blob_t blob;
    blob_t *b2;

    check_setup(&blob, "toto string");
    b2 = blob_dup(&blob);


    /* blit cstr */
    blob_blit_cstr(&blob, 4, "turlututu");
    check_blob_invariants(&blob);
    fail_if(strcmp((const char *)blob.data, "tototurlututu") != 0,
            "blit_cstr failed");
    fail_if(blob.len != strlen("tototurlututu"),
            "blit_cstr failed");

    /* blit data */
    blob_blit_data(&blob, 6, ".:.:.:.", 7);
    check_blob_invariants(&blob);
    fail_if(strcmp((const char *)blob.data, "tototu.:.:.:.") != 0,
            "blit_data failed");
    fail_if(blob.len != strlen("tototu.:.:.:."),
            "blit_cstr failed");

    /* blit */
    blob_blit(&blob, blob.len, b2);
    check_blob_invariants(&blob);
    fail_if(strcmp((const char *)blob.data, "tototu.:.:.:.toto string") != 0,
            "blit_data failed");
    fail_if(blob.len != strlen("tototu.:.:.:.toto string"),
            "blit_cstr failed");

    check_teardown(&blob, &b2);
}
END_TEST

/*.....................................................................}}}*/
/* test insert functions                                               {{{*/

START_TEST(check_insert)
{
    blob_t blob;
    blob_t *b2;

    check_setup(&blob, "05");
    b2 = blob_new();
    blob_set_cstr(b2, "67");


    /* insert cstr */
    blob_insert_cstr(&blob, 1, "1234");
    check_blob_invariants(&blob);
    fail_if(strcmp((const char *)blob.data, "012345") != 0,
            "insert failed");
    fail_if(blob.len != strlen("012345"),
            "insert failed");

    /* insert data */
    blob_insert_data(&blob, 20, "89", 2);
    check_blob_invariants(&blob);
    fail_if(strcmp((const char *)blob.data, "01234589") != 0,
            "insert_data failed");
    fail_if(blob.len != strlen("01234589"),
            "insert_data failed");

    /* insert */
    blob_insert(&blob, 6, b2);
    check_blob_invariants(&blob);
    fail_if(strcmp((const char *)blob.data, "0123456789") != 0,
            "insert failed");
    fail_if(blob.len != strlen("0123456789"),
            "insert failed");

    check_teardown(&blob, &b2);
}
END_TEST

/*.....................................................................}}}*/
/* test append functions                                               {{{*/

START_TEST(check_append)
{
    blob_t blob;
    blob_t *b2;

    check_setup(&blob, "01");
    b2 = blob_new();
    blob_set_cstr(b2, "89");


    /* append cstr */
    blob_append_cstr(&blob, "2345");
    check_blob_invariants(&blob);
    fail_if(strcmp((const char *)blob.data, "012345") != 0,
            "append failed");
    fail_if(blob.len != strlen("012345"),
            "append failed");

    /* append data */
    blob_append_data(&blob, "67", 2);
    check_blob_invariants(&blob);
    fail_if(strcmp((const char *)blob.data, "01234567") != 0,
            "append_data failed");
    fail_if(blob.len != strlen("01234567"),
            "append_data failed");

    /* append */
    blob_append(&blob, b2);
    check_blob_invariants(&blob);
    fail_if(strcmp((const char *)blob.data, "0123456789") != 0,
            "append failed");
    fail_if(blob.len != strlen("0123456789"),
            "append failed");

    check_teardown(&blob, &b2);
}
END_TEST

START_TEST(check_append_file_data)
{
    const char file[] = "check.data";
    const char data[] = "my super data, just for the fun, and for the test \n"
                        "don't you like it ?\n"
                        "tests are soooo boring !!!";

    blob_t blob;
    int fd;

    blob_init(&blob);

    fd = open(file, O_WRONLY | O_CREAT | O_TRUNC, 0640);
    fail_if(fd < 0, "sample file not created");
    fail_if(write(fd, &data, strlen(data)) != sstrlen(data),
            "data not written");
    close(fd);

    fail_if(blob_append_file_data(&blob, file) != sstrlen(data),
            "file miscopied");
    check_blob_invariants(&blob);
    fail_if(blob.len != sstrlen(data),
            "file miscopied");
    fail_if(strcmp((const char *)blob.data, data) != 0,
            "garbage copied");

    unlink(file);

    blob_wipe(&blob);
}
END_TEST
/*.....................................................................}}}*/
/* test kill functions                                                 {{{*/

START_TEST(check_kill)
{
    blob_t blob;
    check_setup(&blob, "0123456789");

    /* kill first */
    blob_kill_first(&blob, 3);
    check_blob_invariants(&blob);
    fail_if(strcmp((const char *)blob.data, "3456789") != 0,
            "kill_first failed");
    fail_if(blob.len != strlen("3456789"),
            "kill_first failed");

    /* kill last */
    blob_kill_last(&blob, 3);
    check_blob_invariants(&blob);
    fail_if(strcmp((const char *)blob.data, "3456") != 0,
            "kill_last failed");
    fail_if(blob.len != strlen("3456"),
            "kill_last failed");

    /* kill */
    blob_kill_data(&blob, 1, 2);
    check_blob_invariants(&blob);
    fail_if(strcmp((const char *)blob.data, "36") != 0,
            "kill failed");
    fail_if(blob.len != strlen("36"),
            "kill failed");

    check_teardown(&blob, NULL);
}
END_TEST

/*.....................................................................}}}*/
/* test printf functions                                               {{{*/

START_TEST(check_printf)
{
    char cmp[81];
    blob_t blob;
    check_setup(&blob, "01234");
    snprintf(cmp, 81, "%080i", 0);

    /* printf first */
    blob_append_fmt(&blob, "5");
    check_blob_invariants(&blob);
    fail_if(strcmp((const char *)blob.data, "012345") != 0,
            "printf failed");
    fail_if(blob.len != strlen("012345"),
            "printf failed");

    blob_append_fmt(&blob, "%s89", "67");
    check_blob_invariants(&blob);
    fail_if(strcmp((const char *)blob.data, "0123456789") != 0,
            "printf failed");
    fail_if(blob.len != strlen("0123456789"),
            "printf failed");

    blob_set_fmt(&blob, "%080i", 0);
    check_blob_invariants(&blob);
    fail_if(strcmp((const char *)blob.data, cmp) != 0,
            "printf failed");
    fail_if(blob.len != sstrlen(cmp),
            "printf failed");

    check_teardown(&blob, NULL);
}
END_TEST

/*.....................................................................}}}*/
/* test blob_urldecode                                                 {{{*/

START_TEST(check_url)
{
    blob_t blob;
    check_setup(&blob, "%20toto%79");

    blob_urldecode(&blob);
    check_blob_invariants(&blob);

    fail_if(strcmp((const char *)blob.data, " totoy") != 0,
            "urldecode failed");
    fail_if(blob.len != sstrlen(" totoy"),
            "urldecode failed");

    check_teardown(&blob, NULL);
}
END_TEST

/*.....................................................................}}}*/
/* test blob_b64                                                       {{{*/

START_TEST(check_b64)
{
    blob_t blob;
    check_setup(&blob, "abcdef");

    blob_b64encode(&blob, 16);
    check_blob_invariants(&blob);

    // TODO: check results
    check_teardown(&blob, NULL);
}
END_TEST

/*.....................................................................}}}*/
/* test blob_search                                                    {{{*/

START_TEST(check_search)
{
    blob_t blob;
    blob_t *b1 = blob_new();
    check_setup(&blob, "toto string");

    /* search data */
    fail_if(blob_search_data(&blob, 0, (void*)"string", 6) != 5,
            "blob_search fails when needle exists");
    fail_if(blob_search_data(&blob, 0, (void*)"bloube", 6) != -1,
            "blob_search fails when needle doesn't exist");

    /* search cstr */
    fail_if(blob_search_cstr(&blob, 0, "string") != 5,
            "blob_search fails when needle exists");
    fail_if(blob_search_cstr(&blob, 0, "bloube") != -1,
            "blob_search fails when needle doesn't exist");

    /* search */
    blob_set_cstr(b1, "string");
    fail_if(blob_search(&blob, 0, b1) != 5,
            "blob_search fails when needle exists");
    blob_set_cstr(b1, "blouble");
    fail_if(blob_search(&blob, 0, b1) != -1,
            "blob_search fails when needle doesn't exist");

    check_teardown(&blob, &b1);
}
END_TEST

/*.....................................................................}}}*/
/* test check_zlib_compress_uncompress                                 {{{*/

START_TEST(check_zlib_compress_uncompress)
{
    blob_t b1;
    blob_t b2;
    blob_t b3;

    blob_init(&b1);
    blob_init(&b2);
    blob_init(&b3);
    blob_set_cstr(&b1, "toto string");

    blob_zlib_compress(&b2, &b1);
    blob_zlib_uncompress(&b3, &b2);

    fail_if(blob_cmp(&b1, &b3),
            "blob_uncompress dest does not correspond to blob_compress src");

    blob_wipe(&b1);
    blob_wipe(&b2);
    blob_wipe(&b3);
}
END_TEST

/*.....................................................................}}}*/
/* test check_zlib                                                     {{{*/

START_TEST(check_raw_compress_uncompress)
{
    blob_t b1;
    blob_t b2;
    blob_t b3;

    blob_init(&b1);
    blob_init(&b2);
    blob_init(&b3);
    blob_set_cstr(&b1, "toto string");

    blob_raw_compress(&b2, &b1);
    blob_raw_uncompress(&b3, &b2);

    fail_if(blob_cmp(&b1, &b3),
            "blob_uncompress dest does not correspond to blob_compress src");

    blob_wipe(&b1);
    blob_wipe(&b2);
    blob_wipe(&b3);
}
END_TEST


/*.....................................................................}}}*/
/* test blob_blob_iconv                                                {{{*/

static int check_gunzip_tpl(const char *file1, const char *file2)
{
    blob_t b1;
    blob_t b2;

    int i = 0;
    int c_typ = 0;

    blob_init(&b1);
    blob_init(&b2);

    blob_file_gunzip(&b1, file1);
    blob_append_file_data(&b2, file2);

    fail_if (blob_cmp(&b1, &b2) != 0, "blob_gunzip failed on: %s, %s",
             file1, file2);

    blob_wipe(&b1);
    blob_wipe(&b2);

    return 0;
}

START_TEST(check_gunzip)
{
    check_gunzip_tpl("samples/example1_zlib.gz", "samples/example1_zlib");
}
END_TEST

/*.....................................................................}}}*/
/* test check_gzip                                                     {{{*/

static int check_gzip_tpl(const char *file1, const char *file2)
{
    blob_t b1;
    blob_t b2;

    int i = 0;
    int c_typ = 0;

    blob_init(&b1);
    blob_init(&b2);

    blob_file_gzip(&b1, file1);

    blob_append_file_data(&b2, file2);

    fail_if (blob_cmp(&b1, &b2) != 0, "blob_gzip failed on: %s, %s",
             file1, file2);

    blob_wipe(&b1);
    blob_wipe(&b2);

    return 0;
}

START_TEST(check_gzip)
{
    check_gzip_tpl("samples/example1_zlib", "samples/example1_zlib.gz");
}
END_TEST

/*.....................................................................}}}*/
/* test blob_blob_iconv                                                {{{*/

static int check_aiconv_templ(const char *file1, const char *file2,
                              const char *encoding)
{
    blob_t b1;
    blob_t b2;

    int i = 0;
    int c_typ = 0;

    blob_init(&b1);
    blob_init(&b2);

    blob_file_auto_iconv(&b1, file1, encoding, &c_typ);

    blob_append_file_data(&b2, file2);
    fail_if (blob_cmp(&b1, &b2) != 0,
             "blob_auto_iconv failed on: %s with"
             " hint \"%s\" encoding\n---\n%.*s\n---\n%.*s",
             file1, encoding,
             b1.len, blob_get_cstr(&b1),
             b2.len, blob_get_cstr(&b2));

    blob_wipe(&b1);
    blob_wipe(&b2);

    return 0;
}

static int check_aiconv_templ_2(const char *file1, const char *file2)
{
    check_aiconv_templ(file1, file2, "UTF-8");
    check_aiconv_templ(file1, file2, "ISO-8859-1");
    check_aiconv_templ(file1, file2, "Windows-1250");

    return 0;
}

START_TEST(check_blob_auto_iconv)
{
    check_aiconv_templ_2("samples/example1.latin1",
                         "samples/example1.utf8");
    check_aiconv_templ_2("samples/example2.windows-1250",
                         "samples/example2.utf8");
    check_aiconv_templ_2("samples/example1.utf8",
                         "samples/example1.utf8");
    check_aiconv_templ("samples/example3.windows-1256",
                       "samples/example3.utf8", "windows-1256");
    blob_iconv_close_all();
}
END_TEST

/*.....................................................................}}}*/
/* test blob_blob_iconv                                                {{{*/

START_TEST(check_blob_iconv_close)
{
    blob_t b1;
    blob_t b2;
    int c_typ = 0;

    blob_init(&b1);
    blob_init(&b2);

    blob_file_auto_iconv(&b2, "samples/example1.latin1", "ISO-8859-1",
                         &c_typ);
    blob_file_auto_iconv(&b2, "samples/example1.utf8", "UTF-8",
                         &c_typ);
    blob_file_auto_iconv(&b2, "samples/example2.windows-1250", "windows-1250",
                         &c_typ);

    fail_if(blob_iconv_close_all() != 3,
            "blob_iconv_close_all has failed to close all handlers");

    blob_wipe(&b1);
    blob_wipe(&b2);
}
END_TEST

START_TEST(check_ira)
{
    blob_t dst, back;

    blob_init(&dst);
    blob_init(&back);
#define TEST_STRING      "Injector test String"
#define TEST_STRING_ENC  "496E6A6563746F72207465737420537472696E67"

    blob_append_ira(&dst, (const byte*)TEST_STRING, strlen(TEST_STRING));

    fail_if(strcmp(blob_get_cstr(&dst), TEST_STRING_ENC),
            "%s(\"%s\") -> \"%s\" : \"%s\"",
            "blob_append_ira",
            TEST_STRING, blob_get_cstr(&dst), TEST_STRING_ENC);
#if 0
    blob_decode_ira(&back, dst);
    fail_if(strcmp(blob_get_cstr(&back), TEST_STRING),
            "blob_decode_ira failure");
#endif

#undef TEST_STRING
#undef TEST_STRING_ENC

    blob_wipe(&dst);
    blob_wipe(&back);
}
END_TEST

START_TEST(check_quoted_printable)
{
    blob_t dst;

    blob_init(&dst);
#define TEST_STRING      "Injector X=1 Gagné! \n Last line "
#define TEST_STRING_ENC  "Injector X=3D1 Gagn=C3=A9!=20\n Last line=20"

    blob_append_quoted_printable(&dst, (const byte*)TEST_STRING,
                                 strlen(TEST_STRING));

    fail_if(strcmp(blob_get_cstr(&dst), TEST_STRING_ENC),
            "%s(\"%s\") -> \"%s\" : \"%s\"",
            "blob_append_quoted_printable",
            TEST_STRING, blob_get_cstr(&dst), TEST_STRING_ENC);
#if 0
    blob_decode_quoted_printable(&back, dst);
    fail_if(strcmp(blob_get_cstr(&back), TEST_STRING),
            "blob_decode_quoted_printable failure");
#endif

#undef TEST_STRING
#undef TEST_STRING_ENC

    blob_wipe(&dst);
}
END_TEST

START_TEST(check_xml_escape)
{
    blob_t dst;

    blob_init(&dst);
#define TEST_STRING      "<a href=\"Injector\" X='1' value=\"&Gagné!\" />"
#define TEST_STRING_ENC  "&lt;a href=&#34;Injector&#34; X=&#39;1&#39; value=&#34;&amp;Gagné!&#34; /&gt;"

    blob_append_xml_escape(&dst, (const byte*)TEST_STRING,
                           strlen(TEST_STRING));

    fail_if(strcmp(blob_get_cstr(&dst), TEST_STRING_ENC),
            "%s(\"%s\") -> \"%s\" : \"%s\"",
            "blob_append_xml_escape",
            TEST_STRING, blob_get_cstr(&dst), TEST_STRING_ENC);
#if 0
    blob_decode_xml_escape(&back, dst);
    fail_if(strcmp(blob_get_cstr(&back), TEST_STRING),
            "blob_decode_xml_escape failure");
#endif

#undef TEST_STRING
#undef TEST_STRING_ENC

    blob_wipe(&dst);
}
END_TEST

/*.....................................................................}}}*/
/* public testing API                                                  {{{*/

Suite *check_make_blob_suite(void)
{
    Suite *s  = suite_create("Blob");
    TCase *tc = tcase_create("Core");

    suite_add_tcase(s, tc);
    tcase_add_test(tc, check_init_wipe);
    tcase_add_test(tc, check_blob_new);
    tcase_add_test(tc, check_set);
    tcase_add_test(tc, check_dup);
    tcase_add_test(tc, check_blit);
    tcase_add_test(tc, check_insert);
    tcase_add_test(tc, check_append);
    tcase_add_test(tc, check_append_file_data);
    tcase_add_test(tc, check_kill);
    tcase_add_test(tc, check_resize);
    tcase_add_test(tc, check_printf);
    tcase_add_test(tc, check_url);
    tcase_add_test(tc, check_b64);
    tcase_add_test(tc, check_ira);
    tcase_add_test(tc, check_quoted_printable);
    tcase_add_test(tc, check_xml_escape);
    tcase_add_test(tc, check_search);
    tcase_add_test(tc, check_raw_compress_uncompress);
    tcase_add_test(tc, check_zlib_compress_uncompress);
    tcase_add_test(tc, check_gunzip);
    tcase_add_test(tc, check_gzip);
    tcase_add_test(tc, check_blob_auto_iconv);
    tcase_add_test(tc, check_blob_iconv_close);

    return s;
}

/*.....................................................................}}}*/
#endif
/*:::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::}}}*/
