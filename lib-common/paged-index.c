/**************************************************************************/
/*                                                                        */
/*  Copyright (C) 2004-2007 INTERSEC SAS                                  */
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
#include <errno.h>
#include <limits.h>

#include <lib-common/err_report.h>
#include <lib-common/mem.h>
#include <lib-common/unix.h>

#include "paged-index.h"

#define O_ISWRITE(m)    (((m) & (O_RDONLY|O_WRONLY|O_RDWR)) != O_RDONLY)

static union {
    char     s[4];
    uint32_t i;
} const magic = { { 'I', 'S', 'P', 'F' } };

MMFILE_FUNCTIONS(pidx_file, pidx_real);

#define PIDX_SHIFT      8
#define PIDX_GROW       1024
#define PAYLOAD_SIZE    (PIDX_PAGE - 2 * ssizeof(int32_t))

/****************************************************************************/
/* whole file related functions                                             */
/****************************************************************************/

/* FIXME make this thread safe */
static void pidx_page_release(pidx_file *pidx, int32_t page)
{
    assert (page > 0 && page < pidx->area->nbpages);

#if 0
    if (!pidx->area->readers)
#else
    {
#endif
        pidx->area->pages[page].next = pidx->area->freelist;
        pidx->area->freelist = page;
        p_clear(pidx->area->pages[page].payload,
                countof(pidx->area->pages[page].payload));
    }
#if 0
    else
    {
        /* mark the page as to be released from version pidx->area->wr_ver */
    }
#endif
}

static int pidx_fsck_mark_page(byte *bits, pidx_file *pidx, int page)
{
    if (page < 0 || page > pidx->area->nbpages || TST_BIT(bits, page)) {
        return -1;
    }

    SET_BIT(bits, page);
    return 0;
}

static int pidx_fsck_recurse(byte *bits, pidx_file *pidx,
                             int page, int seglevel)
{
    /* OG: Should check that this page is unmarked, mark it and iterate
     * or recurse */
    if (seglevel) {
        int i;

        for (i = 0; i < PIDX_PAGE / 4; i++) {
            int pg = pidx->area->pages[page].refs[i];
            if (!pg)
                continue;

            if (pidx_fsck_mark_page(bits, pidx, pg))
                return -1;

            if (pidx_fsck_recurse(bits, pidx, pg, seglevel - 1) < 0)
                return -1;
        }
    } else {
        while ((page = pidx->area->pages[page].next) != 0) {
            if (pidx_fsck_mark_page(bits, pidx, page))
                return -1;
        }
    }

    return 0;
}

static void upgrade_to_1_1(pidx_file *pidx)
{
    /* NEW IN 1.1:
       readers, rd_ver and wr_ver
     */
    pidx->area->readers = 0;
    pidx->area->wr_ver  = 0;
    pidx->area->rd_ver  = 0;

    pidx->area->major = 1;
    pidx->area->minor = 1;
}

/** \brief checks and repair idx files.
 * \param    pidx     the paginated index file to check/fix.
 * \param    dofix
 *     what shall be done with @pidx:
 *         - 0 means check only.
 *         - 1 means fix if necessary.
 *         - 2 means assume broken and fix.
 * \return
 *   - 0 if check is sucessful.
 *   - 1 if the file was fixed (modified) but that the result is a valid file.
 *   - -1 if the check failed and that either the file is not fixable or that
 *        fixing it was not allowed.
 */
