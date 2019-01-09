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

#include <pthread.h>
#include <dlfcn.h>
#include <lib-common/core.h>

static pthread_key_t key_g;

struct start_pair {
    void *(*fn)(void *);
    void *arg;
};

static void libcommon_thread_on_exit(void *unused)
{
    mem_pool_t *t = t_pool();
    mem_stack_pool_delete(&t);
}

static void *start_wrapper(void *data)
{
    struct start_pair *pair = data;
    void *(*fn)(void *) = pair->fn;
    void *arg = pair->arg;

    p_delete(&data);
    pthread_setspecific(key_g, (void *)1);
    return fn(arg);
}

__attribute__((visibility("default")))
int pthread_create(pthread_t *restrict thread,
                   const pthread_attr_t *restrict attr,
                   void *(*fn)(void *), void *restrict arg)
{
    static typeof(pthread_create) *real_pthread_create;
    struct start_pair *pair = p_new(struct start_pair, 1);
    int res;

    if (unlikely(!real_pthread_create)) {
        pthread_key_create(&key_g, &libcommon_thread_on_exit);
        real_pthread_create = dlsym(RTLD_NEXT, "pthread_create");
    }
    pair->fn  = fn;
    pair->arg = arg;
    res = (*real_pthread_create)(thread, attr, &start_wrapper, pair);
    if (res < 0)
        p_delete(&pair);
    return res;
}
