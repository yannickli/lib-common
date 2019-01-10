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

#include <valgrind/valgrind.h>
#include <lib-common/arith.h>
#include <lib-common/time.h>
#include <lib-common/container.h>
#include <lib-common/el.h>
#include "thr.h"

#if !defined(__x86_64__) && !defined(__i386__)
#  error "this file assumes a strict memory model and is probably buggy on !x86"
#endif

#define CACHE_LINE_SIZE   64

typedef struct thr_qnode_t thr_qnode_t;
struct thr_qnode_t {
    mpsc_node_t  qnode;
    thr_job_t   *job;
    thr_syn_t   *syn;
} __attribute__((aligned(CACHE_LINE_SIZE)));

struct thr_queue_t {
    thr_job_t             run;
    thr_job_t             destroy;
    mpsc_queue_t          q;
    unsigned volatile     closing;
} __attribute__((aligned(CACHE_LINE_SIZE)));

typedef struct thr_info_t {
    /** top of the deque.
     * this variable is accessed through shared_read, and modified through
     * atomic_bool_cas which ensure load/store consistency. Hence no barrier
     * is needed to read it.
     */
    unsigned   top;
    /** bottom of the deque.
     * this variable is only modified by the current thread, hence it doesn't
     * need read barriers to read it. Though other threads do need a read
     * barrier before acces, and the owner of the deque must publish new
     * values through a write barrier.
     */
    unsigned   bot;
    unsigned   alive;
    pthread_t  thr;
    char       padding_0[CACHE_LINE_SIZE];

    struct deque_entry {
        thr_job_t *job;
        thr_syn_t *syn;
    } q[THR_JOB_MAX];
    char       padding_1[CACHE_LINE_SIZE];

#define NCACHE_MAX    1024
    size_t       ncache_sz;
    thr_qnode_t  ncache;

#ifndef NDEBUG
    struct thr_acc {
        uint64_t time;
        unsigned jobs_queued;
        unsigned jobs_run;
        unsigned ec_gets;
        unsigned ec_waits;
    } acc;
#endif
} thr_info_t;

static struct {
    unsigned volatile stopping;
    thr_evc_t         ec;
    pthread_barrier_t start_bar;

    thr_info_t       *threads;
    el_t              before;
    thr_queue_t       main_queue;
    uint64_t          reset_time;
    proctimer_t       st;
} thr_job_g;
#define _G  thr_job_g

static __thread int self_tid_g = (-1);
static __thread thr_info_t *self_g;

size_t thr_parallelism_g;
thr_queue_t *const thr_queue_main_g = &_G.main_queue;
static thr_evc_t *thr0_cur_ec_g;

/* Tracing {{{ */
#ifndef NDEBUG

void thr_acc_reset(void)
{
    for (size_t i = 0; i < thr_parallelism_g; i++) {
        p_clear(&_G.threads[i].acc, 1);
    }
    _G.reset_time = hardclock();
    proctimer_start(&_G.st);
}

static size_t int_width(uint64_t i)
{
    int w = 1;

    for (; i > 1; i /= 10)
        w++;
    return w;
}

void thr_acc_set_affinity(size_t offs)
{
    for (size_t i = 0; i < thr_parallelism_g; i++) {
        cpu_set_t set;

        CPU_ZERO(&set);
        CPU_SET((i + offs) % thr_parallelism_g, &set);
        if (pthread_setaffinity_np(_G.threads[i].thr, sizeof(set), &set))
            e_panic("cannot set affinity");
    }
}

