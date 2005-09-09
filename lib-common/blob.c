#include <string.h>

#include "macros.h"
#include "blob.h"
#include "mem_pool.h"


#define INITIAL_BUFFER_SIZE 256

/*
 * a blob has a vital invariant, making every parse function avoid buffer read
 * overflows.
 *
 * there is *always* a \0 in the data at position len.
 * implyng that size is always >= len+1
 *
 */
typedef struct {
    /* public interface */
    ssize_t len;
    unsigned char * data;

    /* private interface */
    ssize_t size;  /* allocated size */
    const pool_t * pool;   /* the pool used for allocation */
} real_blob_t;


#define REAL(blob) ((real_blob_t*)(blob))

/******************************************************************************/
/* Blob creation / deletion                                                   */
/******************************************************************************/

/* create a new, empty buffer */
blob_t * blob_new(pool_t * pool)
{
    real_blob_t * blob = p_new(pool, real_blob_t, 1);

    blob->len  = 0;
    blob->size = INITIAL_BUFFER_SIZE;
    blob->data = p_new(pool, unsigned char, blob->size);
    blob->pool = pool;

    blob->data[blob->len] = 0;

    return (blob_t*) blob;
}

/* @see strdup(3) */
blob_t * blob_dup(pool_t * pool, blob_t * blob)
{
    real_blob_t * dst = p_new(pool, real_blob_t, 1);
    real_blob_t * src = REAL(blob);
    dst->len  = src->len;
    dst->size = src->size;
    dst->pool = pool;

    dst->data = p_new(pool, unsigned char, src->size);
    memcpy(dst, src, src->len+1); /* +1 for the blob_t \0 */

    return (blob_t*)dst;
}

/* XXX unlike strcat(3), blob_cat *creates* a new blob that is the
 * concatenation of two blobs.
 */
blob_t * blob_cat(pool_t * pool, blob_t * blob1, blob_t * blob2)
{
    blob_t * res = blob_dup(pool, blob1);
    blob_append(res, blob2);
    return res;
}

/* resize a blob to the new size.
 *
 * the min(blob->len, newlien) first bytes are preserved
 */
void blob_resize(blob_t * blob, ssize_t newlen)
{
    real_blob_t * rblob = REAL(blob);
    ssize_t newsize;
    
    if (rblob->size > newlen) {
        rblob->len = newlen;
        rblob->data[rblob->len] = 0;
        return;
    }

    newsize     = MEM_ALIGN(newlen+1);
    rblob->data = rblob->pool->realloc(rblob->data, newsize);
    rblob->len  = newlen;
    rblob->size = newsize;

    rblob->data[rblob->len] = 0;
}

/* delete a buffer. the pointer is set to 0 */
void blob_delete(blob_t ** blob)
{
    p_delete(REAL(*blob)->pool, REAL(*blob)->data);
    p_delete(REAL(*blob)->pool, *blob);
}

/******************************************************************************/
/* Blob properties                                                            */
/******************************************************************************/

/* returns the position of the first '\0' in the blob, or -1 if the blob has
   none */
ssize_t blob_is_cstr(blob_t * blob)
{
    ssize_t pos;
    for (pos = 0; pos < blob->len; pos++) {
        if (REAL(blob)->data[pos] == '\0') {
            return pos;
        }
    }
    return -1;
}

/******************************************************************************/
/* Blob manipulations                                                         */
/******************************************************************************/

void blob_append(blob_t * dest, blob_t * src)
{
    ssize_t oldlen = dest->len;

    blob_resize(dest, dest->len + src->len);
    memcpy(REAL(dest)->data + oldlen, src->data, src->len);
}

/******************************************************************************/
/* Blob comparisons                                                           */
/******************************************************************************/

/* @see memcmp(3) */
int blob_cmp(blob_t * blob1, blob_t * blob2)
{
    if (blob1->len == blob2->len) {
        return memcmp(blob1, blob2, blob1->len);
    }
    else {
        ssize_t len = MIN(blob1->len, blob2->len);
        int     res = memcmp(blob1, blob2, len);
        if (res != 0) {
            return res;
        }
        return (blob1->len == len) ? -1 : 1;
    }
}

/******************************************************************************/
/* Module init                                                                */
/******************************************************************************/

void blob_init(void)
{
}

void blob_deinit(void)
{
}

