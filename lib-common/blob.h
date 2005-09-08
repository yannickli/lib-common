#ifndef IS_BLOB_H
#define IS_BLOB_H

#include <unistd.h>

#include "mem_pool.h"

typedef struct {
    const ssize_t len;
    const unsigned char * const data;
} blob_t;


/******************************************************************************/
/* Blob creation / deletion                                                   */
/******************************************************************************/

blob_t * blob_new(pool_t * pool);
blob_t * blob_dup(pool_t * pool, blob_t * blob);
blob_t * blob_cat(pool_t * pool, blob_t * blob1, blob_t * blob2);

void     blob_resize(blob_t * blob, ssize_t newlen);
void     blob_delete(blob_t ** blob);

/******************************************************************************/
/* Blob manipulations                                                         */
/******************************************************************************/

void blob_append(blob_t * dest, blob_t * src);

/******************************************************************************/
/* Blob comparisons                                                           */
/******************************************************************************/

int blob_cmp(blob_t * blob1, blob_t * blob2);

/******************************************************************************/
/* Module init                                                                */
/******************************************************************************/

void blob_init(void);
void blob_deinit(void);

#endif
