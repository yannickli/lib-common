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

#include "qtlsf.h"

static void *tlsf_malloc(mem_pool_t *_mp, size_t size, mem_flags_t flags);
static void *tlsf_realloc(mem_pool_t *_mp, void *ptr,
                          size_t oldsize, size_t newsize, mem_flags_t flags);
static void tlsf_free(mem_pool_t *_mp, void *ptr, mem_flags_t flags);

static mem_pool_t tlsf_pool = {
    .malloc  = &tlsf_malloc,
    .realloc = &tlsf_realloc,
    .free    = &tlsf_free,
};

static void *tlsf_malloc(mem_pool_t *_mp, size_t size, mem_flags_t flags)
{
    assert (_mp == &tlsf_pool);
    return libc_malloc(size, sizeof(void *), flags);
}

static void *tlsf_realloc(mem_pool_t *_mp, void *ptr,
                          size_t oldsize, size_t newsize, mem_flags_t flags)
{
    assert (_mp == &tlsf_pool);
    return libc_realloc(ptr, oldsize, newsize, sizeof(void *), flags);
}

static void tlsf_free(mem_pool_t *_mp, void *ptr, mem_flags_t flags)
{
    assert (_mp == &tlsf_pool);
    libc_free(ptr, flags);
}

mem_pool_t *tlsf_pool_new(size_t minpagesize)
{
    return &tlsf_pool;
}

void tlsf_pool_delete(mem_pool_t **_mpp)
{
    *_mpp = NULL;
}
