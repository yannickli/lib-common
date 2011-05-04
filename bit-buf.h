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

#ifndef IS_LIB_INET_BIT_BUF_H
#define IS_LIB_INET_BIT_BUF_H

#include <lib-common/arith.h>
#include <lib-common/str.h>
#include <lib-common/container.h>

typedef struct bb_t {
    sb_t        sb;
    uint8_t     wbit;
    flag_t      not_sb_owner : 1;
#ifndef NDEBUG
    qv_t(u32)   marks;
#endif
} bb_t;

/* API {{{ */
static ALWAYS_INLINE bb_t *bb_init(bb_t *bb)
{
    p_clear(bb, 1);
#ifndef NDEBUG
    qv_init(u32, &bb->marks);
#endif

    return bb;
}

static ALWAYS_INLINE void bb_wipe(bb_t *bb)
{
    if (likely(!bb->not_sb_owner)) {
        sb_wipe(&bb->sb);
    }

#ifndef NDEBUG
    qv_wipe(u32, &bb->marks);
#endif
}

static ALWAYS_INLINE bb_t bb_init_sb(sb_t *sb)
{
    bb_t bb;

    p_clear(&bb, 1);
    bb.sb = *sb;
    bb.not_sb_owner = true;
#ifndef NDEBUG
    qv_init(u32, &bb.marks);
#endif

    return bb;
}

static inline bb_t *
bb_init_full(bb_t *bb, void *buf, int blen, int bsize, int mem_pool)
{
    *bb = (bb_t){
        .sb = *sb_init_full(&bb->sb, buf, blen, bsize, mem_pool),
    };

    return bb;
}

#define bb_inita(bb, sz)  bb_init_full(bb, alloca(sz), 0, (sz), MEM_STATIC)

#define BB(name, sz) \
    bb_t name = {                                           \
        .sb = {                                             \
            .data = alloca(sz),                             \
            .size = (STATIC_ASSERT((sz) < (64 << 10)), sz), \
        },                                                  \
    }

#define BB_1k(name)    BB(name, 1 << 10)
#define BB_8k(name)    BB(name, 8 << 10)

static ALWAYS_INLINE void bb_reset(bb_t *bb)
{
    sb_reset(&bb->sb);
    bb->wbit = 0;
#ifndef NDEBUG
    qv_clear(u32, &bb->marks);
#endif
}

static ALWAYS_INLINE size_t bb_len(bb_t *bb)
{
    return bb->sb.len * 8 - ((8 - bb->wbit) % 8);
}

static inline void bb_add_bit(bb_t *bb, bool v)
{
    sb_t *sb = &bb->sb;

    if (!bb->wbit) {
        sb_addc(sb, 0);
    }

    if (v) {
        sb->data[sb->len - 1] |= (0x80 >> bb->wbit);
    }

    if (likely(bb->wbit < 7)) {
        bb->wbit++;
    } else {
        bb->wbit = 0;
    }
}

static inline void bb_add_bits(bb_t *bb, uint8_t bits, uint8_t blen)
{
    sb_t *sb = &bb->sb;

    if (unlikely(!blen))
        return;

    if (unlikely(!bb->wbit)) {
        sb_addc(sb, bits << (8 - blen));
    } else {
        if (bb->wbit + blen <= 8) {
            sb->data[sb->len - 1] |= (bits << (8 - bb->wbit - blen));
        } else {
            uint16_t u16  = (bits << (16 - bb->wbit - blen));
            be16_t   be16 = cpu_to_be16(u16);

            sb_addc(sb, 0);
            *(be16_t *)&sb->data[sb->len - 2] |= be16;
        }
    }

    bb->wbit += blen;
    bb->wbit %= 8;
}

static inline void bb_add_byte(bb_t *bb, uint8_t b)
{
    bb_add_bits(bb, b, 8);
}

static inline void bb_align(bb_t *bb)
{
    bb->wbit = 0;
}

struct bit_stream_t;
void bb_add_bs(bb_t *bb, struct bit_stream_t bs);

#ifndef NDEBUG
static inline void bb_push_mark(bb_t *bb)
{
    qv_append(u32, &bb->marks, bb_len(bb));
}

static inline void bb_pop_mark(bb_t *bb)
{
    qv_shrink(u32, &bb->marks, 1);
}

static inline void bb_reset_mark(bb_t *bb)
{
    bb->marks.tab[bb->marks.len - 1] = bb_len(bb);
}

#    define e_trace_bb_tail(lvl, bb, fmt, ...)  \
    ({                                                                     \
        bit_stream_t bs = bs_init_bb(bb);                                  \
                                                                           \
        __bs_skip(&bs, bb->marks.tab[bb->marks.len - 1]);                  \
        e_trace_bs(lvl, &bs, fmt, ##__VA_ARGS__);                          \
    })

#else
#    define bb_push_mark(...)
#    define bb_pop_mark(...)
#    define bb_reset_mark(...)
#    define e_trace_bb_tail(...)
#endif

/* }}} */
/* Printing helpers {{{ */
char *t_print_bits(uint8_t bits, uint8_t bstart, uint8_t blen);
char *t_print_bb(const bb_t *bb, size_t *len);

#define e_trace_bb(lvl, bb, fmt, ...)  \
{                                                                      \
    bit_stream_t __bs = bs_init_bb(bb);                                \
                                                                       \
    e_trace_bs(lvl, &__bs, fmt, ##__VA_ARGS__);                        \
}

/* }}} */
#endif
