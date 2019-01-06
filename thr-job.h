/**************************************************************************/
/*                                                                        */
/*  Copyright (C) 2004-2019 INTERSEC SA                                   */
/*                                                                        */
/*  Should you receive a copy of this source code, you must check you     */
/*  have a proper, written authorization of INTERSEC to hold it. If you   */
/*  don't have such an authorization, you must DELETE all source code     */
/*  files in your possession, and inform INTERSEC of the fact you obtain  */
/*  these files. Should you not comply to these terms, you can be         */
/*  prosecuted in the extent permitted by applicable law.                 */
/*                                                                        */
/**************************************************************************/

#if !defined(IS_LIB_COMMON_THR_H) || defined(IS_LIB_COMMON_THR_JOB_H)
#  error "you must include thr.h instead"
#else
#define IS_LIB_COMMON_THR_JOB_H

#define THR_JOB_MAX   256

typedef struct thr_job_t   thr_job_t;
typedef struct thr_syn_t   thr_syn_t;
typedef struct thr_queue_t thr_queue_t;

struct thr_job_t {
    void (*run)(thr_job_t *, thr_syn_t *);
};

/** \brief Synchronization structure to wait for the completion of a batch.
 *
 * It is allowed to add jobs on a thr_syn_t iff you hold a reference on the
 * thr_syn_t, which can be done:
 * - using thr_syn__{retain,release} directly (though you must already be sure
 *   the object is owned for that);
 * - being inside a job queued on that thr_syn_t;
 * - being the owner (IOW having done thr_syn_init() and being responsible for
 *   doing the thr_syn_wipe() later).
 *
 * A typical thr_syn_t usage is:
 *   thr_syn_init(&syn);
 *   [... queue jobs using for example thr_syn_schedule() ...]
 *   thr_syn_wait(&syn); // wait for completion
 *   [... do something with the result ...]
 *   thr_syn_wipe(&syn); // be sure nobody holds references anymore
 *
 * Note that thr_syn_wipe should be done the latest possible since it will
 * busy loop waiting for thr_syn_t#refcnt holders to disappear.
 */
struct thr_syn_t {
    /** number of jobs registered on this thr_syn_t */
    atomic_uint pending;
    /** 1 for the owner + 1 for each people inside a thr_syn_wait() call */
    atomic_uint refcnt;
    /** the eventcount used for the blocking part of the thr_syn_wait() */
    thr_evc_t         ec;
} __attribute__((aligned(64)));

#ifdef __has_blocks
static ALWAYS_INLINE thr_job_t *thr_job_from_blk(block_t blk)
{
    return (thr_job_t *)((uintptr_t)Block_copy(blk) | 1);
}
#endif

/** \brief returns the amount of parralelism (number of threads) used.
 *
 * This may be useful for some kind of problems where you just want to split a
 * large amount of treatments of almost constant time in a very efficient way.
 *
 * For example, AND-ing two large bitmaps can be speed-up that way splitting
 * the bitmaps in thr_parallelism_g chunks.
 *
 * If your treatments aren't in constant time, it's better to chunk in large
 * enough segments so that the job queuing/dequeing/stealing overhead isn't
 * too large, but small enough so that the fact that jobs aren't run in equal
 * time can be compensated through rescheduling (stealing). In that case,
 * remember that jobs are posted to your local thread. So it's even better if
 * you can split the jobs along the road so that several threads receive jobs
 * in the fast path.
 */
extern size_t thr_parallelism_g;
extern thr_queue_t *const thr_queue_main_g;

/** \brief returns the id of the current thread in [0 .. thr_parallelism_g[
 */
size_t thr_id(void) __leaf __attribute__((pure));

