/**************************************************************************/
/*                                                                        */
/*  Copyright (C) INTERSEC SA                                             */
/*                                                                        */
/*  Should you receive a copy of this source code, you must check you     */
/*  have a proper, written authorization of INTERSEC to hold it. If you   */
/*  don't have such an authorization, you must DELETE all source code     */
/*  files in your possession, and inform INTERSEC of the fact you obtain  */
/*  these files. Should you not comply to these terms, you can be         */
/*  prosecuted in the extent permitted by applicable law.                 */
/*                                                                        */
/**************************************************************************/

#include "core.h"

static void *debug_malloc(struct mem_pool_t *m, size_t s, mem_flags_t f)
{
    return libc_malloc(s, 0, f);
}

static void *debug_realloc(struct mem_pool_t *m, void *p, size_t o, size_t n, mem_flags_t f)
{
    return libc_realloc(p, o, n, 0, f);
}

static void debug_free(struct mem_pool_t *m, void *p, mem_flags_t f)
{
    free(p);
}

mem_pool_t mem_pool_malloc = {
    .malloc  = &debug_malloc,
    .realloc = &debug_realloc,
    .free    = &debug_free,
};
