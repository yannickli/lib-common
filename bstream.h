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

#ifndef IS_LIB_COMMON_BSTREAM_H
#define IS_LIB_COMMON_BSTREAM_H

#include "core.h"
#include "unix.h"

#if !defined(__USE_FILE_OFFSET64) && !defined(_LARGEFILE64_SOURCE) && \
    !defined(__USE_LARGEFILE64) && !defined(__MINGW) && !defined(__MINGW32__)
typedef loff_t off64_t;
#define lseek64(stream, off, whence) llseek(stream, off, whence)
loff_t llseek(int fd, loff_t offset, int whence);
#endif

#define BSTREAM_ISREAD(m)  (((m) & (O_RDONLY|O_WRONLY|O_RDWR)) == O_RDONLY)
#define BSTREAM_ISWRITE(m) (((m) & (O_RDONLY|O_WRONLY|O_RDWR)) == O_WRONLY)

typedef struct BSTREAM {
    int mode;   /* same as mode argument for open() */
    int fd;
    off64_t pos;
    unsigned char *pread;
    unsigned char *pread_end;
    unsigned char *pwrite_start;
    unsigned char *pwrite;
    unsigned char *pwrite_end;
    flag_t eof : 1;
    flag_t error : 1;
#define DEFAULT_BUFSIZ (64 * 1024)
    size_t bufsiz;
    unsigned char buf[0]; /* malloc'ed later */
} BSTREAM;

static inline int beof(BSTREAM *stream) {
    return stream->eof;
}

static inline int berror(BSTREAM *stream) {
    return stream->error;
}

static inline void bclearerr(BSTREAM *stream) {
    stream->eof = stream->error = false;
}

static inline int bfileno(BSTREAM *stream) {
    return stream->fd;
}

static inline BSTREAM *battach_bufsize(int fd, int mode, int bufsize)
{
    BSTREAM *stream;

    if (fd < 0)
        return NULL;

    if (bufsize < 0) {
        /* Unbuffered */
        bufsize = 1;
    } else
    if (bufsize == 0) {
        bufsize = DEFAULT_BUFSIZ;
    }

    stream = p_new_extra(BSTREAM, bufsize);
    stream->mode   = mode;
    stream->fd     = fd;
    stream->pos    = 0;
    stream->pread  = stream->pread_end =
        stream->pwrite = stream->pwrite_end =
        stream->pwrite_start = stream->buf;
    stream->eof = false;
    stream->error = false;
    stream->bufsiz = bufsize;
    if (BSTREAM_ISWRITE(stream->mode)) {
        stream->pwrite_end += bufsize;
    }

    return stream;
}

static inline BSTREAM *battach(int fd, int mode)
{
    return battach_bufsize(fd, mode, 0);
}

static inline BSTREAM *bopen(const char *filename, int mode)
{
    return battach(open(filename, mode, 0644), mode);
}

/* For internal use only */
static inline ssize_t bread_buffer(int fd, unsigned char *buf, size_t count)
{
    for (;;) {
        ssize_t n = read(fd, buf, count);
        if (n >= 0 || (errno != EINTR && errno != EAGAIN)) {
            return n;
        }
    }
}

/* For internal use only */
static inline int bfilbuf(BSTREAM *stream)
{
    ssize_t n;

    if (!BSTREAM_ISREAD(stream->mode) || stream->error) {
        return -1;
    }

    if (stream->eof) {
        /* EOF is sticky */
        return 0;
    }

    stream->pread = stream->pread_end = stream->buf;

    n = bread_buffer(stream->fd, stream->buf, stream->bufsiz);
    if (n < 0) {
        stream->error = true;
    } else
    if (n == 0) {
        stream->eof = true;
    } else {
        stream->pos       += n;
        stream->pread_end += n;
    }
    return n;
}

static inline ssize_t bread_call(BSTREAM *stream, void *ptr, size_t count)
{
    unsigned char *buf = ptr;
    ssize_t n, toread, nbread = 0;

    if (!BSTREAM_ISREAD(stream->mode)) {
        return -1;
    }

    /* Keep trying until EOF */
    for (;;) {
        n = stream->pread_end - stream->pread;
        if ((size_t)n > count) {
            n = count;
        }

        /* Copy from buffer. */
        buf     = mempcpy(buf, stream->pread, n);
        stream->pread += n;
        nbread += n;
        count  -= n;

        if (count == 0)
            break;

        if (stream->error) {
            return nbread ? nbread : -1;
        }
        if (stream->eof) {
            /* EOF is sticky */
            break;
        }

        /* Try and read full blocks directly to the destination */
        if (count >= stream->bufsiz) {
            toread = count - (count % stream->bufsiz);
            n = bread_buffer(stream->fd, buf, toread);
            if (n < 0) {
                stream->error = true;
                continue;
            }
            if (n == 0) {
                stream->eof = true;
                continue;
            }
            stream->pos += n;
            buf    += n;
            nbread += n;
            count  -= n;
            if (count == 0)
                break;
        }
        bfilbuf(stream);
    }
    return nbread;
}

static inline ssize_t bread(BSTREAM *stream, void *ptr, size_t count)
{
    if (count <= (size_t)(stream->pread_end - stream->pread)) {
        /* Handle chunk that fits in the buffer. */
        memcpy(ptr, stream->pread, count);
        stream->pread += count;
        return count;
    }
    return bread_call(stream, ptr, count);
}

static inline int bgetc(BSTREAM *stream)
{
    if (stream->pread < stream->pread_end || bfilbuf(stream) > 0) {
        return *stream->pread++;
    } else {
        return -1;
    }
}

