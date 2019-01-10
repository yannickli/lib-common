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

#include "thr.h"

spsc_queue_t *spsc_queue_init(spsc_queue_t *q, size_t v_size)
{
    spsc_node_t *n = p_new(spsc_node_t, 1);

    p_clear(q, 1);
    q->tail = q->first = q->head_copy = n;
    atomic_store_explicit(&q->head, n, memory_order_relaxed);
    return q;
}

void spsc_queue_wipe(spsc_queue_t *q)
{
    spsc_node_t *n = q->first;
    do {
        spsc_node_t *next = atomic_load_explicit(&n->next, memory_order_relaxed);

        p_delete(&n);
        n = next;
    } while (n);
}