void thr_acc_trace(int lvl, const char *fmt, ...)
{
    uint64_t avg, wall = hardclock() - _G.reset_time;
    unsigned waste, speedup;

    struct thr_acc total = { .time = 0, };
    struct thr_acc width = { .time = 0, };
    va_list  ap;
    SB_8k(sb);

    proctimer_stop(&_G.st);

    va_start(ap, fmt);
    sb_addvf(&sb, fmt, ap);
    va_end(ap);

    width.time = int_width(wall / 1000000);
    for (size_t i = 0; i < thr_parallelism_g; i++) {
        struct thr_acc *acc = &_G.threads[i].acc;

        total.time        += acc->time;
        total.jobs_queued += acc->jobs_queued;
        total.jobs_run    += acc->jobs_run;
        total.ec_gets     += acc->ec_gets;
        total.ec_waits    += acc->ec_waits;

        width.time         = MAX(width.time,        int_width(acc->time / 1000000));
        width.jobs_queued  = MAX(width.jobs_queued, int_width(acc->jobs_queued));
        width.jobs_run     = MAX(width.jobs_run,    int_width(acc->jobs_run));
        width.ec_gets      = MAX(width.ec_gets,     int_width(acc->ec_gets));
        width.ec_waits     = MAX(width.ec_waits,    int_width(acc->ec_waits));
    }

    e_trace(lvl, "----- %*pM", sb.len, sb.data);
    sb_wipe(&sb);

    if (total.jobs_run == 0) {
        e_trace(lvl, "----- No jobs since last reset");
        return;
    }

#define TIME_FMT_ARG(t)   (int)width.time, (int)((t) / 1000000)

    for (size_t i = 0; i < thr_parallelism_g; i++) {
        struct thr_acc *acc = &_G.threads[i].acc;

        e_trace(lvl, " %2zd: %*uM, %*u queued, %*u run, "
                "%*u gets, %*u waits", i, TIME_FMT_ARG(acc->time),
                width.jobs_queued, acc->jobs_queued,
                width.jobs_run,    acc->jobs_run,
                width.ec_gets,     acc->ec_gets,
                width.ec_waits,    acc->ec_waits);
    }
    e_trace(lvl, "wall %*uM, %*u queued, %*u run, "
            "%*u gets, %*u waits", TIME_FMT_ARG(wall),
            width.jobs_queued, total.jobs_queued,
            width.jobs_run,    total.jobs_run,
            width.ec_gets,     total.ec_gets,
            width.ec_waits,    total.ec_waits);
    avg     = (uint64_t)(total.time / thr_parallelism_g);
    waste   = (uint64_t)(wall - avg) * 10000 / wall;
    speedup = total.time * 100 / wall;
    e_trace(lvl, "avg  %*uM (%d.%02d%% waste, %d.%02dx speedup)",
            TIME_FMT_ARG(avg), waste / 100, waste % 100,
            speedup / 100, speedup % 100);
    e_trace(lvl, "job  %*jd cycles in avg",
            (int)width.time, total.time / total.jobs_run);
    e_trace(lvl, "cost %*jd cycles of overhead per job", (int)width.time,
            (wall * thr_parallelism_g - total.time) / total.jobs_run);
    e_trace(lvl, "     %s", proctimer_report(&_G.st, NULL));
    sb_wipe(&sb);
#undef TIME_FMT_ARG
}

#endif
/* }}} */
/* atomic deque {{{ */

static bool job_run(thr_job_t *job, thr_syn_t *syn)
{
#ifndef NDEBUG
    unsigned long start = hardclock();

    self_g->acc.jobs_run++;
#endif

    if ((uintptr_t)job & 3) {
#if 0
        block_t blk = (block_t)((uintptr_t)job & ~(uintptr_t)3);

        blk();
        if ((uintptr_t)job & 1)
            Block_release(blk);
#endif
    } else {
        (*job->run)(job, syn);
    }
#ifndef NDEBUG
    self_g->acc.time += (hardclock() - start);
#endif
    if (syn)
        thr_syn__job_done(syn);
    return true;
}

void thr_syn_schedule(thr_syn_t *syn, thr_job_t *job)
{
    unsigned bot, top;
    struct deque_entry *e;

#ifndef NDEBUG
    self_g->acc.jobs_queued++;
#endif

    if (syn)
        thr_syn__job_prepare(syn);
    bot = shared_read(self_g->bot);
    top = shared_read(self_g->top);
    if (unlikely((int)(bot - top) >= THR_JOB_MAX)) {
        job_run(job, syn);
        return;
    }

    e = &self_g->q[bot % THR_JOB_MAX];
    shared_write(e->job, job);
    shared_write(e->syn, syn);
    shared_write(self_g->bot, bot + 1);

    thr_ec_signal_relaxed(&_G.ec);
    if (syn)
        thr_ec_signal_relaxed(&syn->ec);
}

void thr_schedule(thr_job_t *job)
{
    thr_syn_schedule(NULL, job);
}

#define cas_top(ti, top__) \
    likely(atomic_bool_cas(&(ti)->top, top__, top__ + 1))