static int pidx_fsck(pidx_file *pidx, int dofix)
{
    bool did_a_fix = false;
    int version;

    if (pidx->size % PIDX_PAGE || pidx->size < 2 * PIDX_PAGE
    ||  ((uint64_t)pidx->size > (uint64_t)INT_MAX * PIDX_PAGE))
        return -1;

    if (pidx->area->wrlock) {
        struct timeval tv;

        if (pid_get_starttime(pidx->area->wrlock, &tv))
            return -1;

        if (pidx->area->wrlockt != (((int64_t)tv.tv_sec << 32) | tv.tv_usec))
            return -1;

        dofix = 0;
    }

    if (pidx->area->magic != magic.i)
        return -1;

    if (pidx->area->nbsegs < 1 || pidx->area->nbsegs > 6
    ||  pidx->area->skip + PIDX_SHIFT * pidx->area->nbsegs > 64)
    {
        return -1;
    }

    /* hook conversions here ! */
    version = PIDX_MKVER(pidx->area->major, pidx->area->minor);
    if (version < PIDX_MKVER(1, 0) || version > PIDX_VERSION)
        return -1;

    if (version != PIDX_VERSION && !dofix)
        return -1;

    if (version < PIDX_MKVER(1, 1)) {
        did_a_fix = true;
        upgrade_to_1_1(pidx);
    }

    if (pidx->area->nbpages != pidx->size / PIDX_PAGE - 1) {
        if (!dofix)
            return -1;

        did_a_fix = true;
        e_error("`%s': corrupted header (nbpages was %d, fixed into %d)",
                pidx->path, pidx->area->nbpages,
                (int)(pidx->size / PIDX_PAGE - 1));
        pidx->area->nbpages = pidx->size / PIDX_PAGE - 1;
    }

    if (pidx->area->freelist < 0
    || pidx->area->freelist >= pidx->area->nbpages)
    {
        if (!dofix)
            return -1;

        e_error("`%s': corrupted header (freelist out of bounds). "
                "force fsck.", pidx->path);
        pidx->area->freelist = 0;
    }

    {
        /* used to store seen pages */
        byte *bits = p_new(byte, (pidx->area->nbpages + 7) / 8);
        int *prev;

        /* check used pages */
        if (pidx_fsck_recurse(bits, pidx, 0, pidx->area->nbsegs) < 0)
            return -1;

        /* check free pages */
        for (prev = &pidx->area->freelist;
             *prev;
             prev = &pidx->area->pages[*prev].next)
        {
            if (pidx_fsck_mark_page(bits, pidx, *prev)) {
                if (!dofix)
                    return -1;

                did_a_fix = true;
                *prev = 0;
                break;
            }
        }

        /* collect lost pages.
           Only do it on dofix as it's a mild problem */
        if (dofix) {
            int page = pidx->area->nbpages - 1;

            while (page > 0) {
                if ((page & 7) == 7 && bits[page >> 3] == 0xff) {
                    page -= 8;
                } else {
                    if (!TST_BIT(bits, page)) {
                        pidx_page_release(pidx, page);
                        did_a_fix = true;
                    }
                    page--;
                }
            }
        }

        p_delete(&bits);
    }

    return did_a_fix;
}

pidx_file *pidx_open(const char *path, int flags, uint8_t skip, uint8_t nbsegs)
{
    pidx_file *pidx;
    int res;

    /* Create or truncate the index if opening for write and:
     * - it does not exist and flag O_CREAT is given
     * - or it does exist and flag O_TRUNC is given
     * bug: O_EXCL is not supported as it is not passed to btree_creat.
     */
    res = access(path, F_OK);
    if (O_ISWRITE(flags)) {
        if ((res && (flags & O_CREAT)) || (!res && (flags & O_TRUNC)))
            return pidx_creat(path, skip, nbsegs);
    }

    if (res) {
        errno = ENOENT;
        return NULL;
    }

    pidx = pidx_real_open(path, flags);
    if (!pidx)
        return NULL;

    res = pidx_fsck(pidx, O_ISWRITE(flags));
    if (res < 0) {
        /* lock check failed, file was not closed properly */
        e_error("Cannot open '%s': already locked", path);
        pidx_close(&pidx);
        errno = EINVAL;
        return NULL;
    }

    if (res > 0) {
        e_error("`%s': corrupted pages, Repaired.", path);
    }

    if (O_ISWRITE(flags)) {
        struct timeval tv;
        pid_t pid = getpid();

        if (pidx->area->wrlock) {
            pidx_close(&pidx);
            errno = EDEADLK;
            return NULL;
        }

        /* OG: problem if file is open for write multiple times from
         * the same pid: the first close will unlock it
         */
        pidx->area->wrlock = pid;
        /* OG: why not patch wrlockt at the same time ?
         * should have a single 64 bit entry with both pid and time
         */
        msync(pidx->area, pidx->size, MS_SYNC);
        if (pidx->area->wrlock != pid) {
            pidx_close(&pidx);
            errno = EDEADLK;
            return NULL;
        }
        pid_get_starttime(pid, &tv);
        pidx->area->wrlockt = ((int64_t)tv.tv_sec << 32) | tv.tv_usec;
        msync(pidx->area, pidx->size, MS_SYNC);
    }

    return pidx;
}

