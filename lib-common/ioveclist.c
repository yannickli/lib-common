/**************************************************************************/
/*                                                                        */
/*  Copyright (C) 2004-2008 INTERSEC SAS                                  */
/*                                                                        */
/*  Should you receive a copy of this source code, you must check you     */
/*  have a proper, written authorization of INTERSEC to hold it. If you   */
/*  don't have such an authorization, you must DELETE all source code     */
/*  files in your possession, and inform INTERSEC of the fact you obtain  */
/*  these files. Should you not comply to these terms, you can be         */
/*  prosecuted in the extent permitted by applicable law.                 */
/*                                                                        */
/**************************************************************************/

#include "ioveclist.h"

void ioveclist_init(ioveclist *l)
{
    l->used = 0;
}

int ioveclist_insert_first(ioveclist *l, const void *data, int size)
{
    if (l->used >= IOVECLIST_OBJS_NUM) {
        return 1;
    }
    memmove(&l->objs[1], &l->objs[0], l->used * sizeof(l->objs[0]));
    l->used++;
    /* this cast is OK because we use iovec only for writing data */
    l->objs[0].iov_base = (void*)data;
    l->objs[0].iov_len = size;

    return 0;
}

int ioveclist_append(ioveclist *l, const void *data, int size)
{
    if (l->used >= IOVECLIST_OBJS_NUM) {
        return 1;
    }
    /* this cast is OK because we use iovec only for writing data */
    l->objs[l->used].iov_base = (void*)data;
    l->objs[l->used].iov_len = size;

    l->used++;
    return 0;
}

int ioveclist_getlen(const ioveclist *l)
{
    int i, total;

    total = 0;
    for (i = 0; i < l->used; i++) {
        total += l->objs[i].iov_len;
    }
    return total;
}

void ioveclist_kill_first(ioveclist *l, ssize_t len)
{
    while (len > 0 && l->used > 0) {
        if (len < (int)l->objs[0].iov_len) {
            /* truncate chunk 0 */
            l->objs[0].iov_base = (byte *)l->objs[0].iov_base + len;
            l->objs[0].iov_len -= len;
            return;
        } else {
            /* Skip chunk 0 and continue. */
            len -= l->objs[0].iov_len;
            l->used--;
            memmove(&l->objs[0], &l->objs[1], l->used * sizeof(l->objs[0]));
        }
    }
}

void iovec_vector_kill_first(iovec_vector *iovs, ssize_t len)
{
    int i = 0;

    while (i < iovs->len && len >= (ssize_t)iovs->tab[i].iov_len) {
        len -= iovs->tab[i++].iov_len;
    }
    iovec_vector_splice(iovs, 0, i, NULL, 0);
    if (iovs->len > 0 && len) {
        iovs->tab[0].iov_base = (byte *)iovs->tab[0].iov_base + len;
        iovs->tab[0].iov_len  += len;
    }
}

int iovec_vector_getlen(iovec_vector *v)
{
    int res = 0;
    for (int i = 0; i < v->len; i++) {
        res += v->tab[i].iov_len;
    }
    return res;
}
