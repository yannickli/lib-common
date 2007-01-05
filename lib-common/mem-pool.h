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

#ifndef IS_LIB_COMMON_MEM_POOL_H
#define IS_LIB_COMMON_MEM_POOL_H

#include <unistd.h>

#include "macros.h"

typedef struct mem_pool {
    void *(*mem_alloc)  (struct mem_pool *mp, ssize_t size);
    void *(*mem_alloc0) (struct mem_pool *mp, ssize_t size);
    /* TODO: deal with realloc at some point */
    void  (*mem_free)   (struct mem_pool *mp, void *mem);
} mem_pool;

mem_pool *mem_malloc_pool_new(void);
void mem_malloc_pool_delete(mem_pool **poolp);

#define mp_new_raw(mp, type, count) \
        ((type *)(mp)->mem_alloc((mp), sizeof(type) * (count)))
#define mp_new(mp, type, count)     \
        ((type *)(mp)->mem_alloc0((mp), sizeof(type) * (count)))

#ifdef __GNUC__

#  define mp_delete(mp, mem_pp)                     \
        ({                                          \
            typeof(**(mem_pp)) **ptr = (mem_pp);    \
            (mp)->mem_free((mp), *ptr);             \
            *ptr = NULL;                            \
        })

#else

#  define mp_delete(mp, mem_p)                      \
        do {                                        \
            void *__ptr = (mem_p);                  \
            (mp)->mem_free((mp), *(void **)__ptr);  \
            *(void **)__ptr = NULL;                 \
        } while (0)

#endif

/* OG: why not static inline for consistency with mem.h ? */
#define MP_GENERIC_NEW(pool, type, prefix) \
    type * prefix##_new(void) {                             \
        return prefix##_init(mp_new_raw(mp, type, 1));      \
    }

#define MP_GENERIC_DELETE(mp, type, prefix) \
    void prefix##_delete(type **var) {                      \
        if (*var) {                                         \
            prefix##_wipe(*var);                            \
            mp_delete(mp, var);                             \
        }                                                   \
    }

#endif /* IS_LIB_COMMON_MEM_FIFO_POOL_H */
