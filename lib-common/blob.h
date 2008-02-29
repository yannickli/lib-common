/**************************************************************************/
/*                                                                        */
/*  Copyright (C) 2004-2008 INTERSEC SAS                                  */
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

#include "mem.h"

/*
 * A blob has a vital invariant, making every parse function avoid
 * buffer read overflows: there is *always* a '\0' in the data at
 * position len, implying that size is always >= len+1
 *
 * When allocated, the start of the data is at .data - .skip.
 */
typedef struct blob_t {
    byte *data;
    int len, size;
    flag_t allocated :  1;
    flag_t skip      : 31;
} blob_t;

/* XXX: a small amount of information used to *never* have a NULL ->data
 * member It should always stay equal to \0 and is writeable so that code
 * enforcing the invariant puting \0 at that place do work.
 */
extern byte blob_slop[1];

#define BLOB_STATIC_INIT  (blob_t){ .data = blob_slop, .size = 1 }

/**************************************************************************/
/* Blob creation / deletion                                               */
/**************************************************************************/

static inline void blob_init2(blob_t *blob, void *buf, int size) {
    assert (size > 0);
    *blob = (blob_t){ .data = buf, .size = size };
    blob->data[0] = '\0';
}
static inline blob_t *blob_init(blob_t *blob) {
    *blob = BLOB_STATIC_INIT;
    /* setup invariant: blob is always NUL terminated */
    assert (blob->data[0] == '\0');
    return blob;
}
static inline void blob_wipe(blob_t *blob) {
    if (blob->allocated) {
        mem_free(blob->data - blob->skip);
    }
    blob_init(blob);
}
#define blob_reinit(b)  blob_wipe(b)
char *blob_detach(blob_t *blob);

GENERIC_NEW(blob_t, blob);
GENERIC_DELETE(blob_t, blob);

blob_t *blob_dup(const blob_t *blob);

/* Get the const char * pointing to blob.data */
static inline const char *blob_get_cstr(const blob_t *blob) {
    return (const char *)blob->data;
}

/* Get the pointer to the NUL at the end of the blob */
static inline const char *blob_get_end(const blob_t *blob) {
    return (const char *)blob->data + blob->len;
}


/**************************************************************************/
/* Blob size/len manipulations                                            */
/**************************************************************************/

static inline void blob_reset(blob_t *blob) {
    /* Remove initial skip if any, and don't release memory */
    blob->size += blob->skip;
    blob->data -= blob->skip;
    blob->skip = 0;
    blob->data[blob->len = 0] = '\0';
}

void blob_ensure(blob_t *blob, int newlen);

static inline void blob_grow(blob_t *blob, int extralen) {
    assert (extralen >= 0);

    if (blob->len + extralen >= blob->size) {
        blob_ensure(blob, blob->len + extralen);
    }
}

static inline void blob_setlen(blob_t *blob, int newlen) {
    assert (newlen >= 0);
    if (newlen <= 0) {
        blob_reset(blob);
    } else {
        blob_ensure(blob, newlen);
        blob->len = newlen;
        blob->data[blob->len] = '\0';
    }
}

static inline void blob_extend(blob_t *blob, int extralen) {
    assert (extralen >= 0);
    blob_setlen(blob, blob->len + extralen);
}

static inline void blob_extend2(blob_t *blob, int extralen, byte init) {
    assert (extralen >= 0);
    blob_grow(blob, extralen);
    memset(blob->data + blob->len, init, extralen);
    blob->len += extralen;
    blob->data[blob->len] = '\0';
}

/**************************************************************************/
/* Blob manipulations                                                     */
/**************************************************************************/

