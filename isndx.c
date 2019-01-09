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

#include "isndx.h"

/* TODO:
 *
 * - deal with empty keys
 * - test
 * - simplify
 * - optimize fixed len data
 * - optimize 3 byte int data
 * - optimize fixed len keys
 * - optimize fixed len records keys
 * - test unsorted page schemes
 * - test one level dispatch scheme
 */

/* Page layout:
 *
 * 0 : 1 byte level
 * 1 : 1 byte size of last record (3 or 6)
 * 2 : 2 byte pagelen
 * 4 : compacted key/value data:
 *     1 [common] byte number of bytes of key shared with previous key
 *     1 [suffix] byte number of bytes of key different from previous key
 *     1 [datalen] byte number of bytes of data
 *     suffix key bytes
 *     datalen data bytes
 *
 * The last entry of a page has a zero length key and may not contain
 * data if in the rightmost page of a node level. This is also
 * indicated by the flag HAS_OVERFLOW.
 */

/* XXX: endianness issue */
#define NDX_GET_PAGELEN(page)       (*(const uint16_t *)((page) + 2))
#define NDX_SET_PAGELEN(page, len)  (*(uint16_t *)((page) + 2) = len)

#define NDX_GET_PAGENO(p)      \
        ((p)[0] | ((p)[1] << 8) | ((p)[2] << 16))
#define NDX_SET_PAGENO(p, n)   \
        ((p)[0] = (n), (p)[1] = (n) >> 8, (p)[2] = (n) >> 16)

typedef struct scan_state_t {
    int offset, exact, common_prev, common_next;
} scan_state_t;

typedef struct insert_state_t {
    struct {
        uint32_t pageno;
        int offset, common_prev, common_next;
    } cache[MAX_DEPTH];
} insert_state;

#define ISNDX_ERROR(...)  isndx_error(__VA_ARGS__)

static int isndx_error(isndx_t *ndx, const char *format, ...)
        __attr_printf__(2,3);

static int isndx_error(isndx_t *ndx, const char *format, ...)
{
    va_list args;

    va_start(args, format);
    ndx->error_code = 1;
    if (ndx->error_stream) {
        fprintf(ndx->error_stream, "isndx: ");
        vfprintf(ndx->error_stream, format, args);
        fprintf(ndx->error_stream, "\n");
    } else {
        vsnprintf(ndx->error_buf, sizeof(ndx->error_buf), format, args);
    }
    va_end(args);

    return -1;
}

int isndx_get_error(isndx_t *ndx, char *buf, int size)
{
    pstrcpy(buf, size, ndx->error_buf);
    return ndx->error_code;
}

FILE *isndx_set_error_stream(isndx_t *ndx, FILE *stream)
{
    FILE *previous = ndx->error_stream;
    ndx->error_stream = stream;
    return previous;
}

static byte *isndx_getpage(const isndx_t *ndx, uint32_t pageno)
{
    return (byte *)ndx->file->area + (pageno << ndx->pageshift);
}

static byte *isndx_newpage(isndx_t *ndx, uint32_t *ppageno)
{
    uint32_t newpageno = ndx->file->area->nbpages;
    ssize_t newfilesize = (ssize_t)(newpageno + 1) << ndx->pageshift;

    if (newfilesize > ndx->file->size) {
        /* Must extend file */
        if (isndx_real_truncate(ndx->file, ndx->file->size +
                                NB_PAGES_GROW * 4096) < 0)
        {
            ISNDX_ERROR(ndx, "Truncate failed");
            return NULL;
        }
    }
    ndx->file->area->nbpages++;
    *ppageno = newpageno;
    return isndx_getpage(ndx, newpageno);
}

isndx_t *isndx_create(const char *path, isndx_create_parms_t *cp)
{
    isndx_t *ndx = p_new(isndx_t, 1);
    int pageshift, pagesize, root;
    byte *page;

    pageshift = (cp && cp->pageshift) ? cp->pageshift : ISNDX_PAGESHIFT;
    pagesize = 1 << pageshift;
    root = 1;

    ndx->file = isndx_real_creat(path, ROUND_SHIFT(pagesize * 2, 12));
    if (!ndx->file) {
        p_delete(&ndx);
        return NULL;
    }

    memcpy(ndx->file->area->magic, ISNDX_MAGIC, 4);
    ndx->file->area->major      = ISNDX_MAJOR;
    ndx->file->area->minor      = ISNDX_MINOR;
    ndx->file->area->pageshift  = ndx->pageshift = pageshift;
    ndx->file->area->pagesize   = ndx->pagesize  = pagesize;
    ndx->file->area->root       = ndx->root      = root;
    ndx->file->area->rootlevel  = ndx->rootlevel = 0;
    ndx->file->area->nbpages    = 2;
    ndx->file->area->nbkeys     = 0;
    ndx->file->area->minkeylen  = cp ? cp->minkeylen  : 1;
    ndx->file->area->maxkeylen  = cp ? cp->maxkeylen  : MAX_KEYLEN;
    ndx->file->area->mindatalen = cp ? cp->mindatalen : 0;
    ndx->file->area->maxdatalen = cp ? cp->maxdatalen : MAX_DATALEN;

    page = isndx_getpage(ndx, root);

    page[0] = 0;        /* leaf page */
    page[1] = 3;        /* empty last record */
    NDX_SET_PAGELEN(page, 7);
    page[4] = 0;        /* no common key bytes */
    page[5] = 0;        /* no suffix key bytes */
    page[6] = 0;        /* no data bytes */

    return ndx;
}

