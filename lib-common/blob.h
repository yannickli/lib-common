#ifndef IS_BLOB_H
#define IS_BLOB_H

#include <ctype.h>
#include <unistd.h>
#include <inttypes.h>

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

void blob_kill_data(blob_t * blob, ssize_t pos, ssize_t len);
void blob_kill_first(blob_t * blob, ssize_t len);
void blob_kill_last(blob_t * blob, ssize_t len);

/******************************************************************************/
/* Blob filtering                                                             */
/******************************************************************************/

typedef int (blob_filter_func_t)(int);
void blob_map(blob_t * blob, blob_filter_func_t * filter);
void blob_map_range(blob_t * blob, ssize_t start, ssize_t end, blob_filter_func_t * filter);

void blob_ltrim(blob_t * blob);
void blob_rtrim(blob_t * blob);
void blob_trim(blob_t * blob);

static inline void blob_tolower(blob_t * blob)
{
    blob_map(blob, &tolower);
}
static inline void blob_tolower_range(blob_t * blob, ssize_t start, ssize_t end)
{
    blob_map_range(blob, start, end, &tolower);
}

static inline void blob_toupper(blob_t * blob)
{
    blob_map(blob, &toupper);
}
static inline void blob_toupper_range(blob_t * blob, ssize_t start, ssize_t end)
{
    blob_map_range(blob, start, end, &toupper);
}

/******************************************************************************/
/* Blob comparisons                                                           */
/******************************************************************************/

int blob_cmp(blob_t * blob1, blob_t * blob2);

/******************************************************************************/
/* Blob parsing                                                               */
/******************************************************************************/

enum blob_parse_status {
    BP_OK     = 0,
    BP_EPARSE = -1,
    BP_ERANGE = -2,
};

static inline bool blob_eop(blob_t * blob, ssize_t pos) {
    return blob->len == pos;
}

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

/** wsp types **/

int     blob_parse_uint8 (blob_t * blob, ssize_t *pos, uint8_t  * answer);
int     blob_parse_uint16(blob_t * blob, ssize_t *pos, uint16_t * answer);
int     blob_parse_uint32(blob_t * blob, ssize_t *pos, uint32_t * answer);
int     blob_parse_uintv (blob_t * blob, ssize_t *pos, uint32_t * answer);


/******************************************************************************/
/* Module init                                                                */
/******************************************************************************/

void blob_init(void);
void blob_deinit(void);

#endif
