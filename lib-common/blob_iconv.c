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

#include <iconv.h>
#include <string.h>
#include <errno.h>
#include <iconv.h>

#include "mem.h"
#include "blob.h"
#include "blob_priv.h"

#define DEBUG_VERB 4

#define ICONV_HANDLES_CACHE_MAX 20
#define ICONV_TYPE_SIZE 15
typedef struct {
    char to[ICONV_TYPE_SIZE];
    char from[ICONV_TYPE_SIZE];
    iconv_t handle;
} iconv_t_cache;

#define iconv(cd, inbuf, inleft, outbuf, outleft) \
    ciconv(cd, inbuf, inleft, outbuf, outleft)

/**
 * iconv_handles is used to keep the iconv_handles so that we must not
 * call iconv_open every time.
 */
static iconv_t_cache *iconv_handles = NULL;

static inline size_t ciconv(iconv_t cd,
                            const char **inbuf, size_t *inbytesleft,
                            char **outbuf, size_t *outbytesleft)
{
    return (iconv)(cd, (char **)inbuf, inbytesleft, outbuf, outbytesleft);
}

/**
 * We only try to detect the encoding if it is one of the known_encodings
 */
static const char *known_encodings[] = {"ASCII", "ISO-8859-1", "Windows-1250",
                                        "Windows-1252", "UTF-8", NULL};
static int r_known_encodings(const char *s)
{
    int i;

    for (i = 0; known_encodings[i]; i++) {
        if (strcasecmp(known_encodings[i], s) == 0) {
            return i;
        }
    }
    return -1;
}

/**
 * Get a handle for the specified couple (to, from) encoding types.
 *
 */