isndx_t *isndx_open(const char *path, int flags)
{
    isndx_t *ndx;
    int res;

    /* Create or truncate the index if opening for write and:
     * - it does not exist and flag O_CREAT is given
     * - or it does exist and flag O_TRUNC is given
     * bug: O_EXCL is not supported as it is not passed to btree_creat.
     */
    res = access(path, F_OK);
    if (O_ISWRITE(flags)) {
        if ((res && (flags & O_CREAT)) || (!res && (flags & O_TRUNC)))
            return isndx_create(path, NULL);
    }

    if (res) {
        errno = ENOENT;
        return NULL;
    }

    ndx = p_new(isndx_t, 1);
    ndx->file = isndx_real_open(path, flags & ~(O_CREAT | O_TRUNC), 0, 0);
    if (!ndx->file) {
        p_delete(&ndx);
        return NULL;
    }

    /* Only perform quick check */
    if (isndx_check(ndx, 0)) {
        /* Bad magic */
        e_error("Corrupted index file: %s", path);
        isndx_real_close(&ndx->file);
        p_delete(&ndx);
        return NULL;
    }

    ndx->pageshift = ndx->file->area->pageshift;
    ndx->pagesize  = ndx->file->area->pagesize;
    ndx->root      = ndx->file->area->root;
    ndx->rootlevel = ndx->file->area->rootlevel;

    return ndx;
}

void isndx_close(isndx_t **ndxp)
{
    if (*ndxp) {
        isndx_real_close(&(*ndxp)->file);
        p_delete(ndxp);
    }
}

static int isndx_scan(isndx_t *ndx, const byte *page,
                      const byte *key, int keylen, int offset,
                      scan_state_t *sst)
{
    int common, comm2, pagelen;
    const byte *p, *p1, *p2, *p3;

    p = page + 4;
    pagelen = NDX_GET_PAGELEN(page);
    p3 = page + pagelen - page[1];
    common = comm2 = 0;

    while (p < p3) {
        common = comm2;
        if (p[0] > comm2) {
            /* current key is smaller */
            p += 3 + p[1] + p[2];
            continue;
        }
        if (p[0] < comm2) {
            /* current key is larger */
            comm2 = p[0];
            sst->exact = false;
            sst->common_prev = common;
            sst->common_next = comm2;
            sst->offset = p - page;
            return 0;
        }
        p1 = p + 3;     /* key bytes */
        p2 = p1 + p[1]; /* data bytes */
        if (p2 + p[2] > p3) {
            /* corrupted page */
            break;
        }
#if 0
        for (; p1 < p2 && comm2 < keylen; comm2++, p1++) {
            if (*p1 != key[comm2])
                break;
        }
        if (comm2 < keylen) {
            if (p1 == p2 || *p1 < key[comm2]) {
                /* current key is smaller */
                p += 3 + p[1] + p[2];
                continue;
            }
            /* current key is larger */
            sst->exact = false;
            sst->common_prev = common;
            sst->common_next = comm2;
            sst->offset = p - page;
            return 0;
        }
        if (p1 == p2) {
            /* found key */
            if ((int)(p - page) < offset) {
                /* continue until correct offset */
                p += 3 + p[1] + p[2];
                continue;
            }
            sst->exact = true;
            sst->common_prev = common;
            sst->common_next = comm2;
            sst->offset = p - page;
            return 0;
        } else {
            /* current key is larger */
            sst->exact = false;
            sst->common_prev = common;
            sst->common_next = comm2;
            sst->offset = p - page;
            return 0;
        }
#else
        for (;; comm2++, p1++) {
            if (p1 == p2) {
                /* current key is a prefix of scan target */
                if (comm2 == keylen) {
                    /* exact match */
                    if ((int)(p - page) >= offset) {
                        /* stop only if target offset was reached */
                        sst->common_prev = common;
                        sst->common_next = comm2;
                        sst->offset = p - page;
                        sst->exact = true;
                        return 0;
                    }
                }
                /* current key is smaller, keep scanning */
                p += 3 + p[1] + p[2];
                break;
            }
            if (comm2 >= keylen) {
                /* current key is larger than scan target */
                sst->exact = false;
                sst->common_prev = common;
                sst->common_next = comm2;
                sst->offset = p - page;
                return 0;
            }
            if (*p1 != key[comm2]) {
                if (*p1 < key[comm2]) {
                    /* current key is smaller than scan target */
                    p += 3 + p[1] + p[2];
                    break;
                } else {
                    /* current key is larger */
                    sst->exact = false;
                    sst->common_prev = common;
                    sst->common_next = comm2;
                    sst->offset = p - page;
                    return 0;
                }
            }
        }
#endif
    }
    if (p == p3) {
        /* current key is block tail */
        /* XXX: should test for RIGHTMOST page:
         * (pageno == 1 || page[1] == 6)
         * but pageno is not known here and passing it is wasteful for
         * just this border case.  There should be a flag in the page
         * header to mark RIGHTMOST pages.
         */
        common = comm2;
        comm2 = 0;
        sst->exact = false;
        sst->common_prev = common;
        sst->common_next = comm2;
        sst->offset = p - page;
        return 0;
    }
    return ISNDX_ERROR(ndx, "Corrupted page in isndx_scan()");
}

