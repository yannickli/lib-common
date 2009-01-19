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

#ifndef NDEBUG
#  include <valgrind/valgrind.h>
#  include <valgrind/memcheck.h>
#  ifndef VALGRIND_MEMPOOL_CHANGE
#    define VALGRIND_MEMPOOL_CHANGE(pool, oaddr, naddr, size) \
        do {                                                  \
             VALGRIND_MEMPOOL_FREE(pool, oaddr);              \
             VALGRIND_MEMPOOL_ALLOC(pool, naddr, size);       \
        } while (0)
#  endif
#else
#  define VALGRIND_CREATE_MEMPOOL(...)
#  define VALGRIND_DESTROY_MEMPOOL(...)
#  define VALGRIND_MAKE_MEM_DEFINED(...)
#  define VALGRIND_MAKE_MEM_NOACCESS(...)
#  define VALGRIND_MEMPOOL_ALLOC(...)
#  define VALGRIND_MEMPOOL_CHANGE(...)
#  define VALGRIND_MEMPOOL_FREE(...)
#endif
#include <sys/mman.h>
#include "mem-pool.h"

#define ROUND_MULTIPLE(n, k) ((((n) + (k) - 1) / (k)) * (k))

typedef struct mem_page_t {
    struct mem_page_t *next;

    int area_size;
    int used_size;
    int used_blocks;

    void *last;           /* last result of an allocation */

    byte area[];
} mem_page_t;

typedef struct mem_block_t {
    int page_offs;
    int blk_size;
    byte area[];
} mem_block_t;

typedef struct mem_fifo_pool_t {
    mem_pool_t funcs;
    mem_page_t *pages;
    mem_page_t *freelist;
    int page_size;
    int nb_pages;
    int occupied;
} mem_fifo_pool_t;


static mem_page_t *pageof(mem_block_t *blk) {
    mem_page_t *page;

    VALGRIND_MAKE_MEM_DEFINED(blk, sizeof(*blk));
    page = (mem_page_t *)((char *)blk + blk->page_offs);
    return page;
}

static inline void blk_protect(mem_block_t *blk)
{
    VALGRIND_MAKE_MEM_NOACCESS(blk, sizeof(*blk));
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
    VALGRIND_CREATE_MEMPOOL(page, 0, true);
    VALGRIND_MAKE_MEM_NOACCESS(page->area, page->area_size);
    return page;
}

static void mem_page_reset(mem_page_t *page)
{
    VALGRIND_MAKE_MEM_DEFINED(page->area, page->used_size);
    p_clear(page->area, page->used_size);
    VALGRIND_MAKE_MEM_NOACCESS(page->area, page->area_size);

    page->used_size   = 0;
    page->used_blocks = 0;
    page->last        = NULL;
}

static void mem_page_delete(mem_page_t **pagep)
{
    if (*pagep) {
        mem_page_t *page = *pagep;
        VALGRIND_DESTROY_MEMPOOL(page);
        VALGRIND_MAKE_MEM_NOACCESS(page->area, page->area_size);
        munmap(page, page->area_size + sizeof(mem_page_t));
        *pagep = NULL;
    }
}

static inline int mem_page_size_left(mem_page_t *page)
{
    return (page->area_size - page->used_size);
}

