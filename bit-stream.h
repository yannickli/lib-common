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

#ifndef IS_LIB_INET_BIT_STREAM_H
#define IS_LIB_INET_BIT_STREAM_H

#include "bit-buf.h"

typedef struct bit_stream_t {
    pstream_t    ps;
    uint8_t      rbit;
    uint8_t      pad;
} bit_stream_t;

/* API {{{ */
static ALWAYS_INLINE bit_stream_t
bs_init(const uint8_t *data, uint8_t bstart, size_t blen)
{
    bit_stream_t bs;

    data   += (bstart / 8);
    bstart %= 8;

    p_clear(&bs, 1);
    bs.ps   = ps_init(data, blen / 8);
    bs.rbit = bstart;
    bs.pad  = (8 - ((bstart + blen) % 8)) % 8;

    return bs;
}

static ALWAYS_INLINE bit_stream_t bs_init_ps(pstream_t *ps, uint8_t pad)
{
    bit_stream_t bs;

    p_clear(&bs, 1);
    bs.ps  = *ps;
    bs.pad = pad;

    return bs;
}

static ALWAYS_INLINE bit_stream_t bs_init_bb(const bb_t *bb)
{
    bit_stream_t bs;

    p_clear(&bs, 1);
    bs.ps  = ps_initsb(&bb->sb);
    bs.pad = (8 - bb->wbit) % 8;

    return bs;
}

static ALWAYS_INLINE size_t bs_len(const bit_stream_t *bs)
{
    return ps_len(&bs->ps) * 8 - bs->rbit - bs->pad;
}

static ALWAYS_INLINE bool bs_has(const bit_stream_t *bs, size_t blen)
{
    return blen <= bs_len(bs);
}

static ALWAYS_INLINE bool bs_has_bytes(const bit_stream_t *bs, size_t olen)
{
    return olen * 8 <= bs_len(bs);
}

static ALWAYS_INLINE bool bs_done(const bit_stream_t *bs)
{
    return !bs_len(bs);
}

static ALWAYS_INLINE void __bs_skip(bit_stream_t *bs, size_t blen)
{
    __ps_skip(&bs->ps, (bs->rbit + blen) / 8);
    bs->rbit = (bs->rbit + blen) % 8;
}

static ALWAYS_INLINE void bs_align(bit_stream_t *bs)
{
    if (bs->rbit) {
        __ps_skip(&bs->ps, 1);
        bs->rbit = 0;
    }
}

static ALWAYS_INLINE bool bs_is_aligned(const bit_stream_t *bs)
{
    return (bs->rbit == 0);
}

static ALWAYS_INLINE void __bs_clip(bit_stream_t *bs, size_t blen)
{
    size_t rm_blen = bs_len(bs) - blen + bs->pad;

    __ps_clip_at(&bs->ps, bs->ps.b_end - (rm_blen / 8));
    bs->pad = rm_blen % 8;
}

static inline bit_stream_t __bs_get_bs(bit_stream_t *bs, size_t blen)
{
    bit_stream_t sub = *bs;

    __bs_clip(&sub, blen);
    __bs_skip(bs, blen);

    return sub;
}

static inline bool __bs_get_bit(bit_stream_t *bs)
{
    uint8_t cur_byte  = *bs->ps.b;
    bool    res       = !!(cur_byte & (0x80 >> bs->rbit));

    if (likely(bs->rbit < 7)) {
        bs->rbit++;
    } else {
        bs->rbit = 0;
        __ps_skip(&bs->ps, 1);
    }

    return res;
}

static inline uint8_t __bs_get_bits(bit_stream_t *bs, size_t blen)
{
    uint8_t u8;

    if (unlikely(!blen))
        return 0;

    if (bs->rbit + blen > 8) {
        be16_t    be16;
        uint16_t  u16;

        be16 = *((const be16_t *)bs->ps.b);
        u16 = be_to_cpu16(be16);
        u16 <<= bs->rbit;        /* Remove bits from "left"   */
        u8 = u16 >> (16 - blen); /* Remove bits from "right"  */
    } else {
        u8 = *(const uint8_t *)bs->ps.b;
        u8 <<= bs->rbit; /* Remove bits from "left"   */
        u8 >>= 8 - blen; /* Remove bits from "right"  */
    }

    __bs_skip(bs, blen);

    return u8;
}

/* TODO optimize */
static inline bool bs_equals(bit_stream_t bs1, bit_stream_t bs2)
{
    size_t len = bs_len(&bs1);

    if (len != bs_len(&bs2))
        return false;

    while (!bs_done(&bs1)) {
        if (__bs_get_bit(&bs1) != __bs_get_bit(&bs2)) {
            return false;
        }
    }

    return true;
}

/* }}} */
/* Printing helpers {{{ */
static inline char *t_print_bs(bit_stream_t bs, size_t *len)
{
    char *res;
    SB_1k(sb);

    while (!bs_done(&bs)) {
        if (!bs.rbit) {
            sb_addc(&sb, '.');
        }

        sb_addc(&sb, __bs_get_bit(&bs) ? '1' : '0');
    }

    if (len) {
        *len = sb.len;
    }

    res = t_dupz(sb.data, sb.len);

    return res;
}

#ifndef NDEBUG
#  define e_trace_bs(lvl, bs, fmt, ...)  \
    ({                                                                     \
        t_scope;                                                           \
        static const char spaces[] = "         ";                          \
                                                                           \
        uint8_t start_blank = (bs)->rbit ? (bs)->rbit + 1 : 0;             \
                                                                           \
        e_trace(lvl, "[ %s%s%s ] --(%2zu) " fmt, spaces + 9 - start_blank, \
                t_print_bs(*(bs), NULL), spaces + 9 - (bs)->pad,           \
                bs_len(bs), ##__VA_ARGS__);                                \
    })

#else
#  define e_trace_bs(...)
#endif

/* }}} */
#endif