/** \brief Schedule one job.
 *
 * By default, jobs are queued in a per-thread deque queue. The deque allows:
 * - non blocking job queuing
 * - non blocking job dequeuing if there is more than one job in queue
 * - CAS-guarded job dequeuing if there is one job in queue
 * - CAS-guarded job stealing from other threads.
 *
 * Jobs order is *NOT* preserved, as the local thread will take the last jobs
 * queued first, and stealing threads will take the first ones first.
 *
 * The job life-cycle is the responsibility of the caller.
 *
 * Note that each thread can only queue up to a fixed number of jobs. Any job
 * in excess is run immediately instead of being queued.
 *
 * In the fast paths, job queuing and dequeuing are very fast (below 40-50
 * cycles probably). Stealing a job in the fast path is pretty fast too,
 * though searching for a stealable job isn't (atm).
 *
 * It's a good idea to be sure that the job you queue is long enough so that
 * the stealing costs are amortized (especially if the job structure is
 * malloc()ed).
 */
void thr_schedule(thr_job_t *job);
void thr_syn_schedule(thr_syn_t *syn, thr_job_t *job);

#ifdef __has_blocks
static ALWAYS_INLINE void thr_schedule_b(block_t blk)
{
    thr_schedule(thr_job_from_blk(blk));
}
static ALWAYS_INLINE void thr_syn_schedule_b(thr_syn_t *syn, block_t blk)
{
    thr_syn_schedule(syn, thr_job_from_blk(blk));
}
#endif

thr_queue_t *thr_queue_create(void) __leaf;
void thr_queue_destroy(thr_queue_t *q, bool wait) __leaf;

/** \brief Queue one job on a serial queue
 *
 * For simplicity, #q can be NULL and then thr_queue* is similar to calling
 * thr_schedule*.
 *
 * Jobs on serial queues run at most one at a time. A specific queue, called
 * thr_queue_main_g see its jobs run in the main thread only.
 */
void thr_queue(thr_queue_t *q, thr_job_t *job);
void thr_queue_sync(thr_queue_t *q, thr_job_t *job);
void thr_syn_queue(thr_syn_t *syn, thr_queue_t *q, thr_job_t *job);

#ifdef __has_blocks
static ALWAYS_INLINE void thr_queue_b(thr_queue_t *q, block_t blk)
{
    thr_queue(q, thr_job_from_blk(blk));
}
static ALWAYS_INLINE void thr_queue_sync_b(thr_queue_t *q, block_t blk)
{
    thr_queue_sync(q, (thr_job_t *)((uintptr_t)blk | 2));
}
static ALWAYS_INLINE void thr_syn_queue_b(thr_syn_t *syn, thr_queue_t *q, block_t blk)
{
    thr_syn_queue(syn, q, thr_job_from_blk(blk));
}
#endif

/** \brief low level function to retain a refcnt
 */
static ALWAYS_INLINE
void thr_syn__retain(thr_syn_t *syn)
{
    unsigned res = atomic_fetch_add(&syn->refcnt, 1);

    assert (res != 0);
}

/** \brief low level function to release a refcnt
 */
static ALWAYS_INLINE
void thr_syn__release(thr_syn_t *syn)
{
    unsigned res = atomic_fetch_sub(&syn->refcnt, 1);

    assert (res != 0);
}

/** \brief initializes a #thr_syn_t structure.
 *
 * the #thr_syn_t is a simple semaphore-like structure to control if a given
 * list of jobs is done. This structure tracks the number of jobs that
 * synchronize together to form a higher level task.
 */
static ALWAYS_INLINE thr_syn_t *thr_syn_init(thr_syn_t *syn)
{
    p_clear(syn, 1);
    thr_ec_init(&syn->ec);
    atomic_init(&syn->pending, 0);
    atomic_init(&syn->refcnt, 1);
    return syn;
}
GENERIC_NEW(thr_syn_t, thr_syn);

static ALWAYS_INLINE void thr_syn_wipe(thr_syn_t *syn)
{
    thr_syn__release(syn);
    while (unlikely(atomic_load_explicit(&syn->refcnt, memory_order_acquire))) {
        cpu_relax();
    }
    thr_ec_wipe(&syn->ec);
}
GENERIC_DELETE(thr_syn_t, thr_syn);

