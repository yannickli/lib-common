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

#include "zlib-wrapper.h"

ssize_t sb_add_compressed(sb_t *out, const void *data, size_t dlen,
                          int level, bool do_gzip)
{
    int err;
    sb_t orig = *out;
    z_stream stream = {
        .next_in   = (Bytef *)data,
        .avail_in  = dlen,
    };

    RETHROW(deflateInit2(&stream, level, Z_DEFLATED,
                         MAX_WBITS + (do_gzip ? 16 : 0),
                         MAX_MEM_LEVEL, Z_DEFAULT_STRATEGY));

    for (;;) {
        stream.next_out  = (Bytef *)sb_grow(out, MAX(stream.avail_in / 2, 128));
        stream.avail_out = sb_avail(out);

        switch ((err = deflate(&stream, Z_FINISH))) {
          case Z_STREAM_END:
            __sb_fixlen(out, orig.len + stream.total_out);
            IGNORE(deflateEnd(&stream));
            return out->len - orig.len;
          case Z_OK:
            /* Compression OK, but not finished, must allocate more space */
            __sb_fixlen(out, orig.len + stream.total_out);
            break;
          default:
            __sb_rewind_adds(out, &orig);
            deflateEnd(&stream);
            return err;
        }
    }
}
