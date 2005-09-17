#ifndef IS_BLOB_H
#define IS_BLOB_H

#include <ctype.h>
#include <unistd.h>
#include <inttypes.h>

#include "macros.h"
#include "mem.h"

typedef struct {
    const ssize_t len;
    const byte * const data;

    /* HERE SO THAT sizeof(array) is ok */
    void const * const __area;
    ssize_t const      __size;
} blob_t;


/******************************************************************************/
/* Blob creation / deletion                                                   */
/******************************************************************************/

#define  blob_new() blob_init(p_new_raw(blob_t, 1))
blob_t * blob_init(blob_t * blob);
void     blob_wipe(blob_t * blob);
static inline void blob_delete(blob_t **blob)
{
    GENERIC_DELETE(blob_wipe, blob);
}

blob_t * blob_dup(const blob_t * blob);
blob_t * blob_cat(const blob_t * blob1, const blob_t * blob2);

void     blob_resize(blob_t * blob, ssize_t newlen);

/******************************************************************************/
/* Blob manipulations                                                         */
/******************************************************************************/

void blob_set(blob_t * dest, const blob_t * src);
void blob_set_data(blob_t * blob, const void * data, ssize_t len);
void blob_set_cstr(blob_t * blob, const char * cstr);

void blob_blit(blob_t * dest, ssize_t pos, const blob_t * src);
void blob_blit_data(blob_t * blob, ssize_t pos, const void * data, ssize_t len);
void blob_blit_cstr(blob_t * blob, ssize_t pos, const char * cstr);

void blob_insert(blob_t * dest, ssize_t pos, const blob_t * src);
void blob_insert_data(blob_t * blob, ssize_t pos, const void * data, ssize_t len);
void blob_insert_cstr(blob_t * blob, ssize_t pos, const char * cstr);

void blob_append(blob_t * dest, const blob_t * src);
void blob_append_data(blob_t * blob, const void * data, ssize_t len);
void blob_append_cstr(blob_t * blob, const char * cstr);

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

int blob_cmp(const blob_t * blob1, const blob_t * blob2);
int blob_icmp(const blob_t * blob1, const blob_t * blob2);

bool blob_is_equal(const blob_t * blob1, const blob_t * blob2);
bool blob_is_iequal(const blob_t * blob1, const blob_t * blob2);

/******************************************************************************/
/* Blob parsing                                                               */
/******************************************************************************/

#include "parse.h"

ssize_t blob_parse_cstr(const blob_t * blob, ssize_t * pos, const char ** answer);
int     blob_parse_long(const blob_t * blob, ssize_t * pos, int base, long * answer);
int     blob_parse_double(const blob_t * blob, ssize_t * pos, double * answer);

/*[ CHECK ]::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::{{{*/
#ifdef CHECK
#include <check.h>

Suite *check_make_blob_suite(void);

#endif
/*:::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::}}}*/
#endif