static inline char *bgets(BSTREAM *stream, char *s, int size)
{
    char *p, *p_end;
    int c;

    if (size <= 0) {
        return NULL;
    }

    p = s;
    p_end = p + size - 1;

#if 1
    {
        /* Optimized loop with single test until end of buffer */
        int avail = stream->pread_end - stream->pread;
        char *p_end0 = p_end;
        if (avail < size)
            p_end0 = p + avail;
        while (p < p_end0) {
            if ((*p++ = *stream->pread++) == '\n') {
                *p = '\0';
                return s;
            }
        }
    }
#endif
    while (p < p_end) {
        c = bgetc(stream);
        if (c == -1) {
            if (p == s) {
                return NULL;
            }
            break;
        }
        *p++ = c;
        if (c == '\n')
            break;
    }
    if (s == p_end && stream->eof) {
        /* Special case for 1 byte buffer:
         * if EOF already seen, return NULL, instead of ""
         */
        return NULL;
    }
    *p = '\0';
    return s;
}

/* For internal use only */
static inline ssize_t bwrite_buffer(int fd, const unsigned char *buf,
                                    size_t count)
{
    ssize_t n, written = 0;

    while (count) {
        n = write(fd, buf, count);
        if (n <= 0) {
            if (n < 0 && (errno == EINTR || errno == EAGAIN)) {
                continue;
            }
            break;
        } else {
            buf     += n;
            written += n;
            count   -= n;
        }
    }
    return written;
}

static inline int bflsbuf(BSTREAM *stream)
{
    size_t towrite, written;

    if (!BSTREAM_ISWRITE(stream->mode) || stream->error) {
        return -1;
    }

    towrite = stream->pwrite - stream->pwrite_start;
    written = bwrite_buffer(stream->fd, stream->pwrite_start, towrite);
    stream->pos += written;
    if (written == towrite) {
        stream->pwrite_start = stream->pwrite = stream->buf;
        return written;
    } else {
        /* Support for partial writes */
        stream->pwrite_start += written;
        return -1;
    }
}

static inline int bflush(BSTREAM *stream)
{
    if (BSTREAM_ISWRITE(stream->mode)) {
        return (bflsbuf(stream) >= 0) ? 0 : -1;
    } else {
        stream->pread = stream->pread_end = stream->buf;
        return 0;
    }
}

/* Should move this to bstream.c */
static ssize_t bwrite_call(BSTREAM *stream, const void *src, size_t count)
{
    const unsigned char *buf = src;
    size_t avail, towrite, tocopy, n;
    size_t written = 0;

    if (!BSTREAM_ISWRITE(stream->mode)) {
        return -1;
    }

    /* Fill the buffer first except if writing a full buffer */
    tocopy = count;
    avail = stream->pwrite_end - stream->pwrite;
    if (tocopy >= avail) {
        tocopy = avail;
        if (avail == stream->bufsiz) {
            /* Optim: buffer is empty, and count is at least BUFSIZ.
             * Do not fill the buffer, write directly from buf. */
            tocopy = 0;
        }
    }

    if (tocopy) {
        buf      = mempcpy(stream->pwrite, buf, tocopy);
        stream->pwrite += tocopy;
        avail   -= tocopy;
        written += tocopy;
        count   -= tocopy;
    }

    if (avail == 0) {
        /* The buffer is full: flush it. */
        if ((n = bflsbuf(stream)) <= 0) {
            /* This return value may be wrong */
            return written ? written : n;
        }
    }

    /* Write as many stream->bufsiz blocks as possible */
    towrite = count - (count % stream->bufsiz);
    if (towrite) {
        n = bwrite_buffer(stream->fd, buf, towrite);
        stream->pos += n;
        written     += n;
        if (n != towrite) {
            return written;
        }
        count -= n;
        buf   += n;
    }

    if (count > 0) {
        /* Write the rest to the buffer */
        stream->pwrite = mempcpy(stream->pwrite, buf, count);
        written += count;
    }

    return written;
}

static inline ssize_t bwrite(BSTREAM *stream, const void *buf, size_t count)
{
    if (count < (size_t)(stream->pwrite_end - stream->pwrite)) {
        /* Handle chunk that fits in the buffer and doesn't fill it. */
        stream->pwrite = mempcpy(stream->pwrite, buf, count);
        return count;
    }
    return bwrite_call(stream, buf, count);
}

static inline int bputc(int c, BSTREAM *stream)
{
    if (stream->pwrite < stream->pwrite_end || bflsbuf(stream) > 0) {
        return *stream->pwrite++ = c;
    } else {
        return -1;
    }
}

static inline int bputs(BSTREAM *stream, const char *s)
{
    return bwrite(stream, s, strlen(s));
}

static inline off64_t btell64(BSTREAM *stream)
{
    return stream->pos + (stream->pwrite - stream->pwrite_start) -
            (stream->pread_end - stream->pread);
}

static inline off64_t bseek64(BSTREAM *stream, long offset, int whence)
{
    /* Should check if seeking inside buffer */
    bflush(stream);
    stream->eof = false;
    return stream->pos = lseek64(stream->fd, offset, whence);
}

static inline int brewind(BSTREAM *stream)
{
    /* Should check if seeking inside buffer */
    bflush(stream);
    stream->eof = false;
    stream->pos = lseek(stream->fd, 0L, SEEK_SET);
    return 0;
}

static inline int bdetach(BSTREAM **streamp)
{
    int ret = bflush(*streamp);
    p_delete(streamp);
    return ret;
}

static inline int bclose(BSTREAM **streamp)
{
    int ret = bflush(*streamp);
    if (p_close(&(*streamp)->fd))
        ret = -1;
    p_delete(streamp);
    return ret;
}

#undef lseek64

#ifdef CHECK
#include <check.h>

Suite *check_bstream_suite(void);
#endif

#endif /* IS_LIB_COMMON_BSTREAM_H */
