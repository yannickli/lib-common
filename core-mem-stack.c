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

#include <sys/user.h>
#include "container.h"

/*
 * Stacked memory allocator
 * ~~~~~~~~~~~~~~~~~~~~~~~~
 *
 * This mostly works like alloca() wrt your stack, except that it's a chain of
 * malloc()ed blocks. It also works like alloca in the sense that it aligns
 * allocated memory bits to the lowest required alignment possible (IOW
 * allocting blocks of sizes < 2, < 4, < 8, < 16 or >= 16 yield blocks aligned
 * to a 1, 2, 4, 8, and 16 boundary respectively.
 *
 * Additionnally to that, you have mem_stack_{push,pop} APIs that push/pop new
 * stack frames (ring a bell ?). Anything that has been allocated since the
 * last push is freed when you call pop.
 *
 * push/pop return void* cookies that you can match if you want to be sure
 * you're not screwing yourself with non matchin push/pops. Matching push/pop
 * should return the same cookie value.
 *
 * Note that a pristine stacked pool has an implicit push() done, with a
 * matching cookie of `NULL`. Note that this last pop rewinds the stack pool
 * into its pristine state in the sense that it's ready to allocate memory and
 * no further push() is needed (an implicit push is done here too).
 *
 * Reallocing the last allocated data is efficient in the sense that it tries
 * to keep the data at the same place.
 *
 *
 * Physical memory is reclaimed based on different heuristics. pop() is not
 * enough to reclaim the whole pool memory, only deleting the pool does that.
 * The pool somehow tries to keep the largest chunks of data allocated around,
 * and to discard the ones that are definitely too small to do anything useful
 * with them. Also, new blocks allocated are always bigger than the last
 * biggest block that was allocated (or of the same size).
 *
 *
 * A word on how it works, there is a list of chained blocks, with the huge
 * kludge that the pool looks like a block enough to be one.
 *
 *   [pool]==[blk 1]==[blk 2]==...==[blk n]
 *     \\                             //
 *      \=============================/
 *
 * The pool contains a based frame pointer, and a stack of chained frames.
 * The frames are allocated into the stacked allocator, except for the base
 * one. Frames points to the first octet in the block "rope" that is free.
 * IOW it looks like this:
 *
 * [ fake block 0 == pool ] [  octets of block 1 ]  [ block 2 ] .... [ block n ]
 *   base_                     frame1_  frame2_                        |
 *        \____________________/      \_/      \_______________________/
 *
 * consecutive frames may or may not be in the same physical block.
 * the bottom of a frame may or may not bi in the same physical block where it
 * lives.
 */

typedef struct stack_blk_t {
    mem_blk_t blk;
    dlist_t   blk_list;
    byte      area[];
} stack_blk_t;

typedef struct frame_t {
    struct frame_t *next;
    stack_blk_t    *blk;
    void           *pos;
    void           *last;
} frame_t;

typedef struct stack_pool_t {
    mem_blk_t  blk;
    dlist_t    blk_list;

    /* XXX: kludge: below this point we're the "blk" data */
    frame_t    base;
    frame_t   *stack;
    size_t     minsize;
    size_t     stacksize;

    mem_pool_t funcs;
} stack_pool_t;

static stack_blk_t *blk_entry(dlist_t *l)
{
    return container_of(l, stack_blk_t, blk_list);
}

static stack_blk_t *blk_create(stack_pool_t *sp, size_t size_hint)
{
    size_t blksize = size_hint + sizeof(stack_blk_t);
    stack_blk_t *blk;

    if (size_hint < sp->minsize)
        size_hint = sp->minsize;
    if (size_hint < sp->stacksize / 8)
        size_hint = sp->stacksize / 8;
    blksize = ROUND_UP(blksize, PAGE_SIZE);
    if (blksize > MEM_ALLOC_MAX)
        e_panic("You cannot allocate that amount of memory");
    sp->minsize = blksize;
    blk = imalloc(blksize, MEM_RAW | MEM_LIBC);
    blk->blk.pool  = &sp->funcs;
    blk->blk.start = blk->area;
    blk->blk.size  = blksize - sizeof(*blk);
    sp->stacksize += blk->blk.size;
    mem_register(&blk->blk);
    dlist_add_tail(&sp->blk_list, &blk->blk_list);
    return blk;
}

static void blk_destroy(stack_pool_t *sp, stack_blk_t *blk)
{
    mem_unregister(&blk->blk);
    dlist_remove(&blk->blk_list);
    ifree(blk, MEM_LIBC);
}

