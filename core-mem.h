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

#if !defined(IS_LIB_COMMON_CORE_H) || defined(IS_LIB_COMMON_CORE_MEM_H)
#  error "you must include <lib-common/core.h> instead"
#else
#define IS_LIB_COMMON_CORE_MEM_H
#include "container-rbtree.h"

#ifndef __USE_GNU
static inline void *mempcpy(void *dst, const void *src, size_t n) {
    memcpy(dst, src, n);
    return (char *)dst + n;
}
#endif
static inline void *memcpyz(void *dst, const void *src, size_t n) {
    *(char *)mempcpy(dst, src, n) = '\0';
    return dst;
}
static inline void *mempcpyz(void *dst, const void *src, size_t n) {
    dst = mempcpy(dst, src, n);
    *(char *)dst = '\0';
    return (char *)dst + 1;
}

/**************************************************************************/
/* Intersec memory pools and APIs core stuff                              */
/**************************************************************************/

typedef unsigned __bitwise__ mem_flags_t;

/**
 * 1 << 30 is 1 Gig, we check that the user hasn't made any mistake with
 * roundings and so on. 1Go of malloc is silly anyways (the glibc limits it to
 * 512Mo anyways).
 */
#define MEM_ALLOC_MAX  (1 << 30)
#define MEM_UNKNOWN    ((size_t)-1)

/*
 * mem_flags_t modify the ialloc functions behaviours.
 *
 * %MEM_RAW::
 *     - imalloc does not zero returned memory if set.
 *     - irealloc does not zero [oldsize .. size[ if set.
 *       Note that irealloc implementations should not trust oldsize if
 *       MEM_RAW is set and oldsize is MEM_UNKNOWN.
 *
 * %MEM_ERRORS_OK::
 *     caller can deal with allocation errors from malloc or realloc.
 *
 * pool origins:
 *
 * %MEM_STATIC::
 *     memory comes from alloca() or a static buffer.
 *
 * %MEM_LIBC::
 *     This flag makes no sense for pools.  irealloc and ifree shall assume
 *     that this memory is allocated using the libc allocator (malloc or
 *     calloc).
 *
 * %MEM_STACK::
 *     This flags asks the allocations to be performed on the system default
 *     stack pool.
 *
 */
#define MEM_POOL_MASK  force_cast(mem_flags_t, 0x00ff)
#define MEM_STATIC     force_cast(mem_flags_t, 0)
#define MEM_OTHER      force_cast(mem_flags_t, 1)
#define MEM_LIBC       force_cast(mem_flags_t, 2)
#define MEM_STACK      force_cast(mem_flags_t, 3)
#define MEM_FLAGS_MASK force_cast(mem_flags_t, 0xff00)
#define MEM_RAW        force_cast(mem_flags_t, 1 << 8)
#define MEM_ERRORS_OK  force_cast(mem_flags_t, 1 << 9)

#if __GNUC_PREREQ(4, 3)
__attribute__((error("you cannot allocate that much memory")))
#endif
extern void __imalloc_too_large(void);

#if __GNUC_PREREQ(4, 3)
__attribute__((error("imalloc can only allocate from stack or libc")))
#endif
extern void __imalloc_cannot_do_this_pool(void);

#if __GNUC_PREREQ(4, 3)
__attribute__((error("reallocaing alloca()ed memory isn't possible")))
#endif
extern void __irealloc_cannot_handle_alloca(void);

__attribute__((warn_unused_result))
void *stack_malloc(size_t size, mem_flags_t flags);
__attribute__((warn_unused_result))
void *stack_realloc(void *mem, size_t oldsize, size_t size, mem_flags_t flags);

__attribute__((warn_unused_result))
void *libc_malloc(size_t size, mem_flags_t flags);
__attribute__((warn_unused_result))
void *libc_realloc(void *mem, size_t oldsize, size_t size, mem_flags_t flags);
static inline void libc_free(void *mem, mem_flags_t flags)
{
    free(mem);
}

/*
 * Intersec memory allocation wrappers allow people to write APIs that are not
 * aware that the memory was actually allocated from a pool.
 *
 * In order for ifree and irealloc to distinguish malloc-ed memory from
 * non malloc-ed pool memory, address ranges for pool memory must be
 * registered with the Intersec memory management module.
 * ifree and irealloc first look up if the address we're trying to
 * free/realloc is part of a pool, dispatching to the appropriate
 * handlers, either the pool handlers or standard libc realloc/free.
 *
 * Of course, this is somewhat expensive (not too much, but enough so
 * that you don't want this to happen in the fast path of some code),
 * so there is a flag to specify you don't want that search: %MEM_LIBC.
 * Note that if the flags passed to ialloc functions is known at
 * compile time and that MEM_LIBC is set, calls to
 * imalloc/irealloc/ifree will compile to straight calls to
 * malloc/realloc/free.
 */
