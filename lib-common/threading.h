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

#ifndef IS_LIB_COMMON_THREADING_H
#define IS_LIB_COMMON_THREADING_H

#include <assert.h>
#include <pthread.h>

#if !defined(__MINGW) && !defined(__MINGW32__)
/* declare an anchor for the module/thing */
#define THREAD_ANCHOR(name)  pthread_t name
#define STATIC_THREAD_ANCHOR(name)  static THREAD_ANCHOR(name)

/* tie the anchor to a given thread */
#define THREAD_TIE(name)     (assert (!name), name = pthread_self())
/* untie the anchor */
#define THREAD_UNTIE(name)   (name = 0)
#define THREAD_CHECK(name)   (name == pthread_self())
/* use that in the begining of each function to tie to the anchor */
#define THREAD_ASSERT(name)  (assert (THREAD_CHECK(name)))

#else
#define THREAD_ANCHOR(name)
#define STATIC_THREAD_ANCHOR(name)
#define THREAD_TIE(name)
#define THREAD_UNTIE(name)
#define THREAD_ASSERT(name)
#endif

#endif /* IS_LIB_COMMON_THREADING_H */
