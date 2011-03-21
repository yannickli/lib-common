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

#include <dlfcn.h>
#include "thr.h"

struct start_pair {
    void *(*fn)(void *);
    void *arg;
};

static void *start_wrapper(void *data)
{
    struct start_pair *pair = data;
    void *(*fn)(void *) = pair->fn;
    void *arg = pair->arg, *res = NULL;

    intersec_thread_on_init();
    pthread_cleanup_push(intersec_thread_on_exit, NULL);
    p_delete(&data);
    res = fn(arg);
    pthread_cleanup_pop(1);
    return res;
}

__attribute__((visibility("default")))
int pthread_create(pthread_t *restrict thread,
                   const pthread_attr_t *restrict attr,
                   void *(*fn)(void *), void *restrict arg)
{
    static typeof(pthread_create) *real_pthread_create;
    struct start_pair *pair = p_new(struct start_pair, 1);
    int res;

    if (unlikely(!real_pthread_create))
        real_pthread_create = dlsym(RTLD_NEXT, "pthread_create");
    pair->fn  = fn;
    pair->arg = arg;
    res = (*real_pthread_create)(thread, attr, &start_wrapper, pair);
    if (res < 0)
        p_delete(&pair);
    return res;
}
