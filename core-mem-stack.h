/**************************************************************************/
/*                                                                        */
/*  Copyright (C) 2004-2018 INTERSEC SA                                   */
/*                                                                        */
/*  Should you receive a copy of this source code, you must check you     */
/*  have a proper, written authorization of INTERSEC to hold it. If you   */
/*  don't have such an authorization, you must DELETE all source code     */
/*  files in your possession, and inform INTERSEC of the fact you obtain  */
/*  these files. Should you not comply to these terms, you can be         */
/*  prosecuted in the extent permitted by applicable law.                 */
/*                                                                        */
/**************************************************************************/

#if !defined(IS_LIB_COMMON_CORE_H) || defined(IS_LIB_COMMON_CORE_MEM_STACK_H)
#  error "you must include core.h instead"
#else
#define IS_LIB_COMMON_CORE_MEM_STACK_H

#include "container-dlist.h"

/*
 * Stacked memory allocator
 * ~~~~~~~~~~~~~~~~~~~~~~~~
 *
 * This allocator mostly has the same properties as the GNU Obstack have.
 *
 * This mostly works like alloca() wrt your stack, except that it's a chain of
 * malloc()ed blocks. It also works like alloca in the sense that it aligns
 * allocated memory bits to the lowest required alignment possible (IOW
 * allocating blocks of sizes < 2, < 4, < 8, < 16 or >= 16 yield blocks aligned
 * to a 1, 2, 4, 8, and 16 boundary respectively.
 *
 * Additionnally to that, you have mem_stack_{push,pop} APIs that push/pop new
 * stack frames (ring a bell ?). Anything that has been allocated since the
 * last push is freed when you call pop.
 *
 * push/pop return void* cookies that you can match if you want to be sure
 * you're not screwing yourself with non matching push/pops. Matching push/pop
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
 * the bottom of a frame may or may not be in the same physical block where it
 * lives.
 */

#ifdef MEM_BENCH
/* defined in mem-bench.h */
struct mem_bench_t;
#endif

typedef struct mem_stack_blk_t {
    size_t      size;
    dlist_t     blk_list;
    uint8_t     area[];
} mem_stack_blk_t;

typedef struct mem_stack_frame_t mem_stack_frame_t;

struct mem_stack_frame_t {
    uintptr_t        prev;
    mem_stack_blk_t *blk;
    uint8_t         *pos;
    uint8_t         *end;
    uint8_t         *last;
};

/* all fields are annotated like this [offset (size) : usage] */
typedef struct mem_stack_pool_t {
    /* hot data : align on cache boundary */
    __attribute__((aligned(64)))
    mem_stack_frame_t   *stack;     /*<  0  (8) : everywhere */
    size_t               alloc_sz;  /*<  8  (8) : alloc */
    uint32_t             alloc_nb;  /*< 16  (4) : alloc */
    uint32_t             nbpops;    /*< 20  (4) : pop */

    mem_pool_t           funcs;     /*< 24 (40) : mp_* functions */

    /* cache line boundary */

    /* cold data : root block */
    size_t               size;      /*< never */
    dlist_t              blk_list;  /*< blk_create */

    mem_stack_frame_t    base;      /*< never */
    uint32_t             minsize;   /*< blk_create */

#ifdef MEM_BENCH
    /* never mind data : bench */
    struct mem_bench_t  *mem_bench;
    dlist_t              pool_list;
#endif
} mem_stack_pool_t;

mem_stack_pool_t *mem_stack_pool_init(mem_stack_pool_t *, int initialsize)
    __leaf;
void              mem_stack_pool_reset(mem_stack_pool_t *)
    __leaf;
void              mem_stack_pool_wipe(mem_stack_pool_t *)
    __leaf;

#ifndef NDEBUG
void mem_stack_protect(mem_stack_pool_t *sp, const mem_stack_frame_t *up_to);
/*
 * sealing a stack frame ensures that people wanting to allocate in that stack
 * use a mem_stack_push/mem_stack_pop or a t_scope first.
 *
 * It's not necessary to unseal before a pop().
 */
#  define mem_stack_prev(frame)  ((mem_stack_frame_t *)((frame)->prev & ~1L))
#  define mem_stack_seal(sp)     ((void)((sp)->stack->prev |=  1L))
#  define mem_stack_unseal(sp)   ((void)((sp)->stack->prev &= ~1L))
#else
#  define mem_stack_prev(frame)       ((mem_stack_frame_t *)(frame)->prev)
#  define mem_stack_seal(sp)          ((void)0)
#  define mem_stack_unseal(sp)        ((void)0)
#  define mem_stack_protect(sp, end)  ((void)0)
#endif

static ALWAYS_INLINE bool mem_stack_is_at_top(mem_stack_pool_t *sp)
{
    return sp->stack == &sp->base;
}

const void *mem_stack_push(mem_stack_pool_t *) __leaf;
#ifndef NDEBUG
const void *mem_stack_pop_libc(mem_stack_pool_t *);
#endif

#ifdef MEM_BENCH
void mem_stack_bench_pop(mem_stack_pool_t *, mem_stack_frame_t *);
#endif
void mem_stack_pool_print_stats(mem_pool_t *);
void mem_stack_pools_print_stats(void);

static ALWAYS_INLINE const void *mem_stack_pop(mem_stack_pool_t *sp)
{
    mem_stack_frame_t *frame = sp->stack;

#ifndef NDEBUG
    /* bypass mem_pool if demanded */
    if (!mem_pool_is_enabled()) {
        return mem_stack_pop_libc(sp);
    }
#endif

    sp->stack = mem_stack_prev(frame);
#ifdef MEM_BENCH
    mem_stack_bench_pop(sp, frame);
#endif
    assert (sp->stack);
    mem_stack_protect(sp, frame);
    if (++sp->nbpops >= UINT16_MAX && mem_stack_is_at_top(sp)) {
        sp->nbpops = 0;
        mem_stack_pool_reset(sp);
    }
    return frame;
}

