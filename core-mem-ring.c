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

#include "arith.h"
#include "container-dlist.h"
#include "thr.h"
#include "log.h"

/*
 * Ring memory allocator
 * ~~~~~~~~~~~~~~~~~~~~~
 *
 * This allocator has mostly the same properties as the stacked allocator but
 * works on a ring.
 *
 * Note: this code shares a big part with core-mem-stack.c which should be
 * merged.
 *
 */

/** Size tuning parameters.
 * These are multiplicative factors over rp_alloc_mean.
 */
#define RESET_MIN   56 /*< minimum size in mem_stack_pool_reset */
#define RESET_MAX  256 /*< maximum size in mem_stack_pool_reset */

typedef struct ring_pool_t ring_pool_t;

typedef struct ring_blk_t {
    void        *start;
    size_t       size;
    dlist_t      blist;
    byte         area[];
} ring_blk_t;

typedef struct frame_t {
    dlist_t      flist;
    ring_blk_t  *blk;
    uintptr_t    rp;
} frame_t;
#define FRAME_IS_FREE   (1ULL << 0)

struct ring_pool_t {
    dlist_t      fhead;
    frame_t     *ring;

    void        *last;
    void        *pos;
    ring_blk_t  *cblk;

    size_t       minsize;
    size_t       ringsize;

    size_t       alloc_sz;
    uint32_t     alloc_nb;
    uint32_t     nbpages;
    spinlock_t   lock;

    uint32_t     frames_cnt;
    uint32_t     nb_frames_release;

    flag_t       alive : 1;

    mem_pool_t   funcs;
};

struct mem_ring_checkpoint {
    frame_t     *frame;
    ring_blk_t  *cblk;
    void        *last;
    void        *pos;
};

static size_t rp_alloc_mean(ring_pool_t *rp)
{
    return rp->alloc_sz / rp->alloc_nb;
}

static ring_blk_t *blk_entry(dlist_t *l)
{
    return dlist_entry(l, ring_blk_t, blist);
}

static inline uint8_t align_boundary(size_t size)
{
    return MIN(16, 1 << bsrsz(size | 1));
}

static inline bool is_aligned_to(const void *addr, size_t boundary)
{
    return ((uintptr_t)addr & (boundary - 1)) == 0;
}

static byte *align_for(const void *mem, size_t size)
{
    size_t bmask = align_boundary(size) - 1;
    return (byte *)(((uintptr_t)mem + bmask) & ~bmask);
}

static ring_blk_t *blk_create(ring_pool_t *rp, size_t size_hint)
{
    size_t blksize = size_hint + sizeof(ring_blk_t);
    size_t alloc_target = MIN(100U << 20, 64 * rp_alloc_mean(rp));
    ring_blk_t *blk;

    if (blksize < rp->minsize) {
        blksize = rp->minsize;
    }
    if (blksize < alloc_target) {
        blksize = alloc_target;
    }
    blksize = ROUND_UP(blksize, PAGE_SIZE);
    icheck_alloc(blksize);
    blk = imalloc(blksize, 0, MEM_RAW | MEM_LIBC);
    blk->start    = blk->area;
    blk->size     = blksize - sizeof(*blk);
    rp->ringsize += blk->size;
    if (likely(rp->cblk)) {
        dlist_add_after(&rp->cblk->blist, &blk->blist);
    } else {
        dlist_init(&blk->blist);
    }
    rp->nbpages++;
    return blk;
}

static void blk_destroy(ring_pool_t *rp, ring_blk_t *blk)
{
    rp->ringsize -= blk->size;
    rp->nbpages--;
    dlist_remove(&blk->blist);
    mem_tool_allow_memory(blk, blk->size + sizeof(*blk), false);
    ifree(blk, MEM_LIBC);
}

static bool blk_contains(const ring_blk_t *blk, const void *ptr)
{
    return (const byte *)ptr >= (const byte *)blk->start
        && (const byte *)ptr <= (const byte *)blk->start + blk->size;
}

static byte *blk_end(ring_blk_t *blk)
{
    return blk->area + blk->size;
}

static ring_blk_t *
frame_get_next_blk(ring_pool_t *rp, size_t size)
{
    ring_blk_t *cur = rp->cblk;
    frame_t *start = dlist_first_entry(&rp->fhead, frame_t, flist);

    while (!dlist_is_empty(&cur->blist)) {
        ring_blk_t *blk = dlist_next_entry(cur, blist);

        /* XXX: start is the offset to a frame, IOW frame == blk->end
         *      cannot happen because a frame cannot have a zero offset
         *      from the next block, even if blocks are allocated
         *      without overhead, such as by mmapping.
         */
        if (blk_contains(blk, start)) {
            break;
        }

        if (blk->size >= size && blk->size > 8 * rp_alloc_mean(rp)) {
            return blk;
        }
        blk_destroy(rp, blk);
    }
    return blk_create(rp, size);
}