static inline void
blob_splice_data(blob_t *blob, int pos, int len, const void *data, int dlen)
{
    assert (pos >= 0 && len >= 0 && dlen >= 0);

    if (pos > blob->len)
        pos = blob->len;
    if ((unsigned)pos + len > (unsigned)blob->len)
        len = blob->len - pos;
    if (pos == 0 && len + blob->skip >= dlen) {
        blob->skip += len - dlen;
        blob->data += len - dlen;
        blob->size -= len - dlen;
        blob->len  -= len - dlen;
        len = dlen;
    } else
    if (len != dlen) {
        blob_ensure(blob, blob->len + dlen - len);
        p_move(blob->data, pos + dlen, pos + len, blob->len - pos - len);
        blob->len += dlen - len;
        blob->data[blob->len] = '\0';
    }
    memcpy(blob->data + pos, data, dlen);
}
static inline void
blob_splice_cstr(blob_t *dest, int pos, int len, const char *src) {
    blob_splice_data(dest, pos, len, src, strlen(src));
}
static inline void
blob_splice(blob_t *dest, int pos, int len, const blob_t *src) {
    blob_splice_data(dest, pos, len, src->data, src->len);
}
static inline void blob_kill_data(blob_t *blob, int pos, int len) {
    blob_splice_data(blob, pos, len, NULL, 0);
}
static inline void blob_kill_first(blob_t *blob, int len) {
    blob_splice_data(blob, 0, len, NULL, 0);
}
static inline void blob_kill_last(blob_t *blob, int len) {
    if (len < blob->len) {
        blob->len -= len;
        blob->data[blob->len] = '\0';
    } else {
        blob_reset(blob);
    }
}
/* OG: should rename to blob_kill_upto */
static inline void blob_kill_at(blob_t *blob, const char *s) {
    blob_kill_first(blob, s - blob_get_cstr(blob));
}
static inline void blob_insert(blob_t *dest, int pos, const blob_t *src) {
    blob_splice_data(dest, pos, 0, src->data, src->len);
}
static inline void blob_insert_cstr(blob_t *blob, int pos, const char *s) {
    blob_splice_data(blob, pos, 0, s, strlen(s));
}
static inline void
blob_insert_data(blob_t *blob, int pos, const void *data, int len) {
    blob_splice_data(blob, pos, 0, data, len);
}

/*** appends ***/

