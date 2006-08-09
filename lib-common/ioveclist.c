/**************************************************************************/
/*                                                                        */
/*  Copyright (C) 2004-2006 INTERSEC                                      */
/*                                                                        */
/*  Should you receive a copy of this source code, you must check you     */
/*  have a proper, written authorization of INTERSEC to hold it. If you   */
/*  don't have such an authorization, you must DELETE all source code     */
/*  files in your possession, and inform INTERSEC of the fact you obtain  */
/*  these files. Should you not comply to these terms, you can be         */
/*  prosecuted in the extent permitted by applicable law.                 */
/*                                                                        */
/**************************************************************************/

#include <errno.h>

#include "ioveclist.h"

void ioveclist_init(ioveclist *l)
{
    l->used = 0;
}

int ioveclist_insert_first(ioveclist *l, const void *data, int size)
{
    int i;

    if (l->used >= IOVECLIST_OBJS_NUM) {
	return 1;
    }
    /* OG: should use memmove ? */
    for (i = l->used; i > 0; i--) {
	l->objs[i] = l->objs[i-1];
    }
    l->used++;
    /* this cast is OK because we use iovec only for writing data */
    l->objs[0].iov_base = (void*)data;
    l->objs[0].iov_len = size;

    return 0;
}

/* write content of ioveclist to fd. fd is non-blocking.
 * Return value :
 *    IOVECLIST_WRITE_ERROR  if write error
 *    IOVECLIST_LATER        if there's more to write
 *    IOVECLIST_EMPTY        if everything's been written
 */
ioveclist_state ioveclist_write(ioveclist *l, int fd)
{
    int nbwritten;
    nbwritten = writev(fd, l->objs, l->used);
    if (nbwritten < 0) {
	if (errno == EINTR || errno == EAGAIN) {
	    return IOVECLIST_LATER;
	}
	return IOVECLIST_WRITE_ERROR;
    }
    /* Skip over the written bytes.
     * */
    while (nbwritten > 0) {
	if (nbwritten < (int) l->objs[0].iov_len) {
	    /* chunk 0 not fully written */
	    l->objs[0].iov_base = (byte *)l->objs[0].iov_base + nbwritten;
	    l->objs[0].iov_len -= nbwritten;
	    return IOVECLIST_LATER;
	} else {
	    /* chunk 0 fully written. Skip it. */
	    nbwritten -= l->objs[0].iov_len;
	    l->used--;
	    int i;
	    for (i = 0; i < l->used; i++) {
		l->objs[i] = l->objs[i + 1];
	    }
	}
    }
    if (l->used) {
	return IOVECLIST_LATER;
    } else {
	return IOVECLIST_EMPTY;
    }
}


