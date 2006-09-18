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

/**************************************************************************/
/* Blob compression/decompression                                         */
/**************************************************************************/

#define GZIP_HEADER_SIZE 10
#define ZLIB_HEADER_SIZE 2
#define DEF_MEM_LEVEL 8

typedef int ZEXPORT (*zlib_fct_t)(Bytef *dest, uLongf *destLen,
                                  const Bytef *source, uLong sourceLen);
typedef int (*ziping_fct_t)(blob_t *dest, const blob_t *src);

/**
 * TODO: Find a cleaner implementation
 *
 */

/**
 * Special implementation of zlib's compress function for gziping a string
 */
static int ZEXPORT gzip_compress(Bytef *dest, uLongf *destLen,
                                 const Bytef *source, uLong sourceLen)
{
    z_stream stream;
    int err;

    stream.next_in = (Bytef*)source;
    stream.avail_in = (uInt)sourceLen;
#ifdef MAXSEG_64K
    /* Check for source > 64K on 16-bit machine: */
    if ((uLong)stream.avail_in != sourceLen) return Z_BUF_ERROR;
#endif
    stream.next_out = dest;
    stream.avail_out = (uInt)*destLen;
    if ((uLong)stream.avail_out != *destLen) return Z_BUF_ERROR;

    stream.zalloc = (alloc_func)0;
    stream.zfree = (free_func)0;
    stream.opaque = (voidpf)0;

    /* Intersec fix added here */
    err = deflateInit2(&stream, Z_DEFAULT_COMPRESSION,
                       Z_DEFLATED, -MAX_WBITS, DEF_MEM_LEVEL,
                       Z_DEFAULT_STRATEGY);
    /* Intersec fix ends here */
    
    if (err != Z_OK) return err;

    err = deflate(&stream, Z_FINISH);
    if (err != Z_STREAM_END) {
        deflateEnd(&stream);
        return err == Z_OK ? Z_BUF_ERROR : err;
    }
    *destLen = stream.total_out;

    err = deflateEnd(&stream);
    return err;
}

/**
 * Special implementation of zlib's uncompress function for ungziping 
 * a string
 */
static int ZEXPORT gzip_uncompress (Bytef *dest, uLongf *destLen,
                                    const Bytef *source, uLong sourceLen)
{
    z_stream stream;
    int err;

    stream.next_in = (Bytef*)source;
    stream.avail_in = (uInt)sourceLen;
    /* Check for source > 64K on 16-bit machine: */
    if ((uLong)stream.avail_in != sourceLen) {
        return Z_BUF_ERROR;
    }
        
    stream.next_out = dest;
    stream.avail_out = (uInt)*destLen;
    if ((uLong)stream.avail_out != *destLen) {
        return Z_BUF_ERROR;
    }
        
    stream.zalloc = (alloc_func)0;
    stream.zfree = (free_func)0;

    /* Intersec fix added here */
    if (sourceLen < 2
    || stream.next_in[0] != 0x1f
    || stream.next_in[1] != 0x8b) {
        return Z_DATA_ERROR;
    }
    err = inflateInit2(&stream, -MAX_WBITS);
    stream.next_in += GZIP_HEADER_SIZE;
    stream.avail_in -= GZIP_HEADER_SIZE;
    /* Intersec fix ends here */
    
    if (err != Z_OK) {
        return err;
    }

    err = inflate(&stream, Z_FINISH);
    if (err != Z_STREAM_END) {
        inflateEnd(&stream);
        if (err == Z_NEED_DICT
            || (err == Z_BUF_ERROR && stream.avail_in == 0)) {
            return Z_DATA_ERROR;
        }
        return err;
    }
    *destLen = stream.total_out;

    err = inflateEnd(&stream);
    return err;
}

/**
 * uncompress data in zlib/gzip format
 */
