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

#ifndef IS_LIB_COMMON_BLOB_H
#define IS_LIB_COMMON_BLOB_H

#include <ctype.h>
#include <unistd.h>
#include <inttypes.h>
#include <time.h>
#include <stdio.h>
#include <assert.h>

#include "macros.h"
#include "mem.h"

#define BLOB_INITIAL_SIZE 32

/*
 * A blob has a vital invariant, making every parse function avoid
 * buffer read overflows: there is *always* a '\0' in the data at
 * position len, implying that size is always >= len+1
 */
typedef struct blob_t {
    /* public interface */
    ssize_t len;
    byte *data;

    /* private interface */
    byte *area;   /* originally allocated block */
    ssize_t size;  /* allocated size */
    byte initial[BLOB_INITIAL_SIZE];
} blob_t;

/**************************************************************************/
/* Blob creation / deletion                                               */
/**************************************************************************/

static inline blob_t *blob_init(blob_t *blob) {
    blob->len  = 0;
    blob->size = BLOB_INITIAL_SIZE;
    blob->area = NULL;
    blob->data = blob->initial;

    /* setup invariant: blob is always NUL terminated */
    blob->data[blob->len] = '\0';

    return blob;
}
static inline void blob_wipe(blob_t *blob) {
    p_delete(&blob->area);
    /* Set the data pointer to NULL to catch errors more easily */
    blob->data = NULL;
}

GENERIC_NEW(blob_t, blob);
GENERIC_DELETE(blob_t, blob);

/* Get the const char * pointing to blob.data */
static inline const char *blob_get_cstr(const blob_t *blob) {
    return (const char *)blob->data;
}


/**************************************************************************/
/* Blob size/len manipulations                                            */
/**************************************************************************/

static inline void blob_reset(blob_t *blob) {
    /* Remove initial skip if any */
    /* Do not release memory */
    if (blob->area) {
        blob->size += (blob->data - blob->area);
        blob->data = blob->area;
    } else {
        blob->size = BLOB_INITIAL_SIZE;
        blob->data = blob->initial;
    }
    blob->len = 0;
    blob->data[blob->len] = '\0';
}

static inline void blob_reinit(blob_t *blob) {
    blob_wipe(blob);
    blob_init(blob);
}

void blob_ensure(blob_t *blob, ssize_t newlen);

static inline void blob_ensure_avail(blob_t *blob, ssize_t extralen) {
    assert (extralen >= 0);

    if (blob->len + extralen >= blob->size) {
        blob_ensure(blob, blob->len + extralen);
    }
}

static inline void blob_resize(blob_t *blob, ssize_t newlen) {
    if (newlen <= 0) {
        blob_reset(blob);
    } else {
        blob_ensure(blob, newlen);
        blob->len = newlen;
        blob->data[blob->len] = '\0';
    }
}

static inline void blob_extend(blob_t *blob, ssize_t extralen) {
    assert (extralen >= 0);

    blob_ensure_avail(blob, extralen);
    blob->len += extralen;
    blob->data[blob->len] = '\0';
}

static inline void blob_extend2(blob_t *blob, ssize_t extralen, byte init) {
    assert (extralen >= 0);

    blob_ensure_avail(blob, extralen);
    memset(blob->data + blob->len, init, extralen);
    blob->len += extralen;
    blob->data[blob->len] = '\0';
}

/**************************************************************************/
/* Blob manipulations                                                     */
/**************************************************************************/

blob_t *blob_dup(const blob_t *blob);

void blob_blit(blob_t *dest, ssize_t pos, const blob_t *src);
void blob_blit_data(blob_t *blob, ssize_t pos, const void *data, ssize_t len);
static inline void blob_blit_cstr(blob_t *blob, ssize_t pos, const char *cstr) {
    blob_blit_data(blob, pos, cstr, strlen(cstr));
}

void blob_insert(blob_t *dest, ssize_t pos, const blob_t *src);
void blob_insert_data(blob_t *blob, ssize_t pos,
                      const void *data, ssize_t len);
static inline void 
blob_insert_cstr(blob_t *blob, ssize_t pos, const char *cstr) {
    blob_insert_data(blob, pos, cstr, strlen(cstr));
}
void blob_insert_byte(blob_t *blob, byte b);

void blob_splice_data(blob_t *blob, ssize_t pos, ssize_t len,
                      const void *data, ssize_t datalen);

static inline void
blob_splice_cstr(blob_t *dest, ssize_t pos, ssize_t len, const char *src) {
    blob_splice_data(dest, pos, len, src, strlen(src));
}

static inline void
blob_splice(blob_t *dest, ssize_t pos, ssize_t len, const blob_t *src) {
    blob_splice_data(dest, pos, len, src->data, src->len);
}

/*** appends ***/

static inline void 
blob_append_data(blob_t *blob, const void *data, ssize_t len) {
    blob_ensure_avail(blob, len);
    memcpy(blob->data + blob->len, data, len);
    blob->len += len;
    blob->data[blob->len] = '\0';
}
static inline void blob_append_cstr(blob_t *blob, const char *cstr) {
    blob_append_data(blob, cstr, strlen(cstr));
}
static inline void blob_append(blob_t *dest, const blob_t *src) {
    blob_append_data(dest, src->data, src->len);
}
static inline void blob_append_byte(blob_t *blob, byte b) {
    blob_extend2(blob, 1, b);
}
void blob_append_cstr_escaped2(blob_t *blob, const char *cstr,
                               const char *toescape, const char *escaped);
