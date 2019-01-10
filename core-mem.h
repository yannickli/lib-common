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
#  error "you must include core.h instead"
#else
#define IS_LIB_COMMON_CORE_MEM_H

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
#define MEM_MMAP       force_cast(mem_flags_t, 4)
#define MEM_FLAGS_MASK force_cast(mem_flags_t, 0xff00)
#define MEM_RAW        force_cast(mem_flags_t, 1 << 8)
#define MEM_ERRORS_OK  force_cast(mem_flags_t, 1 << 9)

#if __GNUC_PREREQ(4, 3) || __has_attribute(error)
__attribute__((error("you cannot allocate that much memory")))
#endif
extern void __imalloc_too_large(void);

#if __GNUC_PREREQ(4, 3) || __has_attribute(error)
__attribute__((error("imalloc can only allocate from stack or libc")))
#endif
extern void __imalloc_cannot_do_this_pool(void);

#if __GNUC_PREREQ(4, 3) || __has_attribute(error)
__attribute__((error("reallocaing alloca()ed memory isn't possible")))
#endif
extern void __irealloc_cannot_handle_alloca(void);

void *stack_malloc(size_t size, size_t alignment,  mem_flags_t flags)
    __leaf __attribute__((warn_unused_result,malloc));
void *stack_realloc(void *mem, size_t oldsize, size_t size, size_t alignment,
                    mem_flags_t flags)
    __leaf __attribute__((warn_unused_result));

void *libc_malloc(size_t size, size_t alignment, mem_flags_t flags)
    __leaf __attribute__((warn_unused_result, malloc));
void *libc_realloc(void *mem, size_t oldsize, size_t size, size_t alignment,
                   mem_flags_t flags)
    __leaf __attribute__((warn_unused_result));
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
void *__imalloc(size_t size, size_t alignment, mem_flags_t flags)
    __leaf __attribute__((warn_unused_result, malloc));
void *__irealloc(void *mem, size_t oldsize, size_t size, size_t alignemnt,
                 mem_flags_t)
    __leaf __attribute__((warn_unused_result));
void __ifree(void *mem, mem_flags_t flags)
    __leaf;

__attribute__((malloc, warn_unused_result))
static ALWAYS_INLINE
void *imalloc(size_t size, size_t alignment, mem_flags_t flags)
{
    if (__builtin_constant_p(alignment) && alignment <= sizeof(void *)) {
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
                return libc_malloc(size, 0, flags);
              case MEM_STACK:
                return stack_malloc(size, 0, flags);
            }
        }
    }
    return __imalloc(size, alignment, flags);
}
#define imalloc(size, flags)  (imalloc)((size), 0, (flags))

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
irealloc(void *mem, size_t oldsize, size_t size, size_t alignment,
         mem_flags_t flags)
{
    if (__builtin_constant_p(alignment) && alignment <= sizeof(void *)) {
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
                return libc_realloc(mem, oldsize, size, 0, flags);
              case MEM_STACK:
                return stack_realloc(mem, oldsize, size, 0, flags);
              default:
                break;
            }

            if (__builtin_constant_p(oldsize < size)) {
                /* Test this condition here to allow for a better expansion
                 * of memset when the compiler can detect correct alignment
                 * and known size for reallocated part.
                 */
                mem = __irealloc(mem, oldsize, size, alignment, flags | MEM_RAW);
                if (!(flags & MEM_RAW) && oldsize < size)
                    memset((char *)mem + oldsize, 0, size - oldsize);
                return mem;
            }
        }
    }
    return __irealloc(mem, oldsize, size, alignment, flags);
}
#define irealloc(mem, oldsize, size, flags)  \
    (irealloc)((mem), (oldsize), (size), 0, (flags))

/**************************************************************************/
/* High Level memory APIs                                                 */
/**************************************************************************/

/* forward pointer for str-mem.h */
static inline void *memcpyz(void *dst, const void *src, size_t n);