__attribute__((warn_unused_result))
void *__imalloc(size_t size, mem_flags_t flags) __attribute__((malloc));
__attribute__((warn_unused_result))
void *__irealloc(void *mem, size_t oldsize, size_t size, mem_flags_t);
void __ifree(void *mem, mem_flags_t flags);

__attribute__((malloc, warn_unused_result))
static ALWAYS_INLINE void *imalloc(size_t size, mem_flags_t flags)
{
    if (__builtin_constant_p(size)) {
        if (size > MEM_ALLOC_MAX)
            __imalloc_too_large();
    }
    if (__builtin_constant_p(flags)) {
        switch (flags & MEM_POOL_MASK) {
          case MEM_STATIC:
          default:
            __imalloc_cannot_do_this_pool();
          case MEM_LIBC:
            return libc_malloc(size, flags);
          case MEM_STACK:
            return stack_malloc(size, flags);
        }
    }
    return __imalloc(size, flags);
}

static ALWAYS_INLINE void ifree(void *mem, mem_flags_t flags)
{
    if (__builtin_constant_p(mem)) {
        if (mem == NULL)
            return;
    }
    if (__builtin_constant_p(flags)) {
        switch (flags & MEM_POOL_MASK) {
          case MEM_STATIC:
          case MEM_STACK:
            return;
          case MEM_LIBC:
            libc_free(mem, flags);
            return;
          default:
            break;
        }
    }
    __ifree(mem, flags);
}

__attribute__((warn_unused_result))
static ALWAYS_INLINE void *
irealloc(void *mem, size_t oldsize, size_t size, mem_flags_t flags)
{
    if (__builtin_constant_p(size)) {
        if (size == 0) {
            ifree(mem, flags);
            return NULL;
        }
        if (size > MEM_ALLOC_MAX)
            __imalloc_too_large();
    }
    if (__builtin_constant_p(flags)) {
        switch (flags & MEM_POOL_MASK) {
          case MEM_STATIC:
            __irealloc_cannot_handle_alloca();
          case MEM_LIBC:
            return libc_realloc(mem, oldsize, size, flags);
          case MEM_STACK:
            return stack_realloc(mem, oldsize, size, flags);
          default:
            break;
        }

        if (__builtin_constant_p(oldsize < size)) {
            /* Test this condition here to allow for a better expansion
             * of memset when the compiler can detect correct alignment
             * and known size for reallocated part.
             */
            mem = __irealloc(mem, oldsize, size, flags | MEM_RAW);
            if (!(flags & MEM_RAW) && oldsize < size)
                memset((char *)mem + oldsize, 0, size - oldsize);
            return mem;
        }
    }
    return __irealloc(mem, oldsize, size, flags);
}


/**************************************************************************/
/* High Level memory APIs                                                 */
/**************************************************************************/

__attribute__((malloc, warn_unused_result))
static inline char *mem_strdup(const char *src)
{
    char *res = strdup(src);
    if (unlikely(res == NULL))
        e_panic("out of memory");
    return res;
}

__attribute__((malloc, warn_unused_result))
static inline void *mem_dup(const void *src, size_t size)
{
    return memcpy(imalloc(size, MEM_RAW | MEM_LIBC), src, size);
}

__attribute__((malloc, warn_unused_result))
static inline void *p_dupz(const void *src, size_t len)
{
    char *res = imalloc(len + 1, MEM_RAW | MEM_LIBC);
    return memcpyz(res, src, len);
}

#define p_alloca(type, count)                                \
        ((type *)memset(alloca(sizeof(type) * (count)),      \
                        0, sizeof(type) * (count)))

#define p_new_raw(type, count)  ((type *)imalloc(sizeof(type) * (count), MEM_RAW | MEM_LIBC))
#define p_new(type, count)      ((type *)imalloc(sizeof(type) * (count), MEM_LIBC))
#define p_new_extra(type, size) ((type *)imalloc(sizeof(type) + (size), MEM_LIBC))
#define p_new_extra_field(type, field, size) \
    p_new_extra(type, fieldsizeof(type, field[0]) * (size))
#define p_clear(p, count)       ((void)memset((p), 0, sizeof(*(p)) * (count)))
#define p_dup(p, count)         mem_dup((p), sizeof(*(p)) * (count))
#define p_strdup(p)             mem_strdup(p)

#define p_delete(pp) \
    ({                                    \
        typeof(**(pp)) **__ptr = (pp);    \
        ifree(*__ptr, MEM_LIBC);          \
        *__ptr = NULL;                    \
    })

#define p_realloc(pp, count) \
      ({                                                                     \
          typeof(**(pp)) **__ptr = (pp);                                     \
          *__ptr = irealloc(*__ptr, MEM_UNKNOWN, sizeof(**__ptr) * (count),  \
                            MEM_LIBC | MEM_RAW);                             \
      })

