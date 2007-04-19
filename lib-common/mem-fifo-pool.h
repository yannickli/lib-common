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

#ifndef IS_LIB_COMMON_MEM_FIFO_POOL_H
#define IS_LIB_COMMON_MEM_FIFO_POOL_H

#include "mem-pool.h"

mem_pool *mem_fifo_pool_new(int page_size_hint);
void mem_fifo_pool_delete(mem_pool **poolp);

void mem_fifo_pool_stats(mem_pool *mp, ssize_t *allocated, ssize_t *used);

#endif /* IS_LIB_COMMON_MEM_FIFO_POOL_H */
