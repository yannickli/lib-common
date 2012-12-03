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

#if !defined(IS_LIB_COMMON_BIT_H) || defined(IS_LIB_COMMON_BIT_BUF_H)
#  error "you must include bit.h instead"
#else
#define IS_LIB_COMMON_BIT_BUF_H

typedef struct bb_t {
    union {
        uint64_t   *data;
        byte       *bytes;
    };
    union {
        struct {
            unsigned offset : 6;
            size_t   word   : 58;
        };
        struct {
            unsigned boffset : 3;
            size_t   b       : 61;
        };
        size_t len;  /* Number of bit used */
    };
    size_t     size     : 62; /* Number of words allocated */
    flag_t     mem_pool :  2;
} bb_t;

struct bit_stream_t;

/* Initialization/Cleanup {{{ */

GENERIC_INIT(bb_t, bb);
GENERIC_NEW(bb_t, bb);

void bb_wipe(bb_t *bb);
GENERIC_DELETE(bb_t, bb);

static inline bb_t *
bb_init_full(bb_t *bb, void *buf, int blen, int bsize, int mem_pool)
{
    size_t used_bytes = DIV_ROUND_UP(blen, 8);

    bb->data = (uint64_t *)buf;
    bb->len = blen;
    bb->size = bsize;
    bb->mem_pool = mem_pool;

    bzero(bb->bytes + used_bytes, bsize * 8 - used_bytes);
    assert (bb->size >= bb->word);
    return bb;
}

#define bb_inita(bb, sz)  \
    bb_init_full(bb, p_alloca(uint64_t, DIV_ROUND_UP(sz, 8)), \
                 0, DIV_ROUND_UP(sz, 8), MEM_STATIC)

#define BB(name, sz) \
    bb_t name = { { .data = p_alloca(uint64_t, DIV_ROUND_UP(sz, 8)) }, \
                    .size = DIV_ROUND_UP(sz, 8) }
#define t_BB(name, sz) \
    bb_t name = { { .data = t_new(uint64_t, DIV_ROUND_UP(sz, 8)) }, \
                    .size = DIV_ROUND_UP(sz, 8),                    \
                    .mem_pool = MEM_STACK }

#define BB_1k(name)    BB(name, 1 << 10)
#define BB_8k(name)    BB(name, 8 << 10)
#define t_BB_1k(name)  t_BB(name, 1 << 10)
#define t_BB_8k(name)  t_BB(name, 8 << 10)

void bb_reset(bb_t *bb);

/** Initialize a bit-buffer from a string-buffer.
 *
 * The bit-buffer takes ownership of the memory and reset the sb so we're
 * guaranteed no dangling pointers will remain in the sb if the bit-buffer
 * perform a reallocation sometime in the future.
 */
void bb_init_sb(bb_t *bb, sb_t *sb);

/** Transfer ownership of the memory to a sb.
 *
 * The bit-buffer lose is resetted during the operation and the sb gain full
 * ownership of the memory.
 */
void bb_transfer_to_sb(bb_t *bb, sb_t *sb);

static inline void bb_align(bb_t *bb)
{
    bb->len = ROUND_UP(bb->len, 8);
}

void __bb_grow(bb_t *bb, size_t extra);

static inline void bb_grow(bb_t *bb, size_t extra)
{
    if (DIV_ROUND_UP(bb->len + extra, 64) >= bb->size) {
        __bb_grow(bb, extra);
    }
}

static inline void bb_growlen(bb_t *bb, size_t extra)
{
    bb_grow(bb, extra);
    bb->len += extra;
}


/* }}} */
/* Write bit, little endian {{{ */

static inline void bb_add_bit(bb_t *bb, bool v)
{
    if (bb->offset == 0) {
        bb_grow(bb, 1);
    }
    if (v) {
        bb->data[bb->word] |= 1ULL << bb->offset;
    }
    bb->len++;
}

static inline void bb_add_bits(bb_t *bb, uint64_t bits, uint8_t blen)
{
    unsigned offset = bb->offset;
    size_t   word   = bb->word;

    if (unlikely(!blen)) {
        return;
    }

    bb_growlen(bb, blen);

    if (blen != 64) {
        bits &= BITMASK_LT(uint64_t, blen);
    }
    bb->data[word] |= bits << offset;

    if (64 - offset < blen) {
        bb->data[word + 1] = bits >> (64 - offset);
    }
}

static inline void bb_add_byte(bb_t *bb, uint8_t b)
{
    bb_add_bits(bb, b, 8);
}

static inline void
__bb_add_unaligned_bytes(bb_t *bb, const byte *b, size_t len)
{
    switch (len % 8) {
      case 7:
        bb_add_bits(bb, get_unaligned_cpu32(b), 32);
        bb_add_bits(bb, get_unaligned_cpu16(b + 4), 16);
        bb_add_bits(bb, *(b + 6), 8);
        break;
      case 6:
        bb_add_bits(bb, get_unaligned_cpu32(b), 32);
        bb_add_bits(bb, get_unaligned_cpu16(b + 4), 16);
        break;
      case 5:
        bb_add_bits(bb, get_unaligned_cpu32(b), 32);
        bb_add_bits(bb, *(b + 4), 8);
        break;
      case 4:
        bb_add_bits(bb, get_unaligned_cpu32(b), 32);
        break;
      case 3:
        bb_add_bits(bb, get_unaligned_cpu16(b), 16);
        bb_add_bits(bb, *(b + 2), 8);
        break;
      case 2:
        bb_add_bits(bb, get_unaligned_cpu16(b), 16);
        break;
      case 1:
        bb_add_bits(bb, *b, 8);
        break;
    }
}