static int isndx_fetch1(isndx_t *ndx,
                        int level, uint32_t pageno,
                        const byte *key, int keylen, sb_t *out)
{
    const byte *page, *p;
    scan_state_t sst;
    int res, found;

    if (pageno < 1 || pageno >= ndx->file->area->nbpages) {
        return ISNDX_ERROR(ndx, "Invalid pageno %u", pageno);
    }
    page = isndx_getpage(ndx, pageno);
    if (*page != level) {
        return ISNDX_ERROR(ndx, "Incorrect page level %d != %d", *page, level);
    }

    /* Scan for the key, stop at first exact match */
    if (isndx_scan(ndx, page, key, keylen, 0, &sst) < 0) {
        return -1;      /* corrupted page */
    }

    found = 0;
    if (level > 0) {
        /* This is a node page */
        for (;;) {
            p = page + sst.offset;
            p += 3 + p[1];
            pageno = NDX_GET_PAGENO(p);
            res = isndx_fetch1(ndx, level - 1, pageno, key, keylen, out);
            if (res < 0) {
                return res;
            }
            found += res;

            /* Keep scanning while match is exact */
            if (!sst.exact)
                break;

            p = page + sst.offset;
            sst.offset += 3 + p[1] + p[2];
            p = page + sst.offset;
            if (p[2] == 0) {
                /* rightmost page of a node level */
                break;
            }
            sst.exact = (p[0] == keylen && p[1] == 0);
        }
    } else {
        /* This is a leaf page */
        while (sst.exact) {
            found++;
            p = page + sst.offset;
            sb_add(out, p + 3 + p[1], p[2]);
            sst.offset += 3 + p[1] + p[2];
            p = page + sst.offset;
            sst.exact = (p[0] == keylen && p[1] == 0);
        }
    }
    /* Should provide continuation indicator to caller */
    return found;
}

int isndx_fetch(isndx_t *ndx, const byte *key, int keylen, sb_t *out)
{
    uint32_t pageno = ndx->file->area->root;
    uint32_t level = ndx->file->area->rootlevel;

    if (keylen < ndx->file->area->minkeylen
    ||  keylen > ndx->file->area->maxkeylen) {
        return ISNDX_ERROR(ndx, "Invalid key length %d", keylen);
    }

    return isndx_fetch1(ndx, level, pageno, key, keylen, out);
}

static int isndx_insert_one(isndx_t *ndx, insert_state *ist, int level,
                             const byte *key, int keylen,
                             const void *data, int datalen);

static int isndx_splitpage(isndx_t *ndx, insert_state *ist,
                            int level, int offset, int chunk)
{
    byte key2[512];
    byte data2[4];
    int key2len, common, split, shift, pagelen, newpagelen;
    byte *page, *newpage, *p, *q, *p1;
    uint32_t newpageno;
    uint32_t pageno = ist->cache[level].pageno;

    newpage = isndx_newpage(ndx, &newpageno);
    if (!newpage)
        return -1;

    page = isndx_getpage(ndx, pageno);

    pagelen = NDX_GET_PAGELEN(page);

    p = page + 4;
    /* Compute split spot with bias to account for pending insert */
    split = pagelen >> 1;
    if (offset < split)
        split -= chunk >> 1;
    else
        split += chunk >> 1;

    /* Special case splitting rightmost page on offset == pagelen - 3 */
    if (pageno == 1 && offset == pagelen - 3) {
        /* XXX: should create new page to the right of this one:
         * indexing in order would create an optimal ordered index
         */
        split = pagelen - 3;
    }

    /* Scan left part to extract key for new page and actual split position */
    p1 = page + split;
    do {
        /* update key */
        memcpy(key2 + p[0], p + 3, p[1]);
        key2len = p[0] + p[1];
        p += 3 + p[1] + p[2];
    } while (p < p1);
    split = p - page;

    /* Copy left part to new page */
    memcpy(newpage, page, split);
    q = newpage + split;
    q[0] = 0;
    q[1] = 0;
    q[2] = 0;
    newpagelen = q + 3 - newpage;
    newpage[1] = 3;     /* empty last record */
    NDX_SET_PAGELEN(newpage, newpagelen);

    /* Insert new key at upper level */
    if (level == ndx->file->area->rootlevel) {
        /* Create a new empty root page */
        uint32_t root;
        byte *rootpage = isndx_newpage(ndx, &root);

        if (!rootpage)
            return ISNDX_ERROR(ndx, "Cannot allocate root page");

        p = rootpage;
        p[0] = level + 1;
        p[1] = 6;       /* last record overflow */
        NDX_SET_PAGELEN(p, 4 + 6);
        p += 4;
        p[0] = 0;
        p[1] = 0;
        p[2] = 3;
        NDX_SET_PAGENO(p + 3, pageno);

        ist->cache[level + 1].pageno = root;
        ist->cache[level + 1].common_prev = 0;
        ist->cache[level + 1].common_next = 0;
        ist->cache[level + 1].offset = 4;

        ndx->file->area->rootlevel = level + 1;
        ndx->file->area->root      = root;
    }

    NDX_SET_PAGENO(data2, newpageno);

    isndx_insert_one(ndx, ist, level + 1, key2, key2len, data2, 3);

    /* Shift contents of old part in old page */
    page = isndx_getpage(ndx, pageno);
    p = page + split;
    q = page + 4;
    common = p[0];
    q[0] = 0;
    q[1] = common + p[1];
    q[2] = p[2];
    pagelen = 4 + common + (pagelen - split);
    NDX_SET_PAGELEN(page, pagelen);
    shift = p + 3 - (q + 3 + common);
    memmove(q + 3 + common, p + 3, pagelen - 4 - 3 - common);
    memcpy(q + 3, key2, common);

    /* Update ist.cache[level] */
    if (offset < newpagelen - 3) {
        ist->cache[level].pageno = newpageno;
    } else
    if (offset == newpagelen - 3) {
        ist->cache[level].offset = 4;
        ist->cache[level].common_prev = 0;
    } else {
        ist->cache[level].offset -= shift;
    }
    return 0;
}

