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
#  define __VALGRIND_PREREQ(x, y)  \
    defined(__VALGRIND_MAJOR__) && defined(__VALGRIND_MINOR__) && \
    (__VALGRIND_MAJOR__ << 16) + __VALGRIND_MINOR__ >= (((x) << 16) + (y))
#else
#  define __VALGRIND_PREREQ(x, y)                0
#  define VALGRIND_CREATE_MEMPOOL(...)           ((void)0)
#  define VALGRIND_DESTROY_MEMPOOL(...)          ((void)0)
#  define VALGRIND_MAKE_MEM_DEFINED(...)         0
#  define VALGRIND_MAKE_MEM_NOACCESS(...)        0
#  define VALGRIND_MAKE_MEM_UNDEFINED(...)       0
#  define VALGRIND_MEMPOOL_ALLOC(...)            ((void)0)
#  define VALGRIND_MEMPOOL_CHANGE(...)           ((void)0)
#  define VALGRIND_MEMPOOL_FREE(...)             ((void)0)
#endif
#include "core.h"

#if __GNUC_PREREQ(4, 6) && !__VALGRIND_PREREQ(3, 7)
#  pragma GCC diagnostic push
#  pragma GCC diagnostic ignored "-Wunused-but-set-variable"
#endif
static inline void VALGRIND_PROT_BLK(mem_blk_t *blk)
{
    (void)(VALGRIND_MAKE_MEM_NOACCESS(blk->start, blk->size));
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
#if __GNUC_PREREQ(4, 6) && !__VALGRIND_PREREQ(3, 7)
#  pragma GCC diagnostic pop
#endif

#endif
