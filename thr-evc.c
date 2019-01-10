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

/* This code has been adapted from http://atomic-ptr-plus.sourceforge.net/

   Copyright 2004-2005 Joseph W. Seigh

   Permission to use, copy, modify and distribute this software and its
   documentation for any purpose and without fee is hereby granted, provided
   that the above copyright notice appear in all copies, that both the
   copyright notice and this permission notice appear in supporting
   documentation.  I make no representations about the suitability of this
   software for any purpose. It is provided "as is" without express or implied
   warranty.
*/

/* Note that this file is #include-able for speed in code that needs it */

#include "thr.h"
#include <sys/syscall.h>
#include <linux/futex.h>

#if !defined(__x86_64__) && !defined(__i386__)
#  error "this file assumes a strict memory model and is probably buggy on !x86"
#endif

#define futex_wait_private(futex, val, ts)  ({                               \
        typeof(futex) __futex = (futex);                                     \
        typeof(val)   __val   = (val);                                       \
        typeof(ts)    __ts    = (ts);                                        \
        int __res;                                                           \
                                                                             \
        for (;;) {                                                           \
            __res = syscall(SYS_futex, (unsigned long)__futex,               \
                            FUTEX_WAIT_PRIVATE, __val, (unsigned long)__ts, 0);\
            if (__res < 0 && errno == EINTR) {                               \
                continue;                                                    \
            }                                                                \
            break;                                                           \
        }                                                                    \
        __res;                                                               \
    })

#define futex_wake_private(futex, nwake)  ({                                 \
        typeof(futex) __futex = (futex);                                     \
        typeof(nwake) __nwake = (nwake);                                     \
        int __res;                                                           \
                                                                             \
        for (;;) {                                                           \
            __res = syscall(SYS_futex, (unsigned long)__futex,               \
                            FUTEX_WAKE_PRIVATE, __nwake, 0, 0);              \
            if (__res < 0 && errno == EINTR) {                               \
                continue;                                                    \
            }                                                                \
            break;                                                           \
        }                                                                    \
        __res;                                                               \
    })

void thr_ec_signal_n(thr_evc_t *ec, int count)
{
#if ULONG_MAX == UINT32_MAX
    /*
     * We don't know how slow things are between a thr_ec_get() and a
     * thr_ec_wait. Hence it's not safe to assume the "count" will never
     * rotate fully between the two.
     *
     * In 64bits mode, well, we have 64-bits atomic increments and all is
     * fine. In 32bits mode, we increment the high word manually every 2^24
     * low-word increment.
     *
     * This assumes that at least one of the 256 threads that will try to
     * perform the "count2" increment won't be stalled between deciding the
     * modulo is zero and the increment itself for an almost full cycle
     * (2^32 - 2^24) of the low word counter.
     */
    if (unlikely(atomic_add_and_get(&ec->count, 1) % (1U << 24) == 0))
        atomic_add(&ec->count2, 1);
#else
    atomic_add(&ec->key, 1);
#endif

    if (atomic_get_and_add(&ec->waiters, 0))
        futex_wake_private(&ec->count, count);
}

static void thr_ec_wait_cleanup(void *arg)
{
    thr_evc_t *ec = arg;
    atomic_sub(&ec->waiters, 1);
}

void thr_ec_timedwait(thr_evc_t *ec, uint64_t key, long timeout)
{
    int canceltype, res;

    mb();

    /*
     * XXX: futex only works on integers (32bits) so we have to check if the
     *      high 32bits word changed. We can do this in a racy way because we
     *      assume it's impossible for the low 32bits to cycle between this
     *      test and the call to futex_wait_private.
     */
    if (unlikely(
#if ULONG_MAX == UINT32_MAX
            (uint32_t)(key >> 32) != ec->count2
#else
            key != ec->key
#endif
    )) {
        pthread_testcancel();
        return;
    }

    atomic_add(&ec->waiters, 1);
    pthread_cleanup_push(&thr_ec_wait_cleanup, ec);
    pthread_setcanceltype(PTHREAD_CANCEL_ASYNCHRONOUS, &canceltype);

    if (timeout > 0) {
        struct timespec spec = {
            .tv_sec  = timeout / 1000,
            .tv_nsec = (timeout % 1000) * 1000000,
        };
        res = futex_wait_private(&ec->count, (uint32_t)key, &spec);
    } else {
        res = futex_wait_private(&ec->count, (uint32_t)key, NULL);
    }
    if (res == 0)
        sched_yield();

    /* XXX passing NULL to pthread_setcanceltype() breaks TSAN */
    pthread_setcanceltype(canceltype, &res);
    pthread_cleanup_pop(1);
}