static int isndx_insert_one(isndx_t *ndx, insert_state *ist, int level,
                            const byte *key, int keylen,
                            const void *data, int datalen)
{
    scan_state_t sst;
    int pagelen, needed, cmp2, chunk;
    byte *page, *p;

    for (;;) {
        /* Compute insert spot */
        page = isndx_getpage(ndx, ist->cache[level].pageno);
        pagelen = NDX_GET_PAGELEN(page);
        if (level) {
            /* We should update key share info without rescanning,
             * but we would need both keys for that and it is complex.
             * We need to rescan to compute actual key share info.
             * pass offset to get the right slot in case of multiple
             * identical keys.
             */
            RETHROW(isndx_scan(ndx, page, key, keylen,
                    ist->cache[level].offset, &sst));
        } else {
            sst.offset = ist->cache[0].offset;
            sst.common_prev = ist->cache[0].common_prev;
            sst.common_next = ist->cache[0].common_next;
        }
        p = page + sst.offset;
        cmp2 = sst.common_next - p[0];
        chunk = 3 + (keylen - sst.common_prev) + datalen;
        needed = chunk - cmp2;
        if (pagelen + needed <= (int)ndx->pagesize)
            break;

        if (isndx_splitpage(ndx, ist, level, sst.offset, chunk) < 0)
            return ISNDX_ERROR(ndx, "Cannot split page");
    }
    /* Insert entry */
    memmove(p + needed, p, pagelen - sst.offset);
    p[chunk + 2] = p[2];
    p[chunk + 1] = p[1] - cmp2;
    p[chunk + 0] = p[0] + cmp2;
    p[0] = sst.common_prev;
    p[1] = keylen - sst.common_prev;
    p[2] = datalen;
    memcpy(p + 3, key + sst.common_prev, keylen - sst.common_prev);
    memcpy(p + 3 + keylen - sst.common_prev, data, datalen);
    pagelen += needed;
    NDX_SET_PAGELEN(page, pagelen);
    ndx->file->area->nbkeys += !level;
    return 0;
}

int isndx_push(isndx_t *ndx, const byte *key, int keylen,
                const void *data, int datalen)
{
    insert_state ist;
    uint32_t pageno;
    const byte *page, *p;
    int level;
    scan_state_t sst;

    if (keylen < ndx->file->area->minkeylen
    ||  keylen > ndx->file->area->maxkeylen) {
        return ISNDX_ERROR(ndx, "Invalid key length %d", keylen);
    }

    if (datalen < ndx->file->area->mindatalen
    ||  datalen > ndx->file->area->maxdatalen) {
        return ISNDX_ERROR(ndx, "Invalid data length %d", datalen);
    }

    pageno = ndx->file->area->root;
    level = ndx->file->area->rootlevel;

    for (;;) {
        if (pageno < 1 || pageno >= ndx->file->area->nbpages) {
            return ISNDX_ERROR(ndx, "Invalid pageno %u", pageno);
        }
        ist.cache[level].pageno = pageno;
        page = isndx_getpage(ndx, pageno);
        if (*page != level) {
            return ISNDX_ERROR(ndx, "Incorrect page level %d != %d", *page, level);
        }
        /* Scan and stop after all exact matches */
        if (isndx_scan(ndx, page, key, keylen, ndx->pagesize, &sst) < 0) {
            return -1;
        }
#if 0
        while (sst.exact) {
            /* comm2 == keylen */
            p = page + sst.offset;
            sst.offset += 3 + p[1] + p[2];
            p = page + sst.offset;
            sst.common_prev = sst.common_next;
            sst.common_next = p[0];
            sst.exact = (sst.common_next == keylen && p[1] == 0);
        }
#endif
        ist.cache[level].offset = sst.offset;
        ist.cache[level].common_prev = sst.common_prev;
        ist.cache[level].common_next = sst.common_next;

        if (level == 0)
            break;

        p = page + sst.offset;
        p += 3 + p[1];
        pageno = NDX_GET_PAGENO(p);
        level--;
    }
    return isndx_insert_one(ndx, &ist, 0, key, keylen, data, datalen);
}

