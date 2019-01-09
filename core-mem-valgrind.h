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

#ifndef IS_LIB_COMMON_CORE_MEM_VALGRIND_H
#define IS_LIB_COMMON_CORE_MEM_VALGRIND_H

#ifndef NDEBUG
#  include <valgrind/valgrind.h>
#  include <valgrind/memcheck.h>
#  ifndef VALGRIND_MEMPOOL_CHANGE
#    define VALGRIND_MEMPOOL_CHANGE(pool, oaddr, naddr, size) \
        do {                                                  \
             VALGRIND_MEMPOOL_FREE(pool, oaddr);              \
             VALGRIND_MEMPOOL_ALLOC(pool, naddr, size);       \
        } while (0)
#  endif
#else
#  define VALGRIND_CREATE_MEMPOOL(...)
#  define VALGRIND_DESTROY_MEMPOOL(...)
#  define VALGRIND_MAKE_MEM_DEFINED(...)
#  define VALGRIND_MAKE_MEM_NOACCESS(...)
#  define VALGRIND_MAKE_MEM_UNDEFINED(...)
#  define VALGRIND_MEMPOOL_ALLOC(...)
#  define VALGRIND_MEMPOOL_CHANGE(...)
#  define VALGRIND_MEMPOOL_FREE(...)
#endif
#include "core.h"

static inline void VALGRIND_PROT_BLK(mem_blk_t *blk)
{
    VALGRIND_MAKE_MEM_NOACCESS(blk->start, blk->size);
}

static inline void VALGRIND_REG_BLK(mem_blk_t *blk)
{
    VALGRIND_CREATE_MEMPOOL(blk, 0, true);
    VALGRIND_PROT_BLK(blk);
}

static inline void VALGRIND_UNREG_BLK(mem_blk_t *blk)
{
    VALGRIND_DESTROY_MEMPOOL(blk);
    VALGRIND_PROT_BLK(blk);
}

#endif
