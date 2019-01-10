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

typedef struct mem_stack_blk_t {
    const void *start;
    size_t      size;
    dlist_t     blk_list;
    uint8_t     area[];
} mem_stack_blk_t;

typedef struct mem_stack_frame_t mem_stack_frame_t;

struct mem_stack_frame_t {
    uintptr_t        prev;
    mem_stack_blk_t *blk;
    uint8_t         *pos;
    uint8_t         *last;
};

typedef struct mem_stack_pool_t {
    const void          *start;
    size_t               size;
    dlist_t              blk_list;

    /* XXX: kludge: below this point we're the "blk" data */
    mem_stack_frame_t    base;
    mem_stack_frame_t   *stack;
    size_t               stacksize;
    uint32_t             minsize;
    uint32_t             nbpages;
    uint32_t             nbpops;

    uint32_t             alloc_nb;
    size_t               alloc_sz;

    mem_pool_t           funcs;
} mem_stack_pool_t;

mem_stack_pool_t *mem_stack_pool_init(mem_stack_pool_t *, int initialsize)
    __leaf;
void              mem_stack_pool_wipe(mem_stack_pool_t *)
    __leaf;

#ifndef NDEBUG
void mem_stack_protect(mem_stack_pool_t *sp);
/*
 * sealing a stack frame ensures that people wanting to allocate in that stack
 * use a t_push/t_pop or a t_scope first.
 *
 * It's not necessary to unseal before a pop().
 */
#  define mem_stack_prev(frame)  ((mem_stack_frame_t *)((frame)->prev & ~1L))
#  define mem_stack_seal(sp)     ((void)((sp)->stack->prev |=  1L))
#  define mem_stack_unseal(sp)   ((void)((sp)->stack->prev &= ~1L))
#else
#  define mem_stack_prev(frame)  ((mem_stack_frame_t *)(frame)->prev)
#  define mem_stack_seal(sp)     ((void)0)
#  define mem_stack_unseal(sp)   ((void)0)
#  define mem_stack_protect(sp)  ((void)0)
#endif

static ALWAYS_INLINE bool mem_stack_is_at_top(mem_stack_pool_t *sp)
{
    return sp->stack == &sp->base;
}

const void *mem_stack_push(mem_stack_pool_t *) __leaf;
static ALWAYS_INLINE const void *mem_stack_pop(mem_stack_pool_t *sp)
{
    mem_stack_frame_t *frame = sp->stack;

    sp->stack = mem_stack_prev(frame);
    assert (sp->stack);
    mem_stack_protect(sp);
    if (++sp->nbpops >= UINT16_MAX && mem_stack_is_at_top(sp)) {
        sp->nbpops = 0;
        mem_stack_pool_wipe(sp);
    }
    return frame;
}

extern __thread mem_stack_pool_t t_pool_g;
#define t_pool()  (&t_pool_g.funcs)

#define t_push()      mem_stack_push(&t_pool_g)
#define t_pop()       mem_stack_pop(&t_pool_g)
#define t_seal()      mem_stack_seal(&t_pool_g)
#define t_unseal()    mem_stack_unseal(&t_pool_g)

/* Deprecated: do not use */
#define __t_pop_and_do(expr)    ({ t_pop(); expr; })
#define t_pop_and_return(expr)  __t_pop_and_do(return expr)
#define t_pop_and_break()       __t_pop_and_do(break)
#define t_pop_and_continue()    __t_pop_and_do(continue)
#define t_pop_and_goto(lbl)     __t_pop_and_do(goto lbl)

char *t_fmt(int *out, const char *fmt, ...)
    __leaf __attr_printf__(2, 3);

#define ta_new(type_t, n, alignment) \
    ((type_t *)stack_malloc((n) * sizeof(type_t), (alignment), MEM_STACK))
#define ta_new_raw(type_t, n, alignment)  \
    ((type_t *)stack_malloc((n) * sizeof(type_t), (alignment), MEM_STACK | MEM_RAW))
#define ta_new_extra(type_t, extra, alignment) \
    ((type_t *)stack_malloc(sizeof(type_t) + (extra), (alignment), MEM_STACK))
#define ta_new_extra_raw(type_t, extra, alignment) \
    ((type_t *)stack_malloc(sizeof(type_t) + (extra), (alignment), MEM_STACK | MEM_RAW))
#define ta_new_extra_field(type_t, field, extra, alignment) \
    ta_new_extra(type_t, fieldsizeof(type_t, field[0]) * (extra), (alignment))
#define ta_new_extra_field_raw(type_t, field, extra, alignment) \
    ta_new_extra_raw(type_t, fieldsizeof(type_t, field[0]) * (extra), (alignment))

#define t_new(type_t, n)                ta_new(type_t, n, alignof(type_t))
#define t_new_raw(type_t, n)            ta_new_raw(type_t, n, alignof(type_t))
#define t_new_extra(type_t, extra)      ta_new_extra(type_t, extra, alignof(type_t))
#define t_new_extra_raw(type_t, extra)  ta_new_extra_raw(type_t, extra, alignof(type_t))
#define t_new_extra_field(type_t, field, extra)  \
    ta_new_extra_field(type_t, field, extra, alignof(type_t))
#define t_new_extra_field_raw(type_t, field, extra) \
    ta_new_extra_field_raw(type_t, field, extra, alignof(type_t))


#define t_dup(p, count)    mp_dup(&t_pool_g.funcs, p, count)
#define t_dupz(p, count)   mp_dupz(&t_pool_g.funcs, p, count)

#ifndef __cplusplus
/*
 * t_scope protects all the code after its use up to the end of the block
 * scope with an implicit t_push(), t_pop() pair.
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
    if (unlikely(*unused != t_pop()))
        e_panic("unbalanced t_stack");
#else
    t_pop();
#endif
}
#define t_scope__(n)  \
    const void *t_scope_##n __attribute__((unused,cleanup(t_scope_cleanup))) = t_push()
#else
/*
 * RAII scoped t_push/t_pop
 */
class TScope {
  public:
    inline TScope() { t_push(); };
    inline ~TScope() { t_pop(); };
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
