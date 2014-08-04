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

#include "container-dlist.h"
#include "thr.h"

#ifdef MEM_BENCH
#include "core-mem-bench.h"

static DLIST(mem_stack_pool_list);
static spinlock_t mem_stack_dlist_lock;
#define WRITE_PERIOD  256
#endif

#ifndef __BIGGEST_ALIGNMENT__
#define __BIGGEST_ALIGNMENT__  16
#endif

#define DEFAULT_ALIGNMENT  __BIGGEST_ALIGNMENT__
#ifndef NDEBUG
# define MIN_ALIGNMENT  sizeof(void *)
#else
# define MIN_ALIGNMENT  1
#endif

static ALWAYS_INLINE size_t sp_alloc_mean(mem_stack_pool_t *sp)
{
    return sp->alloc_sz / sp->alloc_nb;
}

static ALWAYS_INLINE mem_stack_blk_t *blk_entry(dlist_t *l)
{
    return container_of(l, mem_stack_blk_t, blk_list);
}

__cold
static mem_stack_blk_t *blk_create(mem_stack_pool_t *sp, size_t size_hint)
{
    size_t blksize = size_hint + sizeof(mem_stack_blk_t);
    mem_stack_blk_t *blk;

    if (blksize < sp->minsize)
        blksize = sp->minsize;
    if (blksize < 64 * sp_alloc_mean(sp))
        blksize = 64 * sp_alloc_mean(sp);
    blksize = ROUND_UP(blksize, PAGE_SIZE);
    if (unlikely(blksize > MEM_ALLOC_MAX))
        e_panic("you cannot allocate that amount of memory");
    blk = imalloc(blksize, 0, MEM_RAW | MEM_LIBC);
    blk->start     = blk->area;
    blk->size      = blksize - sizeof(*blk);
    sp->stacksize += blk->size;
    dlist_add_tail(&sp->blk_list, &blk->blk_list);
    sp->nbpages++;

#ifdef MEM_BENCH
    sp->mem_bench->malloc_calls++;
    sp->mem_bench->current_allocated += blk->size;
    sp->mem_bench->total_allocated += blksize;
    mem_bench_update(sp->mem_bench);
    mem_bench_print_csv(sp->mem_bench);
#endif

    return blk;
}

__cold
static void blk_destroy(mem_stack_pool_t *sp, mem_stack_blk_t *blk)
{
#ifdef MEM_BENCH
    /* if called by mem_stack_pool_wipe,
     * the mem_bench might be deleted
     */
    if (sp->mem_bench) {
        sp->mem_bench->current_allocated -= blk->size;
        mem_bench_update(sp->mem_bench);
        mem_bench_print_csv(sp->mem_bench);
    }
#endif

    sp->stacksize -= blk->size;
    sp->nbpages--;
    dlist_remove(&blk->blk_list);
    mem_tool_allow_memory(blk, blk->size + sizeof(*blk), false);
    ifree(blk, MEM_LIBC);
}

static ALWAYS_INLINE mem_stack_blk_t *
frame_get_next_blk(mem_stack_pool_t *sp, mem_stack_blk_t *cur, size_t alignment,
                   size_t size)
{
    mem_stack_blk_t *blk;

#ifdef MEM_BENCH
    sp->mem_bench->alloc.nb_slow_path++;
#endif

    dlist_for_each_entry_safe_continue(cur, blk, &sp->blk_list, blk_list) {
        size_t needed_size = size;
        uint8_t *aligned_area;

        aligned_area = (uint8_t *)mem_align_ptr((uintptr_t)blk->area, alignment);
        needed_size += aligned_area - blk->area;

        if (blk->size >= needed_size)
            return blk;
        blk_destroy(sp, blk);
    }

    if ((offsetof(mem_stack_blk_t, area) & BITMASK_LT(size_t, alignment)) != 0) {
        /* require enough free space so we're sure we can allocate the size
         * bytes properly aligned.
         */
        size += 1 << alignment;
    }
    return blk_create(sp, size);
}

static ALWAYS_INLINE uint8_t *frame_end(mem_stack_frame_t *frame)
{
    mem_stack_blk_t *blk = frame->blk;
    return blk->area + blk->size;
}

