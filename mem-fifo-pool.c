/**************************************************************************/
/*                                                                        */
/*  Copyright (C) 2004-2008 INTERSEC SAS                                  */
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
#include "mmappedfile.h"
#include "mem-pool.h"
#include "list.h"

#define ROUND_MULTIPLE(n, k) ((((n) + (k) - 1) / (k)) * (k))

#define POOL_DEALLOCATED  1

typedef struct mem_page_t {
    struct mem_page_t *next;
    struct mem_page_t *prev;

    flag_t full : 1;

    int area_size;
    int used_size;
    int used_blocks;

    void *last;                 /* last result of an allocation */
    struct mem_block_t *lock;   /* current scatter allocator    */

    byte area[];
} mem_page_t;
DLIST_FUNCTIONS(mem_page_t, mem_page);

typedef struct mem_block_t {
    int page_offs;
    int blk_size;
    struct mem_block_t *next;   /* scatter block chain          */
    byte area[];
} mem_block_t;

typedef struct mem_simple_block_t {
    int sblk_size;
    int sblk_free;
    byte area[];
} mem_simple_block_t;

typedef struct mem_fifo_pool_t {
    mem_pool_t funcs;
    mem_page_t *pages;
    mem_page_t *fullpages;
    mem_page_t *freelist;
    int page_size;
    int nb_pages;
    int occupied;
} mem_fifo_pool_t;

typedef struct mem_scatter_pool_t {
    mem_pool_t funcs;
    mem_fifo_pool_t *mfp;
    mem_page_t *locked;
    int refcnt;
} mem_scatter_pool_t;

static mem_simple_block_t *sblkof(void *mem) {
    mem_simple_block_t *sblk;
    sblk = (mem_simple_block_t*)((byte *)mem - sizeof(mem_simple_block_t));
    if (unlikely(sblk->sblk_free > 0))
        e_panic("double free heap corruption *** %p ***", mem);
    return sblk;
}
static mem_block_t *blkof(void *mem) {
    mem_block_t *blk = (mem_block_t*)((byte *)mem - sizeof(mem_block_t));
    if (unlikely(blk->page_offs > 0))
        e_panic("double free heap corruption *** %p ***", mem);
    return blk;
}
static mem_page_t *pageof(mem_block_t *blk) {
    return (mem_page_t *)((char *)blk + blk->page_offs);
}

