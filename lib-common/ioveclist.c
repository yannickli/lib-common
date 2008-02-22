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
