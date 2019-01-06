/**************************************************************************/
/*                                                                        */
/*  Copyright (C) 2004-2019 INTERSEC SA                                   */
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

#include "core.h"

/* XXX tlsf is deprecate. It was known to be broken and is now an alias to
 * malloc */
static inline mem_pool_t *tlsf_pool_new(size_t minpagesize)
{
    return &mem_pool_libc;
}

static inline void tlsf_pool_delete(mem_pool_t **mpp) {
    *mpp = NULL;
}

#endif