#define p_realloc0(pp, old, now) \
    ({                                                                       \
        typeof(**(pp)) **__ptr = (pp);                                       \
        size_t sz = sizeof(**__ptr);                                         \
        *__ptr = irealloc(*__ptr, sz * (old), sz * (now), MEM_LIBC);         \
    })

static inline void (p_delete)(void **p) {
    p_delete(p);
}

#define sizeof_type_is_one(t) \
    __builtin_choose_expr(__builtin_types_compatible_p(t, void *), 1,        \
                          sizeof(t) == 1)

#define memcpy_check_types(e1, e2) \
    STATIC_ASSERT(                                                           \
        __builtin_choose_expr(                                               \
            __builtin_choose_expr(                                           \
                __builtin_types_compatible_p(typeof(e2), void *),            \
                true, sizeof(*(e2)) == 1), true,                             \
            __builtin_types_compatible_p(typeof(*(e1)), typeof(*(e2)))))

#define p_move(to, src, n) \
    ({ memcpy_check_types(to, src); memmove(to, src, sizeof(*(to)) * (n)); })
#define p_copy(to, src, n) \
    ({ memcpy_check_types(to, src); memcpy(to, src, sizeof(*(to)) * (n)); })

#define p_move2(p, to, from, n) \
    ({ typeof(*(p)) *__p = (p); p_move(__p + (to), __p + (from), n); })
#define p_copy2(p, to, from, n) \
    ({ typeof(*(p)) __p = (p); p_copy(__p + (to), __p + (from), n); })

#define p_alloc_nr(x) (((x) + 16) * 3 / 2)

#define DO_INIT(type, prefix) \
    __attr_nonnull__((1))                                   \
    type * prefix##_init(type *var) {                       \
        p_clear(var, 1);                                    \
        return var;                                         \
    }
#define DO_NEW(type, prefix) \
    __attribute__((malloc)) type * prefix##_new(void) {     \
        return prefix##_init(p_new_raw(type, 1));           \
    }
#define DO_WIPE(type, prefix) \
    __attr_nonnull__((1))                                   \
    void prefix##_wipe(type *var __unused__) {}

#define DO_DELETE(type, prefix) \
    __attr_nonnull__((1))                                   \
    void prefix##_delete(type **var) {                      \
        if (*var) {                                         \
            prefix##_wipe(*var);                            \
            p_delete(var);                                  \
        }                                                   \
    }

#define GENERIC_INIT(type, prefix)    static inline DO_INIT(type, prefix)
#define GENERIC_NEW(type, prefix)     static inline DO_NEW(type, prefix)
#define GENERIC_WIPE(type, prefix)    static inline DO_WIPE(type, prefix)
#define GENERIC_DELETE(type, prefix)  static inline DO_DELETE(type, prefix)
#define GENERIC_FUNCTIONS(type, prefix) \
    GENERIC_INIT(type, prefix)    GENERIC_NEW(type, prefix) \
    GENERIC_WIPE(type, prefix)    GENERIC_DELETE(type, prefix)

/**************************************************************************/
/* Memory pools high level APIs                                           */
/**************************************************************************/

/*
 * Intersec memory allocation wrappers allow people writing pools that are
 * treated transparently through irealloc/ifree. This though requires that
 * those pools register the address ranges they work on.
 *
 * Of course, it's impractical for a general purpose malloc. This clearly
 * assume that your allocator work on a relatively small number of
 * address ranges.
 *
 * The pool is free to either embed the mem_blk_t structure into its block
 * structure, or to allocate new ones. It's not assumed that the mem_blk_t
 * actually reside inside of the block it describes.
 *
 * TODO: interestingly enough, the pool doesn't need to track its blocks,
 * mem_register does it already. So maybe there is something to work on
 * here...
 */
typedef struct mem_pool_t {
    void *(*malloc) (struct mem_pool_t *, size_t, mem_flags_t);
    void *(*realloc)(struct mem_pool_t *, void *, size_t, size_t, mem_flags_t);
    void  (*free)   (struct mem_pool_t *, void *, mem_flags_t);
} mem_pool_t;

typedef struct mem_blk_t {
    mem_pool_t *pool;
    rb_node_t   node;
    const void *start;
    size_t      size;
} mem_blk_t;

void mem_register(mem_blk_t *);
void mem_unregister(mem_blk_t *);
mem_blk_t *mem_blk_find(const void *addr);
void mem_for_each(mem_pool_t *, void (*)(mem_blk_t *, void *), void *);

__attribute__((malloc, warn_unused_result))
static inline void *memp_dup(mem_pool_t *mp, const void *src, size_t size)
{
    return memcpy(mp->malloc(mp, size, MEM_RAW), src, size);
}