pidx_file *pidx_creat(const char *path, uint8_t skip, uint8_t nbsegs)
{
    struct timeval tv;
    pidx_file *pidx;
    int nbpages = PIDX_GROW - 1;
    pid_t pid = getpid();

    if (nbsegs > 64 / PIDX_SHIFT || skip + PIDX_SHIFT * nbsegs > 64) {
        errno = EINVAL;
        return NULL;
    }

    assert (sizeof(pidx_page) == sizeof(pidx_t));

    pidx = pidx_real_creat(path, (nbpages + 1) * PIDX_PAGE);
    if (!pidx)
        return NULL;

    pidx->area->magic    = magic.i;
    pidx->area->major    = PIDX_MAJOR;
    pidx->area->minor    = PIDX_MINOR;
    pidx->area->skip     = skip;
    pidx->area->nbsegs   = nbsegs;
    pidx->area->nbpages  = nbpages;
    pidx->area->readers  = 0;
    pidx->area->wr_ver   = 0;
    pidx->area->rd_ver   = 0;

    /* this creates the links: 1 -> 2 -> ... -> nbpages - 1 -> NIL */
    while (--nbpages > 1) {
        pidx->area->pages[nbpages - 1].next = nbpages;
    }
    pidx->area->freelist = 1;

    pid_get_starttime(pid, &tv);
    pidx->area->wrlock = pid;
    /* OG: same remark as above */
    msync(pidx->area, pidx->size, MS_SYNC);
    if (pidx->area->wrlock != pid) {
        pidx_close(&pidx);
        errno = EDEADLK;
        return NULL;
    }
    pidx->area->wrlock  = pid;
    pidx->area->wrlockt = ((int64_t)tv.tv_sec << 32) | tv.tv_usec;
    msync(pidx->area, pidx->size, MS_SYNC);

    return pidx;
}

void pidx_close(pidx_file **f)
{
    if (*f) {
        if ((*f)->area->wrlock && !(*f)->ro) {
            pid_t pid = getpid();

            if ((*f)->area->wrlock == pid) {
                struct timeval tv;

                pid_get_starttime(pid, &tv);
                if ((*f)->area->wrlockt ==
                    (((int64_t)tv.tv_sec << 32) | tv.tv_usec))
                {
                    msync((*f)->area, (*f)->size, MS_SYNC);
                    (*f)->area->wrlock  = 0;
                    (*f)->area->wrlockt = 0;
                } else {
                    /* OG: if same pid but different starttime, should
                     * unlock as well!
                     */
                }
            }
        }
        pidx_real_close(f);
    }
}

int pidx_clone(pidx_file *pidx, const char *filename)
{
    FILE *f;
    int to_write;
    int res = 0;
    pidx_t hdr;

    f = fopen(filename, "wb");
    if (!f) {
        return -1;
    }

    memcpy(&hdr, pidx->area, sizeof(hdr));
    hdr.wrlock  = 0;
    hdr.wrlockt = 0;
    hdr.readers = 0;
    hdr.rd_ver  = 0;
    hdr.wr_ver  = 0;

    to_write = sizeof(hdr);
    while (to_write > 0) {
        int i = fwrite((byte *)&hdr + (sizeof(hdr) - to_write),
                       1, to_write, f);
        if (i <= 0) {
            res = -1;
            goto exit;
        }
        to_write -= i;
    }

    to_write = pidx->size - sizeof(hdr);
    while (to_write > 0) {
        int i = fwrite((byte *)pidx->area + (pidx->size - to_write),
                       1, to_write, f);
        if (i <= 0) {
            res = -1;
            goto exit;
        }
        to_write -= i;
    }

exit:
    p_fclose(&f);
    if (res < 0) {
        /* Clean partial and corrupted file */
        unlink(filename);
    }
    return res;
}

/****************************************************************************/
/* low level page related functions                                         */
/****************************************************************************/

static inline int int_bits_range(uint64_t idx, int start, int width)
{
    return (idx << start) >> (64 - width);
}

/* returns:
 *  + -1 for invalid data
 *  +  0 for not found
 *  +  positive integer index in pidx->pages[...]
 */
static int32_t pidx_page_find(const pidx_file *pidx, uint64_t idx)
{
    const int maxshift = pidx->area->skip + PIDX_SHIFT * pidx->area->nbsegs;

    int shift = pidx->area->skip;
    int32_t page = 0;

    if (int_bits_range(idx, 0, shift)) {
        return -1;
    }

    for (; shift <= maxshift; shift += PIDX_SHIFT) {
        page = pidx->area->pages[page].refs[int_bits_range(idx, shift, PIDX_SHIFT)];
        if (!page)
            return 0;
    }

    return page;
}

