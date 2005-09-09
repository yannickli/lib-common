#ifndef IS_BLOB_H
#define IS_BLOB_H

#include <unistd.h>

#include "mem_pool.h"
#include "macros.h"

typedef struct {
    const ssize_t len;
    const void * const data;
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
/* Blob properties                                                            */
/******************************************************************************/

ssize_t blob_is_cstr(blob_t * blob);

/******************************************************************************/
/* Blob manipulations                                                         */
/******************************************************************************/

void blob_blit(blob_t * dest, ssize_t pos, blob_t * src);
void blob_blit_data(blob_t * blob, ssize_t pos, const void * data, ssize_t len);
void blob_blit_cstr(blob_t * blob, ssize_t pos, const unsigned char * cstr);

void blob_insert(blob_t * dest, ssize_t pos, blob_t * src);
void blob_insert_data(blob_t * blob, ssize_t pos, const void * data, ssize_t len);
void blob_insert_cstr(blob_t * blob, ssize_t pos, const unsigned char * cstr);

void blob_append(blob_t * dest, blob_t * src);
void blob_append_data(blob_t * blob, const void * data, ssize_t len);
void blob_append_cstr(blob_t * blob, const unsigned char * cstr);

/******************************************************************************/
/* Blob comparisons                                                           */
/******************************************************************************/

int blob_cmp(blob_t * blob1, blob_t * blob2);

/******************************************************************************/
/* Blob parsing                                                               */
/******************************************************************************/

enum blob_parse_status {
    BP_EPARSE = -1,
    BP_ERANGE = -2,
};

/* general things about parsing functions :
 
Arguments :
   in case of successful parse, pos is positionned at the octet just after the
   last parsed octet, or after NULs in case of c-str-like tokens.

   in case of error, pos, answer and other function-modified arguments are never
   modified.

Return values:
   
   negative return values are always an error.
   0 return value is always a success.
   positive return values are a success, and generally have a special meaning.

Notes:

   parse function assume pos is in the blob.
   though, they may return a pos that is equal to blob->len, meaning that the
   parse has reach its end.
 */

ssize_t blob_parse_cstr(blob_t * blob, ssize_t * pos, const unsigned char ** answer);
int     blob_parse_long(blob_t * blob, ssize_t * pos, int base, long * answer);
int     blob_parse_double(blob_t * blob, ssize_t * pos, double * answer);

/******************************************************************************/
/* Module init                                                                */
/******************************************************************************/

void blob_init(void);
void blob_deinit(void);

#endif
