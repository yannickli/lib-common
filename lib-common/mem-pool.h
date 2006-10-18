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

#ifndef IS_LIB_COMMON_MEM_POOL_H
#define IS_LIB_COMMON_MEM_POOL_H

#include "macros.h"

typedef struct mem_pool {
    void *(*mem_alloc)  (struct mem_pool *mp, ssize_t size);
    void *(*mem_alloc0) (struct mem_pool *mp, ssize_t size);
    void  (*mem_free)   (struct mem_pool *mp, void *mem);
} mem_pool;

#define mp_new_raw(mp, type, count)  ((type *)(mp)->mem_alloc((mp), sizeof(type) * (count)))
#define mp_new(mp, type, count)      ((type *)(mp)->mem_alloc0((mp), sizeof(type) * (count)))
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


#endif /* IS_LIB_COMMON_MEM_FIFO_POOL_H */
