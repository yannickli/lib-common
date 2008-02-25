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

#ifndef MINGCC
#include <iconv.h>

#include "blob.h"
#include "string_is.h"

#define DEBUG_VERB 4

#define ICONV_HANDLES_CACHE_MAX 20
#define ICONV_TYPE_SIZE 15
typedef struct iconv_t_cache {
    char to[ICONV_TYPE_SIZE + 1];
    char from[ICONV_TYPE_SIZE + 1];
    iconv_t handle;
} iconv_t_cache;

/**
 * iconv_handles is used to keep the iconv_handles so that we must not
 * call iconv_open every time.
 */
static iconv_t_cache *iconv_handles = NULL;

#if !defined(__CYGWIN__)
/* Work around broken iconv() prototype in GLIBC */
#define iconv(cd, inbuf, inleft, outbuf, outleft) \
    ciconv(cd, inbuf, inleft, outbuf, outleft)
static inline size_t ciconv(iconv_t cd,
                            const char **inbuf, size_t *inbytesleft,
                            char **outbuf, size_t *outbytesleft)
{
    return (iconv)(cd, (char **)inbuf, inbytesleft, outbuf, outbytesleft);
}
#endif

/**
 * We only try to detect the encoding if it is one of the known_encodings
 */
static struct {
    const char *name;
    int encoding;
} const known_encodings[] = {
    { "ASCII",        ENC_ASCII },
    { "ISO-8859-1",   ENC_ISO_8859_1 },
    { "Windows-1250", ENC_WINDOWS_1250 },
    { "Windows-1252", ENC_WINDOWS_1252 },
    { "UTF-8",        ENC_UTF8 },
    { NULL, -1 },
};

static int r_known_encodings(const char *s)
{
    int i;

    for (i = 0; known_encodings[i].name; i++) {
        if (strcasecmp(known_encodings[i].name, s) == 0) {
            break;
        }
    }
    return known_encodings[i].encoding;
}

/**
 * Get a handle for the specified couple (to, from) encoding types.
 *
 */
static iconv_t get_iconv_handle(const char *to, const char *from)
{
    static unsigned int current_iconv = 0;
    int i = 0;

    if (from == NULL || to == NULL) {
        return (iconv_t) -1;
    }
    if (iconv_handles == NULL) {
        iconv_handles = p_new(iconv_t_cache, ICONV_HANDLES_CACHE_MAX);
    }
    for (i = 0; i < ICONV_HANDLES_CACHE_MAX; i++) {
        if (iconv_handles[i].handle
        &&  strncasecmp(to, iconv_handles[i].to, ICONV_TYPE_SIZE) == 0
        &&  strncasecmp(from, iconv_handles[i].from, ICONV_TYPE_SIZE) == 0)
        {
            return iconv_handles[i].handle;
        }
    }
    current_iconv %= ICONV_HANDLES_CACHE_MAX;
    if (iconv_handles[current_iconv].handle) {
        iconv_close(iconv_handles[current_iconv].handle);
        iconv_handles[current_iconv].handle = NULL;
    }
    pstrcpy(iconv_handles[current_iconv].to,
            sizeof(iconv_handles[current_iconv].to), to);
    pstrcpy(iconv_handles[current_iconv].from,
            sizeof(iconv_handles[current_iconv].from), from);
    return (iconv_handles[current_iconv++].handle = iconv_open(to, from));
}

/**
 * Close all handles for iconv functions
 * Return the number of handles closed
 */
int blob_iconv_close_all(void)
{
    int i;
    int res = 0;

    if (iconv_handles == NULL) {
        return -1;
    }
    for (i = 0; i < ICONV_HANDLES_CACHE_MAX; i++) {
        if (iconv_handles[i].handle) {
            if (iconv_close(iconv_handles[i].handle) < 0) {
                e_trace(DEBUG_VERB, "An iconv handle cannot be closed");
            } else {
                iconv_handles[i].handle = NULL;
                res++;
            }
        }
    }
    p_delete(&iconv_handles);
    return res;
}

/**
 * This function tries to detect the encoding of the string
 * We also give a hint for the detection
 *
 */
