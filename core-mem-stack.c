/**************************************************************************/
/*                                                                        */
/*  Copyright (C) 2004-2011 INTERSEC SAS                                  */
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
#include "container.h"
#include "thr.h"

#ifndef __BIGGEST_ALIGNMENT__
#define __BIGGEST_ALIGNMENT__  16
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
        e_panic("You cannot allocate that amount of memory");
    blk = imalloc(blksize, MEM_RAW | MEM_LIBC);
    blk->blk.pool  = &sp->funcs;
    blk->blk.start = blk->area;
    blk->blk.size  = blksize - sizeof(*blk);
    sp->stacksize += blk->blk.size;
    mem_register(&blk->blk);
    dlist_add_tail(&sp->blk_list, &blk->blk_list);
    sp->nbpages++;
    return blk;
}

__cold
static void blk_destroy(mem_stack_pool_t *sp, mem_stack_blk_t *blk)
{
    sp->stacksize -= blk->blk.size;
    sp->nbpages--;
    mem_unregister(&blk->blk);
    dlist_remove(&blk->blk_list);
    ifree(blk, MEM_LIBC);
}

static ALWAYS_INLINE mem_stack_blk_t *
frame_get_next_blk(mem_stack_pool_t *sp, mem_stack_blk_t *cur, size_t size)
{
    mem_stack_blk_t *blk;

    dlist_for_each_entry_safe_continue(cur, blk, &sp->blk_list, blk_list) {
        if (blk->blk.size >= size)
            return blk;
        blk_destroy(sp, blk);
    }
    return blk_create(sp, size);
}

static ALWAYS_INLINE uint8_t *frame_end(mem_stack_frame_t *frame)
{
    mem_stack_blk_t *blk = frame->blk;
    return blk->area + blk->blk.size;
}

