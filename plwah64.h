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

#ifndef QKV_PLWAH64_H
#define QKV_PLWAH64_H

#include <lib-common/container.h>

/* Structures {{{ */

typedef struct plwah64_literal_t {
#if __BYTE_ORDER == __BIG_ENDIAN
    uint64_t is_fill : 1;
    uint64_t bits    : 63;
#else
    uint64_t bits    : 63;
    uint64_t is_fill : 1; /* Always 0 */
#endif
} plwah64_literal_t;

typedef struct plwah64_fill_t {
#if __BYTE_ORDER == __BIG_ENDIAN
    uint64_t is_fill   : 1; /* Always 1 */
    uint64_t val       : 1; /* 0 describe a sequence of 0,
                               1 describe a sequence of 1 */
    uint64_t position5 : 6;
    uint64_t position4 : 6;
    uint64_t position3 : 6;
    uint64_t position2 : 6;
    uint64_t position1 : 6;
    uint64_t position0 : 6;
    uint64_t counter   : 26; /* Can adress at most 63 * (2^26 - 1) bits in a
                                single word. */
#else
    uint64_t counter   : 26; /* Can adress at most 63 * (2^26 - 1) bits in a
                                single word. */
    uint64_t position0 : 6;
    uint64_t position1 : 6;
    uint64_t position2 : 6;
    uint64_t position3 : 6;
    uint64_t position4 : 6;
    uint64_t position5 : 6;
    uint64_t val       : 1; /* 0 describe a sequence of 0,
                               1 describe a sequence of 1 */
    uint64_t is_fill   : 1; /* Always 1 */
#endif
} plwah64_fill_t;

typedef struct plwah64_fillp_t {
#if __BYTE_ORDER == __BIG_ENDIAN
    uint64_t is_fill   : 1; /* Always 1 */
    uint64_t val       : 1; /* 0 describe a sequence of 0,
                               1 describe a sequence of 1 */
    uint64_t positions : 36;
    uint64_t counter   : 26; /* Can adress at most 63 * (2^26 - 1) bits in a
                                single word. */
#else
    uint64_t counter   : 26; /* Can adress at most 63 * (2^26 - 1) bits in a
                                single word. */
    uint64_t positions : 36;
    uint64_t val       : 1; /* 0 describe a sequence of 0,
                               1 describe a sequence of 1 */
    uint64_t is_fill   : 1; /* Always 1 */
#endif
} plwah64_fillp_t;

#define PLWAH64_FILL_INIT(Type, Count)                                       \
        { .is_fill = 1, .val = Type, .counter = Count }
#define PLWAH64_FILL_INIT_V(Type, Count)                                     \
        (plwah64_fill_t)PLWAH64_FILL_INIT(Type, Count)
#define PLWAH64_MAX_COUNTER       ((1UL << 26) - 1)
#define PLWAH64_MAX_BITS_IN_WORD  (63 * PLWAH64_MAX_COUNTER)

typedef union plwah64_t {
    uint64_t          word;
    plwah64_literal_t lit;
    plwah64_fill_t    fill;
    plwah64_fillp_t   fillp;
    struct {
#if __BYTE_ORDER == __BIG_ENDIAN
        uint64_t is_fill : 1;
        uint64_t bits    : 63;
#else
        uint64_t bits    : 63;
        uint64_t is_fill : 1; /* Always 0 */
#endif
    };
} plwah64_t;
qvector_t(plwah64, plwah64_t);

typedef struct plwah64_map_t {
    uint64_t      bit_len;
    uint8_t       remain;
    qv_t(plwah64) bits;
} plwah64_map_t;
#define PLWAH64_MAP_INIT    { .bit_len = 0, .remain = 0 }
#define PLWAH64_MAP_INIT_V  (plwah64_map_t)PLWAH64_MAP_INIT

/* }}} */
/* Utility functions {{{ */

static inline
void plwah64_append_fill(qv_t(plwah64) *map, plwah64_fill_t fill)
{
    plwah64_t  elm = { .fill = fill };
    plwah64_t *last;
    assert (fill.is_fill);
    if (map->len == 0) {
        qv_append(plwah64, map, elm);
        return;
    }
    last = qv_last(plwah64, map);
    if (last->is_fill && last->fill.val == fill.val && !last->fillp.positions) {
        last->fill.counter   += fill.counter;
        last->fillp.positions = elm.fillp.positions;
    } else {
        qv_append(plwah64, map, elm);
    }
}

