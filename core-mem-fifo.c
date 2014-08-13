/**************************************************************************/
/*                                                                        */
/*  Copyright (C) 2004-2014 INTERSEC SA                                   */
/*                                                                        */
/*  Should you receive a copy of this source code, you must check you     */
/*  have a proper, written authorization of INTERSEC to hold it. If you   */
/*  don't have such an authorization, you must DELETE all source code     */
/*  files in your possession, and inform INTERSEC of the fact you obtain  */
/*  these files. Should you not comply to these terms, you can be         */
/*  prosecuted in the extent permitted by applicable law.                 */
/*                                                                        */
/**************************************************************************/

#include "core.h"

#ifdef MEM_BENCH

#include "core-mem-bench.h"
#include "thr.h"

static DLIST(mem_fifo_pool_list);
static spinlock_t mem_fifo_dlist_lock;
static FILE *mem_fifo_data_file;
static spinlock_t mem_fifo_file_lock;

#endif

typedef struct mem_page_t {
    void    *start;
    size_t   size;
    uint32_t used_size;
    uint32_t used_blocks;

    void    *last;           /* last result of an allocation */

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

#ifdef MEM_BENCH
    /* Instrumentation */
    mem_bench_t  mem_bench;
    dlist_t      pool_list;
#endif

} mem_fifo_pool_t;

static ALWAYS_INLINE mem_page_t *pageof(mem_block_t *blk)
{
    return (mem_page_t *)((char *)blk - blk->page_offs);
}