static mem_page_t *mem_page_new(mem_fifo_pool_t *mfp)
{
    int size = mfp->page_size - sizeof(mem_page_t);
    mem_page_t *page;

    page = mmap(NULL, mfp->page_size, PROT_READ | PROT_WRITE,
                      MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
    if (page == MAP_FAILED) {
        e_panic(E_UNIXERR("mmap"));
    }

    page->area_size = size;
    return page;
}

static void mem_page_reset(mem_page_t *page)
{
    page->full        = 0;
    page->prev        = NULL;
    page->next        = NULL;
    page->used_size   = 0;
    page->used_blocks = 0;
    page->last        = NULL;
    p_clear(page->area, page->area_size);
}

static void mem_page_delete(mem_page_t **pagep)
{
    if (*pagep) {
        munmap((*pagep), (*pagep)->area_size + sizeof(mem_page_t));
        *pagep = NULL;
    }
}

static inline int mem_page_size_left(mem_page_t *page)
{
    return (page->area_size - page->used_size);
}

static mem_page_t *mfp_get_page(mem_fifo_pool_t *mfp, int size)
{
    mem_page_t **pagep = &mfp->pages;
    mem_page_t *page;

    while ((page = *pagep)) {
        if (mem_page_size_left(page) < size) {
            page->full = true;
            mem_page_list_take(&mfp->pages, page);
            mem_page_list_append(&mfp->fullpages, page);
            continue;
        }
        if (!page->lock)
            return page;
        if (page->next == mfp->pages)
            break;
        pagep = &(*pagep)->next;
    }

    if (mfp->freelist)
        return  mem_page_list_popfirst(&mfp->freelist);

    page = mem_page_new(mfp);
    mfp->nb_pages++;
    mem_page_list_prepend(&mfp->pages, page);
    return page;
}

static void *mfp_alloc(mem_fifo_pool_t *mfp, int size)
{
    mem_block_t *blk;
    mem_page_t *page;

    /* Must round size up to keep proper alignment */
    size = ROUND_MULTIPLE((unsigned)size + sizeof(mem_block_t), 8);

    if (size > mfp->page_size - ssizeof(mem_page_t)) {
        /* Should just map a larger page, yet we need a maximum value */
        e_panic(E_PREFIX("tried to alloc %d bytes, cannot have more than %d"),
                size, mfp->page_size);
    }

    page = mfp_get_page(mfp, size);
    blk = (mem_block_t *)(page->area + page->used_size);
    blk->page_offs = (intptr_t)page - (intptr_t)blk;
    blk->blk_size    = size;
    mfp->occupied   += size;
    page->used_size += size;
    page->used_blocks++;
    return page->last = blk->area;
}

static void mfp_free(mem_fifo_pool_t *mfp, void *mem)
{
    mem_block_t *blk;
    mem_page_t *page;

    if (!mem)
        return;

    blk = blkof(mem);
    mfp->occupied -= blk->blk_size;

    page = pageof(blk);
    assert (!page->lock || page->lock == blk);
    page->lock = NULL;

    if (page->full && --page->used_blocks <= 0) {
        mem_page_list_take(&mfp->fullpages, page);

        if (mfp->freelist || mfp->nb_pages == 1) {
            mem_page_delete(&page);
            mfp->nb_pages--;
        } else {
            /* Clear area to 0 to ensure allocated blocks are
             * cleared.  mfp_malloc relies on this.
             */
            mem_page_reset(page);
            mem_page_list_append(&mfp->freelist, page);
        }
    } else {
        blk->page_offs = POOL_DEALLOCATED;
    }

    if (blk->next)
        mfp_free(mfp, blk->next);
}

static void *mfp_realloc(mem_fifo_pool_t *mfp, void *mem, int size)
{
    mem_block_t *blk;
    mem_page_t *page;
    void *res;

    if (unlikely(size < 0))
        e_panic(E_PREFIX("invalid memory size %d"), size);
    if (unlikely(size == 0)) {
        mfp_free(mfp, mem);
        return NULL;
    }

    if (!mem)
        return mfp_alloc(mfp, size);

    blk = blkof(mem);
    if (size <= blk->blk_size - ssizeof(blk))
        return mem;

    /* optimization if it's the last block allocated */
    page = pageof(blk);
    if (mem == page->last
    && size + ssizeof(blk) - blk->blk_size <= mem_page_size_left(page))
    {
        assert (!page->lock);
        size = ROUND_MULTIPLE((size_t)size, 8);
        blk->blk_size   += size;
        page->used_size += size;
        mfp->occupied   += size;
        return mem;
    }

    res = mfp_alloc(mfp, size);
    memcpy(res, mem, blk->blk_size);
    mfp_free(mfp, mem);
    return res;
}

static mem_pool_t const mem_fifo_pool_funcs = {
    (void *)&mfp_alloc,
    (void *)&mfp_alloc, /* we use maps, always set to 0 */
    (void *)&mfp_realloc,
    (void *)&mfp_free,
};

mem_pool_t *mem_fifo_pool_new(int page_size_hint)
{
    mem_fifo_pool_t *mfp;

    mfp             = p_new(mem_fifo_pool_t, 1);
    mfp->funcs      = mem_fifo_pool_funcs;
    mfp->page_size  = MAX(16 * 4096, ROUND_MULTIPLE(page_size_hint, 4096));

    return &mfp->funcs;
}

void mem_fifo_pool_delete(mem_pool_t **poolp)
{
    if (*poolp) {
        mem_fifo_pool_t *mfp = (mem_fifo_pool_t *)(*poolp);

#ifndef NDEBUG
        if (mfp->fullpages || mfp->pages)
            e_error("possible memory leak in memory pool %p", *poolp);
#endif

        while (mfp->freelist) {
            mem_page_t *page = mem_page_list_popfirst(&mfp->freelist);
            mem_page_delete(&page);
            mfp->nb_pages--;
        }

        while (mfp->fullpages) {
            mem_page_t *page = mem_page_list_popfirst(&mfp->fullpages);
            mem_page_delete(&page);
            mfp->nb_pages--;
        }

        while (mfp->pages) {
            mem_page_t *page = mem_page_list_popfirst(&mfp->pages);
            mem_page_delete(&page);
            mfp->nb_pages--;
        }

        p_delete(poolp);
    }
}

void mem_fifo_pool_stats(mem_pool_t *mp, ssize_t *allocated, ssize_t *used)
{
    mem_fifo_pool_t *mfp = (mem_fifo_pool_t *)(mp);
    /* we don't want to account the 'spare' page as allocated, it's an
       optimization that should not leak. */
    *allocated = (mfp->nb_pages - (mfp->freelist != NULL))
               * (mfp->page_size - ssizeof(mem_page_t));
    *used      = mfp->occupied;
}

static void *msp_alloc(mem_scatter_pool_t *msp, int size)
{
    mem_page_t *page = msp->locked;
    mem_simple_block_t *sblk;
    mem_block_t *blk;

    assert (page && page->lock);

    size = ROUND_MULTIPLE((unsigned)size + sizeof(mem_simple_block_t), 8);
    blk  = page->lock;

    if (size <= mem_page_size_left(page)) {
        sblk = (mem_simple_block_t *)(blk->area + blk->blk_size);
        blk->blk_size      += size;
        page->used_size    += size;
        msp->mfp->occupied += size;
    } else {
        sblk = mfp_alloc(msp->mfp, size);
        page->lock = NULL;

        blk = blk->next = blkof(sblk);
        msp->locked = pageof(blk);
        msp->locked->lock = blk;
    }

    sblk->sblk_size = size - sizeof(mem_simple_block_t);
    return sblk->area;
}

static void msp_free(mem_scatter_pool_t *msp, void *mem)
{
    sblkof(mem)->sblk_free = 1;
}

static void *msp_realloc(mem_scatter_pool_t *mfp, void *mem, int size)
{
    mem_simple_block_t *sblk = sblkof(mem);

    if (unlikely(size < 0))
        e_panic(E_PREFIX("invalid memory size %d"), size);
    if (unlikely(size == 0)) {
        sblk->sblk_free = 1;
        return NULL;
    }

    mem = msp_alloc(mfp, size);
    memcpy(mem, sblk->area, sblk->sblk_size);
    return mem;
}

mem_pool_t *mem_scatter_pool_new(mem_pool_t *mp)
{
    mem_fifo_pool_t *mfp = (mem_fifo_pool_t *)mp;
    mem_scatter_pool_t *msp;
    mem_block_t *blk;

    msp = mfp_alloc(mfp, sizeof(mem_scatter_pool_t));
    msp->funcs.mem_alloc   = (void *)&msp_alloc;
    msp->funcs.mem_alloc0  = (void *)&msp_alloc;
    msp->funcs.mem_realloc = (void *)&msp_realloc;
    msp->funcs.mem_free    = (void *)&msp_free;
    msp->refcnt = 1;
    msp->mfp = mfp;

    blk = blkof(msp);
    msp->locked = pageof(blk);
    msp->locked->lock = blk;
    return &msp->funcs;
}

void mem_scatter_pool_unlock(mem_pool_t *mp)
{
    mem_scatter_pool_t *msp = (mem_scatter_pool_t *)mp;
    msp->locked->lock = NULL;
    msp->locked = NULL;
}

mem_pool_t *mem_scatter_pool_dup(mem_pool_t *mp)
{
    ((mem_scatter_pool_t *)mp)->refcnt++;
    return mp;
}

void mem_scatter_pool_delete(mem_pool_t **mp)
{
    mem_scatter_pool_t **msp = (mem_scatter_pool_t **)mp;
    if (*msp) {
        if (--(*msp)->refcnt <= 0) {
            mfp_free((*msp)->mfp, (*msp));
        }
        *msp = NULL;
    }
}