static inline
void plwah64_append_literal(qv_t(plwah64) *map, plwah64_literal_t lit)
{
    plwah64_t *last;
    assert (!lit.is_fill);
    if (lit.bits == 0) {
        plwah64_append_fill(map, PLWAH64_FILL_INIT_V(0, 1));
        return;
    } else
    if (lit.bits == 0x7fffffffffffffffUL) {
        plwah64_append_fill(map, PLWAH64_FILL_INIT_V(1, 1));
    }
    if (map->len == 0) {
        qv_append(plwah64, map, (plwah64_t)lit);
        return;
    }
    last = qv_last(plwah64, map);
    if (last->is_fill && last->fillp.positions == 0) {
        uint64_t word = lit.bits;;
        if (last->fill.val == 1) {
            word = ~word;
        };
#define FIND_AND_SET_POS(i)  do {                                            \
            size_t __bit = bsf64(word);                                      \
            last->fill.position##i = __bit + 1;                              \
            word = word & ~(1UL << __bit);                                   \
        } while (0)
        switch (bitcount64(word)) {
          case 6:
            FIND_AND_SET_POS(5);
          case 5:
            FIND_AND_SET_POS(4);
          case 4:
            FIND_AND_SET_POS(3);
          case 3:
            FIND_AND_SET_POS(2);
          case 2:
            FIND_AND_SET_POS(1);
          case 1:
            FIND_AND_SET_POS(0);
            return;
          case 0:
            e_panic("This should not happen");
#undef FIND_AND_SET_POS
      }
    }
    qv_append(plwah64, map, (plwah64_t)lit);
}

static inline
void plwah64_append_word(qv_t(plwah64) *map, plwah64_t word)
{
    STATIC_ASSERT(sizeof(plwah64_t) == sizeof(uint64_t));
    STATIC_ASSERT(sizeof(plwah64_literal_t) == sizeof(uint64_t));
    STATIC_ASSERT(sizeof(plwah64_fill_t) == sizeof(uint64_t));
    if (word.is_fill) {
        plwah64_append_fill(map, word.fill);
    } else {
        plwah64_append_literal(map, word.lit);
    }
}


