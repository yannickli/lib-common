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

#ifndef __USE_GNU
static inline void *mempcpy(void *dst, const void *src, size_t n) {
    memcpy(dst, src, n);
    return dst + n;
}
#endif
static inline void memcpyz(void *dst, const void *src, size_t n) {
    *(char *)mempcpy(dst, src, n) = '\0';
}

#define MEM_ALIGN_SIZE  8
#define MEM_ALIGN(size) \
    (((size) + MEM_ALIGN_SIZE - 1) & ~((ssize_t)MEM_ALIGN_SIZE - 1))

#define check_enough_mem(mem)                      \
    do {                                           \
        if (unlikely((mem) == NULL)) {             \
            e_panic(E_PREFIX("out of memory"));    \
        }                                          \
    } while (0)

static inline __attribute__((malloc)) void *mem_alloc(int size)
{
    void *mem;

    if (unlikely(size < 0))
        e_panic(E_PREFIX("invalid memory size %d"), size);
    if (unlikely(size == 0))
        return NULL;
    mem = malloc(size);
    check_enough_mem(mem);
    return mem;
}

static inline __attribute__((malloc)) void *mem_alloc0(int size)
{
    void *mem;

    if (unlikely(size < 0))
        e_panic(E_PREFIX("invalid memory size %d"), size);
    if (unlikely(size == 0))
        return NULL;
    mem = calloc(size, 1);
    check_enough_mem(mem);
    return mem;
}

/* OG: should pass old size */
static inline void mem_realloc(void **ptr, int newsize)
{
    if (unlikely(newsize < 0))
        e_panic(E_PREFIX("invalid memory size %d"), newsize);
    if (unlikely(newsize == 0)) {
        free(*ptr);
        *ptr = NULL;
    } else {
        *ptr = realloc(*ptr, newsize);
        check_enough_mem(*ptr);
    }
}

static inline void mem_realloc0(void **ptr, int oldsize, int newsize)
{
    mem_realloc(ptr, newsize);
    if (newsize > oldsize) {
        memset((byte *)(*ptr) + oldsize, 0, newsize - oldsize);
    }
}

static inline void mem_free(void *mem) {
    free(mem);
}

static inline char *mem_strdup(const char *src) {
    char *res = strdup(src);
    //check_enough_mem(res);
    return res;
}

static inline __attribute__((malloc)) void *mem_dup(const void *src, int size)
{
    return memcpy(mem_alloc(size), src, size);
}

static inline void mem_move(void *p, int to, int from, int len) {
    memmove((char *)p + to, (const char *)p + from, len);
}

static inline void mem_copy(void *p, int to, int from, int len) {
    memcpy((char *)p + to, (const char *)p + from, len);
}

static inline __attribute__((malloc)) void *p_dupz(const void *src, int len)
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

#else

#  define p_delete(pp)                           \
        do {                                     \
            void *__ptr = (pp);                  \
            mem_free(*(void **)__ptr);           \
            *(void **)__ptr = NULL;              \
        } while (0)

#  define p_realloc(pp, count)                      \
    mem_realloc((void*)(pp), sizeof(**(pp)) * (count))

#endif

#  define p_move(p, to, from, n)    \
    mem_move((p), sizeof(*(p)) * (to), sizeof(*(p)) * (from), sizeof(*(p)) * (n))
#  define p_copy(p, to, from, n)    \
    mem_copy((p), sizeof(*(p)) * (to), sizeof(*(p)) * (from), sizeof(*(p)) * (n))

/* OG: Size requested from the system should be computed in a
 * way that yields a small number of different sizes:
 * for (newsize = blob->size;
 *      newsize <= newlen;
 *      newsize = newsize * 3 / 2) {
 *      continue;
 * }
 */
#  define p_alloc_nr(x) (((x) + 16) * 3 / 2)

/* OG: should avoid multiple evaluation of macro arguments */
#  define p_allocgrow(pp, goalnb, allocnb)                  \
    do {                                                    \
        if ((goalnb) > *(allocnb)) {                        \
            if (p_alloc_nr(*(allocnb)) < (goalnb)) {        \
                *(allocnb) = (goalnb);                      \
            } else {                                        \
                *(allocnb) = p_alloc_nr(*(allocnb));        \
            }                                               \
            p_realloc(pp, *(allocnb));                      \
        }                                                   \
    } while (0)

#define p_realloc0(pp, old, now)                   \
    do {                                           \
        int __old = (old), __now = (now);          \
        p_realloc(pp, __now);                      \
        if (__now > __old) {                       \
            p_clear(*(pp) + __old, __now - __old); \
        }                                          \
    } while (0)

static inline void (p_delete)(void **p) {
    p_delete(p);
}

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
#define DO_RESET(type, prefix) \
    __attr_nonnull__((1))                                   \
    void prefix##_reset(type *var) {                        \
        prefix##_wipe(var);                                 \
        prefix##_init(var);                                 \
    }

#define GENERIC_INIT(type, prefix)    static inline DO_INIT(type, prefix)
#define GENERIC_NEW(type, prefix)     static inline DO_NEW(type, prefix)
#define GENERIC_WIPE(type, prefix)    static inline DO_WIPE(type, prefix)
#define GENERIC_DELETE(type, prefix)  static inline DO_DELETE(type, prefix)
#define GENERIC_RESET(type, prefix)   static inline DO_RESET(type, prefix)
#define GENERIC_FUNCTIONS(type, prefix) \
    GENERIC_INIT(type, prefix)    GENERIC_NEW(type, prefix) \
    GENERIC_WIPE(type, prefix)    GENERIC_DELETE(type, prefix)

#endif