static inline void bb_add_bytes(bb_t *bb, const byte *b, size_t len)
{
    if (bb->boffset == 0) {
        bb_grow(bb, len * 8);
        memcpy(bb->bytes + bb->b, b, len);
        bb->len += len * 8;
    } else {
        size_t words;
        const size_t unaligned = ROUND_UP((intptr_t)b, 8) - (intptr_t)b;

        /* Align pointer to make arithmetic simpler */
        if (unlikely(unaligned)) {
            if (len < 8) {
                /* Don't waste time to align the buffer, there is nothing to
                 * align. */
                __bb_add_unaligned_bytes(bb, b, len);
                return;
            }
            __bb_add_unaligned_bytes(bb, b, unaligned);
            len -= unaligned;
            b   += unaligned;
        }

        words = len / 8;
        for (size_t i = 0; i < words; i++) {
            bb_add_bits(bb, *(const uint64_t *)b, 64);
            b += 8;
        }

        __bb_add_unaligned_bytes(bb, b, len % 8);
    }
}

static inline void bb_add0s(bb_t *bb, size_t len)
{
    bb_growlen(bb, len);
}

static inline void bb_add1s(bb_t *bb, size_t len)
{
    unsigned boffset = bb->boffset;
    size_t   b       = bb->b;

    bb_growlen(bb, len);

    if (boffset != 0) {
        bb->bytes[b] |= BITMASK_GE(uint64_t, boffset);
        if (len <= 8 - boffset) {
            bb->bytes[b] &= BITMASK_LT(uint64_t, boffset + len);
            return;
        }
        len -= 8 - boffset;
        b++;
    }

    if (len >= 8) {
        memset(&bb->bytes[b], 0xff, len / 8);
    }
    if (len % 8) {
        b += len / 8;
        bb->bytes[b] = BITMASK_LT(uint64_t, len % 8);
    }
}

void bb_add_bs(bb_t *bb, const struct bit_stream_t *bs) __leaf;

/* }}} */
/* Write bit, big endian {{{ */

static inline void bb_be_add_bit(bb_t *bb, bool v)
{
    if (bb->offset == 0) {
        bb_grow(bb, 1);
    }
    if (v) {
        bb->bytes[bb->b] |= 0x80 >> bb->boffset;
    }
    bb->len++;
}

static inline void bb_be_add_bits(bb_t *bb, uint64_t bits, uint8_t blen)
{
    byte *b;
    size_t offset = bb->boffset;
    size_t byte_no = bb->b;
    size_t remain = blen;

    if (unlikely(!blen))
        return;

    bb_growlen(bb, blen);

    if (blen != 64) {
        bits &= BITMASK_LT(uint64_t, blen);
    }
    b = bb->bytes + byte_no;

    if (offset + blen <= 8) {
        *b |= bits << (8 - (offset + blen));
        return;
    }
    if (offset) {
        remain -= 8 - offset;
        *b |= bits >> remain;
        b++;
    }
    while (remain >= 8) {
        remain -= 8;
        *b = bits >> remain;
        b++;
    }
    if (remain) {
        *b = bits << (8 - remain);
    }
}

static inline void bb_be_add_byte(bb_t *bb, uint8_t b)
{
    bb_be_add_bits(bb, b, 8);
}

static inline void bb_be_add_bytes(bb_t *bb, const byte *b, size_t len)
{
    if (bb->boffset == 0) {
        bb_grow(bb, len * 8);
        memcpy(bb->bytes + bb->b, b, len);
        bb->len += len * 8;
    } else {
        for (size_t i = 0; i < len; i++) {
            bb_be_add_byte(bb, b[i]);
        }
    }
}

void bb_be_add_bs(bb_t *bb, const struct bit_stream_t *bs) __leaf;


/* }}} */
/* Marking {{{ */

static ALWAYS_INLINE void bb_push_mark(bb_t *bb) { }
static ALWAYS_INLINE void bb_pop_mark(bb_t *bb) { }
static ALWAYS_INLINE void bb_reset_mark(bb_t *bb) { }
#define e_trace_be_bb_tail(...)  e_trace_be_bb(__VA_ARGS__)

/* }}} */
/* Printing helpers {{{ */

char *t_print_bits(uint8_t bits, uint8_t bstart, uint8_t blen)
    __leaf;

char *t_print_be_bb(const bb_t *bb, size_t *len)
    __leaf;

char *t_print_bb(const bb_t *bb, size_t *len)
    __leaf;


#ifndef NDEBUG
#   define e_trace_be_bb(lvl, bb, fmt, ...)  \
{                                                                      \
    bit_stream_t __bs = bs_init_bb(bb);                                \
                                                                       \
    e_trace_be_bs(lvl, &__bs, fmt, ##__VA_ARGS__);                     \
}

#   define e_trace_bb(lvl, bb, fmt, ...)  \
{                                                                      \
    bit_stream_t __bs = bs_init_bb(bb);                                \
                                                                       \
    e_trace_bs(lvl, &__bs, fmt, ##__VA_ARGS__);                        \
}
#else
#   define e_trace_be_bb(...)
#   define e_trace_bb(...)
#endif

/* }}} */
#endif