static int isndx_enumerate_page(isndx_t *ndx, uint32_t pageno, int level,
                                int upkeylen, const byte *upkey, int flags,
                                void *opaque,
                                int (*cb)(void *opaque, isndx_t *ndx,
                                          const byte *key, int klen,
                                          const void *data, int len))
{
    byte key[2 * (MAX_KEYLEN + 1)];
    uint32_t childpageno;
    int pagelen, keylen;
    int status = 0;
    const byte *page, *p, *p1, *p2;
    int tail, nkeys;

    page = isndx_getpage(ndx, pageno);

    pagelen = NDX_GET_PAGELEN(page);

    if (level != page[0]) {
        status |= ISNDX_ERROR(ndx, "page %u: invalid level=%d, expected=%d",
                              pageno, page[0], level);
        status = -1;
    }

    if (pagelen < 7 || pagelen > (int)ndx->file->area->pagesize) {
        status |= ISNDX_ERROR(ndx, "page %u: invalid pagelen=%d",
                              pageno, pagelen);
        status = -1;
        return status;
    }

    tail = ((flags & ISNDX_CHECK_ISRIGHTMOST) && level) ? 6 : 3;

    if (page[1] != tail) {
        status |= ISNDX_ERROR(ndx, "page %u: incorrect page tail=%d,"
                              " expected=%d",
                              pageno, page[1], tail);
    }

    nkeys = 0;
    p = page + 4;
    p2 = page + pagelen - tail;
    keylen = 0;

    while (p < p2) {
        p1 = p + 3 + p[1] + p[2];
        if (p1 > p2) {
            status |= ISNDX_ERROR(ndx, "page %u:%d: key data overflow",
                                  pageno, (int)(p - page));
        }
        if (p[0] > keylen) {
            status |= ISNDX_ERROR(ndx, "page %u:%d: incorrect common=%d keylen=%d",
                                  pageno, (int)(p - page), p[0], keylen);
        }
        if (p[0] + p[1] == 0) {
            status |= ISNDX_ERROR(ndx, "page %u:%d: invalid empty key",
                                  pageno, (int)(p - page));
        }
        if (p[0] + p[1] > MAX_KEYLEN) {
            status |= ISNDX_ERROR(ndx, "page %u:%d: incorrect suffix=%d keylen=%d",
                                  pageno, (int)(p - page), p[1], p[0] + p[1]);
        }
        if (p[0] < keylen) {
            if (p[1] == 0 || p[3] < key[p[0]]) {
                status |= ISNDX_ERROR(ndx, "page %u:%d: key out of order",
                                      pageno, (int)(p - page));
            } else
            if (p[3] == key[p[0]]) {
                status |= ISNDX_ERROR(ndx, "page %u:%d: incorrect prefix",
                                      pageno, (int)(p - page));
            }
        }
        memcpy(key + p[0], p + 3, p[1]);
        keylen = p[0] + p[1];
        if (level > 0) {
            if (p[2] != 3) {
                status |= ISNDX_ERROR(ndx, "page %u:%d: incorrect datalen=%d",
                                      pageno, (int)(p - page), p[2]);
            }
            childpageno = NDX_GET_PAGENO(p + 3 + p[1]);
            if (childpageno < 1 || childpageno >= ndx->file->area->nbpages) {
                status |= ISNDX_ERROR(ndx, "page %u:%d: incorrect child page=%u",
                                      pageno, (int)(p - page), childpageno);
            } else {
                int res = isndx_enumerate_page(ndx, childpageno, level - 1,
                                               keylen, key,
                                               flags & ~ISNDX_CHECK_ISRIGHTMOST,
                                               opaque, cb);
                if (res == 1)
                    return 1;
                status |= res;
            }
        } else {
            nkeys++;
            if ((*cb)(opaque, ndx, key, keylen, p + 3 + p[1], p[2]))
                return 1;
        }
        p = p1;
    }
    if (p[0] != 0 || p[1] != 0 || p[2] != tail - 3) {
        status |= ISNDX_ERROR(ndx, "page %u:%d: invalid tail %d %d %d",
                              pageno, (int)(p - page), p[0], p[1], p[2]);
    }
    if (tail == 6) {
        childpageno = NDX_GET_PAGENO(p + 3);
        if (childpageno < 1 || childpageno >= ndx->file->area->nbpages) {
            status |= ISNDX_ERROR(ndx, "page %u:%d: incorrect child page=%u",
                                  pageno, (int)(p - page), childpageno);
        } else {
            int res = isndx_enumerate_page(ndx, childpageno, level - 1,
                                           upkeylen, upkey,
                                           flags | ISNDX_CHECK_ISRIGHTMOST,
                                           opaque, cb);
            if (res == 1)
                return 1;
            status |= res;
        }
    } else {
        if (upkeylen) {
            if (keylen != upkeylen || memcmp(key, upkey, keylen)) {
                status |= ISNDX_ERROR(ndx, "page %u:%d: upkey differs from last key"
                                      " '%.*s' != '%.*s'",
                                      pageno, (int)(p - page),
                                      keylen, key, upkeylen, upkey);
            }
        }
    }
    //ndx->nkeys += nkeys;
    //ndx->npages++;

    return status;
}