/* returns the index of a clean page, full of zeroes */
static int32_t pidx_page_getfree(pidx_file *pidx)
{
    int32_t i, res;

    if (pidx->area->freelist) {
        res = pidx->area->freelist;
        pidx->area->freelist = pidx->area->pages[pidx->area->freelist].next;
        pidx->area->pages[res].next = 0;
        return res;
    }

    res = pidx_real_truncate(pidx, pidx->size + PIDX_GROW * PIDX_PAGE);
    if (res < 0) {
        if (!pidx->area) {
            e_panic("Not enough memory !");
        }
        return res;
    }

    /* res == oldnbpages is the new free page, link the NB_PAGES_GROW - 1
       remaining pages like that:
       res + 1 -> ... -> res + NB_PAGES_GROW - 1 -> NIL
     */
    res = pidx->area->nbpages;
    for (i = res + PIDX_GROW - 1; i > res + 1; i--) {
        pidx->area->pages[i - 1].next = i;
    }
    pidx->area->freelist = res + 1;
    pidx->area->nbpages += PIDX_GROW;

    return res;
}

static int32_t pidx_page_list_getfree(pidx_file *pidx, int len)
{
    int needed = MAX(0, len - 1) / PAYLOAD_SIZE + 1;
    int32_t res = 0;

    while (needed-- > 0) {
        int32_t pg = pidx_page_getfree(pidx);

        pidx->area->pages[pg].next = res;
        res = pg;
    }

    return res;
}

/****************************************************************************/
/* low level keys related functions                                         */
/****************************************************************************/

int pidx_key_first(const pidx_file *pidx, uint64_t minval, uint64_t *res)
{
    const int skip = pidx->area->skip;
    const pidx_page *pages = pidx->area->pages;

    int32_t page, *path = p_alloca(int32_t, pidx->area->nbsegs);
    int pos, key;

    if (int_bits_range(minval, 0, skip)) {
        return -1;
    }

    pos  = 0;
    page = path[pos] = 0;
    key  = int_bits_range(minval, skip + PIDX_SHIFT * pos, PIDX_SHIFT);

    for (;;) {
        while (!pages[page].refs[key]) {
            int rbits;

            if (++key < countof(pages[page].refs)) {
                rbits  = 64 - PIDX_SHIFT * (pos + 1) - skip;
                minval = ((minval >> rbits) + 1) << rbits;
            } else {
                if (--pos < 0)
                    return -1;
                page   = path[pos];
                rbits  = 64 - PIDX_SHIFT * (pos + 1) - skip;
                minval = ((minval >> rbits) + 1) << rbits;
                key    = int_bits_range(minval, skip + PIDX_SHIFT * pos,
                                        PIDX_SHIFT);
            }
        }

        if (pos == pidx->area->nbsegs) {
            *res = minval;
            return 0;
        }
        page = path[++pos] = pages[page].refs[key];
        key  = int_bits_range(minval, skip + PIDX_SHIFT * pos, PIDX_SHIFT);
    }
}


int pidx_key_last(const pidx_file *pidx, uint64_t maxval, uint64_t *res)
{
    const int skip = pidx->area->skip;
    const pidx_page *pages = pidx->area->pages;

    int32_t page, *path = p_alloca(int32_t, pidx->area->nbsegs);
    int pos, key;

    if (int_bits_range(maxval, 0, skip)) {
        maxval &= 0xffffffffffffffffULL >> skip;
    }

    pos  = 0;
    page = path[pos] = 0;
    key  = int_bits_range(maxval, skip + PIDX_SHIFT * pos, PIDX_SHIFT);

    for (;;) {
        while (!pages[page].refs[key]) {
            int rbits;

            if (--key >= 0) {
                rbits  = 64 - PIDX_SHIFT * (pos + 1) - skip;
                maxval = ((maxval >> rbits) << rbits) - 1;
            } else {
                if (--pos < 0)
                    return -1;
                page   = path[pos];
                rbits  = 64 - PIDX_SHIFT * (pos + 1) - skip;
                maxval = ((maxval >> rbits) << rbits) - 1;
                key    = int_bits_range(maxval, skip + PIDX_SHIFT * pos,
                                        PIDX_SHIFT);
            }
        }

        if (pos == pidx->area->nbsegs) {
            *res = maxval;
            return 0;
        }
        page = path[++pos] = pages[page].refs[key];
        key  = int_bits_range(maxval, skip + PIDX_SHIFT * pos, PIDX_SHIFT);
    }
}

/****************************************************************************/
/* high level functions                                                     */
/****************************************************************************/

int pidx_data_getslice(const pidx_file *pidx, uint64_t idx,
                       byte *out, int start, int len)
{
    int32_t page  = pidx_page_find(pidx, idx);
    const byte *orig = out;

    while (page && len) {
        pidx_page *pg = pidx->area->pages + page;
        int32_t size = *(int32_t *)pg->payload;

        if (size > PIDX_PAGE - 2 * ssizeof(int32_t))
            return -1;

        if (start > size) {
            start -= size;
        } else {
            int copy = MIN(len, size - start);

            memcpy(out, pg->payload + sizeof(int32_t) + start, copy);
            out += copy;
            len -= copy;
            start = 0;
        }
        page = pg->next;
    }

    return out - orig;
}

