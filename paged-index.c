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

#include <sys/mman.h>
#include "unix.h"
#include "paged-index.h"

static union {
    char     s[4];
    uint32_t i;
} const magic = { { 'I', 'S', 'P', 'F' } };

MMFILE_FUNCTIONS(pidx_file, pidx_real);

#define PIDX_SHIFT      8
#define PIDX_GROW       1024

/****************************************************************************/
/* whole file related functions                                             */
/****************************************************************************/

static void pidx_page_release(pidx_file *pidx, int32_t page)
{
    assert (page > 0 && page < pidx->area->nbpages);

    p_clear(&pidx->area->pages[page], 1);
    pidx->area->pages[page].next = pidx->area->freelist;
    pidx->area->freelist = page;
}

static int pidx_fsck_mark_page(byte *bits, pidx_file *pidx, int page)
{
    if (page < 0 || page > pidx->area->nbpages || TST_BIT(bits, page)) {
        e_trace(1, "Bug for page %d", page);
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

            RETHROW(pidx_fsck_recurse(bits, pidx, pg, seglevel - 1));
        }
    } else {
#ifndef NDEBUG
        int page0 = page;
#endif
        while ((page = pidx->area->pages[page].next) != 0) {
            if (pidx_fsck_mark_page(bits, pidx, page)) {
                e_trace(1, "bug in data page %d (link of %d)", page, page0);
                return -1;
            }
        }
    }

    return 0;
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

        if (pidx->area->wrlock != getpid())
            return -1;

        if (pid_get_starttime(pidx->area->wrlock, &tv))
            return -1;

        if (pidx->area->wrlockt != MAKE64(tv.tv_sec, tv.tv_usec))
            return -1;

        dofix = 0;
    }

    if (pidx->area->magic != magic.i)
        return -1;

    if (pidx->area->nbsegs < 1 || pidx->area->nbsegs > 6
    ||  pidx->area->skip + PIDX_SHIFT * pidx->area->nbsegs > 64)
    {
        e_trace(1, "Bad pidx definition");
        return -1;
    }

    /* hook conversions here ! */
    version = PIDX_MKVER(pidx->area->major, pidx->area->minor);
    if (version < PIDX_MKVER(1, 0) || version > PIDX_VERSION)
        return -1;

    if (version != PIDX_VERSION && !dofix)
        return -1;

    if (pidx->area->nbpages != pidx->size / PIDX_PAGE - 1) {
        e_trace(1, "Bad nb pages");
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
        e_trace(1, "Bad freelist");
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
        if (pidx_fsck_recurse(bits, pidx, 0, pidx->area->nbsegs) < 0) {
            e_trace(1, "Bad check on used pages");
            p_delete(&bits);
            return -1;
        }

        /* check free pages */
        for (prev = &pidx->area->freelist;
             *prev;
             prev = &pidx->area->pages[*prev].next)
        {
            if (pidx_fsck_mark_page(bits, pidx, *prev)) {
                e_trace(1, "Bad check on freelist pages (%d)", *prev);
                if (!dofix) {
                    p_delete(&bits);
                    return -1;
                }

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
                        e_trace(1, "Force move page %d to freelist", page);
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
    bool force = false;

    assert (sizeof(pidx_page) == sizeof(pidx_t));

    if (nbsegs > 64 / PIDX_SHIFT || skip + PIDX_SHIFT * nbsegs > 64) {
        errno = EINVAL;
        return NULL;
    }

    pidx = pidx_real_open(path, flags, MMO_RANDOM | MMO_TLOCK,
                          PIDX_GROW * PIDX_PAGE);
    if (!pidx)
        return NULL;

    if (flags & O_NONBLOCK) {
        if (O_ISWRITE(flags)) {
            e_warning("Force mode must be used in read-only");
        } else {
            flags &= ~O_NONBLOCK;
            force = true;
        }
    }

    if ((flags & (O_TRUNC | O_CREAT)) && !pidx->area->magic) {
        int nbpages;

        pidx->area->magic    = magic.i;
        pidx->area->major    = PIDX_MAJOR;
        pidx->area->minor    = PIDX_MINOR;
        pidx->area->skip     = skip;
        pidx->area->nbsegs   = nbsegs;
        pidx->area->nbpages  = nbpages = pidx->size / ssizeof(pidx_page) - 1;
        pidx->area->wrlock   = 0;
        pidx->area->wrlockt  = 0;

        /* this creates the links: 1 -> 2 -> ... -> nbpages - 1 -> NIL */
        while (--nbpages > 1) {
            p_clear(pidx->area->pages + nbpages - 1, 1);
            pidx->area->pages[nbpages - 1].next = nbpages;
        }
        pidx->area->freelist = 1;
    } else {
        res = pidx_fsck(pidx, O_ISWRITE(flags));
        if (res < 0) {
            if (!force) {
                /* lock check failed, file was not closed properly */
                e_error("Cannot open '%s': already locked", path);
                pidx_close(&pidx);
                errno = EINVAL;
                return NULL;
            } else {
                e_error("Already locked, but force mode used");
            }
        }

        if (res > 0) {
            e_error("`%s': corrupted pages, Repaired.", path);
        }
    }

    if (O_ISWRITE(flags)) {
        struct timeval tv;
        pid_t pid = getpid();

        if (pidx->area->wrlock) {
            pidx_real_close(&pidx);
            errno = EDEADLK;
            return NULL;
        }

        pid_get_starttime(pid, &tv);
        pidx->area->wrlock = pid;
        pidx->area->wrlockt = MAKE64(tv.tv_sec, tv.tv_usec);
        msync(pidx->area, pidx->size, MS_SYNC);
    }

    if (pidx_real_unlockfile(pidx) < 0) {
        int save_errno = errno;
        pidx_close(&pidx);
        errno = save_errno;
        return NULL;
    }

    return pidx;
}

void pidx_close(pidx_file **fp)
{
    if (*fp) {
        pidx_file *f = *fp;

        pidx_real_wlock(f);
        if (f->writeable && f->refcnt <= 1
        &&  f->area->wrlock == getpid())
        {
            msync(f->area, f->size, MS_SYNC);
            f->area->wrlock  = 0;
            f->area->wrlockt = 0;
        }
        pidx_real_close_wlocked(fp);
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

    pidx_real_rlock(pidx);

    memcpy(&hdr, pidx->area, sizeof(hdr));
    hdr.wrlock  = 0;
    hdr.wrlockt = 0;

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
    pidx_real_unlock(pidx);
    return res;
}

/****************************************************************************/
/* low level page related functions                                         */
/****************************************************************************/

static int int_bits_range(uint64_t idx, int start, int width)
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
        if (unlikely(page >= pidx->area->nbpages)) {
            e_warning("%s: broken pidx structure", pidx->path);
            return 0;
        }
    }

    return page;
}

/* returns the index of a clean page, full of zeroes */
static int32_t pidx_page_getfree(pidx_file *pidx)
{
    int32_t i, res;

    if (pidx->area->freelist) {
        res = pidx->area->freelist;
        pidx->area->freelist = pidx->area->pages[res].next;
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
    int needed = MAX(0, len - 1) / PIDX_PAYLOAD_SIZE + 1;
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

int pidx_key_first(pidx_file *pidx, uint64_t minval, uint64_t *res)
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

    pidx_real_rlock(pidx);
    for (;;) {
        while (!pages[page].refs[key]) {
            int rbits;

            while (++key == countof(pages[page].refs)) {
                if (--pos < 0)
                    goto notfound;
                page    = path[pos];
                rbits   = 64 - PIDX_SHIFT * (pos + 1) - skip;
                minval &= BITMASK_GE(uint64_t, rbits);
                key     = int_bits_range(minval, skip + PIDX_SHIFT * pos,
                                         PIDX_SHIFT);
            }
            rbits  = 64 - PIDX_SHIFT * (pos + 1) - skip;
            minval = ((minval >> rbits) + 1) << rbits;
        }

        if (pos == pidx->area->nbsegs) {
            pidx_real_unlock(pidx);
            *res = minval;
            return 0;
        }
        page = path[++pos] = pages[page].refs[key];
        key  = int_bits_range(minval, skip + PIDX_SHIFT * pos, PIDX_SHIFT);
    }

  notfound:
    pidx_real_unlock(pidx);
    return -1;
}


int pidx_key_last(pidx_file *pidx, uint64_t maxval, uint64_t *res)
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

    pidx_real_rlock(pidx);
    for (;;) {
        while (!pages[page].refs[key]) {
            int rbits;

            if (--key >= 0) {
                rbits  = 64 - PIDX_SHIFT * (pos + 1) - skip;
                maxval = ((maxval >> rbits) << rbits) - 1;
            } else {
                uint64_t old = maxval;

                if (--pos < 0)
                    goto notfound;
                page   = path[pos];
                rbits  = 64 - PIDX_SHIFT * (pos + 1) - skip;
                maxval = ((maxval >> rbits) << rbits) - 1;
                if (maxval > old) /* overflow */
                    goto notfound;
                key    = int_bits_range(maxval, skip + PIDX_SHIFT * pos,
                                        PIDX_SHIFT);
            }
        }

        if (pos == pidx->area->nbsegs) {
            pidx_real_unlock(pidx);
            *res = maxval;
            return 0;
        }
        page = path[++pos] = pages[page].refs[key];
        key  = int_bits_range(maxval, skip + PIDX_SHIFT * pos, PIDX_SHIFT);
    }

  notfound:
    pidx_real_unlock(pidx);
    return -1;
}

/****************************************************************************/
/* high level functions                                                     */
/****************************************************************************/

int pidx_data_getslice(pidx_file *pidx, uint64_t idx,
                       byte *out, int start, int len)
{
    const byte *orig = out;
    int32_t page;

    pidx_real_rlock(pidx);
    page = pidx_page_find(pidx, idx);

    while (page && len) {
        pidx_page *pg = pidx->area->pages + page;
        int32_t size = *(int32_t *)pg->payload;

        if (size > PIDX_PAGE - 2 * ssizeof(int32_t)) {
            pidx_real_unlock(pidx);
            return -1;
        }

        if (start > size) {
            start -= size;
        } else {
            int copy = MIN(len, size - start);

            out  = mempcpy(out, pg->payload + sizeof(int32_t) + start, copy);
            len -= copy;
            start = 0;
        }
        page = pg->next;
    }

    pidx_real_unlock(pidx);
    return out - orig;
}

void *pidx_data_getslicep(pidx_file *pidx, uint64_t idx,
                          int start, int len)
{
    int32_t page;

    pidx_real_rlock(pidx);
    page = pidx_page_find(pidx, idx);

    while (page && len) {
        pidx_page *pg = pidx->area->pages + page;
        int32_t size = *(int32_t *)pg->payload;

        if (size > PIDX_PAGE - 2 * ssizeof(int32_t)) {
            pidx_real_unlock(pidx);
            return NULL;
        }

        if (start < size) {
            pidx_real_unlock(pidx);

            if (start + len > size)
                return NULL;

            return pg->payload + sizeof(int32_t) + start;
        }

        start -= size;
        page = pg->next;
    }

    pidx_real_unlock(pidx);
    return NULL;
}

int pidx_data_get(pidx_file *pidx, uint64_t idx, sb_t *out)
{
    sb_t orig = *out;
    int32_t page;

    pidx_real_rlock(pidx);
    page = pidx_page_find(pidx, idx);

    while (page) {
        pidx_page *pg = pidx->area->pages + page;
        int32_t size = *(int32_t *)pg->payload;

        if (size > PIDX_PAGE - 2 * ssizeof(int32_t))
            goto error;
        sb_add(out, pg->payload + sizeof(int32_t), size);
        page = pg->next;
    }

    pidx_real_unlock(pidx);
    return out->len - orig.len;

  error:
    pidx_real_unlock(pidx);
    return __sb_rewind_adds(out, &orig);
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

    /* stage 2: allocate a new clean list of pages and copy the data */
    newpage = pidx_page_list_getfree(pidx, len);
    oldpage = &pidx->area->pages[page].refs[k];
    page = newpage;
    if (newpage <= 0)
        return -1;

    /* XXX do while is correct, we always want at least one page ! */
    do {
        pidx_page *pg = pidx->area->pages + page;
        const int chunk = MIN(PIDX_PAYLOAD_SIZE, len);

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
    pidx_real_wlock(pidx);
    while (page) {
        int32_t next = pidx->area->pages[page].next;
        pidx_page_release(pidx, page);
        page = next;
    }
    pidx_real_unlock(pidx);

    return 0;
}

void pidx_data_release(pidx_file *pidx, uint64_t idx)
{
    uint32_t path[64 / PIDX_SHIFT + 1];
    int32_t page = 0;
    int shift = pidx->area->skip;

    if (int_bits_range(idx, 0, shift))
        return;

    pidx_real_wlock(pidx);

    for (int i = 0; i < pidx->area->nbsegs; i++) {
        int pos = int_bits_range(idx, shift + i * PIDX_SHIFT, PIDX_SHIFT);
        path[i + 1] = page = pidx->area->pages[page].refs[pos];
        if (!page)
            goto done;
    }

    while (page) {
        int32_t next = pidx->area->pages[page].next;
        pidx_page_release(pidx, page);
        page = next;
    }

    for (int i = pidx->area->nbsegs - 1; i >= 1; i--) {
        int pos = int_bits_range(idx, shift + i * PIDX_SHIFT, PIDX_SHIFT);
        pidx_page *pg = pidx->area->pages + path[i];

        pg->refs[pos] = 0;
        for (int j = 0; j < PIDX_PAGE / 4; j++) {
            if (pg->refs[j])
                goto done;
        }
        pidx_page_release(pidx, path[i]);
    }
    pidx->area->pages[0].refs[int_bits_range(idx, shift, PIDX_SHIFT)] = 0;

  done:
    pidx_real_unlock(pidx);
    return;
}