__attribute__((malloc, warn_unused_result))
static inline void *mp_dupz(mem_pool_t *mp, const void *src, size_t len)
{
    char *res = mp->malloc(mp, len + 1, MEM_RAW);
    return memcpyz(res, src, len);
}
__attribute__((malloc, warn_unused_result))
static inline void *mp_strdup(mem_pool_t *mp, const char *src)
{
    return memp_dup(mp, src, strlen(src) + 1);
}


#define mp_new_raw(mp, type, count) \
        ((type *)(mp)->malloc((mp), sizeof(type) * (count), MEM_RAW))
#define mp_new(mp, type, count)     \
        ((type *)(mp)->malloc((mp), sizeof(type) * (count), 0))
#define mp_new_extra(mp, type, size) \
        ((type *)(mp)->malloc((mp), sizeof(type) + (size), 0))
#define mp_new_extra_field(mp, type, field, size) \
        mp_new_extra(mp, type, fieldsizeof(type, field[0]) * (size))
#define mp_dup(mp, p, count)        \
        memp_dup((mp), (p), sizeof(*(p)) * (count))

#ifdef __GNUC__
#  define mp_delete(mp, pp)                 \
        ({                                  \
            typeof(**(pp)) **__ptr = (pp);  \
            (mp)->free((mp), *__ptr, 0);    \
            *__ptr = NULL;                  \
        })
#else
#  define mp_delete(mp, pp)                         \
        do {                                        \
            void *__ptr = (pp);                     \
            (mp)->free((mp), *(void **)__ptr, 0);   \
            *(void **)__ptr = NULL;                 \
        } while (0)
#endif

#define DO_MP_NEW(mp, type, prefix)                     \
    __attribute__((malloc)) type * prefix##_new(void) { \
        return prefix##_init(mp_new_raw(mp, type, 1));  \
    }

#define DO_MP_DELETE(mp, type, prefix)   \
    void prefix##_delete(type **var) {   \
        if (*(var)) {                    \
            prefix##_wipe(*var);         \
            mp_delete(mp, var);          \
        }                                \
    }

#define GENERIC_MP_NEW(mp, type, prefix)      \
    static inline DO_MP_NEW(mp, type, prefix)
#define GENERIC_MP_DELETE(mp, type, prefix)   \
    static inline DO_MP_DELETE(mp, type, prefix)

/*----- core-mem-fifo.c -----*/
mem_pool_t *mem_fifo_pool_new(int page_size_hint);
void mem_fifo_pool_delete(mem_pool_t **poolp);
void mem_fifo_pool_stats(mem_pool_t *mp, ssize_t *allocated, ssize_t *used);


/*----- core-mem-stack.c -----*/
mem_pool_t *mem_stack_pool_new(int initialsize);
void mem_stack_pool_delete(mem_pool_t **);

const void *mem_stack_push(mem_pool_t *);
const void *mem_stack_pop(mem_pool_t *);
void mem_stack_rewind(mem_pool_t *, const void *);

mem_pool_t *t_pool(void);
void t_pool_destroy(void);

#define t_push()      mem_stack_push(t_pool())
#define t_pop()       mem_stack_pop(t_pool())
#define t_rewind(p)   mem_stack_rewind(t_pool(), p)

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
#define t_dup(p, count)    mp_dup(t_pool(), p, count)
#define t_dupz(p, count)   mp_dupz(t_pool(), p, count)


/*----- core-mem-ring.c -----*/
mem_pool_t *mem_ring_pool_new(int initialsize);
void mem_ring_pool_delete(mem_pool_t **);

const void *mem_ring_newframe(mem_pool_t *);
const void *mem_ring_getframe(mem_pool_t *);
const void *mem_ring_seal(mem_pool_t *);

const void *mem_ring_checkpoint(mem_pool_t *);
void mem_ring_rewind(mem_pool_t *, const void *);

void mem_ring_release(const void *);
void mem_ring_dump(const mem_pool_t *);

mem_pool_t *r_pool(void);
void r_pool_destroy(void);

#define r_newframe()                mem_ring_newframe(r_pool())
#define r_seal()                    mem_ring_seal(r_pool())
#define r_getframe()                mem_ring_getframe(r_pool())
#define r_release(ptr)              mem_ring_release(ptr)
#define r_checkpoint()              mem_ring_checkpoint(r_pool())
#define r_rewind(ptr)               mem_ring_rewind(r_pool(), ptr)

#define r_new(type_t, n)            mp_new(r_pool(), type_t, n)
#define r_new_raw(type_t, n)        mp_new_raw(r_pool(), type_t, n)
#define r_new_extra(type_t, extra)  mp_new_extra(r_pool(), type_t, extra)
#define r_dup(p, count)             mp_dup(r_pool(), p, count)
#define r_dupz(p, count)            mp_dupz(r_pool(), p, count)

#endif
