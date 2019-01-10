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

#if !defined(IS_LIB_COMMON_THR_H) || defined(IS_LIB_COMMON_THR_MPSC_H)
#  error "you must include thr.h instead"
#else
#define IS_LIB_COMMON_THR_MPSC_H

#if !defined(__x86_64__) && !defined(__i386__)
#  error "this file assumes a strict memory model and is probably buggy on !x86"
#endif

/*
 * This file provides an implementation of lock-free intrusive MPSC queue.
 *
 * MPSC stands for Multiple-Producer, Single-Consumer.
 *
 * It means that it's possible to concurently add elements to these queues
 * without taking any locks, but that only one single consumer is allowed at
 * any time.
 */

typedef struct mpsc_node_t  mpsc_node_t;
typedef struct mpsc_queue_t mpsc_queue_t;

/** \brief node to embed into structures to be put in an mpsc queue.
 */
struct mpsc_node_t {
    mpsc_node_t *volatile next;
};

/** \brief head of an mpsc queue.
 */
struct mpsc_queue_t {
    mpsc_node_t  head;
    mpsc_node_t *volatile tail;
};

/** \brief static initializer for mpsc_queues.
 */
#define MPSC_QUEUE_INIT(name)   { .tail = &(name).head }

/** \brief initializes an mpsc_queue.
 */
static inline mpsc_queue_t *mpsc_queue_init(mpsc_queue_t *q)
{
    p_clear(q, 1);
    q->tail = &q->head;
    return q;
}

/** \brief tells whether an mpsc queue looks empty or not.
 *
 * This should be only used from the only mpsc consumer thread.
 *
 * The function may return false positives but no false negatives: the
 * consumer thread is the sole consumer, hence if it sees a non empty queue
 * the queue can't be freed without it knowing about it.
 *
 * If the function returns true though, some other thread may enqueue
 * something at the same time and the function will return a false positive,
 * hence the name "the queue looks empty but maybe it isn't".
 *
 * \param[in]  q  the queue
 * \returns true if the queue looks empty, false else.
 */
static inline bool mpsc_queue_looks_empty(mpsc_queue_t *q)
{
    return q->head.next == NULL;
}

/** \brief enqueue a task in the queue.
 *
 * This function has a vital role as it does empty -> non empty transition
 * detections which make it suitable to use like:
 *
 * <code>
 *     if (mpsc_queue_push(some_q, some_node))
 *         schedule_processing_of(some_q);
 * </code>
 *
 * \param[in]   q    the queue
 * \param[in]   n    the node to push
 * \returns true if the queue was empty before the push, false else.
 */
static inline bool mpsc_queue_push(mpsc_queue_t *q, mpsc_node_t *n)
{
    mpsc_node_t *prev;

    n->next = NULL;
    prev = atomic_xchg(&q->tail, n);
    prev->next = n;
    return prev == &q->head;
}

/** \brief type used to enumerate through a queue during a drain.
 */
typedef struct mpsc_it_t {
    mpsc_queue_t *q;
    mpsc_node_t  *h;
} mpsc_it_t;

/** \brief initiates the iterator to start a drain.
 *
 * \warning
 *   it's forbidden not to drain fully after this function has been called.
 *
 * a typical drain looks like that:
 *
 * <code>
 *   {
 *       mpsc_it_t it;
 *
 *   #define doit(n)  ({ process_node(n); freenode(n); })
 *       mpsc_queue_drain_start(&it, &path->to.your_queue);
 *       do {
 *           mpsc_node_t *n = mpsc_queue_drain_fast(&it, doit);
 *           process_node(n);
 *           // XXX do NOT freenode(n) here mpsc_queue_drain_end will use it
 *       } while (!mpsc_queue_drain_end(&it, freenode));
 *   }
 * </code>
 *
 * It is important to note that the queue may go through the draining loop
 * multiple times if jobs are enqueued while the queue is drained.
 *
 * \param[out]  it   the iterator to initialize
 * \param[in]   q    the queue to drain
 */
static inline void mpsc_queue_drain_start(mpsc_it_t *it, mpsc_queue_t *q)
{
    it->q = q;
    it->h = q->head.next;
    q->head.next = NULL;
    /* breaks if someone called mpsc_queue_drain_start with the queue empty */
    assert (it->h);
}

