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
#  define __VALGRIND_PREREQ(x, y)  \
    defined(__VALGRIND_MAJOR__) && defined(__VALGRIND_MINOR__) && \
    (__VALGRIND_MAJOR__ << 16) + __VALGRIND_MINOR__ >= (((x) << 16) + (y))
#else
#  define __VALGRIND_PREREQ(x, y)                0
#  define VALGRIND_CREATE_MEMPOOL(...)           ((void)0)
#  define VALGRIND_DESTROY_MEMPOOL(...)          ((void)0)
#  define VALGRIND_MAKE_MEM_DEFINED(...)         ((void)0)
#  define VALGRIND_MAKE_MEM_NOACCESS(...)        ((void)0)
#  define VALGRIND_MAKE_MEM_UNDEFINED(...)       ((void)0)
#  define VALGRIND_MALLOCLIKE_BLOCK(...)         ((void)0)
#  define VALGRIND_FREELIKE_BLOCK(...)           ((void)0)
#  define RUNNING_ON_VALGRIND                    false
#endif
#include "core.h"

#endif
