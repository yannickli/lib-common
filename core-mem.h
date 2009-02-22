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
static inline void memcpyz(void *dst, const void *src, size_t n) {
    *(char *)mempcpy(dst, src, n) = '\0';
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

/*
 * mem_flags_t modify the ialloc functions behaviours.
 *
 * %MEM_RAW::
 *     - imalloc does not zeroes returned memory if set.
 *     - irealloc does not zeroes [oldsize .. size[ if set.
 *       Note that irealloc implementations should not trust oldsize if
 *       MEM_RAW is set.
 *
 * %MEM_LIBC::
 *     This flag makes no sense for pools.  irealloc and ifree shall assume
 *     that this memory is allocated using the libc allocator (malloc or
 *     calloc).
 *
 */
#define MEM_RAW        force_cast(mem_flags_t, 1 << 0)
#define MEM_LIBC       force_cast(mem_flags_t, 1 << 1)

__attribute__((error("you cannot allocate that much memory")))
extern void __imalloc_too_large(void);

/*
 * Intersec memory allocation wrappers allow people to write APIs that are not
 * aware that the memory was actually allocated from a pool.
 *
 * This requires that the pool (as in non malloc-ed memory) the memory comes
 * from has registered its address ranges into the Intersec memory management
 * module. If it had, then calling irealloc/ifree will look up if the address
 * we're trying to realloc/free is part of a pool, if yess it'll call the pool
 * operations, else it will fallback to realloc/free.
 *
 * Of course, this is expensive (not too much, but enough so that you don't
 * want this to happen in the fast path of some code), so there is a flag to
 * specify you don't want that search: %MEM_LIBC. Note that if the flags
 * passed to ialloc functions is known at compile time and that MEM_LIBC is
 * set, imalloc/irealloc/ifree will compile to a straight call to
 * malloc/realloc/free.
 */
void *__imalloc(size_t size, mem_flags_t flags) __attribute__((malloc));
void __irealloc(void **mem, size_t oldsize, size_t size, mem_flags_t);
void __ifree(void *mem, mem_flags_t flags);

__attribute__((malloc,always_inline)) static inline void *
imalloc(size_t size, mem_flags_t flags)
{
    void *res;
    if (__builtin_constant_p(size)) {
        if (size == 0)
            return NULL;
        if (size > MEM_ALLOC_MAX)
            __imalloc_too_large();
    }
    if (__builtin_constant_p(flags)) {
        if (flags & MEM_RAW) {
            res = malloc(size);
        } else {
            res = calloc(1, size);
        }
    } else {
        res = __imalloc(size, flags);
    }
    if (unlikely(res == NULL))
        e_panic("out of memory");
    return res;
}

__attribute__((always_inline)) static inline void
ifree(void *mem, mem_flags_t flags)
{
    if (__builtin_constant_p(flags)) {
        if (flags & MEM_LIBC) {
            free(mem);
            return;
        }
    }
    __ifree(mem, flags);
}

__attribute__((always_inline)) static inline void
irealloc(void **mem, size_t oldsize, size_t size, mem_flags_t flags)
{
    if (__builtin_constant_p(oldsize)) {
        if (oldsize == 0) {
            *mem = imalloc(size, flags);
            return;
        }
    }
    if (__builtin_constant_p(size)) {
        if (size == 0) {
            ifree(*mem, flags);
            *mem = NULL;
            return;
        }
        if (size > MEM_ALLOC_MAX)
            __imalloc_too_large();
    }
    if (__builtin_constant_p(flags)) {
        if (flags & MEM_LIBC) {
            char *res = realloc(*mem, size);
            if (unlikely(res == NULL))
                e_panic("out of memory");
            if (!(flags & MEM_RAW) && oldsize < size)
                memset(res + oldsize, 0, size - oldsize);
            *mem = res;
            return;
        }
    }
    __irealloc(mem, oldsize, size, flags);
}


/**************************************************************************/
/* Memory high level APIs                                                 */
/**************************************************************************/

#define mem_alloc(size)               imalloc(size, MEM_LIBC | MEM_RAW)
#define mem_alloc0(size)              imalloc(size, MEM_LIBC)
#define mem_realloc(mem, size)        irealloc(mem, 1, size, MEM_LIBC | MEM_RAW)
#define mem_realloc0(mem, old, size)  irealloc(mem, old, size, MEM_LIBC)
#define mem_free(mem)                 ifree(mem, MEM_LIBC);

static inline char *mem_strdup(const char *src) {
    char *res = strdup(src);
    if (unlikely(res == NULL))
        e_panic("out of memory");
    return res;
}

static inline __attribute__((malloc)) void *mem_dup(const void *src, size_t size)
{
    return memcpy(mem_alloc(size), src, size);
}

static inline void mem_move(void *p, size_t to, size_t from, size_t len) {
    memmove((char *)p + to, (const char *)p + from, len);
}

static inline void mem_copy(void *p, size_t to, size_t from, size_t len) {
    memcpy((char *)p + to, (const char *)p + from, len);
}

static inline __attribute__((malloc)) void *p_dupz(const void *src, size_t len)
{
    char *res = mem_alloc(len + 1);
    memcpyz(res, src, len);
    return res;
}

#define p_alloca(type, count)                                \
        ((type *)memset(alloca(sizeof(type) * (count)),      \
                        0, sizeof(type) * (count)))

#define p_new_raw(type, count)  ((type *)mem_alloc(sizeof(type) * (count)))
#define p_new(type, count)      ((type *)mem_alloc0(sizeof(type) * (count)))
#define p_new_extra(type, size) ((type *)mem_alloc0(sizeof(type) + (size)))
#define p_clear(p, count)       ((void)memset((p), 0, sizeof(*(p)) * (count)))
#define p_dup(p, count)         mem_dup((p), sizeof(*(p)) * (count))
#define p_strdup(p)             mem_strdup(p)

#ifdef __GNUC__

#  define p_delete(pp)                        \
        ({                                    \
            typeof(**(pp)) **__ptr = (pp);    \
            mem_free(*__ptr);                 \
            *__ptr = NULL;                    \
        })

#  define p_realloc(pp, count)                                     \
        ({                                                         \
            typeof(**(pp)) **__ptr = (pp);                         \
            mem_realloc((void*)__ptr, sizeof(**__ptr) * (count));  \
        })

#define p_realloc0(pp, old, now)                                   \
    ({                                                             \
        typeof(**(pp)) **__ptr = (pp);                             \
        size_t sz = sizeof(**__ptr);                               \
        mem_realloc0((void *)__ptr, sz * (old), sz * (now));       \
    })

#else

#  define p_delete(pp)                           \
        do {                                     \
            void *__ptr = (pp);                  \
            mem_free(*(void **)__ptr);           \
            *(void **)__ptr = NULL;              \
        } while (0)

#  define p_realloc(pp, count)                      \
    mem_realloc((void*)(pp), sizeof(**(pp)) * (count))
#  define p_realloc0(pp, old, now)                    \
    mem_realloc0((void *)(pp), sizeof(**(pp)) * (old), sizeof(**(pp)) * (now))

#endif
static inline void (p_delete)(void **p) {
    p_delete(p);
}


#define p_move(p, to, from, n)    \
    mem_move((p), sizeof(*(p)) * (to), sizeof(*(p)) * (from), sizeof(*(p)) * (n))
#define p_copy(p, to, from, n)    \
    mem_copy((p), sizeof(*(p)) * (to), sizeof(*(p)) * (from), sizeof(*(p)) * (n))

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
 * assume that you allocator work on relatively few address ranges.
 *
 * The pool is free to either embed the mem_blk_t structure into its block
 * structure, or to allocate new ones. It's not assumed that the mem_blk_t
 * actually is inside of the block it describes.
 *
 * TODO: interestingly enough, the pool doesn't need to track its blocks,
 * mem_register does it already. So maybe there is something to work on
 * here...
 */
typedef struct mem_pool_t {
    void *(*malloc) (struct mem_pool_t *, size_t, mem_flags_t);
    void  (*realloc)(struct mem_pool_t *, void **, size_t, size_t, mem_flags_t);
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

__attribute__((malloc))
static inline void *memp_dup(mem_pool_t *mp, const void *src, size_t size)
{
    return memcpy(mp->malloc(mp, size, MEM_RAW), src, size);
}

__attribute__((malloc))
static inline void *mp_dupz(mem_pool_t *mp, const void *src, size_t len)
{
    char *res = mp->malloc(mp, len + 1, MEM_RAW);
    memcpyz(res, src, len);
    return res;
}
__attribute__((malloc))
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

#endif
