/**************************************************************************/
/*                                                                        */
/*  Copyright (C) 2004-2007 INTERSEC SAS                                  */
/*                                                                        */
/*  Should you receive a copy of this source code, you must check you     */
/*  have a proper, written authorization of INTERSEC to hold it. If you   */
/*  don't have such an authorization, you must DELETE all source code     */
/*  files in your possession, and inform INTERSEC of the fact you obtain  */
/*  these files. Should you not comply to these terms, you can be         */
/*  prosecuted in the extent permitted by applicable law.                 */
/*                                                                        */
/**************************************************************************/

#include "blob.h"
#include "robuf.h"

robuf *robuf_init(robuf *rob)
{
    rob->data = NULL;
    rob->size = 0;
    rob->type = ROBUF_TYPE_NONE;
    blob_init(&rob->blob);
    return rob;
}

void robuf_wipe(robuf *rob)
{
    switch (rob->type) {
      case ROBUF_TYPE_NONE:
        break;
      case ROBUF_TYPE_BLOB:
        blob_wipe(&rob->blob);
        break;
      case ROBUF_TYPE_CONST:
        break;
      case ROBUF_TYPE_MALLOC:
        /* XXX: we know we "own" that data. This cast is valid */
        p_delete((byte **)&rob->data);
        break;
    }
    rob->data = NULL;
    rob->size = 0;
    rob->type = ROBUF_TYPE_NONE;
}

void robuf_reset(robuf *rob)
{
    robuf_wipe(rob);
    robuf_init(rob);
}

void robuf_make_const(robuf *dst, const byte *data, int size)
{
    robuf_wipe(dst);
    dst->type = ROBUF_TYPE_CONST;
    dst->data = data;
    dst->size = size;
}

void robuf_make_malloced(robuf *dst, const byte *data, int size)
{
    robuf_wipe(dst);
    dst->type = ROBUF_TYPE_MALLOC;
    dst->data = data;
    dst->size = size;
}

#if 0
/* RFE : I would like to be able to do this. At the moment, blob_swap_contents
 * does not exist, so we have to make this a two functions process, which
 * makes it a very efficient mine field... */
void robuf_make_from_blob(robuf *dst, blob_t *src)
{
    robuf_dst_wipe(dst);
    dst->type = ROBUF_TYPE_BLOB;
    blob_swap_contents(dst->blob, src);
}
#else
/* This code should be obsoleted asap. */
blob_t *robuf_make_blob(robuf *dst)
{
    robuf_wipe(dst);
    dst->type = ROBUF_TYPE_BLOB;
    return &dst->blob;
}

void robuf_blob_consolidate(robuf *dst)
{
    dst->data = dst->blob.data;
    dst->size = dst->blob.len;
}
#endif