static iconv_t get_iconv_handle(const char *to, const char *from)
{
    static int current_iconv = 0;
    int i = 0;

    if (from == NULL || to == NULL) {
        return (iconv_t) -1;
    }
    if (iconv_handles == NULL) {
        iconv_handles = p_new(iconv_t_cache, ICONV_HANDLES_CACHE_MAX);
    }
    for (i = 0; i < ICONV_HANDLES_CACHE_MAX; i++) {
        if (iconv_handles[i].to && iconv_handles[i].from
        &&  strncasecmp(to, iconv_handles[i].to, ICONV_TYPE_SIZE) == 0
        &&  strncasecmp(from, iconv_handles[i].from, ICONV_TYPE_SIZE) == 0)
        {
            return iconv_handles[i].handle;
        }
    }
    current_iconv = current_iconv >= ICONV_HANDLES_CACHE_MAX ?
                    0 : current_iconv;
    if (iconv_handles[current_iconv].handle) {
        iconv_close(iconv_handles[current_iconv].handle);
    }
    strncpy(iconv_handles[current_iconv].to, to, ICONV_TYPE_SIZE);
    strncpy(iconv_handles[current_iconv].from, from, ICONV_TYPE_SIZE);
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
        if (iconv_handles[i].to && iconv_handles[i].from
            && iconv_handles[i].handle) {
            if (iconv_close(iconv_handles[i].handle) < 0) {
                e_trace(DEBUG_VERB, "An iconv handle cannot be closed");
            } else {
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
static int detect_encoding(const blob_t *src, int offset,
                           const char *type_hint)
{
    int i;
    int enc = ENC_ASCII;
    const char *s = blob_get_cstr(src);
    
    if (!src) {
        return -1;
    }

    for (i = offset; i < src->len; i++) {
        if (s[i] > 0x7F) {
            if (enc == ENC_ASCII) {
                enc = r_known_encodings(type_hint);
                if (enc < 0) {
                    return -1;
                }
            }
        }

        if (s[i] >= 0x80 && s[i] <= 0x9F) {
            /* ISO-8859-1 and UTF 8 Impossible here */
            /* So either Win-1250 or Win-1252 */
            if (s[i] == 0x83 || s[i] == 0x88 || s[i] == 0x89 || s[i] == 0x9c) {
                /* win-1252 */
                enc = ENC_WINDOWS_1252;
                e_trace(DEBUG_VERB, "Win-1252 probable: %x", s[i]);
                if (s[i] != 0x9c) {
                    /* win-1252 very probable */
                    break;
                }
            } else {
                /* Win-1250 */
                enc = ENC_WINDOWS_1250;
                e_trace(DEBUG_VERB, "Win-1250 probable: %x", s[i]);
                continue;
            }
        }
        
        if (s[i] >= 0xA0 && s[i] <= 0xBF) {
            /* UTF 8 Impossible here */
            e_trace(DEBUG_VERB, "Win-1250 quite probable, ISO-8859-1 probable"
                    ": %x", s[i]);
            enc = (enc != ENC_ISO_8859_1) ? ENC_WINDOWS_1250 : enc;
            continue;
        }
        
        if (s[i] >= 0xC0 && s[i] <= 0xDF) {
            /* FIXME... s[i+1] is out of range when evaluated !!! */
            if (i == (src->len - 1) || s[i + 1] < 0x80) {
                /* UTF 8 Impossible here */
                e_trace(DEBUG_VERB, "Win-1250, ISO-8859-1 equaly"
                        "probable: %x", s[i]);
                if (enc == ENC_UTF8) {
                    enc = ENC_ISO_8859_1;
                }
                continue;
            }
            if (s[i + 1] >= 0x80 && s[i + 1] <= 0xBF) {
                enc = ENC_UTF8;
                e_trace(DEBUG_VERB, "UTF-8 very probable: %x %x",
                        s[i], s[i + 1]);
                break;
            }
        }
        
        if (s[i] >= 0xE0 && s[i] <= 0xEF) {
            if (i + 2 >= src->len) {
                /* UTF 8 Impossible here */
                if (enc == ENC_UTF8) {
                    enc = ENC_ISO_8859_1;
                }
                continue;                
            }
            if (s[i + 1] >= 0x80 && s[i + 1] <= 0xBF
            &&  s[i + 2] >= 0x80 && s[i + 2] <= 0xBF) {
                enc = ENC_UTF8;
                e_trace(DEBUG_VERB, "UTF-8 extremely probable: %x %x", s[i],
                        s[i + 1]);
                break;
            } else {
                if (s[i] == 0xE0 || enc == ENC_UTF8) {
                    enc = ENC_ISO_8859_1;
                    e_trace(DEBUG_VERB, "ISO-8859-1 a little probable");
                    continue;
                }
            }
        }
    }
    return enc;
}

/**
 * Blob_iconv_priv converts the string src from type to UTF-8
 * The encoding starts at offset_src within src.
 * 
 * Return - 0 if success
 *        - <0 otherwise  
 */
static int blob_iconv_priv(blob_t *dst, const blob_t *src, int offset_src,
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

    data = (const char *)src->data + offset_src;
    init_len_out = dst->len;
    len_in = src->len - offset_src;
    total_len = init_len_out + len_in * 2;
    len_out = len_in * 2;
    
    blob_resize(dst, total_len);
    out = (char *)((real_blob_t *) dst)->data + init_len_out;
    
    ic_h = get_iconv_handle("UTF-8", type);
    while (iconv(ic_h, &data, &len_in, &out, &len_out) != 0
           && try < 5) {
        if (errno == E2BIG) {
            len_out = 256;
            blob_resize(dst, total_len + len_out);
            out = (char *)((real_blob_t *) dst)->data + total_len;
            total_len += len_out;
            try++;
        } else {
            blob_resize(dst, 0);
            return -2;
        }
    }
    total_len -= len_out;
    blob_resize(dst, total_len);
    return 0;
}

/**
 * Blob_iconv calls blob_iconv_priv with the offset set to 0
 *
 */
int blob_iconv(blob_t *dst, const blob_t *src, const char *type)
{
    return blob_iconv_priv(dst, src, 0, type);
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
int blob_auto_iconv (blob_t *dst, const blob_t *src, const char *type_hint,
                     int *chosen_encoding)
{
    int i = 0;
    int enc = ENC_ASCII;
    int res = 0;

    if (src == NULL || dst == NULL) {
        e_trace(DEBUG_VERB, "Internal error");
        return -1;
    }

    blob_resize(dst, 0);
    if (r_known_encodings(type_hint) < 0) {
        e_trace(DEBUG_VERB, "Skip type: %s", type_hint);
        if (blob_iconv_priv(dst, src, 0, type_hint) < 0) {
            return -1;
        }
        return 2;
    }
    for (i = 0; src->data[i]; i++) {
        if (src->data[i] > 0x7F) {
            enc = detect_encoding(src, i, type_hint);
            break;
        } else {
            blob_append_byte(dst, src->data[i]);
        }
    }

    *chosen_encoding = enc;
    e_trace(DEBUG_VERB, " -- Chosen Encoding: %s", known_encodings[enc]);
    
    if (src->data[i]) {
        if ((res = blob_iconv_priv(dst, src, i, known_encodings[enc])) < 0) {
            return -1;
        }
    }
       
    return *chosen_encoding == r_known_encodings(type_hint);
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
