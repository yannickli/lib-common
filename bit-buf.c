/**************************************************************************/
/*                                                                        */
/*  Copyright (C) 2004-2011 INTERSEC SAS                                  */
/*                                                                        */
/*  Should you receive a copy of this source code, you must check you     */
/*  have a proper, written authorization of INTERSEC to hold it. If you   */
/*  don't have such an authorization, you must DELETE all source code     */
/*  files in your possession, and inform INTERSEC of the fact you obtain  */
/*  these files. Should you not comply to these terms, you can be         */
/*  prosecuted in the extent permitted by applicable law.                 */
/*                                                                        */
/**************************************************************************/

#include "bit-buf.h"
#include "bit-stream.h"

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

/* TODO optimize */
void bb_add_bs(bb_t *bb, bit_stream_t bs)
{
    size_t len;

    while (bs_has(&bs, 8)) {
        bb_add_bits(bb, __bs_get_bits(&bs, 8), 8);
    }

    len = bs_len(&bs);
    bb_add_bits(bb, __bs_get_bits(&bs, len), len);
}