static stack_blk_t *frame_get_next_blk(stack_pool_t *sp, stack_blk_t *cur, size_t size)
{
    while (cur->blk_list.next != &sp->blk_list) {
        stack_blk_t *blk = dlist_next_entry(cur, blk_list);
        if (blk->blk.size >= size)
            return blk;
        blk_destroy(sp, blk);
    }
    return blk_create(sp, size);
}

static byte *frame_end(frame_t *frame)
{
    stack_blk_t *blk = frame->blk;
    return blk->area + blk->blk.size;
}

static inline int align_boundary(size_t size)
{
    int bsr = 31 ^ __builtin_clz(size | 1);
    return MIN(16, 1 << bsr);
}

static byte *align_start(frame_t *frame, size_t size)
{
    return (byte *)ROUND_UP((uintptr_t)frame->pos, align_boundary(size));
}

static void *sp_alloc(mem_pool_t *_sp, size_t size, mem_flags_t flags)
{
    stack_pool_t *sp = container_of(_sp, stack_pool_t, funcs);
    frame_t *frame = sp->stack;
    byte *res = align_start(frame, size);

    if (res + size > frame_end(frame)) {
        stack_blk_t *blk = frame_get_next_blk(sp, frame->blk, size);
        frame->blk = blk;
        frame->pos = res = blk->area;
    }
    if (!(flags & MEM_RAW))
        memset(res, 0, size);
    frame->pos = res + size;
    return frame->last = res;
}

static void sp_free(mem_pool_t *_sp, void *mem, mem_flags_t flags)
{
}

static void sp_realloc(mem_pool_t *_sp, void **memp,
                       size_t oldsize, size_t size, mem_flags_t flags)
{
    stack_pool_t *sp = container_of(_sp, stack_pool_t, funcs);
    frame_t *frame = sp->stack;
    void *mem = *memp;
    byte *res;

    if (unlikely(oldsize == MEM_UNKNOWN))
        e_panic("stack pools do not support reallocs with unknown old size");
    if (size == 0) {
        *memp = NULL;
        return;
    }
    if (oldsize >= size)
        return;

    if (mem == frame->last
    &&  align_boundary(oldsize) == align_boundary(size)
    &&  (byte *)frame->last + size <= frame_end(sp->stack))
    {
        res = sp->stack->pos = (byte *)frame->last + size;
    } else {
        res = sp_alloc(_sp, size, flags | MEM_RAW);
        memcpy(res, mem, oldsize);
    }
    if (!(flags & MEM_RAW))
        memset(res + oldsize, 0, oldsize - size);
    *memp = res;
}

static mem_pool_t const pool_funcs = {
    .malloc  = &sp_alloc,
    .realloc = &sp_realloc,
    .free    = &sp_free,
};

mem_pool_t *mem_stack_pool_new(int initialsize)
{
    stack_pool_t *sp = p_new(stack_pool_t, 1);

    dlist_init(&sp->blk_list);
    sp->blk.pool   = &sp->funcs;
    sp->blk.start  = blk_entry(&sp->blk_list)->area;
    sp->blk.size   = sizeof(stack_pool_t) - sizeof(stack_blk_t);

    sp->base.blk   = blk_entry(&sp->blk_list);
    sp->base.pos   = blk_entry(&sp->blk_list) + sizeof(*sp);
    sp->stack      = &sp->base;

    /* 640k should be enough for everybody =) */
    if (initialsize <= 0)
        initialsize = 640 << 10;
    sp->minsize    = ROUND_UP(MAX(1, initialsize), PAGE_SIZE);
    sp->funcs      = pool_funcs;
    return &sp->funcs;
}

void mem_stack_pool_delete(mem_pool_t **spp)
{
    stack_pool_t *sp = container_of(*spp, stack_pool_t, funcs);
    if (sp) {
        dlist_for_each_safe(e, &sp->blk_list) {
            blk_destroy(sp, blk_entry(e));
        }
        p_delete(&sp);
        *spp = NULL;
    }
}

const void *mem_stack_push(mem_pool_t *_sp)
{
    stack_pool_t *sp = container_of(_sp, stack_pool_t, funcs);
    frame_t *frame = sp_alloc(_sp, sizeof(frame), 0);

    frame->blk  = sp->stack->blk;
    frame->pos  = sp->stack->pos;
    frame->next = sp->stack;
    return sp->stack = frame;
}

const void *mem_stack_pop(mem_pool_t *_sp)
{
    stack_pool_t *sp = container_of(_sp, stack_pool_t, funcs);
    frame_t *frame = sp->stack;

    if (frame->next)
        return sp->stack = frame->next;
    frame->blk  = blk_entry(&sp->blk_list);
    frame->pos  = blk_entry(&sp->blk_list) + sizeof(*sp);
    frame->last = NULL;
    return NULL;
}