static void *sp_reserve(mem_stack_pool_t *sp, size_t asked, size_t alignment,
                        mem_stack_blk_t **blkp, uint8_t **end)
{
    uint8_t           *res;
    mem_stack_frame_t *frame = sp->stack;

    res = (uint8_t *)mem_align_ptr((uintptr_t)frame->pos, alignment);

    if (unlikely(res + asked > frame_end(frame))) {
        mem_stack_blk_t *blk = frame_get_next_blk(sp, frame->blk, alignment,
                                                  asked);

        *blkp = blk;
        res   = blk->area;
        res   = (uint8_t *)mem_align_ptr((uintptr_t)res, alignment);
        mem_tool_disallow_memory(blk->area, res - blk->area);
    } else {
        *blkp = frame->blk;
        mem_tool_disallow_memory(frame->pos, res - frame->pos);
    }
    mem_tool_allow_memory(res, asked, false);

    /* compute a progressively forgotten mean of the allocation size.
     *
     * Every 64k allocations, we divide the sum of allocations by four so that
     * the distant past has less and less consequences on the mean in the hope
     * that it will converge.
     *
     * As the t_stack should not be used for large allocations, at least it's
     * clearly not its typical usage, ignore the allocation larger than 128M
     * from this computation. Those hence will always yield a malloc (actually
     * a mmap) which is fine.
     */
    if (asked < 128 << 20) {
        if (unlikely(sp->alloc_sz + asked < sp->alloc_sz)
        ||  unlikely(sp->alloc_nb >= UINT16_MAX))
        {
            sp->alloc_sz /= 4;
            sp->alloc_nb /= 4;
        }
        sp->alloc_sz += asked;
        sp->alloc_nb += 1;
    }

    *end = res + asked;
    return res;
}

__flatten
static void *sp_alloc(mem_pool_t *_sp, size_t size, size_t alignment,
                      mem_flags_t flags)
{
    mem_stack_pool_t *sp = container_of(_sp, mem_stack_pool_t, funcs);
    mem_stack_frame_t *frame = sp->stack;
    uint8_t *res;
#ifdef MEM_BENCH
    proctimer_t ptimer;
    proctimer_start(&ptimer);
#endif

    THROW_NULL_IF(size == 0);

#ifndef NDEBUG
    if (unlikely(frame == &sp->base))
        e_panic("allocation performed without a t_scope");
    if (frame->prev & 1)
        e_panic("allocation performed on a sealed stack");
    size += 1 << alignment;
#endif
    res = sp_reserve(sp, size, alignment, &frame->blk, &frame->pos);
    if (!(flags & MEM_RAW)) {
#ifdef MEM_BENCH
        /* since sp_free is no-op, we use the fields to measure memset */
        proctimer_t free_timer;
        proctimer_start(&free_timer);
#endif
        memset(res, 0, size);

#ifdef MEM_BENCH
        proctimer_stop(&free_timer);
        proctimerstat_addsample(&sp->mem_bench->free.timer_stat, &free_timer);

        sp->mem_bench->free.nb_calls++;
        mem_bench_update(sp->mem_bench);
#endif
    }

#ifndef NDEBUG
    res += 1 << alignment;
    ((void **)res)[-1] = sp->stack;
    mem_tool_disallow_memory(res - (1 << alignment), 1 << alignment);
#endif

#ifdef MEM_BENCH
    proctimer_stop(&ptimer);
    proctimerstat_addsample(&sp->mem_bench->alloc.timer_stat, &ptimer);

    sp->mem_bench->alloc.nb_calls++;
    sp->mem_bench->current_used += size;
    sp->mem_bench->total_requested += size;
    mem_bench_update(sp->mem_bench);
#endif
    return frame->last = res;
}

static void sp_free(mem_pool_t *_sp, void *mem)
{
}

