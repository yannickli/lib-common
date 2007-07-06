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
    void *(*mem_realloc)(struct mem_pool *mp, void *mem, ssize_t size);
    void  (*mem_free)   (struct mem_pool *mp, void *mem);
} mem_pool;

mem_pool *mem_malloc_pool_new(void);
void mem_malloc_pool_delete(mem_pool **poolp);

static inline void *memp_dup(mem_pool *mp, const void *src, ssize_t size)
{
    void *res = mp->mem_alloc(mp, size);
    return memcpy(res, src, size);
}

static inline void *mp_dupstr(mem_pool *mp, const void *src, ssize_t len)
{
    char *res = mp->mem_alloc(mp, len + 1);
    memcpy(res, src, len);
    res[len] = '\0';
    return res;
}

#define mp_new_raw(mp, type, count) \
        ((type *)(mp)->mem_alloc((mp), sizeof(type) * (count)))
#define mp_new(mp, type, count)     \
        ((type *)(mp)->mem_alloc0((mp), sizeof(type) * (count)))
#define mp_dup(mp, p, count)        \
        memp_dup((mp), (p), sizeof(*(p)) * (count))

#ifdef __GNUC__

#  define mp_delete(mp, pp)                 \
        ({                                  \
            typeof(**(pp)) **__ptr = (pp);  \
            (mp)->mem_free((mp), *__ptr);   \
            *__ptr = NULL;                  \
        })

#  define mp_realloc(mp, pp, count)                                       \
        ({                                                                \
            typeof(**(pp)) **__ptr = (pp);                                \
            mp->mem_realloc(mp, (void*)__ptr, sizeof(**__ptr) * (count)); \
        })

#else

#  define mp_delete(mp, pp)                         \
        do {                                        \
            void *__ptr = (pp);                     \
            (mp)->mem_free((mp), *(void **)__ptr);  \
            *(void **)__ptr = NULL;                 \
        } while (0)

#  define mp_realloc(mp, pp, count)                 \
    (mp)->mem_realloc((mp), (void*)(pp), sizeof(**(pp)) * (count))

#endif

#define mp_realloc0(pp, old, now)                  \
    do {                                           \
        ssize_t __old = (old), __now = (now);      \
        mp_realloc(mp, pp, __now);                 \
        if (__now > __old) {                       \
            p_clear(*(pp) + __old, __now - __old); \
        }                                          \
    } while (0)

#define DO_MP_NEW(mp, type, prefix)                     \
    type * prefix##_new(void) {                         \
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

#endif /* IS_LIB_COMMON_MEM_FIFO_POOL_H */
