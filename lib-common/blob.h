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

#ifndef IS_LIB_COMMON_BLOB_H
#define IS_LIB_COMMON_BLOB_H

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


/**************************************************************************/
/* Blob creation / deletion                                               */
/**************************************************************************/

blob_t *blob_init(blob_t *blob);
GENERIC_NEW(blob_t, blob);
void blob_wipe(blob_t *blob);
GENERIC_DELETE(blob_t, blob);

blob_t *blob_dup(const blob_t *blob);
blob_t *blob_cat(const blob_t *blob1, const blob_t *blob2);

#if 0
/* FIXME: this function is nasty, and has bad semantics: blob should know if
 * it owns the buffer. This makes the programmer write really horrible code
 * atm.
 */
void blob_set_payload(blob_t *blob, ssize_t len, void *buf, ssize_t bufsize);
#endif
void blob_resize(blob_t *blob, ssize_t newlen);

/**
 *  Get the const char * pointing to blob.data
 */
static inline const char *blob_get_cstr(const blob_t *blob)
{
    return (const char *)blob->data;
}

/**
 *  Get a pointer to blob.data as a byte *
 */
static inline byte *blob_get_data(blob_t *blob)
{
    return (byte *)(void *)blob->data;
}

/**************************************************************************/
/* Blob manipulations                                                     */
/**************************************************************************/

void blob_set(blob_t *dest, const blob_t *src);
void blob_set_data(blob_t *blob, const void *data, ssize_t len);
static inline void blob_set_cstr(blob_t *blob, const char *cstr) {
    blob_set_data(blob, cstr, strlen(cstr));
}

void blob_blit(blob_t *dest, ssize_t pos, const blob_t *src);
void blob_blit_data(blob_t *blob, ssize_t pos, const void *data, ssize_t len);
static inline void blob_blit_cstr(blob_t *blob, ssize_t pos, const char *cstr)
{
    blob_blit_data(blob, pos, cstr, strlen(cstr));
}

void blob_insert(blob_t *dest, ssize_t pos, const blob_t *src);
void blob_insert_data(blob_t *blob, ssize_t pos,
                      const void *data, ssize_t len);
static inline void 
blob_insert_cstr(blob_t *blob, ssize_t pos, const char *cstr)
{
    blob_insert_data(blob, pos, cstr, strlen(cstr));
}

void blob_insert_byte(blob_t *blob, byte b);

void blob_append(blob_t *dest, const blob_t *src);
void blob_append_data(blob_t *blob, const void *data, ssize_t len);
static inline void blob_append_cstr(blob_t *blob, const char *cstr)
{
    blob_append_data(blob, cstr, strlen(cstr));
}

void blob_append_byte(blob_t *blob, byte b);

void blob_kill_data(blob_t *blob, ssize_t pos, ssize_t len);
void blob_kill_first(blob_t *blob, ssize_t len);
void blob_kill_last(blob_t *blob, ssize_t len);

/**************************************************************************/
/* Blob file functions                                                    */
/**************************************************************************/

ssize_t blob_append_file_data(blob_t *blob, const char *filename);
ssize_t blob_append_fread(blob_t *blob, ssize_t size, ssize_t nmemb, FILE *f);
static inline
ssize_t blob_fread(blob_t *blob, ssize_t size, ssize_t nmemb, FILE *f)
{
    blob_resize(blob, 0);
    return blob_append_fread(blob, size, nmemb, f);
}

/* negative count means "auto" */
ssize_t blob_append_read(blob_t *blob, int fd, ssize_t count);

/**************************************************************************/
/* Blob printf functions                                                  */
/**************************************************************************/

ssize_t blob_append_vfmt(blob_t *blob, const char *fmt, va_list ap);
ssize_t blob_append_fmt(blob_t *blob, const char *fmt, ...)
        __attr_format__(2,3);

ssize_t blob_set_vfmt(blob_t *blob, const char *fmt, va_list ap);
ssize_t blob_set_fmt(blob_t *blob, const char *fmt, ...)
        __attr_format__(2,3);

/**************************************************************************/
/* Blob search functions                                                  */
/**************************************************************************/

/* not very efficent ! */

ssize_t blob_search(const blob_t *haystack, ssize_t pos,
                    const blob_t *needle);
