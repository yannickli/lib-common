/**************************************************************************/
/*                                                                        */
/*  Copyright (C) 2004-2006 INTERSEC                                      */
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

#include "macros.h"
#include "err_report.h"
#include "string_is.h"


#define check_enough_mem(mem)                                   \
    do {                                                        \
        if ((mem) == NULL) {                                    \
            e_fatal(FATAL_NOMEM, E_PREFIX("out of memory"));    \
        }                                                       \
    } while(0)

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
        memset((char *)mem + oldsize, 0, newsize - oldsize);
    }
    return mem;
}

static inline void *memdup(const void *src, ssize_t size)
{
    void *res = mem_alloc(size);
    memcpy(res, src, size);
    return res;
}

#define p_new_raw(type, count)  ((type *)mem_alloc(sizeof(type) * (count)))
#define p_new(type, count)      ((type *)mem_alloc0(sizeof(type) * (count)))
#define p_clear(p, count)       ((void)memset((p), 0, sizeof(*(p)) * (count)))
#  define p_dup(p, count)       memdup((p), sizeof(*(p)) * (count))

#ifdef __GNUC__

#  define p_delete(mem_pp)                          \
        ({                                          \
            typeof(**(mem_pp)) **ptr = (mem_pp);    \
            free(*ptr);                             \
            *ptr = NULL;                            \
        })

#else

#  define p_delete(mem_p)                           \
        do {                                        \
            void *__ptr = (mem_p);                  \
            free(*(void **)__ptr);                  \
            *(void **)__ptr = NULL;                 \
        } while (0)

#endif

/* OG: should find a better name */
#define p_renew(type, mem, oldcount, newcount) \
    ((type *)mem_realloc0((mem), (oldcount) * sizeof(type), \
                          (newcount) * sizeof(type)))

#define GENERIC_NEW(type, prefix) \
    static inline type *prefix##_new(void) {                \
        return prefix##_init(p_new_raw(type, 1));           \
    }
#define GENERIC_INIT(type, prefix) \
    static inline type *prefix##_init(type *var) {          \
        p_clear(var, 1);                                    \
        return var;                                         \
    }

#define GENERIC_DELETE(type, prefix) \
    static inline void prefix##_delete(type **var) {        \
        if (*var) {                                         \
            (prefix ## _wipe)(*var);                        \
            p_delete(var);                                  \
        }                                                   \
    }

#endif /* IS_LIB_COMMON_MEM_H */