static void *rp_reserve(ring_pool_t *rp, size_t size, ring_blk_t **blkp)
{
    byte *res = align_for(rp->pos, size);

    /* Note for programmers: if you abort() here, it's because you're
     * allocating in a pool where you haven't performed a r_newframe() first
     */
    assert (rp->pos);

    if (unlikely(res + size > blk_end(rp->cblk))) {
        ring_blk_t *blk = frame_get_next_blk(rp, size);

        *blkp = blk;
        res = blk->area;
    } else {
        *blkp = rp->cblk;
    }
    mem_tool_allow_memory(res, size, false);
    if (unlikely(rp->alloc_sz + size < rp->alloc_sz)
    ||  unlikely(rp->alloc_nb == UINT32_MAX))
    {
        rp->alloc_sz /= 2;
        rp->alloc_nb /= 2;
    }
    rp->alloc_sz += size;
    rp->alloc_nb += 1;
    return res;
}

__flatten
static void *rp_alloc(mem_pool_t *_rp, size_t size, size_t alignment,
                      mem_flags_t flags)
{
    ring_pool_t *rp = container_of(_rp, ring_pool_t, funcs);
    byte *res;

    if (unlikely(alignment > 16)) {
        e_panic("mem_pool_ring does not support alignments greater than 16");
    }

    if (unlikely((size == 0))) {
        return MEM_EMPTY_ALLOC;
    }

    spin_lock(&rp->lock);
    res = rp_reserve(rp, size, &rp->cblk);
    rp->pos = res + size;
    rp->last = res;
    spin_unlock(&rp->lock);

    if (!(flags & MEM_RAW)) {
        memset(res, 0, size);
    }

    return res;
}

static void rp_free(mem_pool_t *_rp, void *mem)
{
}

static void *rp_realloc(mem_pool_t *_rp, void *mem, size_t oldsize,
                        size_t size, size_t alignment, mem_flags_t flags)
{
    ring_pool_t *rp = container_of(_rp, ring_pool_t, funcs);
    byte *res;

    if (unlikely(alignment > 16)) {
        e_panic("mem_pool_ring does not support alignments greater than 16");
    }

    if (unlikely(oldsize == MEM_UNKNOWN)) {
        e_panic("ring pools do not support reallocs with unknown old size");
    }

    if (unlikely(mem == MEM_EMPTY_ALLOC)) {
        mem = NULL;
    }

    if (oldsize >= size) {
        if (mem == rp->last) {
            rp->pos = (byte *)mem + size;
        }
        mem_tool_disallow_memory((byte *)mem + size, oldsize - size);
        return size ? mem : MEM_EMPTY_ALLOC;
    }

    if (mem != NULL && mem == rp->last
    &&  is_aligned_to(mem, align_boundary(size))
    &&  (byte *)rp->last + size <= blk_end(rp->cblk))
    {
        rp->pos = (byte *)rp->last + size;
        rp->alloc_sz  += size - oldsize;
        mem_tool_allow_memory(mem, size, true);
        res = mem;
    } else {
        res = rp_alloc(_rp, size, alignment, flags | MEM_RAW);
        if (mem != NULL) {
            memcpy(res, mem, oldsize);
            mem_tool_allow_memory(mem, oldsize, false);
        }
    }
    if (!(flags & MEM_RAW)) {
        memset(res + oldsize, 0, size - oldsize);
    }
    return res;
}


static mem_pool_t const pool_funcs = {
    .malloc  = &rp_alloc,
    .realloc = &rp_realloc,
    .free    = &rp_free,
    .mem_pool = MEM_OTHER | MEM_BY_FRAME,
};

#ifndef NDEBUG
static void
mem_ring_protect(const ring_pool_t *rp, const ring_blk_t *blk,
                 const void *_start, const void *_end)
{
    const byte *end   = _end;
    const byte *start = _start;

    while (!blk_contains(blk, end)) {
        mem_tool_disallow_memory(start, blk->area + blk->size - start);
        blk   = dlist_next_entry(blk, blist);
        start = blk->start;
    }
    mem_tool_disallow_memory(start, end - start);
}
#else
#define mem_ring_protect(...)  ((void)0)
#endif

