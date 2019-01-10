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

#include "core-mem-valgrind.h"
#include "core.h"

typedef struct mem_page_t {
    mem_blk_t page;
    uint32_t  used_size;
    uint32_t  used_blocks;

    void *last;           /* last result of an allocation */

    byte __attribute__((aligned(8))) area[];
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
    uint32_t    used_blocks;
    size_t      map_size;

    /* p_delete codepath */
    bool        alive;
    mem_pool_t **owner;
} mem_fifo_pool_t;


static mem_page_t *pageof(mem_block_t *blk) {
    mem_page_t *page;

    (void)VALGRIND_MAKE_MEM_DEFINED(blk, sizeof(*blk));
    page = (mem_page_t *)((char *)blk - blk->page_offs);
    return page;
}

static void blk_protect(mem_block_t *blk)
{
    (void)VALGRIND_MAKE_MEM_NOACCESS(blk, sizeof(*blk));
}

static mem_page_t *mem_page_new(mem_fifo_pool_t *mfp, uint32_t minsize)
{
    mem_page_t *page = mfp->freepage;
    uint32_t mapsize;

    if (page && page->page.size >= minsize) {
        mfp->freepage = NULL;
        return page;
    }

    if (minsize < mfp->page_size - sizeof(mem_page_t)) {
        mapsize = mfp->page_size;
    } else {
        mapsize = ROUND_UP(minsize + sizeof(mem_page_t), 4096);
    }

    page = mmap(NULL, mapsize, PROT_READ | PROT_WRITE,
                MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
    if (page == MAP_FAILED) {
        e_panic(E_UNIXERR("mmap"));
    }

    page->page.start = page->area;
    page->page.pool  = &mfp->funcs;
    page->page.size  = mapsize - sizeof(mem_page_t);
    mem_register(&page->page);
    mfp->nb_pages++;
    mfp->map_size   += mapsize;
    return page;
}

static void mem_page_reset(mem_page_t *page)
{
    (void)VALGRIND_MAKE_MEM_DEFINED(page->area, page->used_size);
    p_clear(page->area, page->used_size);
    VALGRIND_PROT_BLK(&page->page);

    page->used_size   = 0;
    page->used_blocks = 0;
    page->last        = NULL;
}

static void mem_page_delete(mem_fifo_pool_t *mfp, mem_page_t **pagep)
{
    mem_page_t *page = *pagep;

    if (page) {
        mfp->nb_pages--;
        mfp->map_size -= page->page.size;
        mem_unregister(&page->page);
        munmap(page, page->page.size + sizeof(mem_page_t));
    }
    *pagep = NULL;
}

static uint32_t mem_page_size_left(mem_page_t *page)
{
    return (page->page.size - page->used_size);
}

#if __GNUC_PREREQ(4, 6) && !__VALGRIND_PREREQ(3, 7)
#  pragma GCC diagnostic push
#  pragma GCC diagnostic ignored "-Wunused-but-set-variable"
#endif
static void *mfp_alloc(mem_pool_t *_mfp, size_t size, mem_flags_t flags)
{
    mem_fifo_pool_t *mfp = container_of(_mfp, mem_fifo_pool_t, funcs);
    mem_block_t *blk;
    mem_page_t *page;
#ifndef NDEBUG
    size_t req_size = size;
#endif

    /* Must round size up to keep proper alignment */
    size = ROUND_UP((unsigned)size + sizeof(mem_block_t), 8);

    if (unlikely(!mfp->alive))
        e_panic("trying to allocate from a dead pool");

    page = mfp->current;
    assert (!page || page->used_blocks != 0);
    if (!page || mem_page_size_left(page) < size) {
        page = mfp->current = mem_page_new(mfp, size);
    }

    blk = (mem_block_t *)(page->area + page->used_size);
    (void)VALGRIND_MAKE_MEM_DEFINED(blk, sizeof(*blk));
    VALGRIND_MEMPOOL_ALLOC(page, blk->area, req_size);
    blk->page_offs = (uintptr_t)blk - (uintptr_t)page;
    blk->blk_size  = size;
    blk_protect(blk);

    mfp->occupied   += size;
    page->used_size += size;
    page->used_blocks++;
    mfp->used_blocks++;
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

    mfp->used_blocks--;
    if (--page->used_blocks > 0)
        return;

    /* specific case for a dying pool */
    if (unlikely(!mfp->alive)) {
        mem_page_delete(mfp, &page);
        if (mfp->nb_pages == 0)
            p_delete(mfp->owner);
        return;
    }

    /* this was the last block, collect this page */
    if (page == mfp->current) {
        mfp->current = NULL;
    }

    /* keep the page around if we have none kept around yet */
    if (mfp->freepage) {
        mem_page_delete(mfp, &page);
    } else {
        mem_page_reset(page);
        mfp->freepage = page;
    }
}

static void *mfp_realloc(mem_pool_t *_mfp, void *mem, size_t oldsize, size_t size, mem_flags_t flags)
{
    mem_fifo_pool_t *mfp = container_of(_mfp, mem_fifo_pool_t, funcs);
    mem_block_t *blk;
    mem_page_t *page;
    size_t alloced_size;
    size_t req_size = size;
    void *res;

    if (unlikely(!mfp->alive))
        e_panic("trying to reallocate from a dead pool");

    if (unlikely(size == 0)) {
        mfp_free(_mfp, mem, flags);
        return NULL;
    }

    if (!mem)
        return mfp_alloc(_mfp, size, flags);

    blk  = container_of(mem, mem_block_t, area);
    page = pageof(blk);

    alloced_size = blk->blk_size - sizeof(*blk);
    if ((flags & MEM_RAW) && oldsize == MEM_UNKNOWN)
        oldsize = alloced_size;
    assert (oldsize <= alloced_size);
    if (req_size <= alloced_size) {
        blk_protect(blk);
        if (!(flags & MEM_RAW) && oldsize < req_size)
            memset(blk->area + oldsize, 0, req_size - oldsize);
        return mem;
    }

    /* optimization if it's the last block allocated */
    if (mem == page->last
    && req_size - alloced_size <= mem_page_size_left(page))
    {
        size_t diff;

        size = ROUND_UP((size_t)req_size + sizeof(*blk), 8);
        diff = size - blk->blk_size;
        blk->blk_size    = size;
        VALGRIND_MEMPOOL_CHANGE(page, blk->area, blk->area, req_size);
        blk_protect(blk);

        mfp->occupied   += diff;
        page->used_size += diff;
        return mem;
    }

    res = memcpy(mfp_alloc(_mfp, size, flags), mem, oldsize);
    mfp_free(_mfp, mem, flags);
    return res;
}
#if __GNUC_PREREQ(4, 6) && !__VALGRIND_PREREQ(3, 7)
#  pragma GCC diagnostic pop
#endif

static mem_pool_t const mem_fifo_pool_funcs = {
    .malloc  = &mfp_alloc,
    .realloc = &mfp_realloc,
    .free    = &mfp_free,
};

mem_pool_t *mem_fifo_pool_new(int page_size_hint)
{
    mem_fifo_pool_t *mfp = p_new(mem_fifo_pool_t, 1);

    STATIC_ASSERT((offsetof(mem_page_t, area) % 8) == 0);
    mfp->funcs     = mem_fifo_pool_funcs;
    mfp->page_size = MAX(16 * 4096, ROUND_UP(page_size_hint, 4096));
    mfp->alive     = true;
    return &mfp->funcs;
}

void mem_fifo_pool_delete(mem_pool_t **poolp)
{
    mem_fifo_pool_t *mfp;

    if (!*poolp)
        return;

    mfp = container_of(*poolp, mem_fifo_pool_t, funcs);
    mfp->alive = false;
    mem_page_delete(mfp, &mfp->freepage);
    if (mfp->current && mfp->current->used_blocks == 0) {
        mem_page_delete(mfp, &mfp->current);
    } else {
        mfp->current = NULL;
    }

    if (mfp->nb_pages) {
        e_trace(0, "keep fifo-pool alive: %d pages in use (mem: %dbytes in %u blocks)",
                mfp->nb_pages, mfp->occupied, mfp->used_blocks);
        mfp->owner   = poolp;
        return;
    }
    p_delete(poolp);
}

void mem_fifo_pool_stats(mem_pool_t *mp, ssize_t *allocated, ssize_t *used)
{
    mem_fifo_pool_t *mfp = (mem_fifo_pool_t *)(mp);
    /* we don't want to account the 'spare' page as allocated, it's an
       optimization that should not leak. */
    *allocated = mfp->map_size -= mfp->freepage ? mfp->freepage->page.size : 0;
    *used      = mfp->occupied;
}
