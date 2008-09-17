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

/* blob_t is a wrapper type for a reallocatable byte array.
 * Its internal representation is accessible to client code but care
 * must be exercised to preserve blob invariants and avoid dangling
 * pointers when blob data is reallocated by blob functions.
 *
 * Here is a description of the blob fields:
 *
 * - <data> is a pointer to the active portion of the byte array.  As
 * <data> may be reallocated by blob operations, it is error prone to
 * copy its value to a local variable for content parsing or other such
 * manipulations.  As <data> may point to non allocated storage, static
 * or dynamic or automatic, returning its value is guaranteed to cause
 * potential problems, current of future.  If some blob data was
 * skipped, <data> may no longer point to the beginning of the original
 * or allocated array.  <data> cannot be NULL.  It is initialized as
 * the address of a global 1 byte array, or a buffer with automatic
 * storage.
 *
 * - <len> is the length in bytes of the blob contents.  A blob invariant
 * specifies that data[len] == '\0'.  This invariant must be kept at
 * all times, and implies that data storage extend at least one byte
 * beyond <len>. <len> is always positive or zero. <len> is initialized
 * to zero for an empty blob.
 *
 * - <size> is the number of bytes available for blob contents starting
 * at <data>.  The blob invariant implies that size > len.
 *
 * - <skip> is a count of byte skipped from the beginning of the
 * original data buffer.  It is used for efficient pruning of initial
 * bytes.
 *
 * - <allocated> is a 1 bit boolean to indicate if blob data was
 * allocated with p_new and must be freed.  Actual allocated buffer
 * starts at .data - .skip.
 *
 * blob invariants:
 * - blob.data != NULL
 * - blob.len >= 0
 * - blob.size > blob.len
 * - blob.data - blob.skip points to an array of at least
 * blob.size + blob.skip bytes.
 * - blob.data[blob.len] == '\0'
 * - if blob.allocated, blob.data - blob.skip is a pointer handled by
 * mem_alloc / mem_free.
 */
typedef struct blob_t {
    byte *data;
    int len, size;
    flag_t allocated  :  1;
    unsigned int skip : 31;
} blob_t;

/* Default byte array for empty blobs. It should always stay equal to
 * \0 and is writeable to simplify some blob functions that enforce the
 * invariant by writing \0 at data[len].
 * This data is thread specific for obscure and ugly reasons: some blob
 * functions may temporarily overwrite the '\0' and would cause
 * spurious bugs in other threads sharing the same blob data.
 * It would be much cleaner if this array was const.
 */
extern __thread byte blob_slop[1];

#if 0
#define blob_check_slop()   assert (blob_slop[0] == '\0')
#else
#define blob_check_slop()
#endif

#define BLOB_STATIC_INIT  (blob_t){ .data = blob_slop, .size = 1 }

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

static inline blob_t *blob_init(blob_t *blob) {
    *blob = BLOB_STATIC_INIT;
    /* check invariant in case blob_slop was overwritten */
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

GENERIC_NEW(blob_t, blob);
GENERIC_DELETE(blob_t, blob);

/* Detach blob data: return allocated buffer with blob contents and
 * reset blob.  Buffer is not reallocated if blob data was already
 * allocated.
 */
char *blob_detach(blob_t *blob);

blob_t *blob_dup(const blob_t *blob);

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

static inline void blob_reset(blob_t *blob) {
    blob_check_slop();
    /* Remove initial skip if any, but do not release memory */
    blob->size += blob->skip;
    blob->data -= blob->skip;
    blob->skip = 0;
    blob->data[blob->len = 0] = '\0';
    blob_check_slop();
}

void blob_ensure(blob_t *blob, int newlen);

/* blob_grow increases the available size but does not change .len */
static inline void blob_grow(blob_t *blob, int extralen) {
    assert (extralen >= 0);

    blob_check_slop();
    if (blob->len + extralen >= blob->size) {
        blob_ensure(blob, blob->len + extralen);
        blob_check_slop();
    }
}

static inline void blob_setlen(blob_t *blob, int newlen) {
    assert (newlen >= 0);
    blob_check_slop();
    if (newlen <= 0) {
        blob_reset(blob);
    } else {
        blob_ensure(blob, newlen);
        blob->len = newlen;
        blob->data[blob->len] = '\0';
    }
    blob_check_slop();
}

/* blob_extend increases the available size and len */
static inline void blob_extend(blob_t *blob, int extralen) {
    assert (extralen >= 0);
    blob_check_slop();
    blob_setlen(blob, blob->len + extralen);
    blob_check_slop();
}

/* blob_extend2 increases and initializes the available size and len */
static inline void blob_extend2(blob_t *blob, int extralen, byte init) {
    assert (extralen >= 0);
    blob_check_slop();
    blob_grow(blob, extralen);
    memset(blob->data + blob->len, init, extralen);
    blob->len += extralen;
    blob->data[blob->len] = '\0';
    blob_check_slop();
}

/**************************************************************************/
/* Blob manipulations                                                     */
/**************************************************************************/

static inline void
blob_grow_front(blob_t *blob, int extralen, char init)
{
    assert (extralen >= 0);
    blob_check_slop();

    if (blob->skip >= extralen) {
        blob->data -= extralen;
        blob->skip -= extralen;
        blob->len  += extralen;
    } else {
        /* TODO: Should malloc/free without using ensure */
        blob_ensure(blob, blob->len + extralen);
        p_move(blob->data - blob->skip, extralen, blob->skip, blob->len + 1);
        blob->data -= blob->skip;
        blob->skip = 0;
        blob->len += extralen;
    }
    memset(blob->data, init, extralen);
    blob_check_slop();
}

static inline void
blob_splice_data(blob_t *blob, int pos, int len, const void *data, int dlen)
{
    assert (pos >= 0 && len >= 0 && dlen >= 0);
    blob_check_slop();

    if (pos > blob->len)
        pos = blob->len;
    if (len > blob->len - pos)
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
    blob_check_slop();
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
    blob_check_slop();
    blob_grow(blob, len);
    memcpy(blob->data + blob->len, data, len);
    blob->len += len;
    blob->data[blob->len] = '\0';
    blob_check_slop();
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
void blob_append_urldecode(blob_t *out, const char *encoded, int len,
                           int flags);
void blob_append_urlencode(blob_t *out, const byte *data, int len);
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
int blob_append_xml_escape(blob_t *dst, const byte *src, int len);
static inline int blob_append_xml_escape_cstr(blob_t *dst, const char *s) {
    return blob_append_xml_escape(dst, (const byte *)s, strlen(s));
}
void blob_append_quoted_printable(blob_t *dst, const byte *src, int len);
void blob_decode_quoted_printable(blob_t *dst, const char *src, int len);

int blob_append_smtp_data(blob_t *dst, const byte *src, int len);
int blob_append_hex(blob_t *dst, const byte *src, int len);

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
void blob_append_base64(blob_t *dst, const byte *src, int len, int width);

/* base64 encoding per packets */
void blob_append_base64_start(blob_t *dst, int len, int width,
                              base64enc_ctx *ctx);
void blob_append_base64_update(blob_t *dst, const void *src, int len,
                               base64enc_ctx *ctx);
void blob_append_base64_finish(blob_t *dst, base64enc_ctx *ctx);

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
Suite *check_append_blob_wbxml_suite(Suite *blob_suite);

#endif
/*:::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::}}}*/
#endif /* IS_LIB_COMMON_BLOB_H */
