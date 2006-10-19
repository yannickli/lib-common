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

    byte *area;
    int size;
    int used_size;
    int used_blocks;
} mem_page;

typedef struct mem_fifo_pool {
    mem_pool funcs;
    mem_page *pages;
    mem_page *freelist;
    int page_size;
    int nb_pages;
} mem_fifo_pool;


static mem_page *mem_page_new(mem_fifo_pool *mfp)
{
    mem_page *page = p_new(mem_page, 1);

    page->size = mfp->page_size;
    page->area = mmap(NULL, page->size, PROT_READ | PROT_WRITE,
                      MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);

    if (page->area == MAP_FAILED) {
        e_panic(E_UNIXERR("mmap"));
    }

    return page;
}

static void mem_page_reset(mem_page *page)
{
    memset(page->area, 0, page->size);
    page->used_size   = 0;
    page->used_blocks = 0;
    page->next        = NULL;
}

static void mem_page_delete(mem_page **pagep)
{
    if (*pagep) {
        if ((*pagep)->area) {
            munmap((*pagep)->area, (*pagep)->size);
        }
        p_delete(pagep);
    }
}

static inline bool mem_page_contains(mem_page *page, void *mem)
{
    return (byte *)mem >= page->area && (byte *)mem < page->area + page->size;
}

static inline int mem_page_size_left(mem_page *page)
{
    return (page->size - page->used_size);
}

static void *mfp_alloc(mem_pool *mp, ssize_t size)
{
    mem_fifo_pool *mfp = (mem_fifo_pool *)mp;
    void *res;

    /* Must round size up to keep proper alignment */
    size = ROUND_MULTIPLE((size_t)size, 8);

    if (size > mfp->page_size) {
        /* Should just map a larger page, yet we need a maximum value */
        e_panic(E_PREFIX("tried to alloc %zd bytes, cannot have more than %d"),
                size, mfp->page_size);
    }

    if (!mfp->pages || mem_page_size_left(mfp->pages) < size) {
        if (mfp->freelist) {
            /* push the first free page on top of the pages list */
            mem_page *page = mfp->freelist;
            mfp->freelist = page->next;
            page->next    = mfp->pages;
            mfp->pages    = page;
        } else {
            /* mmap a new page */
            mem_page *page = mem_page_new(mfp);
            mfp->nb_pages++;
            page->next = mfp->pages;
            mfp->pages = page;
        }
    }

    res = mfp->pages->area + mfp->pages->used_size;
    mfp->pages->used_size += size;
    mfp->pages->used_blocks++;
    return res;
}

static void mfp_free(struct mem_pool *mp, void *mem)
{
    mem_fifo_pool *mfp = (mem_fifo_pool *)mp;
    mem_page **pagep;

    if (!mem)
        return;

    for (pagep = &mfp->pages; *pagep; pagep = &(*pagep)->next) {
        mem_page *page = *pagep;

        if (!mem_page_contains(page, mem)) {
            continue;
        }

        if (--page->used_blocks == 0) {
            *pagep = page->next;
            /* OG: should release last page, so empty pool is indeed
             * empty */
            if (mfp->freelist) {
                mem_page_delete(&page);
                mfp->nb_pages--;
            } else {
                mem_page_reset(page);
                page->next    = mfp->freelist;
                mfp->freelist = page;
            }
        }
        return;
    }

    e_error("Dealloc of %p from fifo-pool %p is incorrect\n", mem, mp);
}

static mem_pool mem_fifo_pool_funcs = {
    &mfp_alloc,
    &mfp_alloc, /* we use maps, always set to 0 */
    &mfp_free
};

mem_pool *mem_fifo_pool_new(int page_size_hint)
{
    mem_fifo_pool *mfp;

    mfp             = p_new(mem_fifo_pool, 1);
    mfp->funcs      = mem_fifo_pool_funcs;
    mfp->page_size  = ROUND_MULTIPLE(page_size_hint, 4096);
    /* OG: this is not needed, an empty pool should be empty */
    mfp->pages      = mem_page_new(mfp);
    mfp->nb_pages   = 1;

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