static mem_page_t *mem_page_new(mem_fifo_pool_t *mfp, uint32_t minsize)
{
    mem_page_t *page = mfp->freepage;
    uint32_t mapsize;

    if (page && page->size >= minsize) {
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

    page->start = page->area;
    page->size  = mapsize - sizeof(mem_page_t);
    mem_tool_disallow_memory(page->area, page->size);
    mfp->nb_pages++;
    mfp->map_size   += mapsize;

#ifdef MEM_BENCH
    mfp->mem_bench.malloc_calls++;
    mfp->mem_bench.current_allocated += mapsize;
    mfp->mem_bench.total_allocated += mapsize;
    mem_bench_update_max(&mfp->mem_bench);
    mem_fifo_write_stats(&mfp->funcs, "newpage");
#endif

    return page;
}

static void mem_page_reset(mem_page_t *page)
{
    mem_tool_allow_memory(page->area, page->used_size, false);
    p_clear(page->area, page->used_size);
    mem_tool_disallow_memory(page->area, page->size);

    page->used_size   = 0;
    page->used_blocks = 0;
    page->last        = NULL;
}

static void mem_page_delete(mem_fifo_pool_t *mfp, mem_page_t **pagep)
{
    mem_page_t *page = *pagep;

    if (page) {
#ifdef MEM_BENCH
        mfp->mem_bench.current_allocated -= page->size + sizeof(mem_page_t);
        mem_fifo_write_stats(&mfp->funcs, "page_delete");
#endif

        mfp->nb_pages--;
        mfp->map_size -= page->size + sizeof(mem_page_t);
        mem_tool_allow_memory(page, page->size + sizeof(mem_page_t), true);
        munmap(page, page->size + sizeof(mem_page_t));
    }
    *pagep = NULL;
}

static uint32_t mem_page_size_left(mem_page_t *page)
{
    return (page->size - page->used_size);
}

static void *mfp_alloc(mem_pool_t *_mfp, size_t size, size_t alignment,
                       mem_flags_t flags)
{
    mem_fifo_pool_t *mfp = container_of(_mfp, mem_fifo_pool_t, funcs);
    mem_block_t *blk;
    mem_page_t *page;
    size_t req_size = size;

#ifdef MEM_BENCH
    proctimer_t ptimer;
    proctimer_start(&ptimer);
#endif

    if (alignment > 3) {
        e_panic("mem_fifo_pool does not support alignments greater than 8");
    }

    /* Must round size up to keep proper alignment */
    size = ROUND_UP((unsigned)size + sizeof(mem_block_t), 8);

    if (unlikely(!mfp->alive))
        e_panic("trying to allocate from a dead pool");

    page = mfp->current;
    assert (!page || page->used_blocks != 0);
    if (!page || mem_page_size_left(page) < size) {
        page = mfp->current = mem_page_new(mfp, size);

#ifdef MEM_BENCH
        mfp->mem_bench.alloc.nb_slow_path++;
#endif
    }

    blk = (mem_block_t *)(page->area + page->used_size);
    mem_tool_allow_memory(blk, sizeof(*blk), true);
    mem_tool_malloclike(blk->area, req_size, 0, true);
    blk->page_offs = (uintptr_t)blk - (uintptr_t)page;
    blk->blk_size  = size;
    mem_tool_disallow_memory(blk, sizeof(*blk));

    mfp->occupied   += size;
    page->used_size += size;
    page->used_blocks++;
    mfp->used_blocks++;

#ifdef MEM_BENCH
    proctimer_stop(&ptimer);
    proctimerstat_addsample(&mfp->mem_bench.alloc.timer_stat, &ptimer);

    mfp->mem_bench.alloc.nb_calls++;
    mfp->mem_bench.current_used = mfp->occupied;
    mfp->mem_bench.total_requested += req_size;
    mem_bench_update_max(&mfp->mem_bench);
    mem_fifo_write_stats(&mfp->funcs, "alloc");
#endif

    return page->last = blk->area;
}

static void mfp_free(mem_pool_t *_mfp, void *mem)
{
    mem_fifo_pool_t *mfp = container_of(_mfp, mem_fifo_pool_t, funcs);
    mem_block_t *blk;
    mem_page_t *page;

#ifdef MEM_BENCH
    proctimer_t ptimer;
    proctimer_start(&ptimer);
#endif

    if (!mem)
        return;

    blk  = container_of(mem, mem_block_t, area);
    mem_tool_allow_memory(blk, sizeof(*blk), true);
    page = pageof(blk);
    mfp->occupied -= blk->blk_size;
    mem_tool_freelike(mem, blk->blk_size - sizeof(*blk), 0);
    mem_tool_disallow_memory(blk, sizeof(*blk));

    mfp->used_blocks--;
    if (--page->used_blocks > 0) {
#ifdef MEM_BENCH
        proctimer_stop(&ptimer);
        proctimerstat_addsample(&mfp->mem_bench.free.timer_stat, &ptimer);

        mfp->mem_bench.free.nb_calls++;
        mfp->mem_bench.current_used = mfp->occupied;
        mem_bench_update_max(&mfp->mem_bench);
        mem_fifo_write_stats(&mfp->funcs, "free");
#endif
        return;
    }

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

#ifdef MEM_BENCH
    proctimer_stop(&ptimer);
    proctimerstat_addsample(&mfp->mem_bench.free.timer_stat, &ptimer);

    mfp->mem_bench.free.nb_calls++;
    mfp->mem_bench.free.nb_slow_path++;
    mfp->mem_bench.current_used = mfp->occupied;
    mem_bench_update_max(&mfp->mem_bench);
    mem_fifo_write_stats(&mfp->funcs, "free");
#endif
}

static void *mfp_realloc(mem_pool_t *_mfp, void *mem, size_t oldsize,
                         size_t size, size_t alignment, mem_flags_t flags)
{
    mem_fifo_pool_t *mfp = container_of(_mfp, mem_fifo_pool_t, funcs);
    mem_block_t *blk;
    mem_page_t *page;
    size_t alloced_size;
    size_t req_size = size;

#ifdef MEM_BENCH
    proctimer_t ptimer;
    proctimer_start(&ptimer);
#endif

    if (alignment > 3) {
        e_panic("mem_fifo_pool does not support alignments greater than 8");
    }

    if (unlikely(!mfp->alive)) {
        e_panic("trying to reallocate from a dead pool");
    }

    if (unlikely(size == 0)) {
        mfp_free(_mfp, mem);
        return NULL;
    }

    if (!mem) {
        return mfp_alloc(_mfp, size, alignment, flags);
    }

    blk  = container_of(mem, mem_block_t, area);
    mem_tool_allow_memory(blk, sizeof(*blk), true);
    page = pageof(blk);

    alloced_size = blk->blk_size - sizeof(*blk);
    if ((flags & MEM_RAW) && oldsize == MEM_UNKNOWN)
        oldsize = alloced_size;
    assert (oldsize <= alloced_size);
    if (req_size <= alloced_size) {
        mem_tool_freelike(mem, oldsize, 0);
        mem_tool_malloclike(mem, req_size, 0, false);
        mem_tool_allow_memory(mem, MIN(req_size, oldsize), true);
        if (!(flags & MEM_RAW) && oldsize < req_size)
            memset(blk->area + oldsize, 0, req_size - oldsize);
    } else
    /* optimization if it's the last block allocated */
    if (mem == page->last
    && req_size - alloced_size <= mem_page_size_left(page))
    {
        size_t diff;

        size = ROUND_UP((size_t)req_size + sizeof(*blk), 8);
        diff = size - blk->blk_size;
        blk->blk_size    = size;

        mfp->occupied   += diff;
        page->used_size += diff;
        mem_tool_freelike(mem, oldsize, 0);
        mem_tool_malloclike(mem, req_size, 0, false);
        mem_tool_allow_memory(mem, MIN(req_size, oldsize), true);
    } else {
        void *old = mem;

        mem = mfp_alloc(_mfp, size, alignment, flags);
        memcpy(mem, old, oldsize);
        mfp_free(_mfp, old);
        return mem;
    }

#ifdef MEM_BENCH
    proctimer_stop(&ptimer);
    proctimerstat_addsample(&mfp->mem_bench.realloc.timer_stat, &ptimer);

    mfp->mem_bench.realloc.nb_calls++;
    mfp->mem_bench.current_used = mfp->occupied;
    mem_bench_update_max(&mfp->mem_bench);
#endif

    mem_tool_disallow_memory(blk, sizeof(*blk));
    return mem;
}

static mem_pool_t const mem_fifo_pool_funcs = {
    .malloc   = &mfp_alloc,
    .realloc  = &mfp_realloc,
    .free     = &mfp_free,
    .mem_pool = MEM_OTHER,
    .min_alignment = 8
};

mem_pool_t *mem_fifo_pool_new(int page_size_hint)
{
    mem_fifo_pool_t *mfp = p_new(mem_fifo_pool_t, 1);

    STATIC_ASSERT((offsetof(mem_page_t, area) % 8) == 0);
    mfp->funcs     = mem_fifo_pool_funcs;
    mfp->page_size = MAX(16 * 4096, ROUND_UP(page_size_hint, 4096));
    mfp->alive     = true;

#ifdef MEM_BENCH
    spin_lock(&mem_fifo_dlist_lock);
    dlist_add_tail(&mem_fifo_pool_list, &mfp->pool_list);
    spin_unlock(&mem_fifo_dlist_lock);
#endif

    return &mfp->funcs;
}

void mem_fifo_pool_delete(mem_pool_t **poolp)
{
    mem_fifo_pool_t *mfp;

    if (!*poolp)
        return;

    mfp = container_of(*poolp, mem_fifo_pool_t, funcs);

#ifdef MEM_BENCH
    dlist_remove(&mfp->pool_list);
    mem_bench_print_human(&mfp->mem_bench, 0);
#endif

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
#ifdef MEM_BENCH
    spin_lock(&mem_fifo_dlist_lock);
    if (dlist_is_empty(&mem_fifo_pool_list)) {
        spin_lock(&mem_fifo_file_lock);
        if (mem_fifo_data_file) {
            fclose(mem_fifo_data_file);
            mem_fifo_data_file = NULL;
        }
        spin_unlock(&mem_fifo_dlist_lock);
    }
    spin_unlock(&mem_fifo_file_lock);
#endif
}

void mem_fifo_pool_stats(mem_pool_t *mp, ssize_t *allocated, ssize_t *used)
{
    mem_fifo_pool_t *mfp = (mem_fifo_pool_t *)(mp);
    /* we don't want to account the 'spare' page as allocated, it's an
       optimization that should not leak. */
    *allocated = mfp->map_size - (mfp->freepage ? mfp->freepage->size : 0);
    *used      = mfp->occupied;
}

#ifdef MEM_BENCH
static int mem_fifo_stats_open_file(void) {
    /* No need for locking here since this is only called by write_stats, and
       the lock is already acquired
     */
    if (!mem_fifo_data_file) {
        mem_fifo_data_file = RETHROW_PN(fopen("./mem.fifo.data", "w"));
    }
    return 0;
}
#endif

void mem_fifo_write_stats(mem_pool_t *mp, const char *context) {
#ifdef MEM_BENCH
    mem_fifo_pool_t *mfp = container_of(mp, mem_fifo_pool_t, funcs);

    spin_lock(&mem_fifo_file_lock);
    if (mem_fifo_data_file || mem_fifo_stats_open_file() >= 0) {
        mem_bench_print_csv(&mfp->mem_bench, context, mem_fifo_data_file);
    }
    spin_unlock(&mem_fifo_file_lock);
#endif
}

void mem_fifo_pools_print_stats(void) {
#ifdef MEM_BENCH
    printf("\x1b[32mPrinting all FIFO allocators state:\x1b[0m\n");
    spin_lock(&mem_fifo_dlist_lock);
    dlist_for_each_safe(n, &mem_fifo_pool_list) {
        mem_fifo_pool_t *mfp = container_of(n, mem_fifo_pool_t, pool_list);
        mem_bench_print_human(&mfp->mem_bench, MEM_BENCH_PRINT_CURRENT);
    }
    spin_unlock(&mem_fifo_dlist_lock);
#endif
}

