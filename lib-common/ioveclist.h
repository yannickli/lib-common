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

#ifndef IS_LIB_COMMON_IOVECLIST_H
#define IS_LIB_COMMON_IOVECLIST_H

#include <sys/uio.h>
#include <lib-common/blob.h>

#define IOVECLIST_OBJS_NUM 64

typedef struct ioveclist {
    struct iovec objs[IOVECLIST_OBJS_NUM];
    int used;
} ioveclist;

typedef enum {
    IOVECLIST_EMPTY,
    IOVECLIST_LATER,
    IOVECLIST_WRITE_ERROR
} ioveclist_state;

void ioveclist_init(ioveclist *l);
int ioveclist_insert_first(ioveclist *l, const void *data, int size);
int ioveclist_append(ioveclist *l, const void *data, int size);
static inline int ioveclist_insert_blob(ioveclist *l, blob_t *blob) {
    return ioveclist_insert_first(l, blob->data, blob->len);
}

ioveclist_state ioveclist_write(ioveclist *l, int fd);

#endif /* IS_LIB_COMMON_IOVECLIST_H */