__attribute__((malloc, warn_unused_result))
static inline char *mem_strdup(const char *src)
{
    char *res = strdup(src);
    if (unlikely(res == NULL))
        e_panic("out of memory");
    return res;
}

__attribute__((malloc, warn_unused_result))
static inline void *mem_dup(const void *src, size_t size, size_t alignment)
{
    return memcpy((imalloc)(size, alignment, MEM_RAW | MEM_LIBC), src, size);
}
#define mem_dup(src, size)  (mem_dup)((src), (size), 0)

__attribute__((malloc, warn_unused_result))
static inline void *p_dupz(const void *src, size_t len)
{
    void *res = imalloc(len + 1, MEM_RAW | MEM_LIBC);
    return memcpyz(res, src, len);
}


/* Aligned pointers allocation helpers */

#ifndef alignof
# define alignof(type)  __alignof__(type)
#endif

#define pa_new_raw(type, count, alignment)                                   \
    ((type *)(imalloc)(sizeof(type) * (count), (alignment), MEM_RAW | MEM_LIBC))
#define pa_new(type, count, alignment)                                       \
    ((type *)(imalloc)(sizeof(type) * (count), (alignment), MEM_LIBC))
#define pa_new_extra(type, size, alignment)                                  \
    ((type *)(imalloc)(sizeof(type) + (size), (alignment), MEM_LIBC))
#define pa_new_extra_field(type, field, size, alignment)                     \
    pa_new_extra(type, fieldsizeof(type, field[0]) * (size), (alignment))
#define pa_new_extra_raw(type, size, alignment)                              \
    ((type *)(imalloc)(sizeof(type) + (size), (alignment), MEM_RAW | MEM_LIBC))
#define pa_new_extra_field_raw(type, field, size, alignment)                 \
    pa_new_extra_raw(type, fieldsizeof(type, field[0]) * (size), (alignment))

#define pa_realloc(pp, count, alignment)                                     \
      ({                                                                     \
          typeof(**(pp)) **__ptr = (pp);                                     \
          *__ptr = (irealloc)(*__ptr, MEM_UNKNOWN, sizeof(**__ptr) * (count),\
                              (alignment), MEM_LIBC | MEM_RAW);              \
      })

#define pa_realloc0(pp, old, now, alignment)                                 \
    ({                                                                       \
        typeof(**(pp)) **__ptr = (pp);                                       \
        size_t sz = sizeof(**__ptr);                                         \
        *__ptr = (irealloc)(*__ptr, sz * (old), sz * (now), (alignment),     \
                            MEM_LIBC);                                       \
    })

#define pa_realloc_extra(pp, extra, alignment)                               \
    ({                                                                       \
        typeof(**(pp)) **__ptr = (pp);                                       \
        *__ptr = (irealloc)(*__ptr, MEM_UNKNOWN, sizeof(**__ptr) + (extra),  \
                            (alignment), MEM_LIBC | MEM_RAW);                \
    })

#define pa_realloc0_extra(pp, old_extra, new_extra, alignment)               \
    ({                                                                       \
        typeof(**(pp)) **__ptr = (pp);                                       \
        *__ptr = (irealloc)(*__ptr, sizeof(**__ptr) + (old_extra),           \
                            sizeof(**__ptr) + (new_extra), (alignment),      \
                            MEM_LIBC);                                       \
    })

#define pa_realloc_extra_field(pp, field, count, alignment)                  \
    pa_realloc_extra(pp, fieldsizeof(typeof(**pp), field[0]) * (count),      \
                     (alignment))

#define pa_realloc0_extra_field(pp, field, old_count, new_count, alignment)  \
    pa_realloc0_extra(pp, fieldsizeof(typeof(**(pp)), field[0]) * (old_count),\
                      fieldsizeof(typeof(**(pp)), field[0]) * (new_count),   \
                      (alignment))

/* Pointer allocations helpers */