ssize_t blob_search_data(const blob_t *haystack, ssize_t pos,
                         const void *needle, ssize_t len);
static inline ssize_t
blob_search_cstr(const blob_t *haystack, ssize_t pos, const char *needle)
{
    return blob_search_data(haystack, pos, needle, strlen(needle));
}

/**************************************************************************/
/* Blob filtering                                                         */
/**************************************************************************/

typedef int (blob_filter_func_t)(int);
void blob_map(blob_t *blob, blob_filter_func_t *filter);
void blob_map_range(blob_t *blob, ssize_t start, ssize_t end,
                    blob_filter_func_t *filter);

void blob_ltrim(blob_t *blob);
void blob_rtrim(blob_t *blob);
void blob_trim(blob_t *blob);

static inline void blob_tolower(blob_t *blob)
{
    blob_map(blob, &tolower);
}
static inline void blob_tolower_range(blob_t *blob,
                                      ssize_t start, ssize_t end)
{
    blob_map_range(blob, start, end, &tolower);
}

static inline void blob_toupper(blob_t *blob)
{
    blob_map(blob, &toupper);
}
static inline void blob_toupper_range(blob_t *blob,
                                      ssize_t start, ssize_t end)
{
    blob_map_range(blob, start, end, &toupper);
}

/**************************************************************************/
/* Blob comparisons                                                       */
/**************************************************************************/

int blob_cmp(const blob_t *blob1, const blob_t *blob2);
int blob_icmp(const blob_t *blob1, const blob_t *blob2);

bool blob_is_equal(const blob_t *blob1, const blob_t *blob2);
bool blob_is_iequal(const blob_t *blob1, const blob_t *blob2);

bool blob_cstart(const blob_t *blob, const char *p, const char **pp);
bool blob_start(const blob_t *blob1, const blob_t *blob2, const byte **pp);
bool blob_cistart(const blob_t *blob, const char *p, const char **pp);
bool blob_istart(const blob_t *blob1, const blob_t *blob2, const byte **pp);

/**************************************************************************/
/* Blob string functions                                                  */
/**************************************************************************/

void blob_urldecode(blob_t *url);
void blob_b64encode(blob_t *blob, int nbpackets);

/**************************************************************************/
/* Blob parsing                                                           */
/**************************************************************************/

#include "parse.h"

ssize_t blob_parse_cstr(const blob_t *blob, ssize_t *pos,
                        const char **answer);


/**************************************************************************/
/* Blob compression/decompression                                         */
/**************************************************************************/

int blob_file_gzip(blob_t *dst, const char *filename);
int blob_file_gunzip(blob_t *dst, const char *filename);

int blob_gzip_compress(blob_t *dest, const blob_t *src);
int blob_gzip_uncompress(blob_t *dest, const blob_t *src);

int blob_zlib_compress(blob_t *dest, blob_t *src);
int blob_zlib_uncompress(blob_t *dest, blob_t *src);

int blob_raw_compress(blob_t *dest, blob_t *src);
int blob_raw_uncompress(blob_t *dest, blob_t *src);


/**************************************************************************/
/* Blob encoding                                                          */
/**************************************************************************/

#define ENC_ASCII        0
#define ENC_ISO_8859_1   1
#define ENC_WINDOWS_1250 2
#define ENC_WINDOWS_1252 3
#define ENC_UTF8         4
int blob_iconv(blob_t *dst, const blob_t *src, const char *type);
int blob_iconv_close_all(void);
/* FIXME? : type_hint is a const char * but chosen_encoding is and int...
 *          shouldn't it be more homogeneous ? */
int blob_auto_iconv(blob_t *dst, const blob_t *src,
                    const char *type_hint, int *chosen_encoding);
int blob_file_auto_iconv(blob_t *dst, const char *filename,
                         const char *type_hint, int *chosen_encoding);
int blob_encode_html(blob_t *dst, const blob_t *src);
int blob_encode_base64(blob_t *dst, const blob_t *src);
int blob_encode_quoted_printable(blob_t *dst, const blob_t *src);
int blob_encode_ia5(blob_t *dst, const blob_t *src);

/*[ CHECK ]::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::{{{*/
#ifdef CHECK
#include <check.h>

Suite *check_make_blob_suite(void);

#endif
/*:::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::}}}*/
#endif /* IS_LIB_COMMON_BLOB_H */