static int blob_generic_uncompress(blob_t *dest, const blob_t *src,
                                   zlib_fct_t uncomp_fct)
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
    while ((err = (*uncomp_fct)(data, &len, src->data, src->len)) != Z_OK &&
           try < 2)
    {
        if (err == Z_BUF_ERROR) {
            len *= 2;
            blob_resize(dest, 0);
            blob_resize(dest, len);
            data = ((real_blob_t *) dest)->data;
            /* FIXME: A 1Mb file filled with 0 compresses into 1056 bytes.
             * It cannot be uncompressed by this function ! Oops !
             * */
            try++;
        } else {
            return -1;
        }
    }
    blob_resize(dest, len);
    return 0;
}

/**
 * uncompress data in zlib format
 */
int blob_uncompress(blob_t *dest, blob_t *src)
{
    return blob_generic_uncompress(dest, src, uncompress);
}

/**
 * uncompress data in gzip format
 */
int blob_gunzip(blob_t *dest, const blob_t *src)
{
    return blob_generic_uncompress(dest, src, gzip_uncompress);
}

/**
 * Blob_generic_compress compress raw data in zlib/gzip format
 */
static int blob_generic_compress(blob_t *dest, const blob_t *src,
                                 zlib_fct_t comp_fct)
{
    int err = 0;
    int try = 0;
    byte *data;
    uLongf len;
        
    if (dest == NULL || src == NULL) {
        return -1;
    }
    len = src->len + 256;
    blob_resize(dest, len);
    data = ((real_blob_t *) dest)->data;
    while ((err = (*comp_fct)(data, &len, src->data, src->len)) != Z_OK &&
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
    blob_resize(dest, len);
    return 0;
}

/**
 * Blob_compress compress raw data in zlib format
 */
int blob_compress(blob_t *dest, blob_t *src)
{
    return blob_generic_compress(dest, src, compress);
}

static void blob_append_reverse_int(blob_t *dest, unsigned long nb)
{
    int i;
    
    for (i = 0; i < 4; i++) {
        blob_append_byte(dest, nb & 0xFF);
        nb >>= 8;
    }
}

/**
 * Blob_gzip compress raw data in gzip format
 *
 * Gzip header + compress data + crc + uncompress data length
 *
 */
int blob_gzip(blob_t *dest, const blob_t *src)
{
    const byte header[10] = {0x1f, 0x8b, 0x08, 0x00, 0x00,
                             0x00, 0x00, 0x00, 0x00, 0x00};
    blob_t tmp;
    int res = 0;
    uLong crc_src;
    uLong len = src->len;
    
    blob_init(&tmp);
    blob_set_data(&tmp, header, 10);
        
    blob_resize(dest, 0);

    crc_src = crc32(0L, Z_NULL, 0);
    crc_src = crc32(crc_src, src->data, src->len);

    res = blob_generic_compress(dest, src, gzip_compress);
    
    blob_append_reverse_int(dest, crc_src);
    blob_append_reverse_int(dest, len);
    
    blob_append(&tmp, dest);
    blob_set(dest, &tmp);      

    blob_wipe(&tmp);
    
    return res;
}


/**
 * blob_file_generic_gzip_gunzip
 *
 */
static int blob_file_generic_gzip_gunzip(blob_t *dst, const char *filename,
                                         ziping_fct_t ziping_fct)
{
    blob_t tmp;
    int res = 0;

    blob_init(&tmp);
    
    if (blob_append_file_data(&tmp, filename) < 0) {
        e_debug(3, "Unable to append data from '%s'\n", filename);
        blob_wipe(&tmp);
        return -1;
    }

    res = (*ziping_fct)(dst, &tmp);
    blob_wipe(&tmp);

    return res;
}

/**
 * blob_file_gzip
 *
 */
int blob_file_gzip(blob_t *dst, const char *filename)
{
    return blob_file_generic_gzip_gunzip(dst, filename, blob_gzip);
}

/**
 * blob_file_gunzip
 *
 */
int blob_file_gunzip(blob_t *dst, const char *filename)
{
    return blob_file_generic_gzip_gunzip(dst, filename, blob_gunzip);
}