#define READ(V)                                                              \
        if (V##pos < 0) {                                                    \
            V##word = V[0];                                                  \
            if (V##word.lit.is_fill) {                                       \
                V##pos = V##word.fill.counter;                               \
            } else {                                                         \
                V##pos = 1;                                                  \
            }                                                                \
            assert (V##pos > 0);                                             \
        }
#define APPLY_POS(V, i)                                                      \
        if (V##word.fill.position##i != 0) {                                 \
            word |= 1UL << (V##word.fill.position##i - 1);                   \
        }
#define NEXT(V)                                                              \
        if (V##pos == 0) {                                                   \
            if (V##word.fill.is_fill && V##word.fillp.positions) {           \
                uint64_t word = 0;                                           \
                APPLY_POS(V, 0);                                             \
                APPLY_POS(V, 1);                                             \
                APPLY_POS(V, 2);                                             \
                APPLY_POS(V, 3);                                             \
                APPLY_POS(V, 4);                                             \
                APPLY_POS(V, 5);                                             \
                V##word.word = word;                                         \
                V##pos = 1;                                                  \
            } else {                                                         \
                V##len--;                                                    \
                V++;                                                         \
            }                                                                \
        }

#define TYPSWAP(x, y)  SWAP(typeof(x), x, y)
#define SWAP_VARS()  do {                                                    \
            TYPSWAP(x, y);                                                   \
            TYPSWAP(xlen, ylen);                                             \
            TYPSWAP(xpos, ypos);                                             \
            TYPSWAP(xword, yword);                                           \
        } while (0)

static inline
void plwah64_or(qv_t(plwah64) *map, const plwah64_t *x, int xlen,
                const plwah64_t *y, int ylen)
{
    int xpos = 0;
    int ypos = 0;

    plwah64_t xword;
    plwah64_t yword;

    while (xlen > 0 && ylen > 0) {
        READ(x);
        READ(y);
        /* TODO: Efficient implementation when the one of the words a 1-fill
         * word.
         */
        if (xword.fill.is_fill && yword.fill.is_fill) {
            int words = MIN(xpos, ypos);
            int type  = !!(xword.fill.val | yword.fill.val);
            plwah64_append_fill(map, PLWAH64_FILL_INIT_V(type, words));
            xpos -= words;
            ypos -= words;
        } else {
            if ((xword.fill.is_fill && xword.fill.val)
            || (yword.fill.is_fill && yword.fill.val)) {
                plwah64_append_fill(map, PLWAH64_FILL_INIT_V(1, 1));
            } else
            if (xword.fill.is_fill) {
                plwah64_append_literal(map, yword.lit);
            } else
            if (yword.fill.is_fill) {
                plwah64_append_literal(map, xword.lit);
            } else {
                xword.word |= yword.word;
                plwah64_append_literal(map, yword.lit);
            }
            xpos--;
            ypos--;
        }
        NEXT(x);
        NEXT(y);
    }
    if (xlen == 0) {
        SWAP_VARS();
    }
    if (xpos > 0) {
        if (xword.fill.is_fill) {
            xword.fill.counter = xpos;
        }
        plwah64_append_word(map, xword);
    }
    for (int i = 0; i < xlen; i++) {
        plwah64_append_word(map, x[i]);
    }
}

static inline
void plwah64_and(qv_t(plwah64) *map, const plwah64_t *x, int xlen,
                 const plwah64_t *y, int ylen)
{
    int xpos = 0;
    int ypos = 0;

    plwah64_t xword;
    plwah64_t yword;

    while (xlen > 0 && ylen > 0) {
        READ(x);
        READ(y);
        /* TODO: Efficient implementation when one of the words is a 0-fill
         * word.
         */
        if (xword.fill.is_fill && yword.fill.is_fill) {
            int words = MIN(xpos, ypos);
            int type  = !!(xword.fill.val & yword.fill.val);
            plwah64_append_fill(map, PLWAH64_FILL_INIT_V(type, words));
            xpos -= words;
            ypos -= words;
        } else {
            if ((xword.fill.is_fill && !xword.fill.val)
            || (yword.fill.is_fill && !yword.fill.val)) {
                plwah64_append_fill(map, PLWAH64_FILL_INIT_V(1, 0));
            } else
            if (xword.fill.is_fill) {
                plwah64_append_literal(map, yword.lit);
            } else
            if (yword.fill.is_fill) {
                plwah64_append_literal(map, xword.lit);
            } else {
                xword.word &= yword.word;
                plwah64_append_literal(map, yword.lit);
            }
            xpos--;
            ypos--;
        }
        NEXT(x);
        NEXT(y);
    }
}

#undef SWAP_VARS
#undef NEXT
#undef APPLY_POS
#undef READ

static inline
int plwah64_build_bit(plwah64_t data[static 2], uint32_t pos)
{
    uint32_t counter = pos / 63;
    if (counter != 0) {
        pos = pos % 63;
    }

    if (counter == 0) {
        data[0].word = 1UL << pos;
        return 1;
    } else
    if (pos < PLWAH64_MAX_BITS_IN_WORD) {
        data[0].fill = PLWAH64_FILL_INIT_V(0, counter);
        data[0].fill.position0 = pos + 1;
        return 1;
    } else {
        counter -= PLWAH64_MAX_COUNTER;
        data[0].fill = PLWAH64_FILL_INIT_V(0, PLWAH64_MAX_COUNTER);
        data[1].fill = PLWAH64_FILL_INIT_V(0, counter);
        data[1].fill.position0 = pos + 1;
        return 2;
    }
}

static inline
void plwah64_add_(plwah64_map_t *map, const byte *bits, uint64_t bit_len,
                  bool fill_with_1)
{
    uint64_t bits_pos = 0;

#define READ_BITS(count)  ({                                                 \
        uint64_t __bits  = 0;                                                \
        uint8_t __count = (count);                                           \
        if (bits == NULL) {                                                  \
            if (fill_with_1) {                                               \
                __bits = (UINT64_C(1) << __count) - 1;                       \
            }                                                                \
            bits_pos += __count;                                             \
        } else {                                                             \
            uint64_t __off   = 0;                                            \
            while (__count > 0) {                                            \
                uint8_t __read = MIN(8 - (bits_pos & 7), __count);           \
                uint8_t __mask = ((1 << __read) - 1) << (bits_pos & 7);      \
                __bits  |= (uint64_t)(bits[bits_pos >> 3] & __mask) << __off;\
                bits_pos += __read;                                          \
                __count  -= __read;                                          \
                __off    += __read;                                          \
            }                                                                \
        }                                                                    \
        __bits;                                                              \
    })

    if (map->remain != 0) {
        int take = MIN(bit_len, map->remain);
        plwah64_t word = { .word = READ_BITS(take) };
        plwah64_t *last;
        assert (map->bits.len > 0);
        word.word <<= 63 - map->remain;
        assert (!word.is_fill);
        last = qv_last(plwah64, &map->bits);
        if (last->is_fill && last->fillp.positions == 0) {
            assert (last->fill.val == 0);
            if (last->fill.counter == 0) {
                map->bits.len--;
            } else {
                last->fill.counter--;
            }
            plwah64_append_literal(&map->bits, word.lit);
        } else
        if (last->is_fill) {
            uint64_t old_bits = 0;
#define APPLY_POS(i)                                                         \
            if (last->fill.position##i) {                                    \
                old_bits |= 1UL << (last->fill.position##i - 1);             \
            }
            APPLY_POS(0);
            APPLY_POS(1);
            APPLY_POS(2);
            APPLY_POS(3);
            APPLY_POS(4);
            APPLY_POS(5);
#undef APPLY_POS
            if (last->fill.val) {
                old_bits = ~old_bits;
            }
            assert ((old_bits & (UINT64_MAX << (63 - map->remain))) == 0);
            word.word |= old_bits;
            last->fillp.positions = 0;
            plwah64_append_literal(&map->bits, word.lit);
        } else {
            assert ((last->word & (UINT64_MAX << (63 - map->remain))) == 0);
            last->word |= word.word;
        }
        map->bit_len += take;
        map->remain  -= take;
    }
    while (bits_pos < bit_len) {
        int take = MIN(63, bit_len);
        plwah64_t word = { .word = READ_BITS(take) };
        assert (map->remain == 0);
        assert (!word.is_fill);
        plwah64_append_literal(&map->bits, word.lit);
        map->bit_len += take;
        if (take != 63) {
            map->remain = 63 - take;
        }
    }
#undef READ_BITS
}

