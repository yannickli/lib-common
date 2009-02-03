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

#include "core.h"

#define blob_t sb_t

#if 0
#define blob_check_slop()   assert (__sb_slop[0] == '\0')
#else
#define blob_check_slop()
#endif

/**************************************************************************/
/* Blob creation / deletion                                               */
/**************************************************************************/

static inline void blob_init2(blob_t *blob, void *buf, int size) {
    assert (size > 0);
    *blob = (blob_t){ .data = buf, .size = size };
    blob->data[0] = '\0';
}

/* Warning: size is evaluated multiple times, but use of alloca
 * requires implementation as a macro.  size should be a constant
 * anyway.
 */
#define blob_inita(blob, size)                \
    do {                                      \
        STATIC_ASSERT((size) < (64 << 10));   \
        blob_init2(blob, alloca(size), size); \
    } while (0)

#define blob_init       sb_init
#define blob_wipe       sb_wipe
#define blob_new        sb_new
#define blob_delete     sb_delete
#define blob_reinit     sb_wipe
#define blob_detach(sb) sb_detach(sb, NULL)

/* Get the const char * pointing to blob.data */
static inline const char *blob_get_cstr(const blob_t *blob) {
    blob_check_slop();
    return (const char *)blob->data;
}

/* Get the pointer to the NUL at the end of the blob */
static inline const char *blob_get_end(const blob_t *blob) {
    blob_check_slop();
    return (const char *)blob->data + blob->len;
}

/**************************************************************************/
/* Blob size/len manipulations                                            */
/**************************************************************************/

#define blob_reset sb_reset
#define blob_grow sb_grow
#define blob_extend sb_growlen

static inline void blob_ensure(blob_t *blob, int newlen) {
    sb_grow(blob, newlen - blob->len);
}
static inline void blob_setlen(blob_t *blob, int newlen) {
    sb_growlen(blob, newlen - blob->len);
}

/* blob_extend2 increases and initializes the available size and len */
static inline void blob_extend2(blob_t *blob, int extralen, byte init) {
    assert (extralen >= 0);
    memset(sb_growlen(blob, extralen), init, extralen);
}

/**************************************************************************/
/* Blob manipulations                                                     */
/**************************************************************************/

static inline void
blob_grow_front(blob_t *blob, int extralen, char init)
{
    assert (extralen >= 0);
    __sb_splice(blob, 0, 0, NULL, extralen);
    memset(blob->data, init, extralen);
    blob_check_slop();
}

static inline void blob_kill_data(blob_t *blob, int pos, int len) {
    sb_splice(blob, pos, len, NULL, 0);
}
#define blob_kill_first sb_skip
#define blob_kill_last  sb_shrink
#define blob_kill_at    sb_skip_upto
static inline void blob_insert(blob_t *dest, int pos, const blob_t *src) {
    sb_splice(dest, pos, 0, src->data, src->len);
}
static inline void blob_insert_cstr(blob_t *blob, int pos, const char *s) {
    sb_splice(blob, pos, 0, s, strlen(s));
}
static inline void
blob_insert_data(blob_t *blob, int pos, const void *data, int len) {
    sb_splice(blob, pos, 0, data, len);
}

/*** appends ***/

#define blob_append_data sb_add
#define blob_append_cstr sb_adds
static inline void blob_append(blob_t *dest, const blob_t *src) {
    sb_add(dest, src->data, src->len);
}
#define blob_append_byte sb_addc

void blob_append_data_escaped2(blob_t *blob, const void *cstr, size_t len,
                               const char *toescape, const char *escaped);
static inline void blob_append_cstr_escaped2(blob_t *blob, const char *cstr,
                               const char *toescape, const char *escaped) {
    blob_append_data_escaped2(blob, cstr, strlen(cstr), toescape, escaped);
}
static inline void
blob_append_cstr_escaped(blob_t *blob, const char *cstr, const char *toescape)
{
    blob_append_cstr_escaped2(blob, cstr, toescape, toescape);
}


/*** copy ***/

