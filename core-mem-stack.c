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

#include "core-mem-valgrind.h"
#include "container.h"

/*
 * Stacked memory allocator
 * ~~~~~~~~~~~~~~~~~~~~~~~~
 *
 * This allocator mostly has the same properties as the GNU Obstack have.
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
 * kludge that the pool looks like a block enough to be one (it allows to have
 * no single point and that the list of blocks is never empty even when there
 * is no physical block of memory allocated). The pool also contains the base
 * frame so that the base frame doesn't prevent any block from being collected
 * at any moment.
 *
 *   [pool]==[blk 1]==[blk 2]==...==[blk n]
 *     \\                             //
 *      \=============================/
 *
 * In addition to the based frame pointer, The pool contains a stack of
 * chained frames.  The frames are allocated into the stacked allocator,
 * except for the base one. Frames points to the first octet in the block
 * "rope" that is free.  IOW it looks like this:
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
    struct frame_t *prev;
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
    size_t     nbpages;

    uint32_t   alloc_sz;
    uint32_t   alloc_nb;

    mem_pool_t funcs;
} stack_pool_t;

static size_t sp_alloc_mean(stack_pool_t *sp)
{
    return sp->alloc_sz / sp->alloc_nb;
}

static stack_blk_t *blk_entry(dlist_t *l)
{
    return container_of(l, stack_blk_t, blk_list);
}

static inline uint8_t align_boundary(size_t size)
{
    return MIN(16, 1 << bsrsz(size | 1));
}

static inline bool is_aligned_to(const void *addr, size_t boundary)
{
    return ((uintptr_t)addr & (boundary - 1)) == 0;
}

static byte *align_for(frame_t *frame, size_t size)
{
    size_t bmask = align_boundary(size) - 1;
    return (byte *)(((uintptr_t)frame->pos + bmask) & ~bmask);
}

static stack_blk_t *blk_create(stack_pool_t *sp, size_t size_hint)
{
    size_t blksize = size_hint + sizeof(stack_blk_t);
    stack_blk_t *blk;

    if (size_hint < sp->minsize)
        size_hint = sp->minsize;
    if (size_hint < 64 * sp_alloc_mean(sp))
        size_hint = 64 * sp_alloc_mean(sp);
    blksize = ROUND_UP(blksize, PAGE_SIZE);
    if (blksize > MEM_ALLOC_MAX)
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

static void blk_destroy(stack_pool_t *sp, stack_blk_t *blk)
{
    sp->stacksize -= blk->blk.size;
    sp->nbpages--;
    mem_unregister(&blk->blk);
    dlist_remove(&blk->blk_list);
    ifree(blk, MEM_LIBC);
}

static stack_blk_t *frame_get_next_blk(stack_pool_t *sp, stack_blk_t *cur, size_t size)
{
    while (cur->blk_list.next != &sp->blk_list) {
        stack_blk_t *blk = dlist_next_entry(cur, blk_list);
        if (blk->blk.size >= size && blk->blk.size > 8 * sp_alloc_mean(sp))
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

static void *sp_reserve(stack_pool_t *sp, size_t size, stack_blk_t **blkp)
{
    frame_t *frame = sp->stack;
    byte *res = align_for(frame, size);

    if (res + size > frame_end(frame)) {
        stack_blk_t *blk = frame_get_next_blk(sp, frame->blk, size);

        *blkp = blk;
        res = blk->area;
    } else {
        *blkp = frame->blk;
    }
    VALGRIND_MAKE_MEM_UNDEFINED(res, size);
    if (unlikely(sp->alloc_sz + size < sp->alloc_sz)
    ||  unlikely(sp->alloc_nb >= UINT16_MAX))
    {
        sp->alloc_sz /= 2;
        sp->alloc_nb /= 2;
    }
    sp->alloc_sz += size;
    sp->alloc_nb += 1;
    return res;
}

static void *sp_alloc(mem_pool_t *_sp, size_t size, mem_flags_t flags)
{
    stack_pool_t *sp = container_of(_sp, stack_pool_t, funcs);
    frame_t *frame = sp->stack;
    byte *res;

    res = sp_reserve(sp, size, &frame->blk);
    if (!(flags & MEM_RAW))
        memset(res, 0, size);
    frame->pos = res + size;
    return frame->last = res;
}

static void sp_free(mem_pool_t *_sp, void *mem, mem_flags_t flags)
{
}

static void *sp_realloc(mem_pool_t *_sp, void *mem,
                        size_t oldsize, size_t size, mem_flags_t flags)
{
    stack_pool_t *sp = container_of(_sp, stack_pool_t, funcs);
    frame_t *frame = sp->stack;
    byte *res;

    if (unlikely(oldsize == MEM_UNKNOWN))
        e_panic("stack pools do not support reallocs with unknown old size");

    if (oldsize >= size) {
        if (mem == frame->last) {
            sp->stack->pos = (byte *)mem + size;
        }
        VALGRIND_MAKE_MEM_NOACCESS((byte *)mem + size, oldsize - size);
        return size ? mem : NULL;
    }

    if (mem != NULL && mem == frame->last
    &&  is_aligned_to(mem, align_boundary(size))
    &&  (byte *)frame->last + size <= frame_end(sp->stack))
    {
        sp->stack->pos = (byte *)frame->last + size;
        sp->alloc_sz  += size - oldsize;
        VALGRIND_MAKE_MEM_DEFINED(mem, size);
        res = mem;
    } else {
        res = sp_alloc(_sp, size, flags | MEM_RAW);
        memcpy(res, mem, oldsize);
        VALGRIND_MAKE_MEM_UNDEFINED(mem, oldsize);
    }
    if (!(flags & MEM_RAW))
        memset(res + oldsize, 0, size - oldsize);
    return res;
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
    sp->base.pos   = sp + 1;
    sp->stack      = &sp->base;

    /* 640k should be enough for everybody =) */
    if (initialsize <= 0)
        initialsize = 640 << 10;
    sp->minsize    = ROUND_UP(MAX(1, initialsize), PAGE_SIZE);
    sp->funcs      = pool_funcs;
    sp->alloc_nb   = 1; /* avoid the division by 0 */
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
    stack_blk_t *blk;
    byte *res = sp_reserve(sp, sizeof(frame_t), &blk);
    frame_t *frame;

    frame = (frame_t *)res;
    frame->blk  = blk;
    frame->pos  = res + sizeof(frame_t);
    frame->last = NULL;
    frame->prev = sp->stack;
    return sp->stack = frame;
}

