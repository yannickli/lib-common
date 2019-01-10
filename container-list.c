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

#include "container.h"

#define OPTIMIZE_SORTED_PREFIX  1
#define OPTIMIZE_SORTED_CHUNKS  1
#define OPTIMIZE_REVERSE_LISTS  1
#define OPTIMIZE_3_ITEM_CHUNKS  0

struct generic_list {
    struct generic_list *next;
};

typedef int (cmpfun)(generic_list *, generic_list *, void *);

static generic_list *generic_list_skip(generic_list *list, int n)
{
    while (n-- > 0) {
        list = list->next;
    }
    return list;
}

static void generic_list_merge(generic_list **head,
                               generic_list **tail,
                               int len1,
                               generic_list *l2, int len2,
                               cmpfun *cmp, void *priv)
{
    generic_list **prev, *l1;

    prev = head;

    for (l1 = *prev;;) {
        if (unlikely((*cmp)(l1, l2, priv) > 0)) {
            *prev = l2;
            for (;;) {
                prev = &l2->next;
                l2 = l2->next;
                if (unlikely(--len2 == 0)) {
                    *prev = l1;
                    while (len1-- > 1) {
                        l1 = l1->next;
                    }
                    l1->next = l2;
                    *tail = l1;
                    return;
                }
                if ((*cmp)(l1, l2, priv) <= 0)
                    break;
            }
            *prev = l1;
        }
        prev = &l1->next;
        l1 = l1->next;
        if (unlikely(--len1 == 0)) {
            *prev = l2;
            while (len2-- > 1) {
                l2 = l2->next;
            }
            *tail = l2;
            return;
        }
    }
}

#if !OPTIMIZE_SORTED_PREFIX
#define generic_list_sort_aux(h, t, l, s, c, p) \
        generic_list_sort_aux(h, t, l, c, p)
#endif

static void generic_list_sort_aux(generic_list **head, generic_list **tail,
                                  int length, int sorted,
                                  cmpfun *cmp, void *priv)
{
    generic_list *l1, *l2, *l3;
    int len1, len2;

    //assert (length >= 2);
    //assert (sorted < length);

#if OPTIMIZE_3_ITEM_CHUNKS
    if (length < 4) {
        /* swap head if needed */
        l1 = *head;
        l2 = l1->next;
        if (cmp(l1, l2, priv) > 0) {
            *head = l2;
            l1->next = l2->next;
            l2->next = l1;
            l2 = l1;
        }
        if (length == 2) {
            *tail = l2;
            return;
        }
        /* length = 3 */
        l3 = l2->next;
        if (cmp(l2, l3,  priv) > 0) {
            /* swap B and C */
            l1 = *head;
            l1->next = l3;
            l2->next = l3->next;
            l3->next = l2;
            l3 = l2;
            l2 = l1->next;
            if (cmp(l1, l2, priv) > 0) {
                *head = l2;
                l1->next = l2->next;
                l2->next = l1;
            }
        }
        *tail = l3;
        return;
    }
    //len1 = (length + 1) >> 1;
    len1 = length >> 1;
    len2 = length - len1;
#else
    if (length == 2) {
        /* swap head if needed */
        l1 = *head;
        l2 = l1->next;
        if (cmp(l1, l2, priv) > 0) {
            *head = l2;
            l1->next = l2->next;
            l2->next = l1;
            *tail = l1;
            return;
        } else {
            *tail = l2;
            return;
        }
    }
    len1 = length >> 1;
    len2 = length - len1;
#endif

#if !OPTIMIZE_3_ITEM_CHUNKS
    if (len1 == 1) {
        l2 = *head;
    } else
#endif
#if OPTIMIZE_SORTED_PREFIX
    if (len1 <= sorted) {
        l2 = generic_list_skip(*head, len1 - 1);
    } else
#endif
    {
        generic_list_sort_aux(head, &l2, len1, sorted, cmp, priv);
    }
    generic_list_sort_aux(&l2->next, tail, len2, sorted - len1, cmp, priv);

    l3 = l2->next;
#if OPTIMIZE_SORTED_CHUNKS
    /* This optimisation is useful for partially sorted lists.
     * Computing the sorted prefix on completely sorted lists only
     * yields a factor of 2 over this more general optimization.
     */
    if (cmp(l2, l3,  priv) <= 0)
        return;
#endif
#if OPTIMIZE_REVERSE_LISTS
    /* This yields a large improvement on reversed lists and a small one
     * on shuffled lists too.  Extra cost on pathological cases is
     * minimal and difficult to demonstrate.
     */
    if (cmp(*head, *tail,  priv) > 0) {
        l1 = *head;
        *head = l3;
        l2->next = (*tail)->next;
        (*tail)->next = l1;
        *tail = l2;
        return;
    }
#endif
    generic_list_merge(head, tail, len1, l3, len2, cmp, priv);
}

void generic_list_sort(generic_list **head, cmpfun *cmp, void *priv)
{
    generic_list *list = *head, *tail;

    if (list && list->next) {
        int length, sorted;

        sorted = 1;
#if OPTIMIZE_SORTED_PREFIX
        /* compute the length of the sorted prefix */
        for (;;) {
            if (!list->next) {
                /* list is already sorted */
                return;
            }
            if (cmp(list, list->next, priv) > 0)
                break;
            list = list->next;
            sorted++;
        }
#endif
        length = sorted;
        /* compute the total length of the list */
        while (list->next) {
            list = list->next;
            length++;
        }
        generic_list_sort_aux(head, &tail, length, sorted, cmp, priv);
    }
}