#define p_new_raw(type, count)       pa_new_raw(type, count, alignof(type))
#define p_new(type, count)           pa_new(type, count, alignof(type))
#define p_new_extra(type, size)      pa_new_extra(type, size, alignof(type))
#define p_new_extra_raw(type, size)  pa_new_extra_raw(type, size, alignof(type))
#define p_new_extra_field(type, field, size)  \
    pa_new_extra_field(type, field, size, alignof(type))
#define p_new_extra_field_raw(type, field, size) \
    pa_new_extra_field_raw(type, field, size, alignof(type))

#define p_realloc(pp, count)        pa_realloc(pp, count, alignof(**(pp)))
#define p_realloc0(pp, old, now)    pa_realloc0(pp, old, now, alignof(**(pp)))
#define p_realloc_extra(pp, extra)  pa_realloc_extra(pp, extra, alignof(**(pp)))
#define p_realloc0_extra(pp, old_extra, new_extra)  \
    pa_realloc0_extra(pp, old_extra, new_extra, alignof(**(pp)))

#define p_realloc_extra_field(pp, field, count)  \
    pa_realloc_extra_field(pp, field, count, alignof(**(pp)))
#define p_realloc0_extra_field(pp, field, old_count, new_count)  \
    pa_realloc0_extra_field(pp, field, old_count, new_count, alignof(**(pp)))

#define p_dup(p, count)         (mem_dup)((p), sizeof(*(p)) * (count), alignof(*(p)))
#define p_strdup(p)             mem_strdup(p)

#define p_delete(pp) \
    ({                                    \
        typeof(**(pp)) **__ptr = (pp);    \
        ifree(*__ptr, MEM_LIBC);          \
        *__ptr = NULL;                    \
    })

#ifndef __cplusplus
static inline void (p_delete)(void **p)
{
    p_delete(p);
}
#endif

/* Generic helpers */

#define p_alloca(type, count)                                \
        ((type *)memset(alloca(sizeof(type) * (count)),      \
                        0, sizeof(type) * (count)))

#define p_clear(p, count)       ((void)memset((p), 0, sizeof(*(p)) * (count)))

#define p_alloc_nr(x) (((x) + 16) * 3 / 2)


/* Structure allocation helpers */

#define DO_INIT(type, prefix) \
    __attr_nonnull__((1))                                   \
    type *prefix##_init(type *var) {                        \
        p_clear(var, 1);                                    \
        return var;                                         \
    }
#define DO_NEW(type, prefix) \
    __attribute__((malloc)) type *prefix##_new(void) {      \
        return prefix##_init(p_new_raw(type, 1));           \
    }
#define DO_NEW0(type, prefix) \
    __attribute__((malloc)) type *prefix##_new(void) {      \
        return p_new(type, 1);                              \
    }

#define DO_WIPE(type, prefix) \
    __attr_nonnull__((1)) void prefix##_wipe(type *var) { }

#define DO_DELETE(type, prefix) \
    __attr_nonnull__((1))                                                   \
    void prefix##_delete(type **varp) {                                     \
        type * const var = *varp;                                           \
                                                                            \
        if (var) {                                                          \
            prefix##_wipe(var);                                             \
            assert (likely(var == *varp) && "pointer corruption detected"); \
            p_delete(varp);                                                 \
        }                                                                   \
    }


#define GENERIC_INIT(type, prefix)                          \
    static __unused__ inline DO_INIT(type, prefix)
#define GENERIC_NEW(type, prefix)                           \
    static __unused__ inline DO_NEW(type, prefix)
#define GENERIC_NEW_INIT(type, prefix)                      \
    GENERIC_INIT(type, prefix)                              \
    static __unused__ inline DO_NEW0(type, prefix)

#define GENERIC_WIPE(type, prefix)                          \
    static __unused__ inline DO_WIPE(type, prefix)
#define GENERIC_DELETE(type, prefix)                        \
    static __unused__ inline DO_DELETE(type, prefix)