static void *sp_realloc(mem_pool_t *_sp, void *mem, size_t oldsize,
                        size_t asked, size_t alignment, mem_flags_t flags)
{
    mem_stack_pool_t *sp = container_of(_sp, mem_stack_pool_t, funcs);
    mem_stack_frame_t *frame = sp->stack;
    uint8_t *res;

#ifdef MEM_BENCH
    proctimer_t ptimer;
    proctimer_start(&ptimer);
#endif

#ifndef NDEBUG
    if (frame->prev & 1)
        e_panic("allocation performed on a sealed stack");
    if (mem != NULL) {
        mem_tool_allow_memory((byte *)mem - sizeof(void *), sizeof(void *), true);
        if (unlikely(((void **)mem)[-1] != sp->stack))
            e_panic("%p wasn't allocated in that frame, realloc is forbidden", mem);
        mem_tool_disallow_memory((byte *)mem - sizeof(void *), sizeof(void *));
    }
    if (unlikely(oldsize == MEM_UNKNOWN))
        e_panic("stack pools do not support reallocs with unknown old size");
#endif

    if (oldsize >= asked) {
        if (mem == frame->last) {
            frame->pos = (uint8_t *)mem + asked;
        }
        mem_tool_disallow_memory((byte *)mem + asked, oldsize - asked);

#ifdef MEM_BENCH
        proctimer_stop(&ptimer);
        proctimerstat_addsample(&sp->mem_bench->realloc.timer_stat, &ptimer);

        sp->mem_bench->realloc.nb_calls++;
        sp->mem_bench->current_used -= oldsize - asked;
        mem_bench_update(sp->mem_bench);
#endif

        return asked ? mem : NULL;
    }

    if (mem != NULL && mem == frame->last
    && frame->last + asked <= frame_end(frame))
    {
        frame->pos = frame->last + asked;
        sp->alloc_sz  += asked - oldsize;
        mem_tool_allow_memory((byte *)mem + oldsize, asked - oldsize, false);
        res = mem;

#ifdef MEM_BENCH
        proctimer_stop(&ptimer);
        proctimerstat_addsample(&sp->mem_bench->realloc.timer_stat, &ptimer);

        sp->mem_bench->realloc.nb_calls++;
        sp->mem_bench->total_requested += asked - oldsize;
        sp->mem_bench->current_used += asked - oldsize;
        mem_bench_update(sp->mem_bench);
#endif
    } else {
        res = sp_alloc(_sp, asked, alignment, flags | MEM_RAW);
        if (mem) {
            memcpy(res, mem, oldsize);
            mem_tool_disallow_memory(mem, oldsize);
        }
    }

    if (!(flags & MEM_RAW))
        p_clear(res + oldsize, asked - oldsize);
    return res;
}

static mem_pool_t const pool_funcs = {
    .malloc   = &sp_alloc,
    .realloc  = &sp_realloc,
    .free     = &sp_free,
    .mem_pool = MEM_STACK | MEM_BY_FRAME,
    .min_alignment = sizeof(void *)
};

mem_stack_pool_t *mem_stack_pool_init(mem_stack_pool_t *sp, int initialsize)
{
    p_clear(sp, 1);
    dlist_init(&sp->blk_list);
    sp->start    = blk_entry(&sp->blk_list)->area;
    sp->size     = sizeof(mem_stack_pool_t) - sizeof(mem_stack_blk_t);

    sp->base.blk = blk_entry(&sp->blk_list);
    sp->base.pos = (void *)(sp + 1);
    sp->stack    = &sp->base;

    /* 640k should be enough for everybody =) */
    if (initialsize <= 0)
        initialsize = 640 << 10;
    sp->minsize  = ROUND_UP(MAX(1, initialsize), PAGE_SIZE);
    sp->funcs    = pool_funcs;
    sp->alloc_nb = 1; /* avoid the division by 0 */

#ifdef MEM_BENCH
    sp->mem_bench = mem_bench_new(LSTR_IMMED_V("stack"), WRITE_PERIOD);
    mem_bench_leak(sp->mem_bench);

    spin_lock(&mem_stack_dlist_lock);
    dlist_add_tail(&mem_stack_pool_list, &sp->pool_list);
    spin_unlock(&mem_stack_dlist_lock);
#endif

    return sp;
}

void mem_stack_pool_reset(mem_stack_pool_t *sp)
{
    dlist_for_each_safe(e, &sp->blk_list) {
        blk_destroy(sp, blk_entry(e));
    }
}

void mem_stack_pool_wipe(mem_stack_pool_t *sp)
{
#ifdef MEM_BENCH
    mem_bench_delete(&sp->mem_bench);

    spin_lock(&mem_stack_dlist_lock);
    dlist_remove(&sp->pool_list);
    spin_unlock(&mem_stack_dlist_lock);
#endif

    mem_stack_pool_reset(sp);
}

