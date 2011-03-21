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

#if !defined(IS_LIB_COMMON_THR_H) || defined(IS_LIB_COMMON_THR_JOB_H)
#  error "you must include thr.h instead"
#else
#define IS_LIB_COMMON_THR_JOB_H

#define THR_TID_MAX    64
#define THR_JOB_MAX   256

typedef struct thr_job_t   thr_job_t;
typedef struct thr_syn_t   thr_syn_t;
typedef struct thr_queue_t thr_queue_t;

struct thr_job_t {
    void (*run)(thr_job_t *, thr_syn_t *);
};

struct thr_syn_t {
    thr_evc_t    ec;
    unsigned     pending;
} __attribute__((aligned(64)));

static ALWAYS_INLINE thr_job_t *thr_job_from_blk(block_t blk)
{
    return (thr_job_t *)((uintptr_t)Block_copy(blk) | 1);
}

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

/** \brief forces the module initialization.
 *
 * This is usually done automatically, but you may have to do it after a fork
 * if you need it.
 */
void thr_initialize(void);

/** \brief forces the module shutdown.
 *
 * This is usually done automatically. Be careful this doesn't checks that all
 * jobs are done, and it shouldn't be used if you're not sure you can.
 */
void thr_shutdown(void);

/** \brief returns the id of the current thread in [0 .. thr_parallelism_g[
 */
__attribute__((pure))
size_t thr_id(void);

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

static ALWAYS_INLINE void thr_schedule_b(block_t blk)
{
    thr_schedule(thr_job_from_blk(blk));
}
static ALWAYS_INLINE void thr_syn_schedule_b(thr_syn_t *syn, block_t blk)
{
    thr_syn_schedule(syn, thr_job_from_blk(blk));
}

thr_queue_t *thr_queue_create(void);
void thr_queue_destroy(thr_queue_t *q, bool wait);

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

/** \brief initializes a #thr_syn_t structure.
 *
 * the #thr_syn_t is a simple semaphore-like structure to control if a given
 * list of jobs is done. This structure tracks the number of jobs that
 * synchronize together to form a higher level task.
 */
static ALWAYS_INLINE thr_syn_t *thr_syn_init(thr_syn_t *syn)
{
    p_clear(syn, 1);
    return syn;
}
GENERIC_NEW(thr_syn_t, thr_syn);

static ALWAYS_INLINE void thr_syn_destroy(thr_syn_t *syn)
{
    assert (syn->pending == 0);
    p_delete(&syn);
}

/** \brief setup a thr_syn_t to fire an event when finished.
 *
 * This must be called after all the jobs have been queued or it may fire
 * before the actual completion due to races.
 */
void thr_syn_notify(thr_syn_t *syn, thr_queue_t *q, thr_job_t *job);
static ALWAYS_INLINE
void thr_syn_notify_b(thr_syn_t *syn, thr_queue_t *q, block_t blk)
{
    thr_syn_notify(syn, q, thr_job_from_blk(blk));
}

/** \brief low level function to account for a task
 */
static ALWAYS_INLINE
void thr_syn__job_prepare(thr_syn_t *syn)
{
    atomic_add(&syn->pending, 1);
}

/** \brief low level function to notify a task is done.
 */
static ALWAYS_INLINE
void thr_syn__job_done(thr_syn_t *syn)
{
    unsigned res = atomic_sub_and_get(&syn->pending, 1);

    if (unlikely(res == UINT_MAX))
        e_panic("dead-lock detected");

    if (res == 0)
        thr_ec_broadcast(&syn->ec);
}

/** \brief wait for the completion of a given macro task.
 *
 * This function waits until all jobs that have been registered as
 * synchronizing against this #thr_syn_t to complete. If there are jobs
 * running, the waiter is used to perform new jobs inbetween to avoid
 * blocking, using the #thr_drain_one function.
 */
void thr_syn_wait(thr_syn_t *syn);

/** \brief process one job from the main queue.
 */
void thr_queue_main_drain(void);

/*- accounting -----------------------------------------------------------*/

#ifndef NDEBUG
void thr_acc_reset(void);
void thr_acc_set_affinity(size_t offs);
void thr_acc_trace(int lvl, const char *fmt, ...) __attribute__((format(printf, 2, 3)));
#else
#define thr_acc_reset()            ((void)0)
#define thr_acc_set_affinity(offs) ((void)0)
#define thr_acc_trace(...)         ((void)0)
#endif

#endif
