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

#include <zlib.h>
#include <stdlib.h>

#include "macros.h"
#include "blob.h"
#include "blob_priv.h"

/******************************************************************************/
/* Blob compression/decompression                                             */
/******************************************************************************/

int blob_compress(blob_t *dest, blob_t *src)
{
    int err = 0;
    int try = 0;
    byte *data;
    uLongf len;
        
    if (dest == NULL || src == NULL) {
        return -1;
    }
    len = src->len + 256;
    blob_resize(dest, 0);
    blob_resize(dest, len);
    data = ((real_blob_t *) dest)->data;
    while ((err = compress(data, &len, src->data, src->len)) != Z_OK &&
           try < 2)
    {
        if (err == Z_BUF_ERROR) {
            len *= 2;
            blob_resize(dest, 0);
            blob_resize(dest, len);
            data = ((real_blob_t *) dest)->data;
            try++;
        } else {
            return -2;
        }
    }
    ((real_blob_t *) dest)->len = len;
    return 0;
}

int blob_uncompress(blob_t *dest, blob_t *src)
{
    uLongf len;
    int err = 0;
    int try = 0;
    byte *data;

    if (dest == NULL || src == NULL) {
        return -1;
    }
    len = src->len * 4;
    blob_resize(dest, 0);
    blob_resize(dest, len);
    data = ((real_blob_t *) dest)->data;
    while ((err = uncompress(data, &len, src->data, src->len)) != Z_OK &&
           try < 2)
    {
        if (err == Z_BUF_ERROR) {
            len *= 2;
            blob_resize(dest, 0);
            blob_resize(dest, len);
            data = ((real_blob_t *) dest)->data;
            try++;
        } else {
            return -1;
        }
    }
    ((real_blob_t *) dest)->len = len;
    return 0;
}