static bool thr_job_dequeue(void)
{
    struct deque_entry *e;
    unsigned top, bot;
    thr_job_t *job;
    thr_syn_t *syn;

    bot = shared_read(self_g->bot) - 1;
    e   = &self_g->q[bot % THR_JOB_MAX];
    job = shared_read(e->job);
    syn = shared_read(e->syn);
    shared_write(self_g->bot, bot);

    mb();

    top = shared_read(self_g->top);
    if (likely((int)(bot - top) > 0))
        return job_run(job, syn);
    if (likely(bot == top) && cas_top(self_g, top)) {
        shared_write(self_g->bot, top + 1);
        return job_run(job, syn);
    }
    shared_write(self_g->bot, bot + 1);
    return false;
}

static bool thr_job_try_steal(thr_info_t *ti)
{
    struct deque_entry *e;
    unsigned top, bot;
    thr_job_t *job;
    thr_syn_t *syn;

    top = shared_read(ti->top);
    bot = shared_read(ti->bot);
    e   = &ti->q[top % THR_JOB_MAX];
    job = shared_read(e->job);
    syn = shared_read(e->syn);
    if ((int)(bot - top) > 0 && cas_top(ti, top))
        return job_run(job, syn);
    return false;
}

#undef cas_top

/* FIXME: optimize for large number of threads, with a loopless fastpath */
static bool thr_job_steal(void)
{
    for (size_t i = self_tid_g; i-- > 0; ) {
        if (thr_job_try_steal(&_G.threads[i]))
            return true;
    }
    for (size_t i = self_tid_g; ++i < thr_parallelism_g; ) {
        if (thr_job_try_steal(&_G.threads[i]))
            return true;
    }
    return false;
}

/* }}} */
/* serial queues {{{ */

static ALWAYS_INLINE thr_qnode_t *thr_qnode_of(mpsc_node_t *node)
{
    return container_of(node, thr_qnode_t, qnode);
}

static thr_qnode_t *thr_qnode_create(void)
{
    if (likely(self_g && self_g->ncache.qnode.next)) {
        thr_qnode_t *n = thr_qnode_of(self_g->ncache.qnode.next);

        self_g->ncache.qnode.next = n->qnode.next;
        self_g->ncache_sz--;
        return n;
    }
    return memalign(64, sizeof(thr_qnode_t));
}

static void thr_qnode_destroy(mpsc_node_t *n)
{
    if (likely(self_g && self_g->ncache_sz < NCACHE_MAX)) {
        self_g->ncache_sz++;
        n->next = self_g->ncache.qnode.next;
        self_g->ncache.qnode.next = n;
    } else {
        free(thr_qnode_of(n));
    }
}

static void thr_queue_wipe(thr_queue_t *q)
{
    assert (mpsc_queue_looks_empty(&q->q));
}
GENERIC_DELETE(thr_queue_t, thr_queue);

static void thr_queue_drain(thr_queue_t *q)
{
    mpsc_it_t it;

    if (unlikely(mpsc_queue_looks_empty(&q->q))) {
        /* The queue should not be marked empty *and* scheduled. the only
         * queue that should go through this slowpath is the main queue
         * since it's not really scheduled like the other ones
         */
        assert (q == thr_queue_main_g);
        return;
    }

#define doit(n) \
    ({  thr_job_t *job = thr_qnode_of(n)->job; \
        thr_syn_t *syn = thr_qnode_of(n)->syn; \
                                               \
        thr_qnode_destroy(n);                  \
        job_run(job, syn);                     \
    })
    mpsc_queue_drain_start(&it, &q->q);
    do {
        thr_qnode_t *n = thr_qnode_of(mpsc_queue_drain_fast(&it, doit));
        job_run(n->job, n->syn);
    } while (!__mpsc_queue_drain_end(&it, thr_qnode_destroy,
                                     ({ thr_job_dequeue() ?: thr_job_steal(); })));
#undef doit

    if (q->closing)
        thr_queue_delete(&q);
}

static void thr_queue_run(thr_job_t *job, thr_syn_t *syn)
{
    thr_queue_drain(container_of(job, thr_queue_t, run));
}

