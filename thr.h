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

#ifndef IS_LIB_COMMON_THR_H
#define IS_LIB_COMMON_THR_H

#include <pthread.h>
#include "core.h"
#include "container-dlist.h"

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

/** \brief declare a function to be run when a thread starts and exits.
 *
 * The init function is run when a thread inits, but not for the main thread,
 * it's up to the programmer to be sure it's done or not needed for this
 * thread.
 *
 * The exit function is run when a thread exit, even when it is the main.
 *
 * If pthreads are in use, and that the host program uses #pthread_create (or
 * #thr_initialize or #pthread_force_use) then this system is active, else
 * hooks are not run for threads.
 *
 * The exit hooks are always run for the main thread, independently from
 * pthreads and this system beeing active or not.
 *
 * \param[in]  init  name of the function to run at thread init.
 * \param[in]  init  name of the function to run at thread exit.
 */
#define thr_hooks(init, exit) \
    static __attribute__((constructor)) void PT_##fn##_exit(void) {          \
        __builtin_choose_expr(__builtin_constant_p(init), (void)0, ({        \
            static struct thr_ctor ctor = { .cb = (init) };                  \
            if (ctor.cb) {                                                   \
                dlist_add_tail(&thr_hooks_g.init_cbs, &ctor.link);           \
            }                                                                \
        }));                                                                 \
        __builtin_choose_expr(__builtin_constant_p(exit), (void)0, ({        \
            static struct thr_ctor ctor = { .cb = (exit) };                  \
            if (ctor.cb) {                                                   \
                dlist_add(&thr_hooks_g.exit_cbs, &ctor.link);                \
            }                                                                \
        }));                                                                 \
    }

void thr_hooks_register(void);
void thr_attach(void); /* previously thr_hooks_at_init */
void thr_detach(void);

/** \brief pulls the pthread hook module (forces a dependency upon pthreads).
 *
 * This function has no other side effects than to pull the intersec phtread
 * hooking mechanism. This call is required when building a public shared
 * library.
 */
void pthread_force_use(void);


#ifndef __cplusplus
int thr_create(pthread_t *restrict thread,
               const pthread_attr_t *restrict attr,
               void *(*fn)(void *), void *restrict arg);
#endif

MODULE_DECLARE(thr_hooks);
MODULE_DECLARE(thr);

#endif
