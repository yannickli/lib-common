#include <stdlib.h>
#include <errno.h>

#include "macros.h"
#include "blob.h"
#include "mem_pool.h"
#include "string_is.h"


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

void blob_insert(blob_t * dest, ssize_t pos, blob_t * src)
{
    blob_insert_data(dest, pos, src->data, src->len);
}

        
/* insert `len' data C octets into a blob.
   `pos' gives the posistion in `blob' where `data' should be inserted.

   if the given `pos' is greater than the length of the input octet string,
   the data is appended.
 */
void blob_insert_data(blob_t * blob, ssize_t pos, const void * data, ssize_t len)
{
    ssize_t oldlen = blob->len;

    if (len == 0) {
        return;
    }

    blob_resize(blob, blob->len + len);
    if (oldlen > pos) {
        memmove(REAL(blob)->data + pos + len, blob->data + pos, oldlen - pos);
    }
    memcpy(REAL(blob)->data + pos, data, len);
}

/* don't insert the NUL ! */
void blob_insert_cstr(blob_t * blob, ssize_t pos, const unsigned char * cstr)
{
    blob_insert_data(blob, pos, cstr, sstrlen(cstr));
}

#define BLOB_APPEND_DATA(blob, data, data_len)    \
    blob_insert_data((blob), (blob)->len, data, data_len)

void blob_append(blob_t * dest, blob_t * src)
{
    BLOB_APPEND_DATA(dest, src->data, src->len);
}

void blob_append_data(blob_t * blob, const void * data, ssize_t len)
{
    BLOB_APPEND_DATA(blob, data, len);
}

void blob_append_cstr(blob_t * blob, const unsigned char * cstr)
{
    BLOB_APPEND_DATA(blob, cstr, sstrlen(cstr));
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
/* Blob parsing                                                               */
/******************************************************************************/

/* try to parse a c-string from the current position in the buffer.

   pos will be incremented to the position right after the NUL
   answer is a pointer *in* the blob, so you have to strdup it if needed.

   @returns :

     positive number or 0
       represent the length of the parsed string (not
       including the leading \0)

     BP_EPARSE
       no \0 was found before the end of the blob
 */

ssize_t blob_parse_cstr(blob_t * blob, ssize_t * pos, const unsigned char ** answer)
{
    real_blob_t * rblob = REAL(blob);
    ssize_t walk = *pos;

    while (walk < blob->len) {
        if (rblob->data[walk] != '\0') {
            ssize_t len = walk - *pos;
            *answer = rblob->data + *pos;
            *pos    = walk+1;
            return len;
        }
        walk ++;
    }

    return BP_EPARSE;
}

/* @returns :

     0         : no error
     BP_EPARSE : equivalent to EINVAL for strtol(3)
     BP_ERANGE : resulting value out of range.
 */

int blob_parse_long(blob_t * blob, ssize_t * pos, int base, long * answer)
{
    char *  endptr;
    ssize_t endpos;
    long    number;
    
    number = strtol(blob->data + *pos, &endptr, base);
    endpos = (void *)endptr - blob->data;

    if (errno == ERANGE) {
        return BP_ERANGE;
    }
    if (errno == EINVAL || endpos > blob->len) {
        return BP_EPARSE;
    }

    *answer = number;
    *pos    = endpos;
    return 0;
    
}

int blob_parse_double(blob_t * blob, ssize_t * pos, double * answer)
{
    char *  endptr;
    ssize_t endpos;
    double  number;
    
    number = strtod(blob->data + *pos, &endptr);
    endpos = (void *)endptr - blob->data;

    if (errno == ERANGE) {
        return BP_ERANGE;
    }
    if (endpos > blob->len) {
        return BP_EPARSE;
    }

    *answer = number;
    *pos    = endpos;
    return 0;
    
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