static void thr_queue_finalize(thr_job_t *job, thr_syn_t *syn)
{
    /*
     * Do nothing in the finalize, thr_queue_drain will perform the deletion
     * itself.
     *
     * The mb() is here so that the test if (q->closing) in thr_queue_drain
     * works properly.
     */
    mb();
}

static thr_queue_t *thr_queue_init(thr_queue_t *q)
{
    p_clear(q, 1);
    mpsc_queue_init(&q->q);
    q->run.run     = &thr_queue_run;
    q->destroy.run = &thr_queue_finalize;
    return q;
}

static void thr_wakeup_thr0(void)
{
    if (self_tid_g != 0) {
        thr_evc_t *ec = atomic_xchg(&thr0_cur_ec_g, NULL);

        if (ec) {
            thr_ec_broadcast(ec);
            shared_write(thr0_cur_ec_g, ec);
        } else {
            pthread_kill(_G.threads[0].thr, SIGCHLD);
        }
    }
}

void thr_syn_queue(thr_syn_t *syn, thr_queue_t *q, thr_job_t *job)
{
    if (likely(q)) {
        thr_qnode_t *n = thr_qnode_create();

        n->job = job;
        n->syn = syn;
        if (syn)
            thr_syn__job_prepare(syn);

        assert (!q->closing || job == &q->destroy);
        if (mpsc_queue_push(&q->q, &n->qnode)) {
            if (q == thr_queue_main_g) {
                thr_wakeup_thr0();
            } else {
                thr_schedule(&q->run);
            }
        }
    } else {
        thr_syn_schedule(syn, job);
    }
}

void thr_queue_sync(thr_queue_t *q, thr_job_t *job)
{
    thr_qnode_t *n;
    thr_syn_t syn;

    thr_syn_init(&syn);
    shared_write(syn.pending, 1);
    n = thr_qnode_create();
    n->job = job;
    n->syn = &syn;
    /*
     * if mpsc_queue_push returns 1, then we're alone !
     */
    assert (!q->closing || job == &q->destroy);
    if (mpsc_queue_push(&q->q, &n->qnode)) {
        if (q == thr_queue_main_g && self_tid_g != 0) {
            thr_wakeup_thr0();
        } else {
            thr_queue_drain(q);
            return;
        }
    }
    thr_syn_wait(&syn);
}

void thr_queue(thr_queue_t *q, thr_job_t *job)
{
    thr_syn_queue(NULL, q, job);
}

thr_queue_t *thr_queue_create(void)
{
    thr_queue_t *q = p_new(thr_queue_t, 1);

    return thr_queue_init(q);
}

void thr_queue_destroy(thr_queue_t *q, bool wait)
{
    assert (q != thr_queue_main_g);
    if (atomic_bool_cas(&q->closing, 0, 1)) {
        if (wait) {
            thr_queue_sync(q, &q->destroy);
        } else {
            thr_queue(q, &q->destroy);
        }
    } else {
        e_panic("destroying queue %p twice !", q);
    }
}

/* }}} */
/* thread run loop {{{ */

static void thr_info_init(int tid)
{
    self_tid_g = tid;
    self_g     = _G.threads + tid;
    shared_write(self_g->alive, true);
    shared_write(self_g->thr, pthread_self());
    pthread_barrier_wait(&_G.start_bar);
    mb();
}

static void thr_info_cleanup(void *unused)
{
    do {
        if (self_tid_g == 0)
            thr_queue_drain(thr_queue_main_g);
    } while (thr_job_dequeue());
    if (self_tid_g == 0)
        thr_queue_wipe(thr_queue_main_g);
    shared_write(self_g->alive, false);
    while (self_g->ncache.qnode.next) {
        thr_qnode_t *n = thr_qnode_of(self_g->ncache.qnode.next);

        self_g->ncache.qnode.next = n->qnode.next;
        free(n);
    }
}

static void *thr_job_main(void *arg)
{
    thr_info_init((uintptr_t)arg);
    pthread_cleanup_push(thr_info_cleanup, NULL);

    while (likely(!shared_read(_G.stopping))) {
        while (likely(thr_job_dequeue()))
            continue;
        if (!thr_job_steal()) {
            uint64_t key = thr_ec_get(&_G.ec);

#ifndef NDEBUG
            self_g->acc.ec_gets++;
#endif
            if (thr_job_steal())
                continue;
            if (!shared_read(_G.stopping)) {
#ifndef NDEBUG
                self_g->acc.ec_waits++;
#endif
                thr_ec_wait(&_G.ec, key);
            }
        }
    }
    pthread_cleanup_pop(1);
    return NULL;
}

