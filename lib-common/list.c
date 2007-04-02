/**************************************************************************/
/*                                                                        */
/*  Copyright (C) 2004-2007 INTERSEC SAS                                  */
/*                                                                        */
/*  Should you receive a copy of this source code, you must check you     */
/*  have a proper, written authorization of INTERSEC to hold it. If you   */
/*  don't have such an authorization, you must DELETE all source code     */
/*  files in your possession, and inform INTERSEC of the fact you obtain  */
/*  these files. Should you not comply to these terms, you can be         */
/*  prosecuted in the extent permitted by applicable law.                 */
/*                                                                        */
/**************************************************************************/

#include "list.h"
#include "macros.h"

struct generic_list {
    struct generic_list *next;
};

typedef int (cmpfun)(generic_list *, generic_list *, void *);

static generic_list *generic_list_poptail(generic_list *list) {
    if (list) {
        generic_list *tmp = list->next;
        list->next = NULL;
        return tmp;
    }
    return NULL;
}

static int
generic_list_split(generic_list **head, generic_list **l1, generic_list **l2,
                   cmpfun *cmp, void *priv)
{
    generic_list *list = *head, *half;

    /* go to the first sorting issue */
    for (;;) {
        if (!list->next)
            return 0;
        if (cmp(list, list->next, priv) > 0)
            break;
        list = list->next;
    }

    if (list == *head) {
        if (!list->next->next) {
            /* swap the two first and fake them be sorted */
            *head = list->next;
            list->next = NULL;
            (*head)->next = list;
            return 0;
        } else {
            *head = NULL;
        }
    } else {
        /* cut off the sorted part */
        list = generic_list_poptail(list);
    }

    /* split the tail in half using dual speed scan */
    *l1 = half = list;
    for (;;) {
        list = list->next;
        if (!list)
            break;
        list = list->next;
        if (!list)
            break;
        half = half->next;
    }
    *l2 = generic_list_poptail(half);
    return 1;
}

static generic_list *
generic_list_merge(generic_list *l1, generic_list *l2, cmpfun *cmp, void *priv)
{
    generic_list *res = l1, **next;
    int strict = 1;

    if (!l2)
        return l1;

    for (next = &res; *next; next = &(*next)->next) {
        if ((*cmp)(*next, l2, priv) >= strict) {
            SWAP(*next, l2);
            strict ^= 1;
        }
    }
    *next = l2;

    return res;
}

void generic_list_sort(generic_list **list, cmpfun *cmp, void *priv)
{
    generic_list *l1, *l2;

    if (*list && generic_list_split(list, &l1, &l2, cmp, priv)) {
        if (l2) {
            generic_list_sort(&l1, cmp, priv);
            generic_list_sort(&l2, cmp, priv);
            l1 = generic_list_merge(l1, l2, cmp, priv);
        }
        *list = generic_list_merge(*list, l1, cmp, priv);
    }
}