static ALWAYS_INLINE
void ring_reset_frame(ring_pool_t *rp, frame_t *frame, bool protect)
{
    assert (rp->ring == frame);

    if (protect && rp->pos) {
        mem_ring_protect(rp, frame->blk, &frame[1], rp->pos);
    }

    rp->last = NULL;
    rp->pos  = NULL;
    rp->cblk = frame->blk;

    /* TODO: evaluate the possibility to reset to the _previous_ frame if:
     * - it exists
     * - it's free
     */
}

static ALWAYS_INLINE
void frame_unregister(frame_t *frame)
{
    dlist_remove(&frame->flist);
    mem_tool_disallow_memory(frame, sizeof(frame_t));
}

static ALWAYS_INLINE
void ring_reset_to_prevframe(ring_pool_t *rp, frame_t *fprev, frame_t *frame)
{
    assert (frame == rp->ring);

    frame_unregister(frame);
    rp->ring   = fprev;
    rp->cblk   = fprev->blk;
    fprev->rp &= ~FRAME_IS_FREE;
}

static ALWAYS_INLINE
void ring_setup_frame(ring_pool_t *rp, ring_blk_t *blk, frame_t *frame)
{
    spin_lock(&rp->lock);
    frame->blk = blk;
    frame->rp  = (uintptr_t)rp;
    dlist_add_tail(&rp->fhead, &frame->flist);

    rp->ring = frame;
    ring_reset_frame(rp, frame, false);
    spin_unlock(&rp->lock);
}

/*------ Public API -{{{-*/

mem_pool_t *__mem_ring_pool_new(int initialsize, const char *file, int line)
{
    ring_pool_t *rp = p_new(ring_pool_t, 1);
    ring_blk_t *blk;

    dlist_init(&rp->fhead);

    /* 640k should be enough for everybody =) */
    if (initialsize <= 0) {
        initialsize = 640 << 10;
    }
    rp->minsize    = ROUND_UP(initialsize, PAGE_SIZE);
    rp->funcs      = pool_funcs;
    rp->alloc_nb   = 1; /* avoid the division by 0 */
    rp->frames_cnt  = 0;
    rp->alive = true;

    /* Makes the first frame */
    blk = blk_create(rp, sizeof(frame_t));
    mem_tool_allow_memory(blk->area, sizeof(frame_t), false);
    ring_setup_frame(rp, blk, acast(frame_t, &blk->area));

    return &rp->funcs;
}

static void __mem_ring_reset(ring_pool_t *rp);

void mem_ring_pool_delete(mem_pool_t **rpp)
{
    if (*rpp) {
        ring_pool_t *rp = container_of(*rpp, ring_pool_t, funcs);

        spin_lock(&rp->lock);

        if (rp->frames_cnt) {
            assert (rp->alive);
            rp->alive = false;

            /* XXX: the log module may have been deleted already,
             * but we cannot depend on it. See #54184 */
            if (MODULE_IS_LOADED(log)) {
                e_trace(0, "keep ring-pool alive: %d frames in use",
                        rp->frames_cnt);
            } else {
                printf("keep ring-pool alive: %d frames in use",
                       rp->frames_cnt);
            }
            spin_unlock(&rp->lock);

            *rpp = NULL;
            return;
        }

        dlist_for_each(e, &rp->cblk->blist) {
            blk_destroy(rp, blk_entry(e));
        }
        blk_destroy(rp, rp->cblk);
        p_delete(&rp);
        *rpp = NULL;
    }
}

/**
 * Unlike the public documentation claims, this function allocates nothing.
 * The ring implementation ensures that we always have an active frame. Here
 * we just check that the programmer is really where he thinks he is and we
 * “enable” the active frame.
 */
const void *mem_ring_newframe(mem_pool_t *_rp)
{
    ring_pool_t *rp = container_of(_rp, ring_pool_t, funcs);

    e_assert_null(panic, rp->pos, "previous memory frame not released!");
    spin_lock(&rp->lock);
    rp->pos = &rp->ring[1];
    rp->frames_cnt++;
    spin_unlock(&rp->lock);

    return rp->ring;
}

const void *mem_ring_getframe(mem_pool_t *_rp)
{
    return container_of(_rp, ring_pool_t, funcs)->ring;
}

