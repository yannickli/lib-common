/**************************************************************************/
/*                                                                        */
/*  Copyright (C) 2004-2007 INTERSEC SAS                                  */
/*                                                                        */
/*  Should you receive a copy of this source code, you must check you     */
/*  have a proper, written authorization of INTERSEC to hold it. If you   */
/*  don't have such an authorization, you must DELETE all source code     */
/*  files in your possession, and inform INTERSEC of the fact you obtain  */
/*  these files. Should you not comply to these terms, you can be         */
/*  prosecuted in the extent permitted by applicable law.                 */
/*                                                                        */
/**************************************************************************/

#ifndef IS_LIB_COMMON_MEM_H
#define IS_LIB_COMMON_MEM_H

#include <unistd.h>
#include <stdlib.h>
#include <malloc.h>
#include <endian.h>

#include "macros.h"
#include "err_report.h"
#include "string_is.h"

#define MEM_ALIGN_SIZE  8
#define MEM_ALIGN(size) \
    (((size) + MEM_ALIGN_SIZE - 1) & ~((ssize_t)MEM_ALIGN_SIZE - 1))

#ifdef __GLIBC__
#  include <byteswap.h>
#  define swab16(x)        __bswap_16(x)
#  define swab16_const(x)  __bswap_constant_16(x)
#  define swab32(x)        __bswap_32(x)
#  define swab32_const(x)  __bswap_constant_32(x)
#else

#define swab32_const(x) \
        ((((x) >> 24) & 0x000000ff) | \
         (((x) >>  8) & 0x0000ff00) | \
         (((x) <<  8) & 0x00ff0000) | \
         (((x) << 24) & 0xff000000))

#define swab16_const(x) \
        ((((x) >> 8) & 0x00ff) | (((x) << 8) & 0xff00))

#define swab32 swab32_const
#define swab16 swab16_const
#endif

#if __BYTE_ORDER == __BIG_ENDIAN
#  define ntohl_const(x)    (x)
#  define ntohs_const(x)    (x)
#  define htonl_const(x)    (x)
#  define htons_const(x)    (x)
#else
#  define ntohl_const(x)    swab32_const(x)
#  define ntohs_const(x)    swab16_const(x)
#  define htonl_const(x)    swab32_const(x)
#  define htons_const(x)    swab16_const(x)
#endif

#define check_enough_mem(mem)                                   \
    do {                                                        \
        if ((mem) == NULL) {                                    \
            e_fatal(FATAL_NOMEM, E_PREFIX("out of memory"));    \
        }                                                       \
    } while (0)

static inline void *mem_alloc(ssize_t size)
{
    void *mem;

    if (size <= 0)
        return NULL;

    mem = malloc(size);
    check_enough_mem(mem);
    return mem;
}

static inline void *mem_alloc0(ssize_t size)
{
    void *mem;

    if (size <= 0)
        return NULL;

    mem = calloc(size, 1);
    check_enough_mem(mem);
    return mem;
}

/* OG: should pass old size */
static inline void mem_realloc(void **ptr, ssize_t newsize)
{
    if (newsize <= 0) {
        free(*ptr);
        *ptr = NULL;
    } else {
        *ptr = realloc(*ptr, newsize);
        check_enough_mem(*ptr);
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

static inline void *mem_dup(const void *src, ssize_t size)
{
    void *res = mem_alloc(size);
    return memcpy(res, src, size);
}

static inline void mem_move(void *p, ssize_t to, ssize_t from, ssize_t len) {
    memmove((char *)p + to, (char *)p + from, len);
}

static inline void mem_copy(void *p, ssize_t to, ssize_t from, ssize_t len) {
    memcpy((char *)p + to, (char *)p + from, len);
}

static inline void *p_dupstr(const void *src, ssize_t len)
{
    char *res = mem_alloc(len + 1);
    memcpy(res, src, len);
    res[len] = '\0';
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
    mem_move((p), sizeof(*p) * (to), sizeof(*p) * (from), sizeof(*p) * (n))
#  define p_copy(p, to, from, n)    \
    mem_copy((p), sizeof(*p) * (to), sizeof(*p) * (from), sizeof(*p) * (n))
#  define p_alloc_nr(x) (((x) + 16) * 3 / 2)

#  define p_allocgrow(pp, goalnb, allocnb)                  \
    do {                                                    \
        if ((goalnb) > *(allocnb)) {                        \
            if (p_alloc_nr(goalnb) > *(allocnb)) {          \
                *(allocnb) = (goalnb);                      \
            } else {                                        \
                *(allocnb) = p_alloc_nr(goalnb);            \
            }                                               \
            p_realloc(pp, *(allocnb));                      \
        }                                                   \
    } while (0)

#define p_realloc0(pp, old, now)                   \
    do {                                           \
        ssize_t __old = (old), __now = (now);      \
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
    type * prefix##_new(void) {                             \
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

#endif /* IS_LIB_COMMON_MEM_H */