int isndx_enumerate(isndx_t *ndx, void *opaque,
                    int (*cb)(void *opaque, isndx_t *ndx,
                              const byte *key, int klen,
                              const void *data, int len))
{
    uint32_t root;
    int rootlevel;

    root = ndx->file->area->root;
    rootlevel = ndx->file->area->rootlevel;

    return isndx_enumerate_page(ndx, root, rootlevel,
                                0, NULL, ISNDX_CHECK_ISRIGHTMOST,
                                opaque, cb);
}

int isndx_check_page(isndx_t *ndx, uint32_t pageno, int level,
                     int upkeylen, const byte *upkey, int flags)
{
    byte key[2 * (MAX_KEYLEN + 1)];
    uint32_t childpageno;
    int pagelen, keylen;
    int status = 0;
    const byte *page, *p, *p1, *p2;
    int tail, nkeys;

    page = isndx_getpage(ndx, pageno);

    pagelen = NDX_GET_PAGELEN(page);

    if (level != page[0]) {
        status |= ISNDX_ERROR(ndx, "page %u: invalid level=%d, expected=%d",
                              pageno, page[0], level);
    }

    if (pagelen < 7 || pagelen > (int)ndx->file->area->pagesize) {
        status |= ISNDX_ERROR(ndx, "page %u: invalid pagelen=%d",
                              pageno, pagelen);
        return status;
    }

    tail = ((flags & ISNDX_CHECK_ISRIGHTMOST) && level) ? 6 : 3;

    if (page[1] != tail) {
        status |= ISNDX_ERROR(ndx, "page %u: incorrect page tail=%d,"
                              " expected=%d",
                              pageno, page[1], tail);
    }

    nkeys = 0;
    p = page + 4;
    p2 = page + pagelen - tail;
    keylen = 0;

    while (p < p2) {
        p1 = p + 3 + p[1] + p[2];
        if (p1 > p2) {
            status |= ISNDX_ERROR(ndx, "page %u:%d: key data overflow",
                                  pageno, (int)(p - page));
        }
        if (p[0] > keylen) {
            status |= ISNDX_ERROR(ndx, "page %u:%d: incorrect common=%d keylen=%d",
                                  pageno, (int)(p - page), p[0], keylen);
        }
        if (p[0] + p[1] == 0) {
            status |= ISNDX_ERROR(ndx, "page %u:%d: invalid empty key",
                                  pageno, (int)(p - page));
        }
        if (p[0] + p[1] > MAX_KEYLEN) {
            status |= ISNDX_ERROR(ndx, "page %u:%d: incorrect suffix=%d keylen=%d",
                                  pageno, (int)(p - page), p[1], p[0] + p[1]);
        }
        if (p[0] < keylen) {
            if (p[1] == 0 || p[3] < key[p[0]]) {
                status |= ISNDX_ERROR(ndx, "page %u:%d: key out of order",
                                      pageno, (int)(p - page));
            } else
            if (p[3] == key[p[0]]) {
                status |= ISNDX_ERROR(ndx, "page %u:%d: incorrect prefix",
                                      pageno, (int)(p - page));
            }
        }
        memcpy(key + p[0], p + 3, p[1]);
        keylen = p[0] + p[1];
        if (level > 0) {
            if (p[2] != 3) {
                status |= ISNDX_ERROR(ndx, "page %u:%d: incorrect datalen=%d",
                                      pageno, (int)(p - page), p[2]);
            }
            childpageno = NDX_GET_PAGENO(p + 3 + p[1]);
            if (childpageno < 1 || childpageno >= ndx->file->area->nbpages) {
                status |= ISNDX_ERROR(ndx, "page %u:%d: incorrect child page=%u",
                                      pageno, (int)(p - page), childpageno);
            } else {
                /* XXX: should check for duplicated pages */
                status |= isndx_check_page(ndx, childpageno, level - 1,
                                           keylen, key,
                                           flags & ~ISNDX_CHECK_ISRIGHTMOST);
            }
        } else {
            nkeys++;
        }
        p = p1;
    }
    if (p[0] != 0 || p[1] != 0 || p[2] != tail - 3) {
        status |= ISNDX_ERROR(ndx, "page %u:%d: invalid tail %d %d %d",
                              pageno, (int)(p - page), p[0], p[1], p[2]);
    }
    if (tail == 6) {
        childpageno = NDX_GET_PAGENO(p + 3);
        if (childpageno < 1 || childpageno >= ndx->file->area->nbpages) {
            status |= ISNDX_ERROR(ndx, "page %u:%d: incorrect child page=%u",
                                  pageno, (int)(p - page), childpageno);
        } else {
            /* XXX: should check for duplicated pages */
            status |= isndx_check_page(ndx, childpageno, level - 1,
                                       upkeylen, upkey,
                                       flags | ISNDX_CHECK_ISRIGHTMOST);
        }
    } else {
        if (upkeylen) {
            if (keylen != upkeylen || memcmp(key, upkey, keylen)) {
                status |= ISNDX_ERROR(ndx, "page %u:%d: upkey differs from last key"
                                      " '%.*s' != '%.*s'",
                                      pageno, (int)(p - page),
                                      keylen, key, upkeylen, upkey);
            }
        }
    }
    ndx->nkeys += nkeys;
    ndx->npages++;

    return status;
}

