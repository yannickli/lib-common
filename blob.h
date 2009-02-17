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

#define blob_inita                    sb_inita
#define blob_init                     sb_init
#define blob_wipe                     sb_wipe
#define blob_new                      sb_new
#define blob_delete                   sb_delete
#define blob_reinit                   sb_wipe
#define blob_detach(sb)               sb_detach(sb, NULL)
#define blob_get_cstr(sb)             ((sb)->data)
#define blob_get_end                  sb_end
#define blob_cmp                      sb_cmp
#define blob_reset                    sb_reset
#define blob_grow                     sb_grow
#define blob_extend                   sb_growlen
#define blob_kill_first               sb_skip
#define blob_kill_last                sb_shrink
#define blob_kill_at                  sb_skip_upto
#define blob_append_data              sb_add
#define blob_append_cstr              sb_adds
#define blob_append                   sb_addsb
#define blob_append_byte              sb_addc
#define blob_set_data                 sb_set
#define blob_set_cstr                 sb_sets
#define blob_set                      sb_setsb
#define blob_append_file_data         sb_read_file
#define blob_append_fread             sb_fread
#define blob_append_read              sb_read
#define blob_append_recv              sb_recv
#define blob_append_recvfrom          sb_recvfrom
#define blob_save_to_file             sb_write_file
#define blob_append_vfmt              sb_addvf
#define blob_append_fmt               sb_addf
#define blob_set_vfmt                 sb_setvf
#define blob_set_fmt                  sb_setf
#define blob_append_data_escaped2     sb_add_slashes
#define blob_append_cstr_escaped2     sb_adds_slashes
#define blob_append_urlencode         sb_add_urlencode
#define blob_append_hex               sb_add_hex
#define blob_append_xml_escape        sb_add_xmlescape
#define blob_append_xml_escape_cstr   sb_adds_xmlescape
#define blob_append_quoted_printable  sb_add_qpe
#define blob_decode_quoted_printable  sb_add_unqpe
#define blob_hexdecode                sb_add_unhex
#define blob_urldecode                sb_urldecode



static inline void blob_setlen(blob_t *blob, int newlen) {
    sb_growlen(blob, newlen - blob->len);
}

/**************************************************************************/
/* Blob manipulations                                                     */
/**************************************************************************/

static inline void blob_kill_data(blob_t *blob, int pos, int len) {
    sb_splice(blob, pos, len, NULL, 0);
}

/**************************************************************************/
/* Blob printf functions                                                  */
/**************************************************************************/

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
