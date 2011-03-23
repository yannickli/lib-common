/**************************************************************************/
/*                                                                        */
/*  Copyright (C) 2004-2011 INTERSEC SAS                                  */
/*                                                                        */
/*  Should you receive a copy of this source code, you must check you     */
/*  have a proper, written authorization of INTERSEC to hold it. If you   */
/*  don't have such an authorization, you must DELETE all source code     */
/*  files in your possession, and inform INTERSEC of the fact you obtain  */
/*  these files. Should you not comply to these terms, you can be         */
/*  prosecuted in the extent permitted by applicable law.                 */
/*                                                                        */
/**************************************************************************/

#ifndef IS_LIB_COMMON_THR_H
#define IS_LIB_COMMON_THR_H

#include <Block.h>
#include <pthread.h>
#include "container.h"

#include "thr-evc.h"
#include "thr-job.h"
#include "thr-spsc.h"
#include "thr-mpsc.h"

extern struct thr_hooks {
    dlist_t init_cbs;
    dlist_t exit_cbs;
} thr_hooks_g;

struct thr_ctor {
    dlist_t link;
    void   (*cb)(void);
};

/** \brief declare a function to be run at pthread_create() time.
 *
 * The function is run when pthread_create() is called, but is not run
 * automatically for the main thread.
 *
 * This code is active if and only if the code uses pthread_create() directly
 * (or through thr_initialize(), or pthread_force_use()).
 *
 * \param[in]  fn  name of the function to run.
 */
#define thread_init(fn) \
    static __attribute__((constructor)) void PT_##fn##_init(void) {  \
        static struct thr_ctor ctor = { .cb = (fn) };                \
        dlist_add_tail(&thr_hooks_g.init_cbs, &ctor.link);           \
    }

/** \brief declare a function to be run when a thread exits.
 *
 * The function is run when a thread exit, even when it is the main.
 *
 * The functions are always run for the main thread, even if pthreads are not in use.
 * Though, for the other threads, hooks are run if and only if the code uses
 * pthread_create() directly (or through thr_initialize(), or
 * pthread_force_use()).
 *
 * \param[in]  fn  name of the function to run.
 */
#define thread_exit(fn) \
    static __attribute__((constructor)) void PT_##fn##_exit(void) {  \
        static struct thr_ctor ctor = { .cb = (fn) };                \
        dlist_add_tail(&thr_hooks_g.exit_cbs, &ctor.link);           \
    }

/** \brief pulls the pthread hook module (forces a dependency upon pthreads).
 *
 * This function has no other side effects than to pull the intersec phtread
 * hooking mechanism. This call is required when building a public shared
 * library.
 */
void pthread_force_use(void);

#endif
