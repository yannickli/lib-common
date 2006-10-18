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
    void *(*mem_realloc)(struct mem_pool *mp, void *mem, ssize_t newsize);
    void  (*mem_free)   (struct mem_pool *mp, void *mem);
} mem_pool;

#endif /* IS_LIB_COMMON_MEM_FIFO_POOL_H */
