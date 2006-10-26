/**************************************************************************/
/*                                                                        */
/*  Copyright (C) 2004-2006 INTERSEC                                      */
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
#include <errno.h>

#include "mem.h"
#include "mem-fifo-pool.h"

#define ROUND_MULTIPLE(n, k) ((((n) + (k) - 1) / (k)) * (k))

typedef struct mem_page {

    struct mem_page *next;
    struct mem_page *__prev__;  /* not used yet */

    int used_size;
    int used_blocks;

    int area_size;
    int page_size;

    byte area[];
} mem_page;

typedef struct mem_block {
    mem_page *page;
    byte area[];
} mem_block;

typedef struct mem_fifo_pool {
    mem_pool funcs;
    mem_page *pages;
    mem_page *freelist;
    int page_size;
    int nb_pages;
} mem_fifo_pool;


static mem_page *mem_page_new(mem_fifo_pool *mfp)
{
    int size = mfp->page_size - sizeof(mem_page);
    mem_page *page;

    page = mmap(NULL, mfp->page_size, PROT_READ | PROT_WRITE,
                      MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
    if (page == MAP_FAILED) {
        e_panic(E_UNIXERR("mmap"));
    }

    p_clear(page, 1);

    page->page_size = mfp->page_size;
    page->area_size = size;

    return page;
}

static void mem_page_reset(mem_page *page)
{
    page->next        = NULL;
    page->__prev__    = NULL;
    page->used_size   = 0;
    page->used_blocks = 0;
    memset(page->area, 0, page->area_size);
}

static void mem_page_delete(mem_page **pagep)
{
    if (*pagep) {
        munmap((*pagep), (*pagep)->page_size);
        *pagep = NULL;
    }
}

static inline int mem_page_size_left(mem_page *page)
{
    return (page->area_size - page->used_size);
}

static void *mfp_alloc(mem_pool *mp, ssize_t size)
{
    mem_fifo_pool *mfp = (mem_fifo_pool *)mp;
    mem_block *blk;
    mem_page *page;

    /* Must round size up to keep proper alignment */
    size = ROUND_MULTIPLE((size_t)size + sizeof(mem_block), 8);

    if (size > mfp->page_size - ssizeof(mem_page)) {
        /* Should just map a larger page, yet we need a maximum value */
        e_panic(E_PREFIX("tried to alloc %zd bytes, cannot have more than %d"),
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

    /* OG: blk->area alignment is not correct */
    blk = (mem_block *)(page->area + page->used_size);
    blk->page = page;
    page->used_size += size;
    page->used_blocks++;
    return blk->area;
}

static void mfp_free(struct mem_pool *mp, void *mem)
{
    mem_fifo_pool *mfp = (mem_fifo_pool *)mp;
    mem_block *blk;

    if (!mem)
        return;

    blk = (mem_block*)((byte *)mem - sizeof(mem_block));

    blk->page->used_blocks--;

    /* if this was the last block, GC the pages */
    if (blk->page->used_blocks == 0) {
        mem_page **pagep;

        for (pagep = &mfp->pages; *pagep; pagep = &(*pagep)->next) {
            mem_page *page = *pagep;

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

static mem_pool const mem_fifo_pool_funcs = {
    &mfp_alloc,
    &mfp_alloc, /* we use maps, always set to 0 */
    &mfp_free,
};

mem_pool *mem_fifo_pool_new(int page_size_hint)
{
    mem_fifo_pool *mfp;

    mfp             = p_new(mem_fifo_pool, 1);
    mfp->funcs      = mem_fifo_pool_funcs;
    mfp->page_size  = ROUND_MULTIPLE(page_size_hint, 4096);

    return (mem_pool *)mfp;
}

void mem_fifo_pool_delete(mem_pool **poolp)
{
    if (*poolp) {
        mem_fifo_pool *mfp = (mem_fifo_pool *)(*poolp);

        while (mfp->freelist) {
            mem_page *page = mfp->freelist;

            mfp->freelist = page->next;
            mem_page_delete(&page);
            mfp->nb_pages--;
        }

        while (mfp->pages) {
            mem_page *page = mfp->pages;

            mfp->pages = page->next;
            mem_page_delete(&page);
            mfp->nb_pages--;
        }

        p_delete(poolp);
    }
}

