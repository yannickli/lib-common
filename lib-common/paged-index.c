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
#include <sys/mman.h>

#include <lib-common/err_report.h>
#include <lib-common/mem.h>

#include "paged-index.h"

static union {
    char     s[4];
    uint32_t i;
} const magic = { { 'I', 'S', 'P', 'F' } };

MMFILE_FUNCTIONS(pidx_file, pidx_real);

/* XXX, FIXME: copied from backend.c, where do we put'em ? */
#define TST_BIT(bits, num)  ((bits)[(unsigned)(num) >> 3] & (1 << ((num) & 7)))
#define SET_BIT(bits, num)  ((bits)[(unsigned)(num) >> 3] |= (1 << ((num) & 7)))
#define RST_BIT(bits, num)  ((bits)[(unsigned)(num) >> 3] &= ~(1 << ((num) & 7)))
#define XOR_BIT(bits, num)  ((bits)[(unsigned)(num) >> 3] ^= (1 << ((num) & 7)))

#define PIDX_SHIFT     8
#define PIDX_GROW   1024

/****************************************************************************/
/* whole file related functions                                             */
/****************************************************************************/

static void pidx_page_release(pidx_file *pidx, int32_t page)
{
    assert (page > 0 && page < pidx->area->nbpages);

    pidx->area->pages[page].next = pidx->area->freelist;
    pidx->area->freelist = page;
    memset(pidx->area->pages[page].payload, 0, PIDX_PAGE - sizeof(int32_t));
}

static int pidx_fsck_mark_page(byte *bits, pidx_file *pidx, int page)
{
    if (page < 0 || page > pidx->area->nbpages || TST_BIT(bits, page))
        return -1;

    SET_BIT(bits, page);
    return 0;
}

static int pidx_fsck_recurse(byte *bits, pidx_file *pidx,
                             int segpage, int seglevel)
{
    int page;

    if (seglevel) {
        int i;

        for (i = 0; i < PIDX_PAGE / 4; i++) {
            page = pidx->area->pages[segpage].refs[i];
            if (!page)
                continue;

            if (pidx_fsck_mark_page(bits, pidx, page))
                return -1;

            if (pidx_fsck_recurse(bits, pidx, page, seglevel - 1) < 0)
                return -1;
        }
    } else {
        for (page = segpage; page; page = pidx->area->pages[page].next) {
            if (pidx_fsck_mark_page(bits, pidx, page))
                return -1;
        }
    }

    return 0;
}

