/**************************************************************************/
/*                                                                        */
/*  Copyright (C) 2004-2019 INTERSEC SA                                   */
/*                                                                        */
/*  Should you receive a copy of this source code, you must check you     */
/*  have a proper, written authorization of INTERSEC to hold it. If you   */
/*  don't have such an authorization, you must DELETE all source code     */
/*  files in your possession, and inform INTERSEC of the fact you obtain  */
/*  these files. Should you not comply to these terms, you can be         */
/*  prosecuted in the extent permitted by applicable law.                 */
/*                                                                        */
/**************************************************************************/

#ifndef IS_LIB_COMMON_ZLIB_WRAPPER_H
#define IS_LIB_COMMON_ZLIB_WRAPPER_H

#include <zlib.h>
#include "core.h"
#include "str-outbuf.h"

#if __has_feature(nullability)
#pragma GCC diagnostic push
#pragma GCC diagnostic error "-Wnullability-completeness"
#if __has_warning("-Wnullability-completeness-on-arrays")
#pragma GCC diagnostic ignored "-Wnullability-completeness-on-arrays"
#endif
#endif

/** Add compressed data in the string buffer.
 *
 * Takes the chunk of data pointed by @p data and @p dlen and add it
 * compressed using zlib in the sb_t @p out.
 *
 * @param out     output buffer
 * @param data    source data
 * @param dlen    size of the data pointed by @p data.
 * @param level   compression level
 * @param do_gzip if true compresses using 'gzip', else use 'deflate'
 *
 * @return amount of data written in the stream or a negative zlib error in
 * case of error.
 */
ssize_t sb_add_compressed(sb_t * nonnull out, const void * nonnull data,
                          size_t dlen, int level, bool do_gzip);

#define ob_add_compressed(ob, data, dlen, level, do_gzip)  \
    OB_WRAP(sb_add_compressed, ob, data, dlen, level, do_gzip)

#if __has_feature(nullability)
#pragma GCC diagnostic pop
#endif

#endif
