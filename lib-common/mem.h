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

#if (__BYTE_ORDER != __BIG_ENDIAN) && (__BYTE_ORDER != __LITTLE_ENDIAN)
#  error __BYTE_ORDER must be __BIG_ENDIAN or __LITTLE_ENDIAN
#endif

#define MEM_ALIGN_SIZE  8
#define MEM_ALIGN(size) \
    (((size) + MEM_ALIGN_SIZE - 1) & ~((ssize_t)MEM_ALIGN_SIZE - 1))

#if __BYTE_ORDER == __BIG_ENDIAN
#  define ntohl_const(x)    (x)
#  define ntohs_const(x)    (x)
#  define htonl_const(x)    (x)
#  define htons_const(x)    (x)
#else
#  define ntohl_const(x)    ((((x) >> 24) & 0x000000ff) | \
                             (((x) >>  8) & 0x0000ff00) | \
                             (((x) <<  8) & 0x00ff0000) | \
                             (((x) << 24) & 0xff000000))
#  define ntohs_const(x)    ((((x) >> 8) & 0x00ff) | (((x) << 8) & 0xff00))
#  define htonl_const(x)    ntohl_const(x)
#  define htons_const(x)    ntohs_const(x)
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

    if (size == 0) {
        return NULL;
    }

    mem = malloc(size);
    check_enough_mem(mem);
    return mem;
}

static inline void *mem_alloc0(ssize_t size)
{
    void *mem;

    if (size == 0) {
        return NULL;
    }

    mem = calloc(size, 1);
    check_enough_mem(mem);
    return mem;
}

static inline char *mem_strdup(const char *src) {
    char *res = strdup(src);
    //check_enough_mem(res);
    return res;
}

static inline void *mem_realloc(void *mem, ssize_t newsize)
{
    mem = realloc(mem, newsize);
    check_enough_mem(mem);
    return mem;
}

static inline void *mem_realloc0(void *mem, ssize_t oldsize, ssize_t newsize)
{
    mem = realloc(mem, newsize);
    check_enough_mem(mem);
    if (oldsize < newsize) {
        memset((byte *)mem + oldsize, 0, newsize - oldsize);
    }
    return mem;
}

static inline void mem_free(void *mem) {
    free(mem);
}

static inline void *mem_dup(const void *src, ssize_t size)
{
    void *res = mem_alloc(size);
    memcpy(res, src, size);
    return res;
}

static inline void *mem_dupstr(const void *src, ssize_t len)
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
#define p_dupstr(p, len)        mem_dupstr((p), (len))
#define p_strdup(p)             mem_strdup(p)

#ifdef __GNUC__

#  define p_delete(mem_pp)                          \
        ({                                          \
            typeof(**(mem_pp)) **ptr = (mem_pp);    \
            mem_free(*ptr);                         \
            *ptr = NULL;                            \
        })

#else

#  define p_delete(mem_p)                           \
        do {                                        \
            void *__ptr = (mem_p);                  \
            mem_free(*(void **)__ptr);              \
            *(void **)__ptr = NULL;                 \
        } while (0)

#endif

/* OG: RFE: should find a better name */
#define p_renew(type, mem, oldcount, newcount) \
    ((type *)mem_realloc0((mem), (oldcount) * sizeof(type), \
                          (newcount) * sizeof(type)))

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
