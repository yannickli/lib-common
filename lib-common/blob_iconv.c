#include <iconv.h>
#include <string.h>
#include <errno.h>

#include "blob.h"
#include "blob_priv.h"

#define iconv(cd, inbuf, inleft, outbuf, outleft) \
    ciconv(cd, inbuf, inleft, outbuf, outleft)

static inline size_t ciconv(iconv_t cd, const char **inbuf, size_t *inbytesleft,
                            char **outbuf, size_t *outbytesleft)
{
    return (iconv)(cd, (char **)inbuf, inbytesleft, outbuf, outbytesleft);
}


static iconv_t ic_h = 0;

/**
 * guess_type try to guess the encoding with the specify string
 */
static const char *guess_encoding (const blob_t *blob)
{
    if (blob == NULL) {
        e_debug(1, "Internal error");
        return NULL;
    }

    return NULL;
}

static int blob_iconv_open(const blob_t *blob, const char *suggest_type)
{
    const char *guess_type;

    guess_type = guess_encoding(blob);
    if (guess_type) {
        if (strcmp(guess_type, suggest_type)) {
            e_debug(1, "the guess_type and suggest_type differ");
            return -1;
        } else {
            suggest_type = guess_type;
        }
    }

    if ((ic_h = iconv_open("UTF-8", suggest_type)) == 0) {
        e_debug(1, "blob_iconv");
        return -1;
    }
    return 0;
}

/**
 * Blob_iconv converts src from suggest_type to UTF-8.
 *
 */
int blob_iconv(blob_t *dst, const blob_t *src, const char *suggest_type)
{
    size_t len_in, len_out, total_len;
    int try = 0;
    const char *data;
    char *out;  
    
    if (dst == NULL || src == NULL || suggest_type == NULL) {
        return -1;
    }

    if (ic_h == 0) {
        blob_iconv_open(src, suggest_type);
    }  
    
    len_in = src->len;
    len_out = len_in * 2;
    total_len = len_out;
    
    data = (const char*) src->data;

    blob_resize(dst, len_out);
    out = (char *)((real_blob_t *) dst)->data;
    

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
            iconv_close(ic_h);
            return -2;
        }
    }
    total_len -= len_out;
    blob_resize(dst, total_len);
    //    iconv_close(ic_h);
    return 0;
}

