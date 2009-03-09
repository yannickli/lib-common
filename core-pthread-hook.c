/**************************************************************************/
/*                                                                        */
/*  Copyright (C) 2004-2009 INTERSEC SAS                                  */
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
    void *(*start_routine)(void *);
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
    pthread_setspecific(key_g, (void *)1);
    return pair->start_routine(pair->arg);
}

__attribute__((visibility("default")))
int pthread_create(pthread_t *restrict thread,
                   const pthread_attr_t *restrict attr,
                   void *(*start_routine)(void *), void *restrict arg)
{
    static typeof(pthread_create) *fn;
    struct start_pair pair = { start_routine, arg };

    if (unlikely(!fn)) {
        pthread_key_create(&key_g, &libcommon_thread_on_exit);
        fn = dlsym(RTLD_NEXT, "pthread_create");
    }
    return (*fn)(thread, attr, &start_wrapper, &pair);
}
