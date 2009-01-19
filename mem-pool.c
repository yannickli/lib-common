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

#include "mem-pool.h"

/*
 * XXX: This pool is used for debugging purpose, be very conservative in how
 *      we deal with memory: always memset it e.g.
 */

static void *mem_malloc_alloc0(mem_pool_t *mp, int size)
{
    return mem_alloc0(size);
}

static void mem_malloc_free(mem_pool_t *mp, void *mem)
{
    mem_free(mem);
}

static void mem_malloc_realloc(mem_pool_t *mp, void **memp, int size)
{
    mem_realloc(memp, size);
}

static mem_pool_t mem_malloc_pool_funcs = {
    &mem_malloc_alloc0,
    &mem_malloc_alloc0,
    &mem_malloc_realloc,
    &mem_malloc_free,
};

mem_pool_t *mem_malloc_pool_new(void)
{
    return &mem_malloc_pool_funcs;
}

void mem_malloc_pool_delete(mem_pool_t **poolp)
{
    *poolp = NULL;
}
