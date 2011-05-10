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

#define __mpsc_queue_drain(q, doit, freenode, relax) \
    do {                                                                   \
        mpsc_queue_t *q_ = (q);                                            \
        mpsc_node_t  *q_head = q_->head.next;                              \
        mpsc_node_t  *q_next;                                              \
                                                                           \
        /* breaks if someone called mpsc_queue_pop with the queue empty */ \
        assert (q_head);                                                   \
                                                                           \
        q_->head.next = NULL;                                              \
        for (;;) {                                                         \
            q_next = q_head->next;                                         \
            while (likely(q_next)) {                                       \
                doit(q_head, true);                                        \
                q_head = q_next;                                           \
                q_next = q_head->next;                                     \
            }                                                              \
            doit(q_head, false);                                           \
                                                                           \
            if (q_head == q_->tail) {                                      \
                if (atomic_bool_cas(&q_->tail, q_head, &q_->head)) {       \
                    freenode(q_head);                                      \
                    break;                                                 \
                }                                                          \
            }                                                              \
            while ((q_next = q_head->next) == NULL) {                      \
                relax();                                                   \
            }                                                              \
            freenode(q_head);                                              \
            q_head = q_next;                                               \
        }                                                                  \
    } while (0)

#endif