const void *mem_ring_seal(mem_pool_t *_rp)
{
    ring_pool_t *rp = container_of(_rp, ring_pool_t, funcs);
    frame_t *last = rp->ring;
    frame_t *frame;
    ring_blk_t *blk;

    /* Makes a new frame */
    frame = rp_reserve(rp, sizeof(frame_t), &blk);
    ring_setup_frame(rp, blk, frame);

    return last;
}

static void __mem_ring_reset(ring_pool_t *rp)
{
    size_t saved_size;
    size_t max_size;
    ring_blk_t *saved_blk = NULL;
    frame_t *start = dlist_first_entry(&rp->fhead, frame_t, flist);

    if (!mem_pool_is_enabled()) {
        return;
    }
    if (rp->frames_cnt) {
        return;
    }

    saved_blk  = NULL;
    saved_size = RESET_MIN * rp_alloc_mean(rp);
    max_size   = RESET_MAX * rp_alloc_mean(rp);

    /* Keep the current block plus the one with more adapted size
     * regarding the mean allocation size.
     */
    dlist_for_each(e, &rp->cblk->blist) {
        ring_blk_t *blk = blk_entry(e);

        /* XXX: do not remove the block which contains the first frame. */
        if (blk_contains(blk, start)) {
            continue;
        }

        if (blk->size > saved_size && blk->size < max_size) {
            if (saved_blk) {
                blk_destroy(rp, saved_blk);
            }
            saved_blk  = blk;
            saved_size = blk->size;
        } else {
            blk_destroy(rp, blk);
        }
    }

    rp->nb_frames_release = 0;
}

void mem_ring_reset(mem_pool_t *_rp)
{
    ring_pool_t *rp = container_of(_rp, ring_pool_t, funcs);

    spin_lock(&rp->lock);
    __mem_ring_reset(rp);
    spin_unlock(&rp->lock);
}

void mem_ring_release(const void *cookie)
{
    frame_t *frame  = (frame_t *)cookie;
    ring_pool_t *rp;
    bool to_delete = false;

    if (!cookie) {
        return;
    }

    rp = (ring_pool_t *)frame->rp;
    assert (!(frame->rp & FRAME_IS_FREE));

    spin_lock(&rp->lock);
    if (rp->ring == frame) {
        if (dlist_is_empty_or_singular(&rp->fhead)) {
            ring_reset_frame(rp, frame, true);
        } else {
            frame_t *fprev = dlist_prev_entry(frame, flist);

            if (fprev->rp & FRAME_IS_FREE) {
                /* TODO We could rewind on more than one frame. */
                ring_reset_to_prevframe(rp, fprev, frame);
            } else {
                ring_reset_frame(rp, frame, true);
            }
        }
    } else {
        frame_t *fnext = dlist_next_entry(frame, flist);

        mem_ring_protect(rp, frame->blk, &frame[1], fnext);

        if (fnext->rp & FRAME_IS_FREE) {
            frame_unregister(fnext);
            fnext = dlist_next_entry(frame, flist);
        }

        if (dlist_first_entry(&rp->fhead, frame_t, flist) == frame) {
            frame_unregister(frame);
        } else {
            frame_t *fprev = dlist_prev_entry(frame, flist);
            ring_blk_t *blk1, *blk2;

            if (fprev->rp & FRAME_IS_FREE) {
                frame_unregister(frame);
                frame = fprev;
            }
            frame->rp |= FRAME_IS_FREE;

            if (fnext == rp->ring && rp->pos == NULL) {
                ring_reset_to_prevframe(rp, frame, fnext);
            } else {
                blk1 = frame->blk;
                blk2 = fnext->blk;
                if (blk1 != blk2 && dlist_next_entry(blk1, blist) != blk2) {
                    dlist_t *first = blk1->blist.next;
                    dlist_t *last  = blk2->blist.prev;
                    dlist_t *at;

                    /* remove elements strictly between blk1 and blk2 */
                    __dlist_remove(&blk1->blist, &blk2->blist);
                    /* and splice first->...->last between at and at->next */
                    at = &rp->cblk->blist;
                    __dlist_splice2(at, at->next, first, last);
                }
            }
        }
    }

    rp->frames_cnt--;
    rp->nb_frames_release++;

    if (rp->nb_frames_release >= 256) {
        __mem_ring_reset(rp);
    }

    to_delete = rp->frames_cnt == 0 && !rp->alive;

    spin_unlock(&rp->lock);

    if (to_delete) {
        mem_pool_t *mp = &rp->funcs;

        mem_ring_pool_delete(&mp);
    }
}