static void *mfp_alloc(mem_fifo_pool_t *mfp, int size)
{
    mem_block_t *blk;
    mem_page_t *page;
#ifndef NDEBUG
    int asked = size;
#endif

    /* Must round size up to keep proper alignment */
    size = ROUND_MULTIPLE((unsigned)size + sizeof(mem_block_t), 8);

    if (size > mfp->page_size - ssizeof(mem_page_t)) {
        /* Should just map a larger page, yet we need a maximum value */
        e_panic(E_PREFIX("tried to alloc %d bytes, cannot have more than %d"),
                size, mfp->page_size);
    }

    page = mfp->pages;
    if (!page || mem_page_size_left(page) < size) {
        if (mfp->freelist) {
            /* push the first free page on top of the pages list */
            page = mfp->freelist;
            mfp->freelist = page->next;
        } else {
            /* mmap a new page */
            page = mem_page_new(mfp);
            mfp->nb_pages++;
        }
        page->next = mfp->pages;
        mfp->pages = page;
    }

    blk = (mem_block_t *)(page->area + page->used_size);
    VALGRIND_MAKE_MEM_DEFINED(blk, sizeof(*blk));
    VALGRIND_MEMPOOL_ALLOC(page, blk + sizeof(*blk), asked);
    blk->page_offs = (intptr_t)page - (intptr_t)blk;
    blk->blk_size  = size;
    blk_protect(blk);

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

    blk  = container_of(mem, mem_block_t, area);
    page = pageof(blk);
    mfp->occupied -= blk->blk_size;
    VALGRIND_MEMPOOL_FREE(page, blk + sizeof(*blk));
    blk_protect(blk);

    /* if this was the last block, GC the pages */
    if (--page->used_blocks <= 0) {
        mem_page_t **pagep;

        if (page == mfp->pages) {
            /* if we're the current page, reset our memory and start over */
            mem_page_reset(page);
        } else
        for (pagep = &mfp->pages->next; *pagep; pagep = &(*pagep)->next) {
            page = *pagep;

            if (page->used_blocks != 0)
                continue;

            *pagep = page->next;

            if (mfp->freelist || mfp->nb_pages == 1) {
                mem_page_delete(&page);
                mfp->nb_pages--;
            } else {
                /* Clear area to 0 to ensure allocated blocks are
                 * cleared.  mfp_malloc relies on this.
                 */
                mem_page_reset(page);
                page->next    = mfp->freelist;
                mfp->freelist = page;
            }
            break;
        }
    }
}

static void mfp_realloc(mem_fifo_pool_t *mfp, void **memp, int size)
{
    mem_block_t *blk;
    mem_page_t *page;
    void *mem = *memp;

    if (unlikely(size < 0))
        e_panic(E_PREFIX("invalid memory size %d"), size);
    if (unlikely(size == 0)) {
        mfp_free(mfp, mem);
        return;
    }

    if (!mem) {
        mfp_alloc(mfp, size);
        return;
    }

    blk  = container_of(mem, mem_block_t, area);
    page = pageof(blk);

    if (size <= blk->blk_size - ssizeof(*blk)) {
        VALGRIND_MEMPOOL_CHANGE(page, blk + sizeof(*blk), blk + sizeof(*blk),
                                size);
        blk_protect(blk);
        return;
    }

    /* optimization if it's the last block allocated */
    if (mem == page->last
    && size + ssizeof(*blk) - blk->blk_size <= mem_page_size_left(page))
    {
        VALGRIND_MEMPOOL_CHANGE(page, blk + sizeof(*blk), blk + sizeof(*blk),
                                size);
        size = ROUND_MULTIPLE((size_t)size, 8);
        blk->blk_size   += size;
        blk_protect(blk);

        mfp->occupied   += size;
        page->used_size += size;
        return;
    }

    memcpy(mfp_alloc(mfp, size), mem, blk->blk_size);
    mfp_free(mfp, mem);
}

static mem_pool_t const mem_fifo_pool_funcs = {
    (void *)&mfp_alloc,
    (void *)&mfp_alloc, /* we use maps, always set to 0 */
    (void *)&mfp_realloc,
    (void *)&mfp_free,
};

mem_pool_t *mem_fifo_pool_new(int page_size_hint)
{
    mem_fifo_pool_t *mfp = p_new(mem_fifo_pool_t, 1);

    mfp->funcs     = mem_fifo_pool_funcs;
    mfp->page_size = MAX(16 * 4096, ROUND_MULTIPLE(page_size_hint, 4096));
    return &mfp->funcs;
}

void mem_fifo_pool_delete(mem_pool_t **poolp)
{
    if (*poolp) {
        mem_fifo_pool_t *mfp = (mem_fifo_pool_t *)(*poolp);

        while (mfp->freelist) {
            mem_page_t *page = mfp->freelist;

            mfp->freelist = page->next;
            mem_page_delete(&page);
            mfp->nb_pages--;
        }

        while (mfp->pages) {
            mem_page_t *page = mfp->pages;

            mfp->pages = page->next;
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