/* }}} */
/* Module init / shutdown {{{ */

static void thr_job_atfork(void)
{
    el_before_unregister(&_G.before);
    p_delete(&_G.threads);
    p_clear(&_G, 1);
    thr_parallelism_g = 0;
}

static void thr_before(el_t ev, el_data_t priv)
{
    thr_queue_drain(thr_queue_main_g);
}

void thr_queue_main_drain(void)
{
    assert (self_tid_g == 0);
    thr_queue_drain(thr_queue_main_g);
}

void thr_initialize(void)
{
    sigset_t fillset;
    sigset_t old;
    pthread_t tid;
    pthread_attr_t attr;

    if (!thr_parallelism_g) {
        pthread_atfork(NULL, NULL, thr_job_atfork);

        thr_parallelism_g = MAX(sysconf(_SC_NPROCESSORS_CONF), 2);
        if (unlikely(RUNNING_ON_VALGRIND))
            thr_parallelism_g = MIN(2, thr_parallelism_g);
        _G.threads = p_new(thr_info_t, thr_parallelism_g);

        pthread_attr_init(&attr);
        pthread_attr_setdetachstate(&attr, PTHREAD_CREATE_DETACHED);
        sigfillset(&fillset);
        pthread_sigmask(SIG_SETMASK, &fillset, &old);
        pthread_barrier_init(&_G.start_bar, NULL, thr_parallelism_g);
        for (size_t i = 1; i < thr_parallelism_g; i++) {
            if (pthread_create(&tid, &attr, thr_job_main, (void *)(uintptr_t)i)) {
                e_fatal("Unable to create new thread: %m");
            }
        }
        thr_info_init(0);
        pthread_barrier_destroy(&_G.start_bar);
        pthread_sigmask(SIG_SETMASK, &old, NULL);
        pthread_attr_destroy(&attr);
        thr_queue_init(thr_queue_main_g);
        _G.before = el_unref(el_before_register(thr_before, NULL));
    }
}

__attribute__((destructor))
void thr_shutdown(void)
{
    if (thr_parallelism_g) {
        shared_write(_G.stopping, true);
        thr_ec_broadcast(&_G.ec);

        for (size_t i = 1; i < thr_parallelism_g; i++) {
            mb();
            while (access_once(_G.threads[i].alive)) {
                thr_ec_broadcast(&_G.ec);
                sched_yield();
            }
        }

        thr_info_cleanup(NULL);

        el_before_unregister(&_G.before);
        p_delete(&_G.threads);
        p_clear(&_G, 1);
        thr_parallelism_g = 0;
    }
}

/* }}} */

size_t thr_id(void)
{
    return self_tid_g;
}

void thr_syn_wait(thr_syn_t *syn)
{
    if (self_tid_g == 0) {
        while (shared_read(syn->pending)) {
            uint64_t key;

            if (!mpsc_queue_looks_empty(&thr_queue_main_g->q)) {
                thr_queue_drain(thr_queue_main_g);
                continue;
            }
            if (likely(thr_job_dequeue()))
                continue;
            if (thr_job_steal())
                continue;

            shared_write(thr0_cur_ec_g, &syn->ec);
            key = thr_ec_get(&syn->ec);
            if (!shared_read(syn->pending))
                goto cas;
            if (!mpsc_queue_looks_empty(&thr_queue_main_g->q))
                goto cas;
            thr_ec_wait(&syn->ec, key);
          cas:
            while (unlikely(!atomic_bool_cas(&thr0_cur_ec_g, &syn->ec, NULL)))
                sched_yield();
        }
    } else {
        while (shared_read(syn->pending)) {
            if (likely(thr_job_dequeue()))
                continue;
            if (!thr_job_steal()) {
                uint64_t key = thr_ec_get(&syn->ec);

                if (!shared_read(syn->pending))
                    break;
                if (thr_job_steal())
                    continue;
                thr_ec_timedwait(&syn->ec, key, 100);
            }
        }
    }
    while (shared_read(syn->unsafe))
        mb();
}
