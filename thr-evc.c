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

#define futex_wait_private(futex, val, ts) \
        syscall(SYS_futex, (unsigned long)futex, FUTEX_WAIT_PRIVATE, val, (unsigned long)ts, 0)

#define futex_wake_private(futex, nwake) \
        syscall(SYS_futex, (unsigned long)futex, FUTEX_WAKE_PRIVATE, nwake, 0, 0)

void thr_ec_signal_n(thr_evc_t *ec, int count)
{
#if ULONG_MAX == UINT32_MAX
    int oldcount = ec->count;

    atomic_add(&ec->count, 1);
    if ((oldcount ^ ec->count) & 0x00010000)
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
     *      assume it's impossible for the low 32bits to overflow between this
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

    pthread_setcanceltype(canceltype, NULL);
    pthread_cleanup_pop(1);
}