#define GENERIC_WIPE_DELETE(type, prefix)                   \
    GENERIC_WIPE(type, prefix)                              \
    GENERIC_DELETE(type, prefix)

#define GENERIC_FUNCTIONS(type, prefix) \
    GENERIC_NEW_INIT(type, prefix)      \
    GENERIC_WIPE_DELETE(type, prefix)


/**************************************************************************/
/* Memory pools high level APIs                                           */
/**************************************************************************/

typedef struct mem_pool_t {
    void *(*malloc) (struct mem_pool_t *, size_t, mem_flags_t);
    void *(*realloc)(struct mem_pool_t *, void *, size_t, size_t, mem_flags_t);
    void  (*free)   (struct mem_pool_t *, void *, mem_flags_t);
} mem_pool_t;

__attribute__((malloc, warn_unused_result))
static inline void *memp_dup(mem_pool_t *mp, const void *src, size_t size)
{
    return memcpy(mp->malloc(mp, size, MEM_RAW), src, size);
}

__attribute__((malloc, warn_unused_result))
static inline void *mp_dupz(mem_pool_t *mp, const void *src, size_t len)
{
    void *res = mp->malloc(mp, len + 1, MEM_RAW);
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
#define DO_MP_NEW0(mp, type, prefix)                    \
    __attribute__((malloc)) type * prefix##_new(void) { \
        return mp_new(mp, type, 1);                     \
    }

#define DO_MP_DELETE(mp, type, prefix)   \
    void prefix##_delete(type **var) {   \
        if (*(var)) {                    \
            prefix##_wipe(*var);         \
            mp_delete(mp, var);          \
        }                                \
    }

#define GENERIC_MP_NEW(mp, type, prefix)      \
    static __unused__ inline DO_MP_NEW(mp, type, prefix)
#define GENERIC_MP_NEW_INIT(mp, type, prefix) \
    GENERIC_INIT(type, prefix)                \
    static __unused__ inline DO_MP_NEW0(mp, type, prefix)

#define GENERIC_MP_DELETE(mp, type, prefix)   \
    static __unused__ inline DO_MP_DELETE(mp, type, prefix)
#define GENERIC_MP_WIPE_DELETE(mp, type, prefix) \
    GENERIC_WIPE(type, prefix)                   \
    GENERIC_MP_DELETE(mp, type, prefix)

#define GENERIC_MP_FUNCTIONS(mp, type, prefix) \
    GENERIC_MP_NEW_INIT(mp, type, prefix)      \
    GENERIC_MP_WIPE_DELETE(mp, type, prefix)

/*----- core-mem-debug.c -----*/
extern mem_pool_t mem_pool_malloc;

/*----- core-mem-fifo.c -----*/
mem_pool_t *mem_fifo_pool_new(int page_size_hint)
    __leaf __attribute__((malloc));
void mem_fifo_pool_delete(mem_pool_t **poolp)
    __leaf;
void mem_fifo_pool_stats(mem_pool_t *mp, ssize_t *allocated, ssize_t *used)
    __leaf;


/*----- core-mem-ring.c -----*/
mem_pool_t *mem_ring_pool_new(int initialsize)
    __leaf __attribute__((malloc));
void mem_ring_pool_delete(mem_pool_t **)
    __leaf;

const void *mem_ring_newframe(mem_pool_t *) __leaf;
const void *mem_ring_getframe(mem_pool_t *) __leaf;
const void *mem_ring_seal(mem_pool_t *) __leaf;

const void *mem_ring_checkpoint(mem_pool_t *) __leaf;
void mem_ring_rewind(mem_pool_t *, const void *) __leaf;

void mem_ring_release(const void *) __leaf;
void mem_ring_dump(const mem_pool_t *) __leaf;

mem_pool_t *r_pool(void) __leaf;
void r_pool_destroy(void) __leaf;

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