void *pidx_data_getslicep(const pidx_file *pidx, uint64_t idx,
                          int start, int len)
{
    int32_t page  = pidx_page_find(pidx, idx);

    while (page && len) {
        pidx_page *pg = pidx->area->pages + page;
        int32_t size = *(int32_t *)pg->payload;

        if (size > PIDX_PAGE - 2 * ssizeof(int32_t))
            return NULL;

        if (start < size) {
            if (start + len > size)
                return NULL;

            return pg->payload + sizeof(int32_t) + start;
        }

        start -= size;
        page = pg->next;
    }

    return NULL;
}

int pidx_data_get(const pidx_file *pidx, uint64_t idx, blob_t *out)
{
    int32_t page  = pidx_page_find(pidx, idx);
    ssize_t pos   = out->len;

    while (page) {
        pidx_page *pg = pidx->area->pages + page;
        int32_t size = *(int32_t *)pg->payload;

        if (size > PIDX_PAGE - 2 * ssizeof(int32_t))
            goto error;
        blob_append_data(out, pg->payload + sizeof(int32_t), size);
        page = pg->next;
    }

    return out->len - pos;

  error:
    blob_kill_last(out, out->len - pos);
    return -1;
}


int pidx_data_set(pidx_file *pidx, uint64_t idx, const byte *data, int len)
{
    const int maxshift = pidx->area->skip + PIDX_SHIFT * pidx->area->nbsegs;

    int32_t *oldpage, page, newpage;
    int k, shift;

    if (int_bits_range(idx, 0, pidx->area->skip)) {
        return -1;
    }

    /* stage 1: search the last pointer to us */

    page = 0;
    for (shift = pidx->area->skip; shift < maxshift; shift += PIDX_SHIFT) {
        k = int_bits_range(idx, shift, PIDX_SHIFT);

        if (!pidx->area->pages[page].refs[k]) {
            newpage = pidx_page_getfree(pidx);
            if (newpage <= 0)
                return -1;

            pidx->area->pages[page].refs[k] = newpage;
        }

        page = pidx->area->pages[page].refs[k];
    }

    k = int_bits_range(idx, shift, PIDX_SHIFT);
    oldpage = &pidx->area->pages[page].refs[k];

    /* stage 2: allocate a new clean list of pages and copy the data */
    page = newpage = pidx_page_list_getfree(pidx, len);
    if (newpage <= 0)
        return -1;

    /* XXX do while is correct, we always want at least one page ! */
    do {
        pidx_page *pg = pidx->area->pages + page;
        const int chunk = MIN(PAYLOAD_SIZE, len);

        assert (page);

        *(int32_t *)pg->payload = chunk;
        memcpy(pg->payload + sizeof(int32_t), data, chunk);
        len  -= chunk;
        data += chunk;
        page  = pg->next;
    } while (len);

    /* stage 3: atomically replace the pointer */
    page = *oldpage;
    *oldpage = newpage;

    /* stage 4: put the old pages in the garbage queue */
    while (page) {
        int32_t next = pidx->area->pages[page].next;
        pidx_page_release(pidx, page);
        page = next;
    }
    if (pidx->area->readers) {
        pidx->area->wr_ver++;
    }

    return 0;
}

static bool
pidx_page_recollect(pidx_file *pidx, uint64_t idx, int shift, int32_t page)
{
    const int maxshift = pidx->area->skip + PIDX_SHIFT * pidx->area->nbsegs;
    int pos = int_bits_range(idx, shift, PIDX_SHIFT);
    pidx_page *pg = pidx->area->pages + page;

    if (shift > maxshift)
        return true;

    if (pidx_page_recollect(pidx, idx, shift + PIDX_SHIFT, pg->refs[pos])) {
        int i;

        if (page) {
            pg->refs[pos] = 0;
            for (i = 0; i < PIDX_PAGE / 4; i++) {
                if (pg->refs[i])
                    return false;
            }

            pidx_page_release(pidx, page);
            return true;
        }
    }

    return false;
}

void pidx_data_release(pidx_file *pidx, uint64_t idx)
{
    int32_t page = pidx_page_find(pidx, idx);

    if (!page)
        return;

    pidx_page_recollect(pidx, idx, pidx->area->skip, 0);

    while (page) {
        int32_t next = pidx->area->pages[page].next;
        pidx_page_release(pidx, page);
        page = next;
    }

    if (pidx->area->readers) {
        pidx->area->wr_ver++;
    }
}
