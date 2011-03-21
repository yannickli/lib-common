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

#include "thr.h"

mpsc_queue_t *mpsc_queue_init(mpsc_queue_t *q)
{
    p_clear(q, 1);
    q->tail = &q->head;
    return q;
}

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
