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

#include "mem.h"
#include "mem-pool.h"

/*
 * XXX: This pool is used for debugging purpose, be very conservative in how
 *      we deal with memory: always memset it e.g.
 */

static void *mem_malloc_alloc0(mem_pool *mp __unused__, ssize_t size)
{
    return mem_alloc0(size);
}

static void mem_malloc_free(mem_pool *mp __unused__, void *mem)
{
    mem_free(mem);
}

static mem_pool mem_malloc_pool_funcs = {
    &mem_malloc_alloc0,
    &mem_malloc_alloc0,
    &mem_malloc_free
};

mem_pool *mem_malloc_pool_new(void)
{
    return &mem_malloc_pool_funcs;
}

void mem_malloc_pool_delete(mem_pool **poolp)
{
    *poolp = NULL;
}