#ifndef NDEBUG
static void mem_stack_protect(stack_pool_t *sp)
{
    stack_blk_t *blk = sp->stack->blk;
    size_t remainsz = frame_end(sp->stack) - (byte *)sp->stack->pos;

    VALGRIND_MAKE_MEM_NOACCESS(sp->stack->pos, remainsz);
    dlist_for_each_entry_continue(blk, blk, &sp->blk_list, blk_list) {
        VALGRIND_PROT_BLK(&blk->blk);
    }
}
#else
#define mem_stack_protect(sp)
#endif

const void *mem_stack_pop(mem_pool_t *_sp)
{
    stack_pool_t *sp = container_of(_sp, stack_pool_t, funcs);
    frame_t *frame = sp->stack;

    if (frame->prev) {
        sp->stack = frame->prev;
        mem_stack_protect(sp);
        return sp->stack;
    }
    frame->blk  = blk_entry(&sp->blk_list);
    frame->pos  = sp + 1; /* XXX: end of sp, see kludge remarks above */
    frame->last = NULL;
    mem_stack_protect(sp);
    return NULL;
}

void mem_stack_rewind(mem_pool_t *_sp, const void *cookie)
{
    stack_pool_t *sp = container_of(_sp, stack_pool_t, funcs);

    if (cookie == NULL) {
        sp->base.blk  = blk_entry(&sp->blk_list);
        sp->base.pos  = sp + 1; /* XXX: end of sp, see kludge remarks above */
        sp->base.last = NULL;
        sp->stack     = &sp->base;
        mem_stack_protect(sp);
        return;
    }
#ifndef NDEBUG
    for (frame_t *frame = sp->stack; frame->prev; frame = frame->prev) {
        if (frame == cookie) {
            sp->stack = frame->prev;
            mem_stack_protect(sp);
            return;
        }
    }
    e_panic("invalid cookie");
#else
    sp->stack = ((frame_t *)cookie)->prev;
#endif
}

mem_pool_t *t_pool(void)
{
    static __thread mem_pool_t *sp;
    if (unlikely(!sp)) {
        sp = mem_stack_pool_new(64 << 10);
    }
    return sp;
}

void *stack_malloc(size_t size, mem_flags_t flags)
{
    return sp_alloc(t_pool(), size, flags);
}

void *stack_realloc(void *mem, size_t oldsize, size_t size, mem_flags_t flags)
{
    return sp_realloc(t_pool(), mem, oldsize, size, flags);
}