/** \brief fast path of the drain.
 *
 * \param[in]   it   the iterator to use
 * \param[in]   doit
 *     a function or macro that takes one mpsc_node_t as an argument: the node
 *     to process.
 * \param[in]   ...  additionnal arguments to pass to \v doit.
 *
 * \returns
 *   a non NULL mpsc_node_t that may be the last one in the queue for
 *   processing purposes. mpsc_queue_drain_fast doesn't process this node, its
 *   up to the caller to do it.
 *   This node should NOT be freed, as mpsc_queue_drain_end will need it.
 */
#define mpsc_queue_drain_fast(it, doit, ...) \
    ({                                                                     \
        mpsc_it_t   *it_ = (it);                                           \
        mpsc_node_t *h_  = it_->h;                                         \
        for (mpsc_node_t *n_; likely(n_ = h_->next); h_ = n_)              \
            doit(h_, ##__VA_ARGS__);                                       \
        it_->h = h_;                                                       \
    })

/** \brief implementation of mpsc_queue_drain_end.
 *
 * Do not use directly unless you want to override relax and know what you are
 * doing.
 */
#define __mpsc_queue_drain_end(it, freenode, relax) \
    ({                                                                     \
        mpsc_it_t    *it_ = (it);                                          \
        mpsc_queue_t *q_  = it_->q;                                        \
        mpsc_node_t  *h_  = it_->h;                                        \
                                                                           \
        if (h_ == q_->tail && atomic_bool_cas(&q_->tail, h_, &q_->head)) { \
            it_->h = NULL;                                                 \
        } else {                                                           \
            while ((it_->h = h_->next) == NULL) {                          \
                relax;                                                     \
            }                                                              \
        }                                                                  \
        freenode(h_);                                                      \
        it_->h == NULL;                                                    \
    })

/** \brief test for the drain completion.
 *
 * \param[in]   it        the iterator to use
 * \param[in]   freenode
 *   a function or macro that will be called on the last node (actually the
 *   one #mpsc_queue_drain_fast returned) when the test for the mpsc queue
 *   emptyness has been done. If you don't need to free your nodes, use
 *   IGNORE.
 *
 * \returns
 *   true if the queue is really empty, in which case the drain is done, false
 *   else in which case the caller has to restart the drain.
 */
#define mpsc_queue_drain_end(it, freenode) \
    __mpsc_queue_drain_end(it, freenode, cpu_relax())



/** \internal
 */
static inline __cold
bool mpsc_queue_pop_slow(mpsc_queue_t *q, mpsc_node_t *head, bool block)
{
    mpsc_node_t *tail = q->tail;
    mpsc_node_t *next;

    if (head == tail) {
        q->head.next = NULL;
        if (atomic_bool_cas(&q->tail, tail, &q->head))
            return true;
        q->head.next = head;
    }

    next = head->next;
    if (next == NULL) {
        if (!block)
            return false;
        while ((next = head->next) == NULL)
            cpu_relax();
    }
    q->head.next = next;
    return true;
}

/** \brief pop one entry from the mpsc queue.
 *
 * \param[in] q      The queue.
 * \param[in] block  If the pop emptied the queue while an insertion is in
 *                   progress, wait for this insertion to be finished.
 *
 * \returns
 *   The node extracted from the queue or NULL if the queue is empty.
 *
 * \warning
 *   This API is unsafe and should be used only in specific use-cases where
 *   the enumeration of a queue must support imbrication (that is a call that
 *   enumerate the queue within an enumeration).
 */
static inline mpsc_node_t *mpsc_queue_pop(mpsc_queue_t *q, bool block)
{
    mpsc_node_t *head = q->head.next;
    mpsc_node_t *next;

    if (head == NULL)
        return NULL;

    if (likely(next = head->next)) {
        q->head.next = next;
        return head;
    }
    if (block) {
        mpsc_queue_pop_slow(q, head, block);
        return head;
    }
    return mpsc_queue_pop_slow(q, head, block) ? head : NULL;
}

#endif