int isndx_check(isndx_t *ndx, int flags)
{
    uint32_t nbpages, root, nbkeys;
    int rootlevel, status = 0;

    if (*(uint32_t*)ndx->file->area->magic != *(uint32_t*)ISNDX_MAGIC) {
        return ISNDX_ERROR(ndx, "invalid magic number: %.4s",
                           ndx->file->area->magic);
    }
    if (ndx->file->area->major != ISNDX_MAJOR
    ||  ndx->file->area->minor != ISNDX_MINOR) {
        if (ndx->file->area->major == 0 && ndx->file->area->minor == 2) {
            STATIC_ASSERT(ISNDX_MAJOR == 1);
            STATIC_ASSERT(ISNDX_MINOR == 0);
            /* Accept version by auto upgrading (by doing nothing by the way) */
        } else {
            return ISNDX_ERROR(ndx, "invalid version number: %d.%d",
                               ndx->file->area->major, ndx->file->area->minor);
        }
    }
    if (ndx->file->area->pagesize < 256 || ndx->file->area->pagesize > 65536) {
        return ISNDX_ERROR(ndx, "invalid pagesize: %u",
                           ndx->file->area->pagesize);
    }
    if ((1LL << ndx->file->area->pageshift) != ndx->file->area->pagesize) {
        return ISNDX_ERROR(ndx, "invalid pagesize: %u != (1 << %u)",
                           ndx->file->area->pagesize,
                           ndx->file->area->pageshift);
    }
    nbpages = ndx->file->area->nbpages;
    if (((int64_t)nbpages << ndx->file->area->pageshift) > ndx->file->size) {
        return ISNDX_ERROR(ndx, "incorrect page number %u, filesize=%lu",
                           nbpages, (unsigned long)ndx->file->size);
    }
    root = ndx->file->area->root;
    rootlevel = ndx->file->area->rootlevel;
    nbkeys = ndx->file->area->nbkeys;

    if (root < 1 || root >= nbpages) {
        return ISNDX_ERROR(ndx, "incorrect root page %u, nbpages=%u",
                           root, nbpages);
    }
    if (rootlevel < 0 || rootlevel >= MAX_DEPTH) {
        return ISNDX_ERROR(ndx, "incorrect root level %d", rootlevel);
    }

    if (ndx->file->area->minkeylen < 1
    ||  ndx->file->area->maxkeylen > MAX_KEYLEN
    ||  ndx->file->area->maxkeylen < ndx->file->area->minkeylen) {
        status |= ISNDX_ERROR(ndx, "invalid key size range %d..%d",
                              ndx->file->area->minkeylen,
                              ndx->file->area->maxkeylen);
    }
    if (ndx->file->area->mindatalen < 0
    ||  ndx->file->area->maxdatalen > MAX_DATALEN
    ||  ndx->file->area->maxdatalen < ndx->file->area->mindatalen) {
        status |= ISNDX_ERROR(ndx, "invalid data size range %d..%d",
                              ndx->file->area->mindatalen,
                              ndx->file->area->maxdatalen);
    }

    if (flags & (ISNDX_CHECK_ALL | ISNDX_CHECK_PAGES)) {
        ndx->npages = 1;
        ndx->nkeys = 0;

        status |= isndx_check_page(ndx, root, rootlevel,
                                   0, NULL,
                                   flags | ISNDX_CHECK_ISRIGHTMOST);

        if (ndx->npages != nbpages) {
            /* XXX: should also keep track of multiple references */
            status |= ISNDX_ERROR(ndx, "%d lost pages",
                                  nbpages - ndx->npages);
        }
        if (ndx->nkeys != nbkeys) {
            status |= ISNDX_ERROR(ndx, "inconsistent key number: "
                                  "nbkeys=%u, actual=%u",
                                  nbkeys, ndx->nkeys);
        }
    }

    return status;
}

static void isndx_dump_key(isndx_t *ndx, const byte *key, int keylen, FILE *fp)
{
    int i, binary;

    for (binary = i = 0; i < keylen; i++) {
        if (key[i] < 0x20 || key[i] >= 0x7f)
            binary++;
    }
    if (binary) {
        /* key has binary bytes */
        if (keylen <= 8) {
            /* Print key as big endian number */
            int64_t num = 0;
            for (i = 0; i < keylen; i++) {
                num = (num << 8) | key[i];
            }
            /* Propagate sign bit */
            num <<= (keylen - 8) * 8;
            num >>= (keylen - 8) * 8;
            fprintf(fp, "%lld", (long long)num);
        } else {
            /* Print key as HEX */
            for (i = 0; i < keylen; i++) {
                fprintf(fp, " %02x", key[i]);
            }
        }
    } else {
        /* Print key as C string */
        //fprintf(fp, "\"%.*s\"", keylen, key);
        putc('\"', fp);
        for (i = 0; i < keylen; i++) {
            int c = key[i];
            if (c == '\\' || c == '\"') {
                putc('\\', fp);
                putc(c, fp);
            } else
            if (c >= ' ') {
                putc(c, fp);
            } else {
                fprintf(fp, "\\x%02x", c);
            }
        }
        putc('\"', fp);
    }
}