int pidx_fsck(pidx_file *pidx, int dofix)
{
    bool did_a_fix = false;

    if (pidx->size % PIDX_PAGE || pidx->size < 2 * PIDX_PAGE
    ||  ((uint64_t)pidx->size > (uint64_t)INT_MAX * PIDX_PAGE))
        return -1;

    if (pidx->area->magic != magic.i)
        return -1;

    if (pidx->area->nbsegs < 1 || pidx->area->nbsegs > 6
    ||  pidx->area->skip + PIDX_SHIFT * pidx->area->nbsegs > 64)
    {
        return -1;
    }

    /* hook conversions here ! */
    if (pidx->area->major != 1 || pidx->area->minor != 0)
        return -1;

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
            int page = pidx->area->nbpages;

            while (page > 0) {
                if (!(page & 7) && bits[page >> 3] == 0xff) {
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
    pidx_file *pidx = pidx_real_open(path, flags);
    int fsck_res;

    if (!pidx)
        return NULL;

    if (flags & O_CREAT && (access(path, F_OK) || flags & O_TRUNC))
        return pidx_creat(path, skip, nbsegs);

    fsck_res = pidx_fsck(pidx, !!(flags & O_WRONLY));
    if (fsck_res < 0) {
        pidx_close(&pidx);
        errno = EINVAL;
    }

    if (fsck_res > 0) {
        e_error("`%s': corrupted pages, Repaired.", path);
    }

#if 0
    if (flags & O_WRONLY) {
        pidx->area->magic = 0;
        msync(pidx->area, pidx->size, MS_SYNC);
    }
#endif

    return pidx;
}

pidx_file *pidx_creat(const char *path, uint8_t skip, uint8_t nbsegs)
{
    pidx_file *pidx;
    int nbpages = PIDX_GROW - 1;

    if (nbsegs > 64 / PIDX_SHIFT || skip + PIDX_SHIFT * nbsegs > 64) {
        errno = EINVAL;
        return NULL;
    }

    assert (sizeof(pidx_page) == sizeof(pidx_t));

    pidx = pidx_real_creat(path, (nbpages + 1) * PIDX_PAGE);
    if (!pidx)
        return NULL;

    pidx->area->magic    = magic.i;
    pidx->area->major    = 1;
    pidx->area->minor    = 0;
    pidx->area->skip     = skip;
    pidx->area->nbsegs   = nbsegs;
    pidx->area->nbpages  = nbpages;

    /* this creates the links: 1 -> 2 -> ... -> nbpages - 1 -> NIL */
    while (--nbpages > 1) {
        pidx->area->pages[nbpages - 1].next = nbpages;
    }
    pidx->area->freelist = 1;

#if 0
    pidx->area->magic = 0;
    msync(pidx->area, pidx->size, MS_SYNC);
#endif
    return pidx;
}

void pidx_close(pidx_file **f)
{
#if 0
    if (*f) {
        if (!(*f)->area->magic) {
            msync((*f)->area, (*f)->size, MS_SYNC);
            (*f)->area->magic = magic.i;
            msync((*f)->area, (*f)->size, MS_SYNC);
        }
    }
#endif
    pidx_real_close(f);
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
int32_t pidx_page_find(pidx_file *pidx, uint64_t idx)
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

/* -1 means not indexable, 0 means could not extend file */
int32_t pidx_page_new(pidx_file *pidx, uint64_t idx)
{
    const int maxshift = pidx->area->skip + PIDX_SHIFT * pidx->area->nbsegs;

    int k, shift;
    int32_t page = 0, newpage;

    if (int_bits_range(idx, 0, pidx->area->skip)) {
        return -1;
    }

    for (shift = pidx->area->skip; shift < maxshift; shift += PIDX_SHIFT) {
        k = int_bits_range(idx, shift, PIDX_SHIFT);

        if (!pidx->area->pages[page].refs[k]) {
            newpage = pidx_page_getfree(pidx);
            if (newpage <= 0)
                return 0;

            pidx->area->pages[page].refs[k] = newpage;
        }

        page = pidx->area->pages[page].refs[k];
    }

    newpage = pidx_page_getfree(pidx);
    if (newpage <= 0)
        return 0;

    k = int_bits_range(idx, shift, PIDX_SHIFT);
    pidx->area->pages[newpage].next = pidx->area->pages[page].refs[k];
    pidx->area->pages[page].refs[k] = newpage;
    return newpage;
}

/****************************************************************************/
/* high functions                                                           */
/****************************************************************************/

int pidx_data_get(pidx_file *pidx, uint64_t idx, blob_t *out)
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
#define PAYLOAD_SIZE   (PIDX_PAGE - 2 * ssizeof(int32_t))

    int32_t curp, page = pidx_page_find(pidx, idx);
    int nbpages = 0, needed;

    needed = MIN(0, len - 1) / PAYLOAD_SIZE + 1;

    for (curp = page; curp; curp = pidx->area->pages[curp].next)
        nbpages++;

    while (needed < nbpages) {
        page = pidx_page_new(pidx, idx);
        if (!page)
            return -1;
        nbpages++;
    }

    /* XXX do while is correct, we always want at least one page ! */
    do {
        pidx_page *pg = pidx->area->pages + page;
        const int chunk = MIN(PAYLOAD_SIZE, len);

        *(int32_t *)pg->payload = chunk;
        memcpy(pg->payload + sizeof(int32_t), data, chunk);
        len  -= chunk;
        data += chunk;
        page  = pg->next;
    } while (len);

    while (page) {
        int32_t next = pidx->area->pages[page].next;
        pidx_page_release(pidx, page);
        page = next;
    }

    return 0;
}

static bool pidx_page_recollect(pidx_file *pidx, uint64_t idx, int shift, int32_t page)
{
    const int maxshift = pidx->area->skip + PIDX_SHIFT * pidx->area->nbsegs;
    int pos = int_bits_range(idx, shift, PIDX_SHIFT);
    pidx_page *pg = pidx->area->pages + page;

    if (shift > maxshift)
        return true;

    if (pidx_page_recollect(pidx, idx, shift + PIDX_SHIFT, pg->refs[pos])) {
        int i;

        pg->refs[pos] = 0;
        for (i = 0; i < PIDX_PAGE / 4; i++) {
            if (pg->refs[i])
                return false;
        }
        pidx_page_release(pidx, page);
    }

    return page ? true : false;
}

void pidx_data_release(pidx_file *pidx, uint64_t idx)
{
    int32_t page = pidx_page_find(pidx, idx);

    if (!page)
        return;

    while (page) {
        int32_t next = pidx->area->pages[page].next;
        pidx_page_release(pidx, page);
        page = next;
    }

    pidx_page_recollect(pidx, idx, pidx->area->skip, 0);
}