const void *mem_ring_checkpoint(mem_pool_t *_rp)
{
    ring_pool_t *rp = container_of(_rp, ring_pool_t, funcs);
    struct mem_ring_checkpoint cp = {
        .frame = rp->ring,
        .cblk  = rp->cblk,
        .last  = rp->last,
        .pos   = rp->pos,
    };
    void *res;

    res = memcpy(rp_alloc(_rp, sizeof(cp), mem_bit_align(_rp, alignof(cp)),
                          MEM_RAW),
                 &cp, sizeof(cp));
    mem_ring_seal(_rp);

    return res;
}

void mem_ring_rewind(mem_pool_t *_rp, const void *ckpoint)
{
    ring_pool_t *rp = container_of(_rp, ring_pool_t, funcs);
    struct mem_ring_checkpoint *cp = (void *)ckpoint;
    frame_t *frame = cp->frame;

    assert (!(frame->rp & FRAME_IS_FREE));
    __dlist_remove(&frame->flist, &rp->fhead);
    rp->ring = frame;
    rp->last = cp->last;
    rp->cblk = cp->cblk;
    rp->pos  = cp->pos;
}

static __thread mem_pool_t *r_pool_g;

mem_pool_t *r_pool(void)
{
    if (unlikely(!r_pool_g)) {
        r_pool_g = mem_ring_pool_new(64 << 10);
    }
    return r_pool_g;
}

void r_pool_destroy(void)
{
    mem_ring_pool_delete(&r_pool_g);
}
thr_hooks(NULL, r_pool_destroy);

static size_t frame_getsize(frame_t *frame, const byte *pos)
{
    ring_pool_t *rp = (ring_pool_t *)(frame->rp & ~FRAME_IS_FREE);
    const byte *start, *endp;
    size_t size = 0;
    ring_blk_t *blk = frame->blk;

    start = (const byte *)frame;
    if (frame == rp->ring) {
        endp = pos;
    } else {
        endp = (const byte *)dlist_next_entry(frame, flist);
    }

    for (;;) {
        if (blk_contains(blk, endp)) {
            size += endp - start;
            break;
        }
        size += blk_end(blk) - start;
        if (dlist_is_last(&rp->cblk->blist, &blk->blist)) {
            break;
        }
        blk = dlist_next_entry(blk, blist);
        start = blk->area;
    }

    return size;
}

void mem_ring_dump(const mem_pool_t *_rp)
{
    ring_pool_t *rp = container_of(_rp, ring_pool_t, funcs);
    int num = 0;
    size_t bytes = 0;
    frame_t *frame;

    if (rp->cblk) {
        bytes += rp->cblk->size;
        dlist_for_each(e, &rp->cblk->blist) {
            ring_blk_t *blk = blk_entry(e);
            bytes += blk->size;
        }
    }

    printf("-- \n");
    printf("-- mem_ring_pool at %p: {\n", rp);

    printf("--   ring=%p\n", rp->ring);
    printf("--   last=%p\n", rp->last);
    printf("--   pos=%p\n", rp->pos);
    printf("--   cblk=%p\n", rp->cblk);

    printf("--   minsize=%zd\n", rp->minsize);
    printf("--   ringsize=%zd\n", rp->ringsize);
    printf("--   bytes=%zd\n", bytes);
    printf("--   nbpages=%d\n", rp->nbpages);
    printf("--   alloc_sz=%zd\n", rp->alloc_sz);
    printf("--   alloc_nb=%d\n", rp->alloc_nb);
    printf("--   mean=%zd\n", rp_alloc_mean(rp));
    printf("--   \n");

    frame = dlist_first_entry(&rp->fhead, frame_t, flist);
    if ((const byte *)frame > frame->blk->area) {
        printf("--   slack: size=%zd\n",
               (const byte *)frame - frame->blk->area);
    }
    dlist_for_each(e, &rp->fhead) {
        frame = container_of(e, frame_t, flist);

        printf("--   frame %d at %p: size=%zd%s\n",
               ++num, frame, frame_getsize(frame, rp->pos),
               (frame->rp & FRAME_IS_FREE) ? " FREE" : "");
    }
    printf("--   unallocated: size=%zd\n",
           frame_getsize(rp->ring, NULL) - frame_getsize(rp->ring, rp->pos));
    printf("-- }\n");
}

size_t mem_ring_memory_footprint(const mem_pool_t *_rp)
{
    ring_pool_t *rp = container_of(_rp, ring_pool_t, funcs);

    /* The ring_pool_t size should count as long as it is malloc'd. */

    return sizeof(*rp) + rp->ringsize;
}

/* }}} */
