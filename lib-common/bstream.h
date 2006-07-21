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

#ifndef BSTREAM_H
#define BSTREAM_H

#include <errno.h>
#include <fcntl.h>
#include <string.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <stdlib.h>

/* For internal use only */
static inline ssize_t bwrite_buffer(int fd, const char *buf, size_t count)
{
    ssize_t n, written = 0;

    while (count) {
        n = write(fd, buf, count);
        if (n < 0) {
            if (errno == EINTR || errno == EAGAIN) {
                continue;
            }
            break;
        } else if (n == 0) {
            break;
        } else {
            buf     += n;
            count   -= n;
            written += n;
        }
    }
    return written;
}

/* For internal use only */
static inline ssize_t bread_buffer(int fd, char *buf, size_t count)
{
    ssize_t n, nbread = 0;

    while (count) {
        n = read(fd, buf, count);
        if (n < 0) {
            if (errno == EINTR || errno == EAGAIN) {
                continue;
            }
            break;
        } else if (n == 0) {
            break;
        } else {
            buf     += n;
            count   -= n;
            nbread  += n;
        }
    }
    return nbread;
}

typedef enum {
    BSTREAM_READ = 1,
    BSTREAM_WRITE = 2,
} bstream_mode;

typedef struct BSTREAM {
    int fd;
    bstream_mode mode;
    char *pread;
    char *pwrite;
    int eof;
#define DEFAULT_BUFSIZ (64 * 1024)
    size_t bufsiz;
    char buf[0]; /* malloc'ed later */
} BSTREAM;

/* Officially exported functions
 */
static inline BSTREAM *battach_bufsize(int fd, bstream_mode mode, int bufsize)
{
    BSTREAM *stream;

    if (fd < 0) {
        return NULL;
    }

    stream = malloc(sizeof(BSTREAM) + bufsize);
    if (!stream) {
        return NULL;
    }

    stream->bufsiz = bufsize;
    stream->fd = fd;
    stream->mode = mode;
    stream->pwrite = stream->buf;
    stream->pread  = stream->buf;
    stream->eof    = 0;
    return stream;
}

static inline BSTREAM *battach(int fd, bstream_mode mode)
{
    return battach_bufsize(fd, mode, DEFAULT_BUFSIZ);
}

static inline int bflush(BSTREAM *stream)
{
    size_t written;
    size_t towrite;

    if (stream->mode != BSTREAM_WRITE) {
        return 0;
    }

    towrite = stream->pwrite - stream->buf;
    written = bwrite_buffer(stream->fd, stream->buf, towrite);
    if (written != towrite) {
        return -1;
    }

    stream->pwrite = stream->buf;

    return 0;
}

static inline ssize_t bread(BSTREAM *stream, void *buf, size_t count)
{
    size_t avail, toread, n;
    size_t nbread = 0;

    if (stream->mode != BSTREAM_READ) {
        return -1;
    }

    avail = stream->pwrite - stream->pread;
    if (avail >= count) {
        /* Read from buffer. Fine. */
        memcpy(buf, stream->pread, count);
        stream->pread += count;
        return count;
    }

    /* We've been asked for more than what we have in the buffer. First
     * copy what we have and reset pointers. */
    memcpy(buf, stream->pread, avail);
    buf    += avail;
    count  -= avail;
    nbread += avail;
    stream->pread = stream->buf;
    stream->pwrite = stream->buf;

    if (stream->eof) {
        return nbread;
    }

    /* Now read more */
    if (count >= stream->bufsiz) {
        /* Read full blocks directly to the buffer */
        toread = count / stream->bufsiz * stream->bufsiz;
        n = bread_buffer(stream->fd, buf, toread);
        if (n != toread) {
            stream->eof = 1;
        }
        buf    += n;
        count  -= n;
        nbread += n;
    }

    if (!stream->eof) {
        /* Read to the internal buf and copy as many as requested */
        n = bread_buffer(stream->fd, stream->pread, stream->bufsiz);
        if (n != stream->bufsiz) {
            stream->eof = 1;
            if (n < count) {
                count = n;
            }
        }
        stream->pwrite = stream->pread + n;
        memcpy(buf, stream->pread, count);
        stream->pread += count;
        nbread += count;
    }

    return nbread;
}

static inline int bgetc(BSTREAM *stream)
{
    char c;
    if (bread(stream, &c, 1) != 1) {
        return -1;
    }
    return c;
}

static inline char *bgets(BSTREAM *stream, char *s, int size)
{
    char *p;
    int n, c;

    if (size <= 0) {
        return NULL;
    }

    n = size - 1;
    p = s;
    while (n > 0) {
        c = bgetc(stream);
        if (c == -1) 
            break;
        n--;
        *p++ = c;
        if (c == '\n')
            break;
    }
    if (p == s) {
        return NULL;
    }
    *p = '\0';
    if (c == -1) {
        return NULL;
    }
    return s;
}

static inline ssize_t bwrite_call(BSTREAM *stream, const void *buf, size_t count);

static inline ssize_t bwrite(BSTREAM *stream, const void *buf, size_t count)
{
    if (stream->mode != BSTREAM_WRITE) {
        return -1;
    }

    /* Fill the buffer first */
    if (stream->pwrite + count < stream->buf + stream->bufsiz) {
        /* small chunk : We will put data into the buffer */
        memcpy(stream->pwrite, buf, count);
        stream->pwrite += count;
        return count;
    }

    return bwrite_call(stream, buf, count);
}

static ssize_t bwrite_call(BSTREAM *stream, const void *buf, size_t count)
{
    size_t avail, towrite, n;
    size_t written = 0;

    avail = stream->bufsiz - (stream->pwrite - stream->buf);
    if (avail != stream->bufsiz) {
        /* Buffer is not empty. We need to fill it before writing */
        towrite = avail;
    } else {
        /* Optim: buffer is empty, and count is bigger than IF_BUFSIZ. 
            * Do not fill the buffer, we will write directly from buf. */
        towrite = 0;
    }

    if (towrite) {
        memcpy(stream->pwrite, buf, towrite);
        stream->pwrite += towrite;
        avail   -= towrite;
        buf     += towrite;
        count   -= towrite;
        written += towrite;
    }

    if (count == 0) {
        /* We're done. */
        return written;
    }

    if (avail == 0) {
        /* The buffer is full. We need to flush it to fd */
        n = bwrite_buffer(stream->fd, stream->buf, stream->bufsiz);
        if (n != stream->bufsiz) {
            written += n;
            return written;
        }
        stream->pwrite -= n;
    }
    
    /* Write as many stream->bufsiz blocks as possible */
    towrite = count / stream->bufsiz * stream->bufsiz;
    if (towrite) {
        n = bwrite_buffer(stream->fd, buf, towrite);
        if (n != towrite) {
            written += n;
            return written;
        }
        written += n;
        count   -= n;
        buf     += n;
    }

    if (count == 0) {
        return written;
    }

    /* Write the rest to the buffer */
    memcpy(stream->buf, buf, count);
    stream->pwrite = stream->buf + count;
    written += count;
    count   -= count;

    return written;
}

static inline int bdetach(BSTREAM *stream)
{
    int ret;
    bflush(stream);
    ret = close(stream->fd);
    stream->pread  = stream->buf;
    stream->pwrite = stream->buf;
    free(stream);
    return ret;
}

#ifdef CHECK
#include <check.h>

Suite *check_bstream_suite(void);
#endif

#endif
