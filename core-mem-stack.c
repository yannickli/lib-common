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

#include "container-dlist.h"
#include "thr.h"

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
    size_t alloc_target = MIN(100U << 20, 64 * sp_alloc_mean(sp));
    mem_stack_blk_t *blk;

    if (blksize < sp->minsize)
        blksize = sp->minsize;
    if (blksize < alloc_target)
        blksize = alloc_target;
    blksize = ROUND_UP(blksize, PAGE_SIZE);
    icheck_alloc(blksize);
    blk = imalloc(blksize, 0, MEM_RAW | MEM_LIBC);
    blk->start     = blk->area;
    blk->size      = blksize - sizeof(*blk);
    sp->stacksize += blk->size;
    dlist_add_tail(&sp->blk_list, &blk->blk_list);
    sp->nbpages++;
    return blk;
}

__cold
static void blk_destroy(mem_stack_pool_t *sp, mem_stack_blk_t *blk)
{
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
    size_t             size  = mem_align_ptr(asked, alignment);

    res = (uint8_t *)mem_align_ptr((uintptr_t)frame->pos, alignment);

    if (unlikely(res + size > frame_end(frame))) {
        mem_stack_blk_t *blk = frame_get_next_blk(sp, frame->blk, alignment,
                                                  size);

        *blkp = blk;
        res   = blk->area;
        res   = (uint8_t *)mem_align_ptr((uintptr_t)res, alignment);
        mem_tool_disallow_memory(blk->area, res - blk->area);
    } else {
        *blkp = frame->blk;
        mem_tool_disallow_memory(frame->pos, res - frame->pos);
    }
    mem_tool_allow_memory(res, asked, false);
    mem_tool_disallow_memory(res + asked, size - asked);

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
    if (size < 128 << 20) {
        if (unlikely(sp->alloc_sz + size < sp->alloc_sz)
        ||  unlikely(sp->alloc_nb >= UINT16_MAX))
        {
            sp->alloc_sz /= 4;
            sp->alloc_nb /= 4;
        }
        sp->alloc_sz += size;
        sp->alloc_nb += 1;
    }

    *end = res + size;
    return res;
}

__flatten
static void *sp_alloc(mem_pool_t *_sp, size_t size, size_t alignment,
                      mem_flags_t flags)
{
    mem_stack_pool_t *sp = container_of(_sp, mem_stack_pool_t, funcs);
    mem_stack_frame_t *frame = sp->stack;
    uint8_t *res;

    THROW_NULL_IF(size == 0);

#ifndef NDEBUG
    if (unlikely(frame == &sp->base))
        e_panic("allocation performed without a t_scope");
    if (frame->prev & 1)
        e_panic("allocation performed on a sealed stack");
    size += 1 << alignment;
#endif
    res = sp_reserve(sp, size, alignment, &frame->blk, &frame->pos);
    if (!(flags & MEM_RAW))
        memset(res, 0, size);
#ifndef NDEBUG
    res += 1 << alignment;
    ((void **)res)[-1] = sp->stack;
    mem_tool_disallow_memory(res - (1 << alignment), 1 << alignment);
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
    size_t size  = mem_align_ptr(asked, alignment);
    uint8_t *res;

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

    if (oldsize >= size) {
        if (mem == frame->last) {
            sp->stack->pos = (uint8_t *)mem + size;
        }
        mem_tool_disallow_memory((byte *)mem + asked, oldsize - asked);
        return size ? mem : NULL;
    }

    if (mem != NULL && mem == frame->last
    && frame->last + size <= frame_end(sp->stack))
    {
        sp->stack->pos = frame->last + size;
        sp->alloc_sz  += size - oldsize;
        mem_tool_allow_memory((byte *)mem + oldsize, asked - oldsize, false);
        res = mem;
    } else {
        res = sp_alloc(_sp, size, alignment, flags | MEM_RAW);
        if (mem) {
            memcpy(res, mem, oldsize);
            mem_tool_disallow_memory(mem, oldsize);
        }
    }
    mem_tool_disallow_memory(res + asked, size - asked);

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

    return sp;
}

void mem_stack_pool_wipe(mem_stack_pool_t *sp)
{
    dlist_for_each_safe(e, &sp->blk_list) {
        blk_destroy(sp, blk_entry(e));
    }
}

const void *mem_stack_push(mem_stack_pool_t *sp)
{
    mem_stack_blk_t *blk;
    uint8_t *end;
    uint8_t *res = sp_reserve(sp, sizeof(mem_stack_frame_t),
                              bsrsz(__BIGGEST_ALIGNMENT__), &blk, &end);
    mem_stack_frame_t *frame;

    frame = (mem_stack_frame_t *)res;
    frame->blk  = blk;
    frame->pos  = end;
    frame->last = NULL;
    frame->prev = (uintptr_t)sp->stack;
    return sp->stack = frame;
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
