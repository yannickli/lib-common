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

#if !defined(IS_LIB_COMMON_THR_H) || defined(IS_LIB_COMMON_THR_MPSC_H)
#  error "you must include thr.h instead"
#else
#define IS_LIB_COMMON_THR_MPSC_H

#if !defined(__x86_64__) && !defined(__i386__)
#  error "this file assumes a strict memory model and is probably buggy on !x86"
#endif

/*
 * This file provides an implementation of lock-free intrusive MPSC queue.
 */

typedef struct mpsc_node_t  mpsc_node_t;
typedef struct mpsc_queue_t mpsc_queue_t;

struct mpsc_node_t {
    mpsc_node_t *volatile next;
};

struct mpsc_queue_t {
    mpsc_node_t  head;
    mpsc_node_t *volatile tail;
};

static inline mpsc_queue_t *mpsc_queue_init(mpsc_queue_t *q)
{
    p_clear(q, 1);
    q->tail = &q->head;
    return q;
}

static inline bool mpsc_queue_looks_empty(mpsc_queue_t *q)
{
    return q->head.next == NULL;
}

static inline bool mpsc_queue_push(mpsc_queue_t *q, mpsc_node_t *n)
{
    mpsc_node_t *prev;

    n->next = NULL;
    prev = atomic_xchg(&q->tail, n);
    prev->next = n;
    return prev == &q->head;
}

typedef struct mpsc_it_t {
    mpsc_queue_t *q;
    mpsc_node_t  *h;
} mpsc_it_t;

static inline void mpsc_queue_drain_start(mpsc_it_t *it, mpsc_queue_t *q)
{
    it->q = q;
    it->h = q->head.next;
    q->head.next = NULL;
    /* breaks if someone called mpsc_queue_drain_start with the queue empty */
    assert (it->h);
}

#define mpsc_queue_drain_fast(it, doit, ...) \
    ({                                                                     \
        mpsc_it_t   *it_ = (it);                                           \
        mpsc_node_t *h_  = it_->h;                                         \
        for (mpsc_node_t *n_; likely(n_ = h_->next); h_ = n_)              \
            doit(h_, ##__VA_ARGS__);                                       \
        it_->h = h_;                                                       \
    })

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
#define mpsc_queue_drain_end(it, freenode) \
    __mpsc_queue_drain_end(it, freenode, cpu_relax())

#endif
