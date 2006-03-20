#ifndef IS_BLOB_H
#define IS_BLOB_H

#include <ctype.h>
#include <unistd.h>
#include <inttypes.h>
#include <time.h>
#include <stdio.h>

#include "macros.h"
#include "mem.h"

#define BLOB_INITIAL_SIZE 32

typedef struct {
    const ssize_t len;
    const byte * const data;

    /* HERE SO THAT sizeof(array) is ok */
    void const * const __area;
    ssize_t const      __size;
    byte const         __init[BLOB_INITIAL_SIZE];
} blob_t;


/******************************************************************************/
/* Blob creation / deletion                                                   */
/******************************************************************************/

#define  blob_new() blob_init(p_new_raw(blob_t, 1))
blob_t *blob_init(blob_t *blob);
void blob_wipe(blob_t *blob);
static inline void blob_delete(blob_t **blob)
{
    GENERIC_DELETE(blob_wipe, blob);
}

blob_t *blob_dup(const blob_t *blob);
blob_t *blob_cat(const blob_t *blob1, const blob_t *blob2);

/* FIXME: this function is nasty, and has a bad semantics: blob should know if
 * it owns the buffer. This makes the programmer write really horrible code
 * atm
 */
void blob_set_payload(blob_t *blob, ssize_t len, void *buf, ssize_t bufsize);
void blob_resize(blob_t *blob, ssize_t newlen);

/******************************************************************************/
/* Blob manipulations                                                         */
/******************************************************************************/

void blob_set(blob_t *dest, const blob_t *src);
void blob_set_data(blob_t *blob, const void *data, ssize_t len);
void blob_set_cstr(blob_t *blob, const char *cstr);

void blob_blit(blob_t *dest, ssize_t pos, const blob_t *src);
void blob_blit_data(blob_t *blob, ssize_t pos, const void *data, ssize_t len);
void blob_blit_cstr(blob_t *blob, ssize_t pos, const char *cstr);

void blob_insert(blob_t *dest, ssize_t pos, const blob_t *src);
void blob_insert_data(blob_t *blob, ssize_t pos, const void *data, ssize_t len);
void blob_insert_cstr(blob_t *blob, ssize_t pos, const char *cstr);

void blob_append(blob_t *dest, const blob_t *src);
void blob_append_data(blob_t *blob, const void *data, ssize_t len);
void blob_append_cstr(blob_t *blob, const char *cstr);

void blob_append_byte(blob_t *blob, byte b);

void blob_kill_data(blob_t *blob, ssize_t pos, ssize_t len);
void blob_kill_first(blob_t *blob, ssize_t len);
void blob_kill_last(blob_t *blob, ssize_t len);

/******************************************************************************/
/* Blob file functions                                                        */
/******************************************************************************/

ssize_t blob_append_file_data(blob_t *blob, const char *filename);
ssize_t blob_append_fread(blob_t *blob, ssize_t size, ssize_t nmemb, FILE *f);
static inline
ssize_t blob_fread(blob_t *blob, ssize_t size, ssize_t nmemb, FILE *f)
{
    blob_resize(blob, 0);
    return blob_append_fread(blob, size, nmemb, f);
}

/******************************************************************************/
/* Blob printf function                                                       */
/******************************************************************************/

ssize_t blob_vprintf(blob_t *blob, ssize_t pos, const char *fmt, va_list ap);
ssize_t blob_printf(blob_t *blob, ssize_t pos, const char *fmt, ...) __attr_format__(3,4);
ssize_t blob_ftime(blob_t *blob, ssize_t pos, const char *fmt, const struct tm *tm);
static inline void blob_ftime_utc(blob_t *blob, ssize_t pos, time_t tm)
{
    blob_ftime(blob, pos, "%a, %d %b %Y %H:%M:%S GMT", gmtime(&tm));
}

/******************************************************************************/
/* Blob search functions                                                      */
/******************************************************************************/

/* not very efficent ! */

ssize_t blob_search(const blob_t *haystack, ssize_t pos, const blob_t *needle);
ssize_t blob_search_data(const blob_t *haystack, ssize_t pos, const void *needle, ssize_t len);
ssize_t blob_search_cstr(const blob_t *haystack, ssize_t pos, const char *needle);

/******************************************************************************/
/* Blob filtering                                                             */
/******************************************************************************/

typedef int (blob_filter_func_t)(int);
void blob_map(blob_t *blob, blob_filter_func_t *filter);
void blob_map_range(blob_t *blob, ssize_t start, ssize_t end, blob_filter_func_t *filter);

void blob_ltrim(blob_t *blob);
void blob_rtrim(blob_t *blob);
void blob_trim(blob_t *blob);

static inline void blob_tolower(blob_t *blob)
{
    blob_map(blob, &tolower);
}
static inline void blob_tolower_range(blob_t *blob, ssize_t start, ssize_t end)
{
    blob_map_range(blob, start, end, &tolower);
}

static inline void blob_toupper(blob_t *blob)
{
    blob_map(blob, &toupper);
}
static inline void blob_toupper_range(blob_t *blob, ssize_t start, ssize_t end)
{
    blob_map_range(blob, start, end, &toupper);
}

/******************************************************************************/
/* Blob comparisons                                                           */
/******************************************************************************/

int blob_cmp(const blob_t *blob1, const blob_t *blob2);
int blob_icmp(const blob_t *blob1, const blob_t *blob2);

bool blob_is_equal(const blob_t *blob1, const blob_t *blob2);
bool blob_is_iequal(const blob_t *blob1, const blob_t *blob2);

/******************************************************************************/
/* Blob string functions                                                      */
/******************************************************************************/

void blob_urldecode(blob_t *url);
void blob_b64encode(blob_t *blob, int nbpackets);

/******************************************************************************/
/* Blob parsing                                                               */
/******************************************************************************/

#include "parse.h"

ssize_t blob_parse_cstr(const blob_t *blob, ssize_t *pos, const char ** answer);

/*[ CHECK ]::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::{{{*/
#ifdef CHECK
#include <check.h>

Suite *check_make_blob_suite(void);

#endif
/*:::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::}}}*/
#endif
