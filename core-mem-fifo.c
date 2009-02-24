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

/*
 * TODO: make the mem_fifo_pool_t refcounted by its pages + itself
 *       so that destroying the pool allow data to outlive it.
 *
 *       That would mean no more clumsy mchannel segfaults due to pools
 *       dropped too early.
 */

#include "core-mem-valgrind.h"
#include "core.h"

typedef struct mem_page_t {
    mem_blk_t page;
    uint32_t  used_size;
    uint32_t  used_blocks;

    void *last;           /* last result of an allocation */

    byte area[];
} mem_page_t;

typedef struct mem_block_t {
    uint32_t page_offs;
    uint32_t blk_size;
    byte     area[];
} mem_block_t;

typedef struct mem_fifo_pool_t {
    mem_pool_t  funcs;
    mem_page_t *freepage;
    mem_page_t *current;
    uint32_t    page_size;
    uint32_t    nb_pages;
    uint32_t    occupied;
} mem_fifo_pool_t;


static mem_page_t *pageof(mem_block_t *blk) {
    mem_page_t *page;

    VALGRIND_MAKE_MEM_DEFINED(blk, sizeof(*blk));
    page = (mem_page_t *)((char *)blk - blk->page_offs);
    return page;
}

static void blk_protect(mem_block_t *blk)
{
    VALGRIND_MAKE_MEM_NOACCESS(blk, sizeof(*blk));
}