static void isndx_dump_data(isndx_t *ndx, const byte *p, int len, FILE *fp)
{
    int i, binary;

    for (binary = i = 0; i < len; i++) {
        if (p[i] < 0x20 || p[i] >= 0x7f)
            binary++;
    }
    if (binary) {
        /* data has binary bytes */
        if (len <= 8) {
            /* Print data as little endian number */
            int64_t num = 0;
            for (i = 0; i < len; i++) {
                num |= (int64_t)p[i] << (i << 3);
            }
            /* Propagate sign bit */
            num |= -(num & ((int64_t)1 << ((len << 3) - 1)));
            fprintf(fp, "%lld", (long long)num);
        } else {
            /* Print data as HEX */
            for (i = 0; i < len; i++) {
                fprintf(fp, " %02x", p[i]);
            }
        }
    } else {
        /* Print data as C string */
        //fprintf(fp, "\"%.*s\"", datalen, data);
        putc('\"', fp);
        for (i = 0; i < len; i++) {
            int c = p[i];
            if (c == '\\' || c == '\"') {
                putc('\\', fp);
                putc(c, fp);
            } else
            if (c >= ' ') {
                putc(c, fp);
            } else {
                fprintf(fp, "\\x%02x", c);
            }
        }
        putc('\"', fp);
    }
}

void isndx_dump_page(isndx_t *ndx, uint32_t pageno, int flags, FILE *fp)
{
    byte key[MAX_KEYLEN + 1];
    int pagelen, prefix, keylen, datalen;
    const byte *page, *p, *p1;

    page = isndx_getpage(ndx, pageno);
    pagelen = NDX_GET_PAGELEN(page);

    fprintf(fp, "Page %d: level=%d tail=%d, pagelen=%d\n",
            pageno, page[0], page[1], pagelen);

    if (flags & (ISNDX_DUMP_ALL | ISNDX_DUMP_KEYS)) {
        p = page + 4;
        p1 = page + pagelen;
        while (p < p1) {
            fprintf(fp, "    %d: key %d %d %d",
                    (int)(p - page), p[0], p[1], p[2]);

            prefix = p[0];
            keylen = p[0] + p[1];
            datalen = p[2];

            if (keylen > MAX_KEYLEN) {
                fprintf(fp, " invalid key length: %d\n", keylen);
                break;
            }
            memcpy(key + p[0], p + 3, p[1]);
            p += 3 + p[1];
            if (keylen > 0 && (keylen > prefix || (flags & ISNDX_DUMP_ALL))) {
                fprintf(fp, " ");
                isndx_dump_key(ndx, key, keylen, fp);
            }
            if (datalen > 0) {
                fprintf(fp, " -- ");
                isndx_dump_data(ndx, p, datalen, fp);
                p += datalen;
            }
            fprintf(fp, "\n");
            if (p != p1) {
                if (keylen < ndx->file->area->minkeylen
                ||  keylen > ndx->file->area->maxkeylen) {
                    fprintf(fp, " key length out of bounds: %d\n", keylen);
                }
                if (datalen < ndx->file->area->mindatalen
                ||  datalen > ndx->file->area->maxdatalen) {
                    fprintf(fp, " data length out of bounds: %d\n", datalen);
                }
            }
        }
        fprintf(fp, "\n");
    }
}

static int isndx_dump_fun(void *opaque, isndx_t *ndx,
                          const byte *key, int klen,
                          const void *data, int len)
{
    FILE *fp = opaque;

    fprintf(fp, "key: ");
    isndx_dump_key(ndx, key, klen, fp);
    fprintf(fp, ", data: ");
    isndx_dump_data(ndx, data, len, fp);
    fprintf(fp, "\n");

    return 0;
}

void isndx_dump(isndx_t *ndx, int flags, FILE *fp)
{
    uint32_t pageno;

    fprintf(fp, "NDX magic   : %.4s\n", (char*)&ndx->file->area->magic);
    fprintf(fp, "    version : %d.%d\n",
            ndx->file->area->major, ndx->file->area->minor);
    fprintf(fp, "    pagesize: %u (1 << %u)\n",
            ndx->file->area->pagesize, ndx->file->area->pageshift);
    fprintf(fp, "    rootpage: %u (level %d)\n",
            ndx->file->area->root, ndx->file->area->rootlevel);
    fprintf(fp, "    nbpages : %u\n", ndx->file->area->nbpages);
    fprintf(fp, "    nbkeys  : %u\n", ndx->file->area->nbkeys);
    fprintf(fp, "    keylen  : %d..%d\n",
            ndx->file->area->minkeylen,
            ndx->file->area->maxkeylen);
    fprintf(fp, "    datalen : %d..%d\n",
            ndx->file->area->mindatalen,
            ndx->file->area->maxdatalen);

    if (flags & ISNDX_DUMP_ENUMERATE) {
        fprintf(fp, "\n");
        isndx_enumerate(ndx, fp, isndx_dump_fun);
    }

    if (flags & (ISNDX_DUMP_ALL | ISNDX_DUMP_KEYS | ISNDX_DUMP_PAGES)) {
        fprintf(fp, "\n");
        for (pageno = 1; pageno < ndx->file->area->nbpages; pageno++) {
            isndx_dump_page(ndx, pageno, flags, fp);
        }
    }
}
