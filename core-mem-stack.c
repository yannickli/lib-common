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

static size_t sp_alloc_mean(mem_stack_pool_t *sp)
{
    return sp->alloc_sz / sp->alloc_nb;
}

static mem_stack_blk_t *blk_entry(dlist_t *l)
{
    return container_of(l, mem_stack_blk_t, blk_list);
}

static inline uint8_t align_boundary(size_t size)
{
    return MIN(16, 1 << bsrsz(size | 1));
}

static inline bool is_aligned_to(const void *addr, size_t boundary)
{
    return ((uintptr_t)addr & (boundary - 1)) == 0;
}

static byte *align_for(mem_stack_frame_t *frame, size_t size)
{
    size_t bmask = align_boundary(size) - 1;
    return (byte *)(((uintptr_t)frame->pos + bmask) & ~bmask);
}

static mem_stack_blk_t *blk_create(mem_stack_pool_t *sp, size_t size_hint)
{
    size_t blksize = size_hint + sizeof(mem_stack_blk_t);
    mem_stack_blk_t *blk;

    blksize += sizeof(mem_stack_blk_t);
    if (blksize < sp->minsize)
        blksize = sp->minsize;
    if (blksize < 64 * sp_alloc_mean(sp))
        blksize = 64 * sp_alloc_mean(sp);
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

static void blk_destroy(mem_stack_pool_t *sp, mem_stack_blk_t *blk)
{
    sp->stacksize -= blk->blk.size;
    sp->nbpages--;
    mem_unregister(&blk->blk);
    dlist_remove(&blk->blk_list);
    ifree(blk, MEM_LIBC);
}

static mem_stack_blk_t *frame_get_next_blk(mem_stack_pool_t *sp, mem_stack_blk_t *cur, size_t size)
{
    while (cur->blk_list.next != &sp->blk_list) {
        mem_stack_blk_t *blk = dlist_next_entry(cur, blk_list);
        if (blk->blk.size >= size && blk->blk.size > 8 * sp_alloc_mean(sp))
            return blk;
        blk_destroy(sp, blk);
    }
    return blk_create(sp, size);
}

static byte *frame_end(mem_stack_frame_t *frame)
{
    mem_stack_blk_t *blk = frame->blk;
    return blk->area + blk->blk.size;
}

static void *sp_reserve(mem_stack_pool_t *sp, size_t size, mem_stack_blk_t **blkp)
{
    mem_stack_frame_t *frame = sp->stack;
    byte *res = align_for(frame, size);

    if (unlikely(res + size > frame_end(frame))) {
        mem_stack_blk_t *blk = frame_get_next_blk(sp, frame->blk, size);

        *blkp = blk;
        res = blk->area;
    } else {
        *blkp = frame->blk;
    }
    (void)VALGRIND_MAKE_MEM_UNDEFINED(res, size);
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

__flatten
static void *sp_alloc(mem_pool_t *_sp, size_t size, mem_flags_t flags)
{
    mem_stack_pool_t *sp = container_of(_sp, mem_stack_pool_t, funcs);
    mem_stack_frame_t *frame = sp->stack;
    byte *res;

#ifndef NDEBUG
    size += sizeof(void *);
#endif
    res = sp_reserve(sp, size, &frame->blk);
    if (!(flags & MEM_RAW))
        memset(res, 0, size);
    frame->pos = res + size;
#ifndef NDEBUG
    res = mempcpy(res, &sp->stack, sizeof(sp->stack));
#endif
    return frame->last = res;
}

static void sp_free(mem_pool_t *_sp, void *mem, mem_flags_t flags)
{
}

static void *sp_realloc(mem_pool_t *_sp, void *mem,
                        size_t oldsize, size_t size, mem_flags_t flags)
{
    mem_stack_pool_t *sp = container_of(_sp, mem_stack_pool_t, funcs);
    mem_stack_frame_t *frame = sp->stack;
    byte *res;

    if (unlikely(oldsize == MEM_UNKNOWN))
        e_panic("stack pools do not support reallocs with unknown old size");

#ifndef NDEBUG
    if (mem != NULL) {
        if (unlikely(((void **)mem)[-1] != sp->stack))
            e_panic("%p wasn't allocated in that frame, realloc is forbidden", mem);
    }
#endif

    if (oldsize >= size) {
        if (mem == frame->last) {
            sp->stack->pos = (byte *)mem + size;
        }
        (void)VALGRIND_MAKE_MEM_NOACCESS((byte *)mem + size, oldsize - size);
        return size ? mem : NULL;
    }

    if (mem != NULL && mem == frame->last
    &&  is_aligned_to(mem, align_boundary(size))
    &&  (byte *)frame->last + size <= frame_end(sp->stack))
    {
        sp->stack->pos = (byte *)frame->last + size;
        sp->alloc_sz  += size - oldsize;
        (void)VALGRIND_MAKE_MEM_DEFINED(mem, size);
        res = mem;
    } else {
        res = sp_alloc(_sp, size, flags | MEM_RAW);
        memcpy(res, mem, oldsize);
        (void)VALGRIND_MAKE_MEM_UNDEFINED(mem, oldsize);
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

mem_stack_pool_t *mem_stack_pool_init(mem_stack_pool_t *sp, int initialsize)
{
    p_clear(sp, 1);
    dlist_init(&sp->blk_list);
    sp->blk.pool   = &sp->funcs;
    sp->blk.start  = blk_entry(&sp->blk_list)->area;
    sp->blk.size   = sizeof(mem_stack_pool_t) - sizeof(mem_stack_blk_t);

    sp->base.blk   = blk_entry(&sp->blk_list);
    sp->base.pos   = sp + 1;
    sp->stack      = &sp->base;

    /* 640k should be enough for everybody =) */
    if (initialsize <= 0)
        initialsize = 640 << 10;
    sp->minsize    = ROUND_UP(MAX(1, initialsize), PAGE_SIZE);
    sp->funcs      = pool_funcs;
    sp->alloc_nb   = 1; /* avoid the division by 0 */

    return sp;
}

mem_pool_t *mem_stack_pool_new(int initialsize)
{
    return &mem_stack_pool_init(p_new_raw(mem_stack_pool_t, 1), initialsize)->funcs;
}

void mem_stack_pool_wipe(mem_stack_pool_t *sp)
{
    dlist_for_each_safe(e, &sp->blk_list) {
        blk_destroy(sp, blk_entry(e));
    }
}

void mem_stack_pool_delete(mem_pool_t **spp)
{
    if (*spp) {
        mem_stack_pool_t *sp = container_of(*spp, mem_stack_pool_t, funcs);

        mem_stack_pool_wipe(sp);
        p_delete(&sp);
        *spp = NULL;
    }
}

const void *mem_stack_push(mem_stack_pool_t *sp)
{
    mem_stack_blk_t *blk;
    byte *res = sp_reserve(sp, sizeof(mem_stack_frame_t), &blk);
    mem_stack_frame_t *frame;

    frame = (mem_stack_frame_t *)res;
    frame->blk  = blk;
    frame->pos  = res + sizeof(mem_stack_frame_t);
    frame->last = NULL;
    frame->prev = sp->stack;
    return sp->stack = frame;
}

#ifndef NDEBUG
static void mem_stack_protect(mem_stack_pool_t *sp)
{
    mem_stack_blk_t *blk = sp->stack->blk;
    size_t remainsz = frame_end(sp->stack) - (byte *)sp->stack->pos;

    (void)VALGRIND_MAKE_MEM_NOACCESS(sp->stack->pos, remainsz);
    dlist_for_each_entry_continue(blk, blk, &sp->blk_list, blk_list) {
        VALGRIND_PROT_BLK(&blk->blk);
    }
}

const void *mem_stack_pop(mem_stack_pool_t *sp)
{
    mem_stack_frame_t *frame = sp->stack;

    assert (frame->prev);
    sp->stack = frame->prev;
    mem_stack_protect(sp);
    return frame;
}
#else
#define mem_stack_protect(sp)
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
#define T_FMT_LEN   256
    va_list ap;
    char *res;
    int len;

    res = sp_alloc(t_pool(), T_FMT_LEN, MEM_RAW);
    va_start(ap, fmt);
    len = vsnprintf(res, T_FMT_LEN, fmt, ap);
    va_end(ap);
    if (unlikely(len >= T_FMT_LEN)) {
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