static void *sp_reserve(mem_stack_pool_t *sp, size_t asked,
                        mem_stack_blk_t **blkp, uint8_t **end)
{
    mem_stack_frame_t *frame = sp->stack;
    uint8_t           *res   = frame->pos;
    size_t             size  = ROUND_UP(asked, __BIGGEST_ALIGNMENT__);

    if (unlikely(res + size > frame_end(frame))) {
        mem_stack_blk_t *blk = frame_get_next_blk(sp, frame->blk, size);

        *blkp = blk;
        res   = blk->area;
    } else {
        *blkp = frame->blk;
    }
    (void)VALGRIND_MAKE_MEM_UNDEFINED(res, asked);
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
static void *sp_alloc(mem_pool_t *_sp, size_t size, mem_flags_t flags)
{
    mem_stack_pool_t *sp = container_of(_sp, mem_stack_pool_t, funcs);
    mem_stack_frame_t *frame = sp->stack;
    uint8_t *res;

    THROW_NULL_IF(size == 0);

#ifndef NDEBUG
    if (unlikely(frame == &sp->base))
        e_panic("allocation performed without a t_scope/t_push");
    if (frame->prev & 1)
        e_panic("allocation performed on a sealed stack");
    size += __BIGGEST_ALIGNMENT__;
#endif
    res = sp_reserve(sp, size, &frame->blk, &frame->pos);
    if (!(flags & MEM_RAW))
        memset(res, 0, size);
#ifndef NDEBUG
    res += __BIGGEST_ALIGNMENT__;
    ((void **)res)[-1] = sp->stack;
#endif
    return frame->last = res;
}

static void sp_free(mem_pool_t *_sp, void *mem, mem_flags_t flags)
{
}

static void *sp_realloc(mem_pool_t *_sp, void *mem,
                        size_t oldsize, size_t asked, mem_flags_t flags)
{
    mem_stack_pool_t *sp = container_of(_sp, mem_stack_pool_t, funcs);
    mem_stack_frame_t *frame = sp->stack;
    size_t size  = ROUND_UP(asked, __BIGGEST_ALIGNMENT__);
    uint8_t *res;

#ifndef NDEBUG
    if (frame->prev & 1)
        e_panic("allocation performed on a sealed stack");
    if (mem != NULL && unlikely(((void **)mem)[-1] != sp->stack))
        e_panic("%p wasn't allocated in that frame, realloc is forbidden", mem);
    if (unlikely(oldsize == MEM_UNKNOWN))
        e_panic("stack pools do not support reallocs with unknown old size");
#endif

    if (oldsize >= size) {
        if (mem == frame->last) {
            sp->stack->pos = (uint8_t *)mem + size;
        }
        (void)VALGRIND_MAKE_MEM_NOACCESS((uint8_t *)mem + asked, oldsize - asked);
        return size ? mem : NULL;
    }

    if (mem != NULL && mem == frame->last
    && frame->last + size <= frame_end(sp->stack))
    {
        sp->stack->pos = frame->last + size;
        sp->alloc_sz  += size - oldsize;
        (void)VALGRIND_MAKE_MEM_UNDEFINED(mem + oldsize, asked - oldsize);
        res = mem;
    } else {
        res = sp_alloc(_sp, size, flags | MEM_RAW);
        memcpy(res, mem, oldsize);
        (void)VALGRIND_MAKE_MEM_NOACCESS(mem, oldsize);
    }
    if (!(flags & MEM_RAW))
        p_clear(res + oldsize, asked - oldsize);
    return res;
}

static mem_pool_t const pool_funcs = {
    .malloc  = &sp_alloc,
    .realloc = &sp_realloc,
    .free    = &sp_free,
};

mem_stack_pool_t *mem_stack_pool_init(mem_stack_pool_t *sp, int initialsize)
{
    p_clear(sp, 1);
    dlist_init(&sp->blk_list);
    sp->blk.pool   = &sp->funcs;
    sp->blk.start  = blk_entry(&sp->blk_list)->area;
    sp->blk.size   = sizeof(mem_stack_pool_t) - sizeof(mem_stack_blk_t);

    sp->base.blk   = blk_entry(&sp->blk_list);
    sp->base.pos   = (void *)(sp + 1);
    sp->stack      = &sp->base;

    /* 640k should be enough for everybody =) */
    if (initialsize <= 0)
        initialsize = 640 << 10;
    sp->minsize    = ROUND_UP(MAX(1, initialsize), PAGE_SIZE);
    sp->funcs      = pool_funcs;
    sp->alloc_nb   = 1; /* avoid the division by 0 */

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
    uint8_t *res = sp_reserve(sp, sizeof(mem_stack_frame_t), &blk, &end);
    mem_stack_frame_t *frame;

    frame = (mem_stack_frame_t *)res;
    frame->blk  = blk;
    frame->pos  = end;
    frame->last = NULL;
    frame->prev = (uintptr_t)sp->stack;
    return sp->stack = frame;
}

#ifndef NDEBUG
void mem_stack_protect(mem_stack_pool_t *sp)
{
    mem_stack_blk_t *blk = sp->stack->blk;
    size_t remainsz = frame_end(sp->stack) - sp->stack->pos;

    (void)VALGRIND_MAKE_MEM_NOACCESS(sp->stack->pos, remainsz);
    dlist_for_each_entry_continue(blk, blk, &sp->blk_list, blk_list) {
        VALGRIND_PROT_BLK(&blk->blk);
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

void *stack_malloc(size_t size, mem_flags_t flags)
{
    return sp_alloc(t_pool(), size, flags);
}

void *stack_realloc(void *mem, size_t oldsize, size_t size, mem_flags_t flags)
{
    return sp_realloc(t_pool(), mem, oldsize, size, flags);
}

char *t_fmt(int *out, const char *fmt, ...)
{
#define T_FMT_LEN   1024
    va_list ap;
    char *res;
    int len;

    res = sp_alloc(t_pool(), T_FMT_LEN, MEM_RAW);
    va_start(ap, fmt);
    len = vsnprintf(res, T_FMT_LEN, fmt, ap);
    va_end(ap);
    if (likely(len < T_FMT_LEN)) {
        sp_realloc(t_pool(), res, T_FMT_LEN, len + 1, MEM_RAW);
    } else {
        res = sp_realloc(t_pool(), res, 0, len + 1, MEM_RAW);
        va_start(ap, fmt);
        len = vsnprintf(res, len + 1, fmt, ap);
        va_end(ap);
    }
    if (out)
        *out = len;
    return res;
#undef T_FMT_LEN
}

__thread mem_stack_pool_t t_pool_g;
