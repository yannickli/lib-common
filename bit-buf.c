/**************************************************************************/
/*                                                                        */
/*  Copyright (C) 2004-2012 INTERSEC SAS                                  */
/*                                                                        */
/*  Should you receive a copy of this source code, you must check you     */
/*  have a proper, written authorization of INTERSEC to hold it. If you   */
/*  don't have such an authorization, you must DELETE all source code     */
/*  files in your possession, and inform INTERSEC of the fact you obtain  */
/*  these files. Should you not comply to these terms, you can be         */
/*  prosecuted in the extent permitted by applicable law.                 */
/*                                                                        */
/**************************************************************************/

#include "bit.h"

void bb_wipe(bb_t *bb)
{
    switch (bb->mem_pool & MEM_POOL_MASK) {
      case MEM_STATIC:
      case MEM_STACK:
        return;
      default:
        ifree(bb->data, bb->mem_pool);
        break;
    }
}

void bb_reset(bb_t *bb)
{
    if (bb->mem_pool == MEM_LIBC && bb->size > (16 << 10)) {
        bb_wipe(bb);
        bb_init(bb);
    } else {
        bzero(bb->data, DIV_ROUND_UP(bb->len, 8));
        bb->len = 0;
    }
}

void bb_init_sb(bb_t *bb, sb_t *sb)
{
    sb_grow(sb, ROUND_UP(sb->len, 8) - sb->len);
    bb_init_full(bb, sb->data, sb->len * 8, DIV_ROUND_UP(sb->size, 8),
                 sb->mem_pool);

    /* We took ownership of the memory so ensure clear the sb */
    sb_init(sb);
}


void bb_transfer_to_sb(bb_t *bb, sb_t *sb)
{
    sb_wipe(sb);
    if (bb->b == (bb->size * 8) - 1) {
        __bb_grow(bb, 8);
    }
    sb_init_full(sb, bb->data, DIV_ROUND_UP(bb->len, 8),
                 bb->size * 8, bb->mem_pool);
    bb_init(bb);
}

void __bb_grow(bb_t *bb, size_t extra)
{
    size_t newlen = DIV_ROUND_UP(bb->len + extra, 64);
    size_t newsz;

    newsz = p_alloc_nr(bb->size);
    if (newsz < newlen) {
        newsz = newlen;
    }
    if (bb->mem_pool == MEM_STATIC) {
        uint64_t *d = p_new_raw(uint64_t, newsz);

        p_copy(d, bb->data, bb->size);
        bzero(d + bb->size, (newsz - bb->size) * 8);
        bb_init_full(bb, d, bb->len, newsz, MEM_LIBC);
    } else {
        bb->data = irealloc(bb->data, bb->size * 8, newsz * 8,
                            bb->mem_pool | MEM_RAW);
        bzero(bb->data + bb->size, (newsz - bb->size) * 8);
        bb->size = newsz;
    }
}


char *t_print_bits(uint8_t bits, uint8_t bstart, uint8_t blen)
{
    char *str = t_new(char, blen + 1);
    char *w   = str;

    for (int i = bstart; i < blen; i++) {
        *w++ = (bits & (1 << i)) ? '1' : '0';
    }

    *w = '\0';

    return str;
}

char *t_print_bb(const bb_t *bb, size_t *len)
{
    bit_stream_t bs = bs_init_bb(bb);

    return t_print_bs(bs, len);
}

void bb_be_add_bs(bb_t *bb, const bit_stream_t *b)
{
    bit_stream_t bs = *b;
    pstream_t ps;

    while (!bs_done(&bs) && !bs_is_aligned(&bs)) {
        bb_be_add_bit(bb, __bs_be_get_bit(&bs));
    }

    ps = __bs_get_bytes(&bs, bs_len(&bs) / 8);
    bb_be_add_bytes(bb, ps.b, ps_len(&ps));

    while (!bs_done(&bs)) {
        bb_be_add_bit(bb, __bs_be_get_bit(&bs));
    }
}
