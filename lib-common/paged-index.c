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

#include "paged-index.h"

static const union {
    char     s[4];
    uint32_t i;
} magic = { { 'I', 'S', 'P', 'F' } };

MMFILE_FUNCTIONS(pidx_file, pidx_real);

/* XXX, FIXME: copied from backend.c, where do we put'em ? */
#define TST_BIT(bits, num)  ((bits)[(unsigned)(num) >> 3] & (1 << ((num) & 7)))
#define SET_BIT(bits, num)  ((bits)[(unsigned)(num) >> 3] |= (1 << ((num) & 7)))
#define RST_BIT(bits, num)  ((bits)[(unsigned)(num) >> 3] &= ~(1 << ((num) & 7)))
#define XOR_BIT(bits, num)  ((bits)[(unsigned)(num) >> 3] ^= (1 << ((num) & 7)))

/****************************************************************************/
/* whole file related functions                                             */
/****************************************************************************/

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

        for (i = 0; i < 1024; i++) {
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

    if (pidx->size & 4095 || pidx->size < 2 * 4096
    ||  ((uint64_t)pidx->size > INT_MAX * 4096LL))
        return -1;

    if (pidx->area->magic != magic.i)
        return -1;

    if (pidx->area->nbsegs < 1 || pidx->area->nbsegs > 6
    ||  pidx->area->skip + 10 * pidx->area->nbsegs > 64)
    {
        return -1;
    }

    /* hook conversions here ! */
    if (pidx->area->major != 1 || pidx->area->minor != 0)
        return -1;

    if (pidx->area->nbpages != pidx->size / 4096 - 1) {
        if (!dofix)
            return -1;

        did_a_fix = true;
        e_error("`%s': corrupted header (nbpages was %d, fixed into %d)",
                pidx->path, pidx->area->nbpages, (int)(pidx->size / 4096 - 1));
        pidx->area->nbpages = pidx->size / 4096 - 1;
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

pidx_file *pidx_open(const char *path, int flags)
{
    pidx_file *pidx = pidx_real_open(path, flags);
    int fsck_res;

    if (!pidx)
        return NULL;

    fsck_res = pidx_fsck(pidx, !!(flags | O_WRONLY));
    if (fsck_res < 0) {
        pidx_real_close(&pidx);
        errno = EINVAL;
    }

    if (fsck_res > 0) {
        e_error("`%s': corrupted pages, Repaired.", path);
    }

    return pidx;
}

pidx_file *pidx_creat(const char *path, int nbpages,
                      uint8_t skip, uint8_t nbsegs)
{
    pidx_file *pidx;

    if (nbsegs > 6 || skip + 10 * skip > 64) {
        errno = EINVAL;
        return NULL;
    }

    /* round nbpages to the first upper 32 multiple - 1 */
    nbpages = MIN(32, (nbpages + 1 + 31) & ~31) - 1;

    assert (sizeof(pidx_page) == 4096 && sizeof(pidx_t) == 4096);

    pidx = pidx_real_creat(path, (nbpages + 1) * sizeof(pidx_page));
    if (!pidx)
        return NULL;

    pidx->area->magic    = magic.i;
    pidx->area->major    = 1;
    pidx->area->minor    = 0;
    pidx->area->skip     = skip;
    pidx->area->nbsegs   = nbsegs;
    pidx->area->nbpages  = nbpages;

    /* this creates the links: 1 -> 2 -> ... -> nbpages - 1 -> NIL */
    while (nbpages-- > 0) {
        pidx->area->pages[nbpages].next = nbpages + 1;
    }
    pidx->area->freelist = 1;

    return pidx;
}

/****************************************************************************/
/* pages related functions                                                  */
/****************************************************************************/

static inline int int_bits_range(uint64_t idx, int start, int width)
{
    return (idx << start) >> (64 - width);
}

void pidx_page_release(pidx_file *pidx, int32_t page)
{
    assert (page > 0 && page < pidx->area->nbpages);

    pidx->area->pages[page].next = pidx->area->freelist;
    pidx->area->freelist = page;
    p_clear(pidx->area->pages[page].payload, 1023);
}

/* returns:
 *  + -1 for invalid data
 *  +  0 for not found
 *  +  positive integer index in pidx->pages[...]
 */
int32_t pidx_page_find(pidx_file *pidx, uint64_t idx)
{
    const int maxshift = pidx->area->skip + 10 * pidx->area->nbsegs;

    int shift = pidx->area->skip;
    int32_t page = 0;

    if (int_bits_range(idx, 0, shift)) {
        return -1;
    }

    for (; shift <= maxshift; shift += 10) {
        page = pidx->area->pages[page].refs[int_bits_range(idx, shift, 10)];
        if (!page)
            return 0;
    }

    return page;
}

/* returns the index of a clean page, full of zeroes */
static int32_t pidx_page_getfree(pidx_file *pidx)
{
#define NB_PAGES_GROW  1024

    int32_t i, res;

    if (pidx->area->freelist) {
        res = pidx->area->freelist;
        pidx->area->freelist = pidx->area->pages[pidx->area->freelist].next;
        pidx->area->pages[res].next = 0;
        return res;
    }

    res = pidx_real_truncate(pidx, pidx->size + NB_PAGES_GROW * 4096);
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
    pidx->area->freelist = res + 1;
    for (i = res + NB_PAGES_GROW - 1; i > res + 1; i--) {
        pidx->area->pages[i - 1].next = i;
    }
    pidx->area->nbpages += NB_PAGES_GROW;

    return res;
}

/* -1 means not indexable, 0 means could not extend file */
int32_t pidx_page_new(pidx_file *pidx, uint64_t idx)
{
    const int maxshift = pidx->area->skip + 10 * pidx->area->nbsegs;

    int k, shift;
    int32_t page = 0, newpage;

    if (int_bits_range(idx, 0, pidx->area->skip)) {
        return -1;
    }

    for (shift = pidx->area->skip; shift < maxshift; shift += 10) {
        k = int_bits_range(idx, shift, 10);

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

    k = int_bits_range(idx, shift, 10);
    pidx->area->pages[newpage].next = pidx->area->pages[page].refs[k];
    pidx->area->pages[page].refs[k] = newpage;
    return newpage;
}
