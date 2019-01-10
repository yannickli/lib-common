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

#ifndef IS_LIB_COMMON_QTLSF_H
#define IS_LIB_COMMON_QTLSF_H

#include <setjmp.h>
#include "core.h"

enum {
    TLSF_OOM_PUBLIC_MASK   = 0x0000ffff,
    TLSF_OOM_ABORT         = (1 << 0),

    TLSF_OOM_INTERNAL_MASK = 0xffff0000,
    TLSF_OOM_LONGJMP       = (1 << 16),
    TLSF_LIMIT_MEMORY      = (1 << 17),
};

void  *tlsf_malloc(mem_pool_t *mp, size_t size, mem_flags_t flags)
    __attribute__((warn_unused_result, malloc));
void  *tlsf_realloc(mem_pool_t *mp, void *ptr,
                    size_t oldsize, size_t newsize, mem_flags_t flags)
    __attribute__((warn_unused_result));
void   tlsf_free(mem_pool_t *mp, void *ptr, mem_flags_t flags);

size_t  tlsf_sizeof(const void *ptr);
size_t  tlsf_extrasizeof(const void *ptr);
ssize_t tlsf_setsizeof(void *ptr, size_t asked);

mem_pool_t *tlsf_pool_new(size_t minpagesize);
void tlsf_pool_reset(mem_pool_t *);
void tlsf_pool_delete(mem_pool_t **);

ssize_t tlsf_pool_add_arena(mem_pool_t *mp, void *ptr, size_t size);
void    tlsf_pool_set_jmpbuf(mem_pool_t *mp, jmp_buf *jbuf, int jmpval);
void    tlsf_pool_limit_memory(mem_pool_t *mp,  size_t memory_limit);
int     tlsf_pool_set_options(mem_pool_t *mp, int options);
int     tlsf_pool_rst_options(mem_pool_t *mp, int options);

#endif
