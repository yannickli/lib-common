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

#if !defined(IS_LIB_COMMON_THR_H) || defined(IS_LIB_COMMON_THR_EVC_H)
#  error "you must include thr.h instead"
#else
#define IS_LIB_COMMON_THR_EVC_H

/*
 * An eventcount usage pattern is usually of the form:
 *
 * #thread 1
 *
 *     for (;;) {
 *         if (some_check) {
 *           again:
 *             do_stuff;
 *         } else {
 *             uint64_t key = thr_ec_get(ec);
 *
 *             if (some_check)
 *                 goto again;
 *             thr_ec_wait(ec, key);
 *         }
 *     }
 *
 *
 * #thread 2
 *
 *     make "some_check" become true;
 *     thr_ec_signal(ec)
 *
 *
 * This allows a pthread_cond_t kind of usage without the overhead of
 * acquiring a mutex, but this also means that the "thing" we want to wait
 * upon is also updated/checked without holding a mutex else it makes no sense
 * to use an eventcount.
 *
 * Note that this is usually a good idea to be sure that the structure which
 * uses the eventcount is aligned to the cache lines to avoid false sharing.
 * This isn't enforced as usually there is other data read/written in the same
 * fashion than the eventcount so we let the eventcount user share the
 * cacheline for this.
 *
 * thr_ec_get(ec):
 *    get the current eventcount "key". This performs a memory barrier which
 *    isn't cheap, so do it only when what you're protecting just failed (see
 *    above)
 *
 * thr_ec_wait(ec, key):
 *    Wait on the ec if its current key really is "key" (else it just quits),
 *    until we're signalled. This really works like futex(ec, FUTEX_WAIT, key)
 *    in a way (and indeed is a wrapper around futex(2)), so read futex(2) for
 *    more details.
 *
 *
 * thr_ec_signal_n(ec, n):
 *    wake up at most "n" threads currently waiting on the eventcount. This
 *    API is usually not called directly, see below.
 *
 * thr_ec_signal_relaxed(ec):
 * thr_ec_signal(ec):
 *    wake up at most one thread currently waiting on the eventcount.
 *    The relaxed version doesn't signal if there is no waiter seen (in a racy
 *    way) at signal time.
 *    The strict version always performs one atomic increment before it checks
 *    for waiters, but is non racy wrt proper use of thr_ec_wait.
 *
 * thr_ec_broadcast_relaxed(ec):
 * thr_ec_broadcast(ec):
 *    wake up all the threads currently waiting on the eventcount.
 *    See thr_ec_signal/thr_ec_signal_relaxed for the discussion about the
 *    relaxed version.
 */

typedef struct thr_evc_t {
    union {
        struct {
#if __BYTE_ORDER == __BIG_ENDIAN
            unsigned volatile count2;
            unsigned volatile count;
#else
            unsigned volatile count;
            unsigned volatile count2;
#endif
        };
        uint64_t key;
    };
    unsigned volatile waiters;
} thr_evc_t;

static ALWAYS_INLINE
uint64_t thr_ec_get(thr_evc_t *ec)
{
    mb();
    return ec->key;
}
void thr_ec_timedwait(thr_evc_t *ec, uint64_t key, long timeout);
#define thr_ec_wait(ec, key)  thr_ec_timedwait(ec, key, 0)
void thr_ec_signal_n(thr_evc_t *ec, int count) __leaf;

static ALWAYS_INLINE
void thr_ec_signal(thr_evc_t *ec)
{
    thr_ec_signal_n(ec, 1);
}
static ALWAYS_INLINE
void thr_ec_signal_relaxed(thr_evc_t *ec)
{
    if (ec->waiters)
        thr_ec_signal(ec);
}

static ALWAYS_INLINE
void thr_ec_broadcast(thr_evc_t *ec)
{
    thr_ec_signal_n(ec, INT_MAX);
}
static ALWAYS_INLINE
void thr_ec_broadcast_relaxed(thr_evc_t *ec)
{
    if (ec->waiters)
        thr_ec_broadcast(ec);
}

#endif
