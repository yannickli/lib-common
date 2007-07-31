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

#ifndef IS_LIB_COMMON_THREADING_H
#define IS_LIB_COMMON_THREADING_H

#include <assert.h>
#include <pthread.h>

#ifndef NDEBUG
#define THREAD_ANCHOR(name)  pthread_t name
#define THREAD_TIE(name)     (assert (!name), name = pthread_self())
#define THREAD_UNTIE(name)   (name = 0)
#define THREAD_ASSERT(name)  (assert (name == pthread_self()))
#else
#define THREAD_ANCHOR(name)
#define THREAD_TIE(name)
#define THREAD_UNTIE(name)
#define THREAD_ASSERT(name)
#endif

#endif /* IS_LIB_COMMON_THREADING_H */
