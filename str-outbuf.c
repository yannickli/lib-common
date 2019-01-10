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

#include <sys/sendfile.h>
#include "unix.h"
#include "str-outbuf.h"

void ob_check_invariants(outbuf_t *ob)
{
    int len = ob->length, sb_len = ob->sb.len;

    htlist_for_each(it, &ob->chunks_list) {
        outbuf_chunk_t *obc = htlist_first_entry(&ob->chunks_list, outbuf_chunk_t, chunks_link);

        sb_len -= obc->sb_leading;
        len    -= obc->sb_leading;
        len    -= (obc->length - obc->offset);
    }

    assert (len == ob->sb_trailing);
    assert (sb_len == ob->sb_trailing);
}

void ob_chunk_close(outbuf_chunk_t *obc)
{
    close(obc->u.fd);
}

void ob_chunk_free(outbuf_chunk_t *obc)
{
    p_delete(&obc->u.vp);
}

void ob_chunk_munmap(outbuf_chunk_t *obc)
{
    munmap(obc->u.vp, obc->length);
}

static void ob_merge_(outbuf_t *dst, outbuf_t *src, bool wipe)
{
    outbuf_chunk_t *obc;

    sb_addsb(&dst->sb, &src->sb);

    if (!htlist_is_empty(&src->chunks_list)) {
        obc = htlist_first_entry(&src->chunks_list, outbuf_chunk_t, chunks_link);
        obc->sb_leading  += dst->sb_trailing;
        dst->sb_trailing  = src->sb_trailing;
        htlist_splice_tail(&dst->chunks_list, &src->chunks_list);
    } else {
        dst->sb_trailing += src->sb_trailing;
    }
    dst->length += src->length;

    if (wipe) {
        sb_wipe(&src->sb);
    } else {
        src->length      = 0;
        src->sb_trailing = 0;
        htlist_init(&src->chunks_list);
        sb_reset(&src->sb);
    }
}

void ob_merge(outbuf_t *dst, outbuf_t *src)
{
    ob_merge_(dst, src, false);
}

void ob_merge_wipe(outbuf_t *dst, outbuf_t *src)
{
    ob_merge_(dst, src, true);
}

void ob_merge_delete(outbuf_t *dst, outbuf_t **srcp)
{
    if (*srcp) {
        ob_merge_(dst, *srcp, true);
        p_delete(srcp);
    }
}

void ob_wipe(outbuf_t *ob)
{
    while (!htlist_is_empty(&ob->chunks_list)) {
        outbuf_chunk_t *obc = htlist_pop_entry(&ob->chunks_list, outbuf_chunk_t, chunks_link);

        ob_chunk_delete(&obc);
    }
    sb_wipe(&ob->sb);
}

static int sb_xread(sb_t *sb, int fd, int size)
{
    void *p = sb_grow(sb, size);
    int res = xread(fd, p, size);

    __sb_fixlen(sb, sb->len + (res < 0 ? 0 : size));
    return res;
}
int ob_xread(outbuf_t *ob, int fd, int size)
{
    RETHROW(sb_xread(&ob->sb, fd, size));
    ob->sb_trailing += size;
    ob->length      += size;
    return 0;
}

int ob_add_file(outbuf_t *ob, const char *file, int size, bool use_mmap)
{
    int fd = RETHROW(open(file, O_RDONLY));

    if (size < 0) {
        struct stat st;

        if (fstat(fd, &st)) {
            PROTECT_ERRNO(close(fd));
            return -1;
        }
        size = st.st_size;
    }

    if (size <= OUTBUF_CHUNK_MIN_SIZE) {
        if (ob_xread(ob, fd, size)) {
            PROTECT_ERRNO(close(fd));
            return -1;
        }
        close(fd);
        return 0;
    }

    if (use_mmap) {
        void *map = mmap(NULL, size, PROT_READ, MAP_SHARED, fd, 0);

        PROTECT_ERRNO(close(fd));
        if (map == MAP_FAILED)
            return -1;
        madvise(map, size, MADV_SEQUENTIAL);
        ob_add_memmap(ob, map, size);
    } else {
        ob_add_sendfile(ob, fd, 0, size, true);
    }
    return 0;
}

static int ob_consume(outbuf_t *ob, int len)
{
    ob->length -= len;

    while (!htlist_is_empty(&ob->chunks_list)) {
        outbuf_chunk_t *obc = htlist_first_entry(&ob->chunks_list, outbuf_chunk_t, chunks_link);

        if (len < obc->sb_leading) {
            sb_skip(&ob->sb, len);
            obc->sb_leading -= len;
            return 0;
        }
        if (obc->sb_leading) {
            sb_skip(&ob->sb, obc->sb_leading);
            len -= obc->sb_leading;
            obc->sb_leading = 0;
        }
        if (obc->offset + len < obc->length) {
            obc->offset += len;
            return 0;
        }
        len -= (obc->length - obc->offset);

        htlist_pop(&ob->chunks_list);
        ob_chunk_delete(&obc);
    }

    assert (len <= ob->sb_trailing);
    sb_skip(&ob->sb, len);
    ob->sb_trailing -= len;
    return 0;
}

int ob_write(outbuf_t *ob, int fd)
{
    struct iovec iov[IOV_MAX];
    int iovcnt = 0, sb_pos = 0;
    ssize_t res;
    off_t offs;

    if (!ob->length)
        return 0;

    htlist_for_each(it, &ob->chunks_list) {
        outbuf_chunk_t *obc = htlist_entry(it, outbuf_chunk_t, chunks_link);

        if (unlikely(iovcnt + 2 >= countof(iov)))
            break;

        if (obc->sb_leading) {
            iov[iovcnt++] = MAKE_IOVEC(ob->sb.data + sb_pos, obc->sb_leading);
            sb_pos += obc->sb_leading;
        }
        if (obc->outbuf_chunk_kind == OUTBUF_CHUNK_KIND_PTR) {
            iov[iovcnt++] = MAKE_IOVEC(obc->u.b + obc->offset,
                                       obc->length - obc->offset);
            continue;
        }
        if (iovcnt) {
            res = RETHROW(writev(fd, iov, iovcnt));
        } else
        switch (obc->outbuf_chunk_kind) {
          case OUTBUF_CHUNK_KIND_SENFILE:
            offs = obc->offset;
            res  = RETHROW(sendfile(fd, obc->u.fd, &offs,
                                    obc->length - obc->offset));
            break;

          default:
            res = 0;
        }
        return ob_consume(ob, res);
    }
    if (ob->sb_trailing) {
        iov[iovcnt++] = MAKE_IOVEC(ob->sb.data + sb_pos, ob->sb_trailing);
        assert (ob->sb.len == sb_pos + ob->sb_trailing);
    }
    res = RETHROW(writev(fd, iov, iovcnt));
    return ob_consume(ob, res);
}