#define blob_set_data sb_set
#define blob_set_cstr sb_sets
static inline void blob_set(blob_t *dest, const blob_t *src) {
    sb_set(dest, src->data, src->len);
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
#define blob_append_read     sb_read
#define blob_append_recv     sb_recv
#define blob_append_recvfrom sb_recvfrom

int blob_save_to_file(const blob_t *blob, const char *filename);

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
void blob_append_urldecode(blob_t *out, const void *encoded, int len, int flags);
void blob_append_urlencode(blob_t *out, const void *data, int len);
void blob_b64decode(blob_t *blob);

int blob_hexdecode(blob_t *out, const void *src, int len);

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
int blob_append_xml_escape(blob_t *dst, const char *src, int len);
static inline int blob_append_xml_escape_cstr(blob_t *dst, const char *s) {
    return blob_append_xml_escape(dst, s, strlen(s));
}
void blob_append_quoted_printable(blob_t *dst, const void *src, int len);
void blob_decode_quoted_printable(blob_t *dst, const char *src, int len);

int blob_append_smtp_data(blob_t *dst, const byte *src, int len);
int blob_append_hex(blob_t *dst, const void *src, int len);

void blob_append_wbxml_href(blob_t *dst, const byte *data, int len);

void blob_append_date_iso8601(blob_t *dst, time_t date);

/* base64 encoding */
typedef struct base64enc_ctx {
    int packs_per_line;
    int pack_num;
    uint8_t nbmissing;
    byte trail;
} base64enc_ctx;

/* Simple encoding */
void blob_append_base64(blob_t *dst, const void *src, int len, int width);

/* base64 encoding per packets */
void blob_append_base64_start(blob_t *dst, int len, int width,
                              base64enc_ctx *ctx);
void blob_append_base64_update(blob_t *dst, const void *src, int len,
                               base64enc_ctx *ctx);
void blob_append_base64_finish(blob_t *dst, base64enc_ctx *ctx);

/* in blob_emi.c */
void blob_append_ira_hex(blob_t *dst, const void *src, int len);
int blob_append_ira_bin(blob_t *dst, const void *src, int len);

int blob_decode_ira_hex_as_latin15(blob_t *dst, const char *src, int len);
int blob_decode_ira_bin_as_latin15(blob_t *dst, const char *src, int len);

int blob_decode_ira_hex_as_utf8(blob_t *dst, const char *src, int len);
int blob_decode_ira_bin_as_utf8(blob_t *dst, const char *src, int len);

int string_decode_base64(void *dst, int size,
                         const char *src, int len);

int string_decode_ira_hex_as_latin15(char *dst, int size,
                                     const char *src, int len);
int string_decode_ira_bin_as_latin15(char *dst, int size,
                                     const char *src, int len);

int string_decode_ira_hex_as_utf8(char *dst, int size,
                                  const char *src, int len);
int string_decode_ira_bin_as_utf8(char *dst, int size,
                                  const char *src, int len);
int gsm7_charlen(int c);
int blob_append_gsm7_packed(blob_t *out, int gsm_start,
                            const char *utf8, int unknown);
int blob_decode_gsm7_packed(blob_t *out, const void *src, int gsmlen,
                            int udhlen);

/* in blob_ebcdic.c */
int blob_decode_ebcdic297(blob_t *dst, const char *src, int len);

/* in blob_utf8.c */
/* OG: should inline this */
int blob_utf8_putc(blob_t *out, int c);

int blob_latin1_to_utf8(blob_t *out, const char *s, int len);
int blob_latin9_to_utf8(blob_t *out, const char *s, int len);
int blob_utf8_to_latin1(blob_t *out, const char *s, int rep);
int blob_utf8_to_latin1_n(blob_t *out, const char *s, int len,  int rep);

/*[ CHECK ]::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::{{{*/
#ifdef CHECK
#include <check.h>

Suite *check_make_blob_suite(void);
Suite *check_append_blob_ebcdic_suite(Suite *blob_suite);
Suite *check_append_blob_wbxml_suite(Suite *blob_suite);

#endif
/*:::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::}}}*/
#endif /* IS_LIB_COMMON_BLOB_H */