static mem_page_t *mem_page_new(mem_fifo_pool_t *mfp)
{
    uint32_t size = mfp->page_size - sizeof(mem_page_t);
    mem_page_t *page;

    if (mfp->freepage) {
        page = mfp->freepage;
        mfp->freepage = NULL;
        return page;
    }

    page = mmap(NULL, mfp->page_size, PROT_READ | PROT_WRITE,
                      MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
    if (page == MAP_FAILED) {
        e_panic(E_UNIXERR("mmap"));
    }

    page->page.start = page->area;
    page->page.pool  = &mfp->funcs;
    page->page.size  = size;
    mem_register(&page->page);
    mfp->nb_pages++;
    return page;
}

static void mem_page_reset(mem_page_t *page)
{
    VALGRIND_MAKE_MEM_DEFINED(page->area, page->used_size);
    p_clear(page->area, page->used_size);
    VALGRIND_PROT_BLK(&page->page);

    page->used_size   = 0;
    page->used_blocks = 0;
    page->last        = NULL;
}

static void mem_page_delete(mem_fifo_pool_t *mfp, mem_page_t **pagep)
{
    if (*pagep) {
        mem_page_t *page = *pagep;

        mfp->nb_pages--;
        mem_unregister(&page->page);
        munmap(page, page->page.size + sizeof(mem_page_t));
        *pagep = NULL;
    }
}

static uint32_t mem_page_size_left(mem_page_t *page)
{
    return (page->page.size - page->used_size);
}

static void *mfp_alloc(mem_pool_t *_mfp, size_t size, mem_flags_t flags)
{
    mem_fifo_pool_t *mfp = container_of(_mfp, mem_fifo_pool_t, funcs);
    mem_block_t *blk;
    mem_page_t *page;

    /* Must round size up to keep proper alignment */
    size = ROUND_UP((unsigned)size + sizeof(mem_block_t), 8);

    if (unlikely(size > mfp->page_size - sizeof(mem_page_t))) {
        /* Should just map a larger page, yet we need a maximum value */
        e_panic(E_PREFIX("tried to alloc %zd bytes, cannot have more than %d"),
                size, mfp->page_size);
    }

    page = mfp->current;
    if (!page || mem_page_size_left(page) < size) {
        mfp->current = page = mem_page_new(mfp);
    }

    blk = (mem_block_t *)(page->area + page->used_size);
    VALGRIND_MAKE_MEM_DEFINED(blk, sizeof(*blk));
    VALGRIND_MEMPOOL_ALLOC(page, blk->area, size);
    blk->page_offs = (uintptr_t)blk - (uintptr_t)page;
    blk->blk_size  = size;
    blk_protect(blk);

    mfp->occupied   += size;
    page->used_size += size;
    page->used_blocks++;
    return page->last = blk->area;
}

static void mfp_free(mem_pool_t *_mfp, void *mem, mem_flags_t flags)
{
    mem_fifo_pool_t *mfp = container_of(_mfp, mem_fifo_pool_t, funcs);
    mem_block_t *blk;
    mem_page_t *page;

    if (!mem)
        return;

    blk  = container_of(mem, mem_block_t, area);
    page = pageof(blk);
    mfp->occupied -= blk->blk_size;
    VALGRIND_MEMPOOL_FREE(page, blk->area);
    blk_protect(blk);

    /* if this was the last block, GC the pages */
    if (--page->used_blocks <= 0) {
        if (page == mfp->current) {
            /* if we're the current page, reset our memory and start over */
            mem_page_reset(page);
        } else {
            if (mfp->freepage || mfp->nb_pages == 1) {
                mem_page_delete(mfp, &page);
            } else {
                /* Clear area to 0 to ensure allocated blocks are
                 * cleared.  mfp_malloc relies on this.
                 */
                mem_page_reset(page);
                mfp->freepage = page;
            }
        }
    }
}

static void mfp_realloc(mem_pool_t *_mfp, void **memp, size_t oldsize, size_t size, mem_flags_t flags)
{
    mem_fifo_pool_t *mfp = container_of(_mfp, mem_fifo_pool_t, funcs);
    mem_block_t *blk;
    mem_page_t *page;
    void *mem = *memp;

    if (unlikely(size > mfp->page_size - sizeof(mem_page_t))) {
        /* Should just map a larger page, yet we need a maximum value */
        e_panic(E_PREFIX("tried to alloc %zd bytes, cannot have more than %d"),
                size, mfp->page_size);
    }
    if (unlikely(size == 0)) {
        mfp_free(_mfp, mem, flags);
        *memp = NULL;
        return;
    }

    if (!mem) {
        *memp = mfp_alloc(_mfp, size, flags);
        return;
    }

    blk  = container_of(mem, mem_block_t, area);
    page = pageof(blk);

    if ((flags & MEM_RAW) && oldsize == MEM_UNKNOWN)
        oldsize = blk->blk_size;
    assert (oldsize <= blk->blk_size);
    if (size <= blk->blk_size - sizeof(*blk)) {
        blk_protect(blk);
        if (!(flags & MEM_RAW) && oldsize < size)
            memset(blk->area + oldsize, 0, size - oldsize);
        return;
    }

    /* optimization if it's the last block allocated */
    if (mem == page->last
    && size + ssizeof(*blk) - blk->blk_size <= mem_page_size_left(page))
    {
        size = ROUND_UP((size_t)size, 8);
        blk->blk_size   += size;
        VALGRIND_MEMPOOL_CHANGE(page, blk->area, blk->area, size);
        blk_protect(blk);

        mfp->occupied   += size;
        page->used_size += size;
        return;
    }

    *memp = memcpy(mfp_alloc(_mfp, size, flags), mem, oldsize);
    mfp_free(_mfp, mem, flags);
}

static mem_pool_t const mem_fifo_pool_funcs = {
    .malloc  = &mfp_alloc,
    .realloc = &mfp_realloc,
    .free    = &mfp_free,
};

mem_pool_t *mem_fifo_pool_new(int page_size_hint)
{
    mem_fifo_pool_t *mfp = p_new(mem_fifo_pool_t, 1);

    mfp->funcs     = mem_fifo_pool_funcs;
    mfp->page_size = MAX(16 * 4096, ROUND_UP(page_size_hint, 4096));
    return &mfp->funcs;
}

void mem_fifo_pool_delete(mem_pool_t **poolp)
{
    if (*poolp) {
        mem_fifo_pool_t *mfp = container_of(*poolp, mem_fifo_pool_t, funcs);
        if (mfp->nb_pages)
            e_trace(0, "dude, you're leaking memory. %d pages left !!", mfp->nb_pages);
        p_delete(poolp);
    }
}

void mem_fifo_pool_stats(mem_pool_t *mp, ssize_t *allocated, ssize_t *used)
{
    mem_fifo_pool_t *mfp = (mem_fifo_pool_t *)(mp);
    /* we don't want to account the 'spare' page as allocated, it's an
       optimization that should not leak. */
    *allocated = (mfp->nb_pages - (mfp->freepage != NULL))
               * (mfp->page_size - ssizeof(mem_page_t));
    *used      = mfp->occupied;
}
