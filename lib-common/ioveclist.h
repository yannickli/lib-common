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

#ifndef IS_LIB_COMMON_IOVECLIST_H
#define IS_LIB_COMMON_IOVECLIST_H

#include <sys/uio.h>
#include "array.h"

#define MAKE_IOVEC(data, len)  (struct iovec){ \
    .iov_base = (void *)(data), .iov_len = (len) }

DO_VECTOR(struct iovec, iovec);
void iovec_vector_kill_first(iovec_vector *l, ssize_t len);
int iovec_vector_getlen(iovec_vector *v);


#endif /* IS_LIB_COMMON_IOVECLIST_H */