/* }}} */

static inline
void plwah64_add(plwah64_map_t *map, const byte *bits, uint64_t bit_len)
{
    plwah64_add_(map, bits, bit_len, false);
}

static inline
void plwah64_add0s(plwah64_map_t *map, uint64_t bit_len)
{
    plwah64_add_(map, NULL, bit_len, false);
}

static inline
void plwah64_add1s(plwah64_map_t *map, uint64_t bit_len)
{
    plwah64_add_(map, NULL, bit_len, true);
}

static inline
void plwah64_reset_map(plwah64_map_t *map)
{
    map->bits.len = 0;
    map->bit_len  = 0;
    map->remain   = 0;
}

static inline
bool plwah64_get(const plwah64_map_t *map, uint32_t pos)
{
    if (pos >= map->bit_len) {
        return false;
    }
    for (int i = 0; i < map->bits.len; i++) {
        plwah64_t word = map->bits.tab[i];
        if (word.lit.is_fill) {
            uint64_t count = word.fill.counter * 63;
            if (pos < count) {
                return !!word.fill.val;
            }
            pos -= count;
            if (pos < 63 && word.fillp.positions != 0) {
#define CHECK_POS(i)                                                         \
                if (word.fill.position##i == pos + 1) {                      \
                    return !word.fill.val;                                   \
                }
                CHECK_POS(0);
                CHECK_POS(1);
                CHECK_POS(2);
                CHECK_POS(3);
                CHECK_POS(4);
                CHECK_POS(5);
#undef CHECK_POS
                pos -= 63;
            }
        } else
        if (pos < 63) {
            return !!(word.word & (1UL << pos));
        } else {
            pos -= 63;
        }
    }
    return false;
}

static inline
void plwah64_debug_print(const plwah64_map_t *map, int len)
{
    for (int i = 0; i < len; i++) {
        plwah64_t m = map->bits.tab[i];
        if (m.is_fill) {
            fprintf(stderr, "* fill word with %d, counter %d, pos %lx\n",
                    m.fill.val, m.fill.counter, (uint64_t)m.fillp.positions);
        } else {
            fprintf(stderr, "* literal word %lx\n", (uint64_t)m.lit.bits);
        }
    }
}

#endif
