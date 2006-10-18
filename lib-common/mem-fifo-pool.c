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

#include "mem.h"
#include "mem-fifo-pool.h"

#define MEM_PAGE_SIZE_LEFT(page) ((page)->size - (page)->used_size)

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
    int pages_size;
} mem_fifo_pool;


static mem_page *mem_page_new(mem_fifo_pool *mfp)
{
    mem_page *page = p_new(mem_page, 1);
    page->area = mmap(NULL, mfp->pages_size, PROT_READ | PROT_WRITE,
                      MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
    if (page->area == MAP_FAILED) {
        page->area = NULL;
        p_delete(&page);
    }

    return page;
}

static void mem_page_reset(mem_page *page)
{
    memset(page->area, 0, page->size);
    page->used_size   = 0;
    page->used_blocks = 0;
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


static void *mfp_alloc(mem_pool *mp, ssize_t size)
{
    mem_fifo_pool *mfp = (mem_fifo_pool *)mp;
    void *res;

    if (size > mfp->pages_size) {
        goto alloc_error;
    }

  alloc_again:
    if (mfp->pages && MEM_PAGE_SIZE_LEFT(mfp->pages) >= size) {
        res = mfp->pages->area + mfp->pages->used_size;
        mfp->pages->used_size += size;
        mfp->pages->used_blocks++;
        return res;
    }

    if (mfp->freelist) {
        mem_page *page;

        /* push the first free page on top of the pages list */
        page = mfp->freelist;
        mfp->freelist = page->next;
        page->next    = mfp->pages;
        mfp->pages    = page->next;
    } else {
        mem_page *page = mem_page_new(mfp);
        if (!page)
            goto alloc_error;

        page->next = mfp->pages;
        mfp->pages = page;
    }

    goto alloc_again;

  alloc_error:
    check_enough_mem(NULL);
    return NULL;
}

static void *mfp_realloc(mem_pool *mp __unused__, void *mem __unused__,
                         ssize_t size __unused__)
{
    /* FIXME: implement */
    abort();
}

static void mfp_free(struct mem_pool *mp, void *mem)
{
    mem_fifo_pool *mfp = (mem_fifo_pool *)mp;
    mem_page **pagep;

    for (pagep = &mfp->pages; *pagep; pagep = &(*pagep)->next) {
        mem_page *page = *pagep;

        if (mem_page_contains(page, mem)) {
            if (--page->used_blocks == 0) {
                *pagep = page->next;
                if (mfp->freelist) {
                    mem_page_delete(&page);
                } else {
                    mem_page_reset(page);
                    mfp->freelist = page;
                }
            }
            return;
        }
    }
}

static mem_pool mem_fifo_pool_funcs = {
    &mfp_alloc,
    &mfp_alloc, /* we use maps, always set to 0 */
    &mfp_realloc,
    &mfp_free
};

#define UPPER_MULTIPLE(n, k) (((n + k - 1) / k) * k)

mem_pool *mem_fifo_pool_new(int pages_size_hint)
{
    mem_fifo_pool *res;

    res             = p_new(mem_fifo_pool, 1);
    res->funcs      = mem_fifo_pool_funcs;
    res->pages_size = UPPER_MULTIPLE(pages_size_hint, 4096);
    return (mem_pool *)res;
}

void mem_fifo_pool_delete(mem_pool **poolp) {
    if (*poolp) {
        mem_fifo_pool *mfp = (mem_fifo_pool *)(*poolp);

        while (mfp->freelist) {
            mem_page *page = mfp->freelist;

            mfp->freelist = page->next;
            mem_page_delete(&page);
        }

        while (mfp->pages) {
            mem_page *page = mfp->pages;

            mfp->pages = page->next;
            mem_page_delete(&page);
        }

        p_delete(poolp);
    }
}