const void *mem_stack_push(mem_stack_pool_t *sp)
{
    mem_stack_blk_t *blk;
    uint8_t *end;
    uint8_t *res = sp_reserve(sp, sizeof(mem_stack_frame_t),
                              bsrsz(__BIGGEST_ALIGNMENT__), &blk, &end);
    mem_stack_frame_t *frame;

#ifdef MEM_BENCH
    /* if the assert fires,
     * it means the stack pool has been wiped by mem_stack_pool_wipe.
     * t_push'ing again is then an incorrect behaviour.
     */
    assert (sp->mem_bench);

    mem_bench_print_csv(sp->mem_bench);
#endif
    frame = (mem_stack_frame_t *)res;
    frame->blk  = blk;
    frame->pos  = end;
    frame->last = NULL;
    frame->prev = (uintptr_t)sp->stack;
    return sp->stack = frame;
}

#ifdef MEM_BENCH
void mem_stack_bench_pop(mem_stack_pool_t *sp, mem_stack_frame_t * frame)
{
    mem_stack_blk_t *last_block = frame->blk;
    int32_t cused = sp->mem_bench->current_used;

    mem_bench_print_csv(sp->mem_bench);
    if (sp->stack->blk == last_block) {
        cused -= (frame->pos - sp->stack->pos
                  - sizeof(mem_stack_frame_t));
    } else {
        cused -= frame->pos - last_block->area;
        last_block = container_of(last_block->blk_list.prev,
                                  mem_stack_blk_t, blk_list);
        while (sp->stack->blk != last_block) {
            cused -= last_block->size;
            /* Note: this is inaccurate, because we don't know the size of the
               unused space at the end of the block
            */
            last_block = container_of(last_block->blk_list.prev,
                                      mem_stack_blk_t, blk_list);
        }
        cused -= (last_block->area + last_block->size
                  - sp->stack->pos + sizeof(mem_stack_frame_t));
    }
    if (cused <= 0 || mem_stack_is_at_top(sp)) {
        cused = 0;
    }
    sp->mem_bench->current_used = cused;
    mem_bench_update(sp->mem_bench);
}
#endif

void mem_stack_pool_print_stats(mem_pool_t *mp) {
#ifdef MEM_BENCH
    mem_stack_pool_t *sp = container_of(mp, mem_stack_pool_t, funcs);
    mem_bench_print_human(sp->mem_bench, MEM_BENCH_PRINT_CURRENT);
#endif
}

void mem_stack_pools_print_stats(void) {
#ifdef MEM_BENCH
    spin_lock(&mem_stack_dlist_lock);
    dlist_for_each_safe(n, &mem_stack_pool_list) {
        mem_stack_pool_t *mp = container_of(n, mem_stack_pool_t, pool_list);
        mem_bench_print_human(mp->mem_bench, MEM_BENCH_PRINT_CURRENT);
    }
    spin_unlock(&mem_stack_dlist_lock);
#endif
}

#ifndef NDEBUG
void mem_stack_protect(mem_stack_pool_t *sp, const mem_stack_frame_t *up_to)
{
    if (up_to->blk == sp->stack->blk) {
        mem_tool_disallow_memory(sp->stack->pos, up_to->pos - sp->stack->pos);
    } else {
        const byte *end = up_to->pos;
        mem_stack_blk_t *end_blk = up_to->blk;
        mem_stack_blk_t *blk = sp->stack->blk;
        size_t remainsz = frame_end(sp->stack) - sp->stack->pos;

        mem_tool_disallow_memory(sp->stack->pos, remainsz);
        dlist_for_each_entry_continue(blk, blk, &sp->blk_list, blk_list) {
            if (blk == end_blk) {
                mem_tool_disallow_memory(blk->start, end - (byte *)blk->start);
                break;
            } else {
                mem_tool_disallow_memory(blk->start, blk->size);
            }
        }
    }
}
#endif

__attribute__((constructor))
static void t_pool_init(void)
{
    mem_stack_pool_init(&t_pool_g, 64 << 10);
}
static void t_pool_wipe(void)
{
    mem_stack_pool_wipe(&t_pool_g);
}
thr_hooks(t_pool_init, t_pool_wipe);

__thread mem_stack_pool_t t_pool_g;