static inline void
blob_append_data(blob_t *blob, const void *data, int len) {
    blob_grow(blob, len);
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
void blob_append_data_escaped2(blob_t *blob, const byte *cstr, size_t len,
                               const char *toescape, const char *escaped);
static inline void blob_append_cstr_escaped2(blob_t *blob, const char *cstr,
                               const char *toescape, const char *escaped) {
    blob_append_data_escaped2(blob, (const byte *)cstr, strlen(cstr),
                              toescape, escaped);
}
static inline void
blob_append_cstr_escaped(blob_t *blob, const char *cstr, const char *toescape)
{
    blob_append_cstr_escaped2(blob, cstr, toescape, toescape);
}


/*** copy ***/

static inline void blob_set_data(blob_t *blob, const void *data, int len) {
    blob_reset(blob);
    blob_append_data(blob, data, len);
}
static inline void blob_set(blob_t *dest, const blob_t *src) {
    blob_set_data(dest, src->data, src->len);
}
static inline void blob_set_cstr(blob_t *blob, const char *cstr) {
    blob_set_data(blob, cstr, strlen(cstr));
}


/**************************************************************************/
/* Blob file functions                                                    */
/**************************************************************************/

int blob_append_file_data(blob_t *blob, const char *filename);
int blob_append_fread(blob_t *blob, int size, int nmemb, FILE *f);
static inline
int blob_fread(blob_t *blob, int size, int nmemb, FILE *f) {
    blob_reset(blob);
    return blob_append_fread(blob, size, nmemb, f);
}
int blob_append_fgets(blob_t *blob, FILE *f);

/* negative count means "auto" */
int blob_append_read(blob_t *blob, int fd, int count);

int blob_append_recv(blob_t *blob, int fd, int count);

struct sockaddr;
int blob_append_recvfrom(blob_t *blob, int fd, int count, int flags,
                             struct sockaddr *from, socklen_t *fromlen);

int blob_save_to_file(blob_t *blob, const char *filename);

/**************************************************************************/
/* Blob printf functions                                                  */
/**************************************************************************/

int blob_append_vfmt(blob_t *blob, const char *fmt, va_list ap)
        __attr_printf__(2,0);
int blob_append_fmt(blob_t *blob, const char *fmt, ...)
        __attr_printf__(2,3);

int blob_set_vfmt(blob_t *blob, const char *fmt, va_list ap)
        __attr_printf__(2,0);
int blob_set_fmt(blob_t *blob, const char *fmt, ...)
        __attr_printf__(2,3);

int blob_pack(blob_t *blob, const char *fmt, ...);
int blob_unpack(const blob_t *blob, int *pos, const char *fmt, ...)
    __must_check__;
int buf_unpack(const byte *buf, int buf_len, int *pos, const char *fmt, ...)
    __must_check__;
int blob_serialize(blob_t *blob, const char *fmt, ...)
    __attr_printf__(2, 3);
int buf_deserialize(const byte *buf, int buf_len,
                    int *pos, const char *fmt, ...);
int blob_deserialize(const blob_t *blob, int *pos, const char *fmt, ...);

/**************************************************************************/
/* Blob search functions                                                  */
/**************************************************************************/

/* not very efficent ! */

int blob_search(const blob_t *haystack, int pos, const blob_t *needle);
int blob_search_data(const blob_t *haystack, int pos,
                     const void *needle, int len);
static inline int
blob_search_cstr(const blob_t *haystack, int pos, const char *needle) {
    return blob_search_data(haystack, pos, needle, strlen(needle));
}

/**************************************************************************/
/* Blob filtering                                                         */
/**************************************************************************/

void blob_ltrim(blob_t *blob);
void blob_rtrim(blob_t *blob);
void blob_trim(blob_t *blob);

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
void blob_append_urldecode(blob_t *out, const char *encoded, int len,
                           int flags);
void blob_b64encode(blob_t *blob, int nbpackets);
void blob_b64decode(blob_t *blob);

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
int blob_append_xml_escape(blob_t *dst, const byte *src, int len);
static inline int blob_append_xml_escape_cstr(blob_t *dst, const char *s) {
    return blob_append_xml_escape(dst, (const byte *)s, strlen(s));
}
void blob_append_quoted_printable(blob_t *dst, const byte *src, int len);

typedef struct base64enc_ctx {
    int width;
    int pack_num;
    uint8_t nbmissing;
    byte trail;
} base64enc_ctx;

GENERIC_FUNCTIONS(base64enc_ctx, base64enc_ctx);

/* Simple encoding */
void blob_append_base64(blob_t *dst, const byte *src, int len, int width);

/* base64 encoding per packets */
void blob_append_base64_update(blob_t *dst, const byte *src, int len,
                               base64enc_ctx *ctx);
void blob_append_base64_finish(blob_t *dst, base64enc_ctx *ctx);

int blob_append_smtp_data(blob_t *dst, const byte *src, int len);
int blob_append_hex(blob_t *dst, const byte *src, int len);

/* in blob_emi.c */
int blob_append_ira_hex(blob_t *dst, const byte *src, int len);
int blob_append_ira_bin(blob_t *dst, const byte *src, int len);

int blob_decode_ira_hex_as_latin15(blob_t *dst, const char *src, int len);
int blob_decode_ira_bin_as_latin15(blob_t *dst, const char *src, int len);

int blob_decode_ira_hex_as_utf8(blob_t *dst, const char *src, int len);
int blob_decode_ira_bin_as_utf8(blob_t *dst, const char *src, int len);

int string_decode_base64(byte *dst, int size,
                         const char *src, int len);

int string_decode_ira_hex_as_latin15(char *dst, int size,
                                     const char *src, int len);
int string_decode_ira_bin_as_latin15(char *dst, int size,
                                     const char *src, int len);

int string_decode_ira_hex_as_utf8(char *dst, int size,
                                  const char *src, int len);
int string_decode_ira_bin_as_utf8(char *dst, int size,
                                  const char *src, int len);

/* in blob_ebcdic.c */
int blob_decode_ebcdic297(blob_t *dst, const byte *src, int len);

/* in blob_utf8.c */
/* OG: should inline this */
int blob_utf8_putc(blob_t *out, int c);

int blob_latin1_to_utf8(blob_t *out, const char *s, int len);
int blob_latin9_to_utf8(blob_t *out, const char *s, int len);

/*[ CHECK ]::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::{{{*/
#ifdef CHECK
#include <check.h>

Suite *check_make_blob_suite(void);
Suite *check_append_blob_ebcdic_suite(Suite *blob_suite);

#endif
/*:::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::}}}*/
#endif /* IS_LIB_COMMON_BLOB_H */