/** \brief setup a thr_syn_t to fire an event when finished.
 *
 * This must be called after all the jobs have been queued or it may fire
 * before the actual completion due to races.
 */
void thr_syn_notify(thr_syn_t *syn, thr_queue_t *q, thr_job_t *job);
#ifdef __has_blocks
static ALWAYS_INLINE
void thr_syn_notify_b(thr_syn_t *syn, thr_queue_t *q, block_t blk)
{
    thr_syn_notify(syn, q, thr_job_from_blk(blk));
}
#endif

/** \brief low level function to account for a task
 *
 * It's allowed to be called if you know for some reason you hold or someone
 * else is holding a reference on the thr_syn_t (either through the refcnt or
 * because you're inside a job that is not yet done and is accounted in this
 * thr_syn_t.
 */
static ALWAYS_INLINE
void thr_syn__job_prepare(thr_syn_t *syn)
{
    thr_syn__retain(syn);
    atomic_fetch_add(&syn->pending, 1);
}

/** \brief low level function to wake up people waiting on a thr_syn_t
 */
static ALWAYS_INLINE
void thr_syn__broacast(thr_syn_t *syn)
{
    thr_ec_broadcast(&syn->ec);
}

/** \brief low level function to notify a task is done.
 */
static ALWAYS_INLINE
void thr_syn__job_done(thr_syn_t *syn)
{
    unsigned res = atomic_fetch_sub(&syn->pending, 1);

    assert (res != 0);
    if (res == 1) {
        thr_syn__broacast(syn);
    }
    thr_syn__release(syn);
}

/** \brief wait for the completion of a given macro task.
 *
 * This function waits until all jobs that have been registered as
 * synchronizing against this #thr_syn_t to complete. If there are jobs
 * running, the waiter is used to perform new jobs in between to avoid
 * blocking, using the #thr_drain_one function.
 */
void thr_syn_wait(thr_syn_t *syn);

#ifdef __has_blocks

/** \brief wait for a condition to become true.
 *
 * This function waits until the given callback returns true. This supposes
 * the condition will become true because of the consumption of the jobs
 * associated with this #thr_syn_t.
 *
 * If there are jobs pending, the waiter is used to perform any jobs in
 * between to avoid blocking, using the #thr_drain_one function.
 *
 * \param[in] syn  A #thr_syn_t on which you which to wait.
 * \param[in] cond A callback returning true when the condition is verified.
 *                 If NULL is provided, this function becomes equivalent to
 *                 \ref thr_syn_wait. The callback must be pure and you have
 *                 no guarantees on the number of times it will be called.
 */
void thr_syn_wait_until(thr_syn_t *syn, bool (BLOCK_CARET cond)(void));

#endif

/** \brief process one job from the main queue.
 */
void thr_queue_main_drain(void);

/** \brief enabled/disable threads-jobs reloading after fork.
 *
 * \return Previous state of <reload_at_fork>.
 */
bool thr_job_reload_at_fork(bool enabled);

/** \brief fork() preserving threads-jobs */
__must_check__
static inline pid_t thr_job_fork(void)
{
    bool prev_val = thr_job_reload_at_fork(true);
    pid_t pid = fork();

    thr_job_reload_at_fork(prev_val);

    return pid;
}

/*- accounting -----------------------------------------------------------*/

#if !defined(NDEBUG) && !defined(__has_tsan)
void thr_acc_reset(void);
void thr_acc_set_affinity(size_t offs);
void thr_acc_trace(int lvl, const char *fmt, ...) __attribute__((format(printf, 2, 3)));
#else
#define thr_acc_reset()            ((void)0)
#define thr_acc_set_affinity(offs) ((void)0)
#define thr_acc_trace(...)         ((void)0)
#endif

#endif