static inline void blob_append_cstr_escaped(blob_t *blob, const char *cstr,
                                            const char *toescape) {
    blob_append_cstr_escaped2(blob, cstr, toescape, toescape);
}


/*** copy ***/

static inline void blob_set_data(blob_t *blob, const void *data, ssize_t len) {
    blob_reset(blob);
    blob_append_data(blob, data, len);
}
static inline void blob_set(blob_t *dest, const blob_t *src) {
    blob_set_data(dest, src->data, src->len);
}
static inline void blob_set_cstr(blob_t *blob, const char *cstr) {
    blob_set_data(blob, cstr, strlen(cstr));
}


/*** kills ***/

void blob_kill_data(blob_t *blob, ssize_t pos, ssize_t len);
static inline void blob_kill_first(blob_t *blob, ssize_t len) {
    if (len < blob->len) {
        blob->data += len;
        blob->size -= len;
        blob->len  -= len;
    } else {
        blob_reset(blob);
    }
}
static inline void blob_kill_last(blob_t *blob, ssize_t len) {
    if (len < blob->len) {
        blob->len  -= len;
        blob->data[blob->len] = '\0';
    } else {
        blob_reset(blob);
    }
}


/**************************************************************************/
/* Blob file functions                                                    */
/**************************************************************************/

ssize_t blob_append_file_data(blob_t *blob, const char *filename);
ssize_t blob_append_fread(blob_t *blob, ssize_t size, ssize_t nmemb, FILE *f);
static inline
ssize_t blob_fread(blob_t *blob, ssize_t size, ssize_t nmemb, FILE *f) {
    blob_reset(blob);
    return blob_append_fread(blob, size, nmemb, f);
}
ssize_t blob_append_fgets(blob_t *blob, FILE *f);

/* negative count means "auto" */
ssize_t blob_append_read(blob_t *blob, int fd, ssize_t count);

ssize_t blob_append_recv(blob_t *blob, int fd, ssize_t count);

ssize_t blob_save_to_file(blob_t *blob, const char *filename);

/**************************************************************************/
/* Blob printf functions                                                  */
/**************************************************************************/

ssize_t blob_append_vfmt(blob_t *blob, const char *fmt, va_list ap)
	__attr_printf__(2,0);
ssize_t blob_append_fmt(blob_t *blob, const char *fmt, ...)
        __attr_printf__(2,3);

ssize_t blob_set_vfmt(blob_t *blob, const char *fmt, va_list ap)
	__attr_printf__(2,0);
ssize_t blob_set_fmt(blob_t *blob, const char *fmt, ...)
        __attr_printf__(2,3);

int blob_pack(blob_t *blob, const char *fmt, ...);
int blob_unpack(const blob_t *blob, int *pos, const char *fmt, ...)
    __attribute__((warn_unused_result));
int buf_unpack(const byte *buf, int buf_len,
               int *pos, const char *fmt, ...)
    __attribute__((warn_unused_result));

/**************************************************************************/
/* Blob search functions                                                  */
/**************************************************************************/

/* not very efficent ! */

ssize_t blob_search(const blob_t *haystack, ssize_t pos,
                    const blob_t *needle);
ssize_t blob_search_data(const blob_t *haystack, ssize_t pos,
                         const void *needle, ssize_t len);
static inline ssize_t
blob_search_cstr(const blob_t *haystack, ssize_t pos, const char *needle) {
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

static inline void blob_tolower(blob_t *blob) {
    blob_map(blob, &tolower);
}
static inline void blob_tolower_range(blob_t *blob,
                                      ssize_t start, ssize_t end) {
    blob_map_range(blob, start, end, &tolower);
}

static inline void blob_toupper(blob_t *blob) {
    blob_map(blob, &toupper);
}
static inline void blob_toupper_range(blob_t *blob,
                                      ssize_t start, ssize_t end) {
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
int blob_append_xml_escape(blob_t *dst, const byte *src, ssize_t len);
int blob_append_quoted_printable(blob_t *dst, const byte *src, ssize_t len);
int blob_append_base64(blob_t *dst, const byte *src, ssize_t len, int width);
int blob_append_smtp_data(blob_t *dst, const byte *src, ssize_t len);
int blob_append_hex(blob_t *dst, const byte *src, ssize_t len);

/* in blob_emi.c */
int blob_append_ira(blob_t *dst, const byte *src, ssize_t len);
int blob_decode_ira(blob_t *dst, const byte *src, ssize_t len);
int string_decode_ira(char *dst, const char *src);

/* in blob_ebcdic.c */
int blob_decode_ebcdic297(blob_t *dst, const byte *src, ssize_t len);

/* in blob_utf8.c */
/* OG: should inline this */
int blob_utf8_putc(blob_t *out, int c);

ssize_t blob_latin1_to_utf8(blob_t *out, const char *s, int len);
ssize_t blob_latin9_to_utf8(blob_t *out, const char *s, int len);

/*[ CHECK ]::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::{{{*/
#ifdef CHECK
#include <check.h>

Suite *check_make_blob_suite(void);
Suite *check_append_blob_ebcdic_suite(Suite *blob_suite);

#endif
/*:::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::}}}*/
#endif /* IS_LIB_COMMON_BLOB_H */
