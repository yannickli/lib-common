/**************************************************************************/
/*                                                                        */
/*  Copyright (C) 2004-2016 INTERSEC SA                                   */
/*                                                                        */
/*  Should you receive a copy of this source code, you must check you     */
/*  have a proper, written authorization of INTERSEC to hold it. If you   */
/*  don't have such an authorization, you must DELETE all source code     */
/*  files in your possession, and inform INTERSEC of the fact you obtain  */
/*  these files. Should you not comply to these terms, you can be         */
/*  prosecuted in the extent permitted by applicable law.                 */
/*                                                                        */
/**************************************************************************/

#include <dlfcn.h>
#include "thr.h"

/*
 * This code gets pulled automatically as soon as pthread_create() is used
 * because we make our symbol strong and visible, and it then overrides the
 * libc one.
 *
 * When we build a shared library, we export our own pthread_create and it
 * also overrides the libc one, meaning that the library can use all intersec
 * APIs and be pthread compatible.
 */

static struct {
    pthread_once_t key_once;
    pthread_key_t  key;
} core_thread_g = {
#define _G  core_thread_g
    .key_once = PTHREAD_ONCE_INIT,
};

static void thr_hooks_at_exit(void *unused)
{
    pthread_setspecific(_G.key, NULL);
    dlist_for_each(it, &thr_hooks_g.exit_cbs) {
        (container_of(it, struct thr_ctor, link)->cb)();
    }
}

static void thr_hooks_atfork_in_child(void)
{
    _G.key_once = PTHREAD_ONCE_INIT;
}

static void thr_hooks_key_setup(void)
{
    pthread_key_create(&_G.key, thr_hooks_at_exit);
    pthread_atfork(NULL, NULL, thr_hooks_atfork_in_child);
}

void thr_hooks_at_init(void)
{
    pthread_once(&_G.key_once, thr_hooks_key_setup);
    if (pthread_getspecific(_G.key) == NULL) {
        pthread_setspecific(_G.key, MAP_FAILED);

        dlist_for_each(it, &thr_hooks_g.init_cbs) {
            (container_of(it, struct thr_ctor, link)->cb)();
        }
    }
}

static void *thr_hooks_wrapper(void *data)
{
    void *(*fn)(void *) = ((void **)data)[0];
    void   *arg         = ((void **)data)[1];

    p_delete(&data);
    thr_hooks_at_init();
    return fn(arg);
}

int thr_create(pthread_t *restrict thread,
               const pthread_attr_t *restrict attr,
               void *(*fn)(void *), void *restrict arg)
{
    static typeof(pthread_create) *real_pthread_create;
    void **pair = p_new(void *, 2);
    int res;

#ifndef __has_asan
    if (unlikely(!real_pthread_create))
        real_pthread_create = dlsym(RTLD_NEXT, "pthread_create");
#else
    if (unlikely(!real_pthread_create))
        real_pthread_create = dlsym(RTLD_DEFAULT, "pthread_create");
#endif
    pair[0] = fn;
    pair[1] = arg;
    res = (*real_pthread_create)(thread, attr, &thr_hooks_wrapper, pair);
    if (res < 0)
        p_delete(&pair);
    return res;
}

#ifndef __has_asan
__attribute__((visibility("default")))
int pthread_create(pthread_t *restrict thread,
                   const pthread_attr_t *restrict attr,
                   void *(*fn)(void *), void *restrict arg)
{
    return thr_create(thread, attr, fn, arg);
}
#endif

void pthread_force_use(void)
{
}