static int detect_encoding(const byte *s, int len, const char *type_hint)
{
    int i, c, enc;

    if (!s) {
        return -1;
    }

    for (i = 0; i < len; i++) {
        if (s[i] >= 0x80)
            goto non_ascii;
    }

    return ENC_ASCII;

  non_ascii:
    enc = r_known_encodings(type_hint);
    if (enc < 0) {
        return -1;
    }

    for (; i < len; i++) {
        c = s[i];
        if (c < 0x80)
            continue;

        if (c <= 0x9F) {
            /* ISO-8859-1 and UTF-8 impossible here */
            /* So either Win-1250 or Win-1252 */
            if (c == 0x83 || c == 0x88 || c == 0x89 || c == 0x9c) {
                /* win-1252 */
                enc = ENC_WINDOWS_1252;
                e_trace(DEBUG_VERB, "Win-1252 probable: %x", c);
                if (c != 0x9c) {
                    /* win-1252 very probable */
                    break;
                }
            } else {
                /* Win-1250 */
                enc = ENC_WINDOWS_1250;
                e_trace(DEBUG_VERB, "Win-1250 probable: %x", c);
                continue;
            }
        } else
        if (c <= 0xBF) {
            /* UTF-8 impossible here */
            e_trace(DEBUG_VERB, "Win-1250 quite probable, ISO-8859-1 probable"
                    ": %x", c);
            enc = (enc != ENC_ISO_8859_1) ? ENC_WINDOWS_1250 : enc;
            continue;
        } else
        if (c <= 0xDF) {
            /* FIXME... s[i+1] is out of range when evaluated !!! */
            if (i + 1 >= len || s[i + 1] < 0x80) {
                /* UTF-8 impossible here */
                e_trace(DEBUG_VERB, "Win-1250, ISO-8859-1 equally"
                        "probable: %x", c);
                if (enc == ENC_UTF8) {
                    enc = ENC_ISO_8859_1;
                }
                continue;
            }
            if ((s[i + 1] & 0xC0) == 0x80) {
                enc = ENC_UTF8;
                e_trace(DEBUG_VERB, "UTF-8 very probable: %x %x",
                        c, s[i + 1]);
                break;
            }
        } else
        if (c <= 0xEF) {
            if (i + 2 >= len) {
                /* UTF-8 impossible here */
                if (enc == ENC_UTF8) {
                    enc = ENC_ISO_8859_1;
                }
                continue;
            }
            if ((s[i + 1] & 0xC0) == 0x80
            &&  (s[i + 2] & 0xC0) == 0x80) {
                enc = ENC_UTF8;
                e_trace(DEBUG_VERB, "UTF-8 extremely probable: %x %x %x",
                        c, s[i + 1], s[i + 2]);
                break;
            } else {
                if (c == 0xE0 || enc == ENC_UTF8) {
                    enc = ENC_ISO_8859_1;
                    e_trace(DEBUG_VERB, "ISO-8859-1 a little probable");
                    continue;
                }
            }
        } else {
            /* 0xF0 and beyond are unlikely UTF-8 */
        }
    }
    return enc;
}

/**
 * Blob_iconv_priv converts the array src of length len from type to UTF-8
 *
 * Return - 0 if success
 *        - <0 otherwise
 */
static int blob_iconv_priv(blob_t *dst, const byte *src, int len,
                           const char *type)
{
    size_t len_in, len_out, total_len, init_len_out;
    int try = 0;
    char *out;
    iconv_t ic_h;
    const char *data;

    if (dst == NULL || src == NULL || type == NULL) {
        return -1;
    }

    data = (const char *)src;
    len_in = len;
    init_len_out = dst->len;
    len_out = len_in * 2;
    total_len = init_len_out + len_in * 2;

    blob_setlen(dst, total_len);
    out = (char *)dst->data + init_len_out;

    ic_h = get_iconv_handle("UTF-8", type);
    while (iconv(ic_h, &data, &len_in, &out, &len_out) != 0 && try < 5) {
        if (errno == E2BIG) {
            len_out = 256;
            blob_setlen(dst, total_len + len_out);
            out = (char *)dst->data + total_len;
            total_len += len_out;
            try++;
        } else {
            blob_reset(dst);
            return -2;
        }
    }
    total_len -= len_out;
    blob_setlen(dst, total_len);
    return 0;
}

/**
 * Blob_iconv calls blob_iconv_priv
 *
 */
int blob_iconv(blob_t *dst, const blob_t *src, const char *type)
{
    return blob_iconv_priv(dst, src->data, src->len, type);
}

/**
 * blob_auto_iconv tries to guess the encoding of the blob and converts
 * it to UTF-8.
 *
 * Returns: -1 if an error occurs.
 *           0 if the type_hint != chosen_encoding
 *           1 if the type_hint == chosen_encoding
 *           2 if the type is not part of the main encoding
 */
int blob_auto_iconv(blob_t *dst, const blob_t *src, const char *type_hint,
                    int *chosen_encoding)
{
    int enc_type, enc;

    if (src == NULL || dst == NULL) {
        e_trace(DEBUG_VERB, "Internal error");
        return -1;
    }

    blob_reset(dst);
    enc_type = r_known_encodings(type_hint);
    if (enc_type < 0) {
        e_trace(DEBUG_VERB, "Skip type: %s", type_hint);
        if (blob_iconv_priv(dst, src->data, src->len, type_hint) < 0) {
            return -1;
        }
        return 2;
    }

    enc = detect_encoding(src->data, src->len, type_hint);
    *chosen_encoding = enc;
    e_trace(DEBUG_VERB, " -- Chosen Encoding: %s", known_encodings[enc].name);
    if (enc == ENC_ASCII || enc == ENC_UTF8) {
        blob_set(dst, src);
    } else {
        if (blob_iconv_priv(dst, src->data, src->len,
                            known_encodings[enc].name) < 0) {
            return -1;
        }
    }

    return (*chosen_encoding == enc_type);
}

/**
 * Blob_file_auto_iconv load a file in a blob and auto_encode it.
 *
 */
int blob_file_auto_iconv(blob_t *dst, const char *filename,
                         const char *suggested_type,
                         int *chosen_encoding)
{
    blob_t tmp;
    int res = 0;

    blob_init(&tmp);

    if (blob_append_file_data(&tmp, filename) < 0) {
        e_trace(DEBUG_VERB, "Unable to append data from '%s'", filename);
        blob_wipe(&tmp);
        return -1;
    }

    res = blob_auto_iconv(dst, &tmp, suggested_type, chosen_encoding);
    blob_wipe(&tmp);

    return res;
}
#endif