extern __thread mem_stack_pool_t t_pool_g;

static ALWAYS_INLINE mem_pool_t *t_pool(void)
{
    return &t_pool_g.funcs;
}

#define t_seal()      mem_stack_seal(&t_pool_g)
#define t_unseal()    mem_stack_unseal(&t_pool_g)

#define t_fmt(fmt, ...)  mp_fmt(&t_pool_g.funcs, NULL, fmt, ##__VA_ARGS__)

/* Aligned pointers allocation helpers */

#define ta_new_raw(type, count, alignment)                                   \
    mpa_new_raw(t_pool(), type, (count), (alignment))
#define ta_new(type, count, alignment)                                       \
    mpa_new(t_pool(), type, (count), (alignment))
#define ta_new_extra(type, size, alignment)                                  \
    mpa_new_extra(t_pool(), type, (size), (alignment))
#define ta_new_extra_raw(type, size, alignment)                              \
    mpa_new_extra_raw(t_pool(), type, (size), (alignment))
#define ta_new_extra_field(type, field, size, alignment)                     \
    mpa_new_extra_field(t_pool(), type, field, (size), (alignment))
#define ta_new_extra_field_raw(type, field, size, alignment)                 \
    mpa_new_extra_field_raw(t_pool(), type, field, (size), (alignment))

#define ta_realloc0(pp, old, now, alignment)                                 \
    mpa_realloc0(t_pool(), (pp), (old), (now), (alignment))
#define ta_realloc_extra(pp, extra, alignment)                               \
    mpa_realloc0_extra(t_pool(), (pp), (extra), (alignment))
#define ta_realloc0_extra(pp, old_extra, new_extra, alignment)               \
    mpa_realloc0_extra(t_pool(), (pp), (old_extra), (new_extra), (alignment))

#define ta_realloc_extra_field(pp, field, count, alignment)                  \
    mpa_realloc_extra_field(t_pool(), (pp), field, (count), (alignment))
#define ta_realloc0_extra_field(pp, field, old_count, new_count, alignment)  \
    mpa_realloc0_extra_field(t_pool(), (pp), field, (old_count),             \
                             (new_count), (alignment))

#define ta_dup(p, count, alignment)                                          \
    mpa_dup(t_pool(), (p), (count), (alignment))

/* Pointer allocations helpers */

#define t_new_raw(type, count)       ta_new_raw(type, count, alignof(type))
#define t_new(type, count)           ta_new(type, count, alignof(type))
#define t_new_extra(type, size)      ta_new_extra(type, size, alignof(type))
#define t_new_extra_raw(type, size)  ta_new_extra_raw(type, size, alignof(type))
#define t_new_extra_field(type, field, size)  \
    ta_new_extra_field(type, field, size, alignof(type))
#define t_new_extra_field_raw(type, field, size) \
    ta_new_extra_field_raw(type, field, size, alignof(type))

#define t_realloc0(tp, old, now)    ta_realloc0(tp, old, now, alignof(**(tp)))
#define t_realloc_extra(tp, extra)  ta_realloc0_extra(tp, extra, alignof(**(tp)))
#define t_realloc0_extra(tp, old_extra, new_extra)  \
    ta_realloc0_extra(tp, old_extra, new_extra, alignof(**(tp)))

#define t_realloc_extra_field(tp, field, count)  \
    ta_realloc_extra_field(tp, field, count, alignof(**(tp)))
#define t_realloc0_extra_field(tp, field, old_count, new_count)  \
    ta_realloc0_extra_field(tp, field, old_count, new_count, alignof(**(tp)))


#define t_dup(p, count)    ta_dup((p), (count), alignof(p))
#define t_dupz(p, count)   mp_dupz(t_pool(), (p), (count))
#define t_strdup(p)        mp_strdup(t_pool(), (p))

#ifndef __cplusplus
/*
 * t_scope protects all the code after its use up to the end of the block
 * scope with an implicit mem_stack_push(), mem_stack_pop() pair.
 *
 * It works using the same principle as C++ RAII, see the C++ TScope class
 * below, but for C.
 *
 * As a rule, it's better to use `t_scope;` just after the scope opening so
 * that it gards the full block properly, e.g.:
 *
 *     {
 *         t_scope;
 *         char *buf = t_new_raw(char, BUFSIZ); // safe
 *
 *         // ...
 *     }
 */
static ALWAYS_INLINE void t_scope_cleanup(const void **unused)
{
#ifndef NDEBUG
    if (unlikely(*unused != mem_stack_pop(&t_pool_g)))
        e_panic("unbalanced t_stack");
#else
    mem_stack_pop(&t_pool_g);
#endif
}
#define t_scope__(n)  \
    const void *t_scope_##n __attribute__((unused,cleanup(t_scope_cleanup))) \
        = mem_stack_push(&t_pool_g)
#else
/*
 * RAII scoped mem_stask_push/mem_stack_pop
 */
class TScope {
  public:
    inline TScope() { mem_stack_push(&t_pool_g); };
    inline ~TScope() { mem_stack_pop(&t_pool_g); };
  private:
    DISALLOW_COPY_AND_ASSIGN(TScope);
    void* operator new(size_t);
    void  operator delete(void *, size_t);
};
#define t_scope__(n)  \
    TScope t_scope_because_cpp_sucks_donkeys_##n
#endif

#define t_scope_(n)  t_scope__(n)
#define t_scope      t_scope_(__LINE__)

#endif
