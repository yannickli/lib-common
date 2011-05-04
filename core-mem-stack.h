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
    mem_blk_t blk;
    dlist_t   blk_list;
    byte      area[];
} mem_stack_blk_t;

typedef struct mem_stack_frame_t mem_stack_frame_t;

struct mem_stack_frame_t {
    mem_stack_frame_t *prev;
    mem_stack_blk_t   *blk;
    void              *pos;
    void              *last;
};

typedef struct mem_stack_pool_t {
    mem_blk_t            blk;
    dlist_t              blk_list;

    /* XXX: kludge: below this point we're the "blk" data */
    mem_stack_frame_t    base;
    mem_stack_frame_t   *stack;
    size_t               minsize;
    size_t               stacksize;
    size_t               nbpages;

    uint32_t             alloc_sz;
    uint32_t             alloc_nb;

    mem_pool_t           funcs;
} mem_stack_pool_t;

mem_pool_t       *mem_stack_pool_new(int initialsize);
mem_stack_pool_t *mem_stack_pool_init(mem_stack_pool_t *, int initialsize);
void              mem_stack_pool_wipe(mem_stack_pool_t *);
void              mem_stack_pool_delete(mem_pool_t **);

const void *mem_stack_push(mem_stack_pool_t *);
#ifndef NDEBUG
const void *mem_stack_pop(mem_stack_pool_t *);
#else
static ALWAYS_INLINE const void *mem_stack_pop(mem_stack_pool_t *sp)
{
    mem_stack_frame_t *frame = sp->stack;

    assert (frame->prev);
    sp->stack = frame->prev;
    return frame;
}
#endif

extern __thread mem_stack_pool_t t_pool_g;
#define t_pool()  (&t_pool_g.funcs)

#define t_push()      mem_stack_push(&t_pool_g)
#define t_pop()       mem_stack_pop(&t_pool_g)

/* Deprecated: do not use */
#define __t_pop_and_do(expr)    ({ t_pop(); expr; })
#define t_pop_and_return(expr)  __t_pop_and_do(return expr)
#define t_pop_and_break()       __t_pop_and_do(break)
#define t_pop_and_continue()    __t_pop_and_do(continue)
#define t_pop_and_goto(lbl)     __t_pop_and_do(goto lbl)

__attr_printf__(2, 3)
char *t_fmt(int *out, const char *fmt, ...);

#define t_new(type_t, n) \
    ((type_t *)imalloc((n) * sizeof(type_t), MEM_STACK))
#define t_new_raw(type_t, n)  \
    ((type_t *)imalloc((n) * sizeof(type_t), MEM_STACK | MEM_RAW))
#define t_new_extra(type_t, extra) \
    ((type_t *)imalloc(sizeof(type_t) + (extra), MEM_STACK))
#define t_new_extra_field(type_t, field, extra) \
    t_new_extra(type_t, fieldsizeof(type_t, field[0]) * (extra))
#define t_dup(p, count)    mp_dup(&t_pool_g.funcs, p, count)
#define t_dupz(p, count)   mp_dupz(&t_pool_g.funcs, p, count)

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
#define t_scope_(n)  t_scope__(n)
#define t_scope      t_scope_(__COUNTER__)

#ifdef __cplusplus
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
#endif

#endif
