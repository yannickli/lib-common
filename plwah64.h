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

typedef struct plwah64_path_t {
    uint64_t bit_in_word;
    int      word_offset;
    bool     in_pos;
} plwah64_path_t;

typedef struct plwah64_map_t {
    uint64_t      bit_len;
    uint32_t      generation;
    uint8_t       remain;
    qv_t(plwah64) bits;
} plwah64_map_t;

#define PLWAH64_WORD_BITS         (63)
#define PLWAH64_POSITION_COUNT    (6)
#define PLWAH64_MAX_COUNTER       ((1UL << 26) - 1)
#define PLWAH64_MAX_BITS_IN_WORD  (PLWAH64_WORD_BITS * PLWAH64_MAX_COUNTER)

#define PLWAH64_MAP_INIT  ({                                                 \
        plwah64_map_t __map;                                                 \
        p_clear(&__map, 1);                                                  \
        __map;                                                               \
    })

#define PLWAH64_FILL_INIT(Type, Count)  ({                                   \
        plwah64_t __fill = { 0 };                                            \
        __fill.is_fill = 1;                                                  \
        __fill.fill.val = !!(Type);                                          \
        __fill.fill.counter = (Count);                                       \
        __fill.fill;                                                         \
    })

#define PLWAH64_FROM_FILL(Fill)  ({                                          \
        plwah64_t __word;                                                    \
        __word.fill = (Fill);                                                \
        __word;                                                              \
    })

#define PLWAH64_FROM_LIT(Lit)  ({                                            \
        plwah64_t __word;                                                    \
        __word.lit = (Lit);                                                  \
        __word;                                                              \
    })

#define PLWAH64_ALL_1     ((UINT64_C(1) << PLWAH64_WORD_BITS) - 1)
#define PLWAH64_ALL_0     UINT64_C(0)

/* }}} */
/* Utility functions {{{ */

#define APPLY_POS(i, Dest, Src)                                              \
    if ((Src).position##i) {                                                 \
        Dest |= UINT64_C(1) << ((Src).position##i - 1);                      \
    }

#define APPLY_POSITIONS(Src)  ({                                             \
        plwah64_t __word = { 0 };                                            \
        APPLY_POS(0, __word.word, Src);                                      \
        APPLY_POS(1, __word.word, Src);                                      \
        APPLY_POS(2, __word.word, Src);                                      \
        APPLY_POS(3, __word.word, Src);                                      \
        APPLY_POS(4, __word.word, Src);                                      \
        APPLY_POS(5, __word.word, Src);                                      \
        if ((Src).val) {                                                     \
            __word.bits = ~__word.bits;                                      \
        }                                                                    \
        __word;                                                              \
    })

#define BUILD_POS(i, Dest, Src)                                              \
      case i + 1: {                                                          \
        size_t __bit = bsf64(Src);                                           \
        (Dest).position##i = __bit + 1;                                      \
        (Src) &= ~(UINT64_C(1) << __bit);                                    \
      }

#define BUILD_POSITIONS(Dest, Src, DONE_CASE, ERR_CASE)                      \
    do {                                                                     \
        uint64_t __word = (Src);                                             \
        if ((Dest).val) {                                                    \
            __word |= UINT64_C(1) << 63;                                     \
            __word  = ~__word;                                               \
        }                                                                    \
        switch (bitcount64(__word)) {                                        \
          BUILD_POS(5, Dest, __word);                                        \
          BUILD_POS(4, Dest, __word);                                        \
          BUILD_POS(3, Dest, __word);                                        \
          BUILD_POS(2, Dest, __word);                                        \
          BUILD_POS(1, Dest, __word);                                        \
          BUILD_POS(0, Dest, __word);                                        \
          DONE_CASE;                                                         \
          break;                                                             \
         default:                                                            \
          ERR_CASE;                                                          \
          break;                                                             \
        }                                                                    \
    } while (0)

#define READ_POSITIONS(Src, CASE)  ({                                        \
        int set_pos = 0;                                                     \
        if ((Src).position0) {                                               \
            CASE(0, (Src).position0 - 1);                                    \
            if ((Src).position1) {                                           \
                CASE(1, (Src).position1 - 1);                                \
                if ((Src).position2) {                                       \
                    CASE(2, (Src).position2 - 1);                            \
                    if ((Src).position3) {                                   \
                        CASE(3, (Src).position3 - 1);                        \
                        if ((Src).position4) {                               \
                            CASE(4, (Src).position4 - 1);                    \
                            if ((Src).position5) {                           \
                                CASE(5, (Src).position5 - 1);                \
                                set_pos = 6;                                 \
                            } else {                                         \
                                set_pos = 5;                                 \
                            }                                                \
                        } else {                                             \
                            set_pos = 4;                                     \
                        }                                                    \
                    } else {                                                 \
                        set_pos = 3;                                         \
                    }                                                        \
                } else {                                                     \
                    set_pos = 2;                                             \
                }                                                            \
            } else {                                                         \
                set_pos = 1;                                                 \
            }                                                                \
        }                                                                    \
        set_pos;                                                             \
    })

#ifdef __cplusplus
#define restrict __restrict
#endif

static inline
void plwah64_normalize_fill_(qv_t(plwah64) *map, int at, int *into)
{
    plwah64_t * restrict prev = &map->tab[*into];
    plwah64_t * restrict cur  = &map->tab[at];
    if (prev->is_fill && prev->fill.val == cur->fill.val
    && !prev->fillp.positions) {
        prev->fill.counter   += cur->fill.counter;
        prev->fillp.positions = cur->fillp.positions;
    } else {
        (*into)++;
        if (*into != at) {
            map->tab[*into] = *cur;
        }
    }
}

static inline
void plwah64_normalize_literal_(qv_t(plwah64) *map, int at, int *into)
{
    plwah64_t * restrict prev = &map->tab[*into];
    plwah64_t * restrict cur  = &map->tab[at];
    if (prev->is_fill && prev->fillp.positions == 0) {
        BUILD_POSITIONS(prev->fill, cur->bits, return,);
    }
    (*into)++;
    if (*into != at) {
        map->tab[*into] = *cur;
    }
}

static inline
bool plwah64_normalize_cleanup_(qv_t(plwah64) *map, int at)
{
    plwah64_t * restrict cur  = &map->tab[at];
    if (cur->is_fill) {
        if (cur->fill.counter == 0 && cur->fillp.positions == 0) {
            /* just drop this word */
            return false;
        } else
        if (cur->fill.counter == 0) {
            *cur = APPLY_POSITIONS(cur->fill);
            return true;
        }
    } else
    if (cur->bits == PLWAH64_ALL_0) {
        *cur = (plwah64_t){ .fill = PLWAH64_FILL_INIT(0, 1) };
        return true;
    } else
    if (cur->bits == PLWAH64_ALL_1) {
        *cur = (plwah64_t){ .fill = PLWAH64_FILL_INIT(1, 1) };
        return true;
    }
    return true;
}

static inline
void plwah64_normalize(qv_t(plwah64) *map, int from, int to)
{
    int pos = from <= 0 ? 0 : from - 1;
    if (to >= map->len) {
        to = map->len;
    }
    if (from < 0) {
        from = 0;
    }
    for (int i = from; i < to; i++) {
        if (!plwah64_normalize_cleanup_(map, i)) {
            continue;
        }
        if (i == 0) {
            continue;
        }
        if (map->tab[i].is_fill) {
            plwah64_normalize_fill_(map, i, &pos);
        } else {
            plwah64_normalize_literal_(map, i, &pos);
        }
    }
    if (pos + 1 != to) {
        qv_splice(plwah64, map, pos + 1, to - pos - 1, NULL, 0);
    }
}

static inline
void plwah64_append_word(qv_t(plwah64) *map, plwah64_t word)
{
    STATIC_ASSERT(sizeof(plwah64_t) == sizeof(uint64_t));
    STATIC_ASSERT(sizeof(plwah64_literal_t) == sizeof(uint64_t));
    STATIC_ASSERT(sizeof(plwah64_fill_t) == sizeof(uint64_t));
    qv_append(plwah64, map, word);
    plwah64_normalize(map, map->len - 2, map->len);
}

static inline
void plwah64_append_fill(qv_t(plwah64) *map, plwah64_fill_t fill)
{
    assert (fill.is_fill);
    plwah64_append_word(map, PLWAH64_FROM_FILL(fill));
}

static inline
void plwah64_append_literal(qv_t(plwah64) *map, plwah64_literal_t lit)
{
    assert (!lit.is_fill);
    plwah64_append_word(map, PLWAH64_FROM_LIT(lit));
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
#define NEXT(V)                                                              \
        if (V##pos == 0) {                                                   \
            if (V##word.fill.is_fill && V##word.fillp.positions) {           \
                V##word = APPLY_POSITIONS(V##word.fill);                     \
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
            plwah64_append_fill(map, PLWAH64_FILL_INIT(type, words));
            xpos -= words;
            ypos -= words;
        } else {
            if ((xword.fill.is_fill && xword.fill.val)
            || (yword.fill.is_fill && yword.fill.val)) {
                plwah64_append_fill(map, PLWAH64_FILL_INIT(1, 1));
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
            plwah64_append_fill(map, PLWAH64_FILL_INIT(type, words));
            xpos -= words;
            ypos -= words;
        } else {
            if ((xword.fill.is_fill && !xword.fill.val)
            || (yword.fill.is_fill && !yword.fill.val)) {
                plwah64_append_fill(map, PLWAH64_FILL_INIT(1, 0));
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
#undef READ

static inline
int plwah64_build_bit(plwah64_t data[2], uint32_t pos)
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
        data[0].fill = PLWAH64_FILL_INIT(0, counter);
        data[0].fill.position0 = pos + 1;
        return 1;
    } else {
        counter -= PLWAH64_MAX_COUNTER;
        data[0].fill = PLWAH64_FILL_INIT(0, PLWAH64_MAX_COUNTER);
        data[1].fill = PLWAH64_FILL_INIT(0, counter);
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
                uint8_t __mask = ((1 << __read) - 1);                        \
                uint8_t __val  = bits[bits_pos >> 3] >> (bits_pos & 7);      \
                __bits  |= (uint64_t)(__val & __mask) << __off;              \
                bits_pos += __read;                                          \
                __count  -= __read;                                          \
                __off    += __read;                                          \
            }                                                                \
        }                                                                    \
        __bits;                                                              \
    })

    if (map->remain != 0) {
        int take = MIN(bit_len, map->remain);
        plwah64_t word = { READ_BITS(take) };
        plwah64_t *last;
        assert (map->bits.len > 0);
        word.word <<= 63 - map->remain;
        assert (!word.is_fill);
        last = qv_last(plwah64, &map->bits);
        if (last->is_fill && last->fillp.positions == 0) {
            assert (last->fill.val == 0);
            if (last->fill.counter == 1) {
                map->bits.len--;
            } else {
                last->fill.counter--;
            }
            plwah64_append_literal(&map->bits, word.lit);
        } else
        if (last->is_fill) {
            plwah64_t old_bits = APPLY_POSITIONS(last->fill);
            assert ((old_bits.word & (UINT64_MAX << (63 - map->remain))) == 0);
            word.word |= old_bits.word;
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
        int take = MIN(63, bit_len - bits_pos);
        plwah64_t word = { READ_BITS(take) };
        assert (map->remain == 0);
        assert (!word.is_fill);
        plwah64_append_literal(&map->bits, word.lit);
        map->bit_len += take;
        if (take != 63) {
            map->remain = 63 - take;
        }
    }
    map->generation++;
#undef READ_BITS
}

static inline
plwah64_path_t plwah64_find(const plwah64_map_t *map, uint64_t pos)
{
    if (pos >= map->bit_len) {
        return (plwah64_path_t){
            /* .bit_in_word = */ pos - map->bit_len,
            /* .word_offset = */ -1,
            /* .in_pos      = */ false
        };
    }
    for (int i = 0; i < map->bits.len; i++) {
        plwah64_t word = map->bits.tab[i];
        if (word.is_fill) {
            uint64_t count = word.fill.counter * 63;
            if (pos < count) {
                return (plwah64_path_t){
                    /* .bit_in_word = */ pos,
                    /* .word_offset = */ i,
                    /* .in_pos      = */ 0,
                };
            }
            pos -= count;
            if (pos < 63 && word.fillp.positions != 0) {
                return (plwah64_path_t){
                    /* .bit_in_word = */ pos,
                    /* .word_offset = */ i,
                    /* .in_pos      = */ true,
                };
            }
        } else
        if (pos < 63) {
            return (plwah64_path_t){
                /* .bit_in_word = */ pos,
                /* .word_offset = */ i,
                /* .in_pos      = */ false,
            };
        } else {
            pos -= 63;
        }
    }
    e_panic("This should not happen");
}

static inline __must_check__
bool plwah64_get_(const plwah64_map_t *map, plwah64_path_t path)
{
    plwah64_t word;
    if (path.word_offset < 0) {
        return false;
    }

    word = map->bits.tab[path.word_offset];
    if (word.is_fill) {
        if (path.in_pos) {
#define CASE(p, Val)                                                         \
            if ((uint64_t)(Val) == path.bit_in_word) {                       \
                return !word.fill.val;                                       \
            }
            READ_POSITIONS(word.fill, CASE);
#undef CASE
        }
        return !!word.fill.val;
    } else {
        return !!(word.word & (UINT64_C(1) << path.bit_in_word));
    }
}

static inline
void plwah64_set_(plwah64_map_t *map, plwah64_path_t path, bool set)
{
    plwah64_t *word;
    const int i = path.word_offset;
    const uint64_t pos = path.bit_in_word;
    if (path.word_offset < 0) {
        if (set) {
            plwah64_add_(map, NULL, path.bit_in_word, false);
            plwah64_add_(map, NULL, 1, true);
        } else {
            plwah64_add_(map, NULL, path.bit_in_word + 1, false);
        }
    }

    word = &map->bits.tab[path.word_offset];
    if (word->is_fill) {
        if (path.in_pos) {
#define CASE(i, Val)                                                         \
            if ((uint64_t)(Val) == pos) {                                    \
                if (!!word->fill.val != !!set) {                             \
                    /* Bit is already at the expected value */               \
                    return;                                                  \
                }                                                            \
                switch (i) {                                                 \
                  case 0: word->fill.position0 = word->fill.position1;       \
                  case 1: word->fill.position1 = word->fill.position2;       \
                  case 2: word->fill.position2 = word->fill.position3;       \
                  case 3: word->fill.position3 = word->fill.position4;       \
                  case 4: word->fill.position4 = word->fill.position5;       \
                  case 5: word->fill.position5 = 0;                          \
                }                                                            \
                return;                                                      \
            }
#define SET_POS(p)                                                           \
              case p:                                                        \
                word->fill.position##p = pos + 1;                            \
                map->generation++;                                           \
                return;

            switch (READ_POSITIONS(word->fill, CASE)) {
              SET_POS(1);
              SET_POS(2);
              SET_POS(3);
              SET_POS(4);
              SET_POS(5);
              case 0:
                e_panic("This should not happen");
              default: {
                plwah64_t new_word = APPLY_POSITIONS(word->fill);
                if (set) {
                    new_word.word |= UINT64_C(1) << pos;
                } else {
                    new_word.word &= ~(UINT64_C(1) << pos);
                }
                word->fillp.positions = 0;
                qv_insert(plwah64, &map->bits, i + 1, new_word);
                map->generation++;
                return;
              } break;
            }
#undef SET_POS
#undef CASE
        } else {
            uint64_t bit_offset;
            uint64_t word_offset;
            if (!!word->fill.val == !!set) {
                /* The bit is already at the good value */
                return;
            }
            bit_offset = pos % 63;
            word_offset = pos / 63;

            /* Split the fill word */
            if (word_offset == (uint64_t)(word->fill.counter - 1)) {
                if (word->fillp.positions) {
                    plwah64_t new_word = APPLY_POSITIONS(word->fill);
                    word->fillp.positions = 0;
                    qv_insert(plwah64, &map->bits, i + 1, new_word);
                }
                if (word_offset == 0) {
                    uint64_t new_word = UINT64_C(1) << bit_offset;
                    if (!set) {
                        new_word = ~new_word;
                    }
                    word->is_fill = false;
                    word->bits    = new_word;
                } else {
                    word->fill.counter--;
                    word->fill.position0 = bit_offset + 1;
                }
            } else {
                word->fill.counter -= word_offset + 1;
                if (word_offset == 0) {
                    plwah64_t new_word = { UINT64_C(1) << bit_offset };
                    if (!set) {
                        new_word.word = ~new_word.word;
                        new_word.is_fill = false;
                    }
                    qv_insert(plwah64, &map->bits, i, new_word);
                } else {
                    plwah64_t new_word = PLWAH64_FROM_FILL(
                        PLWAH64_FILL_INIT(!set, word_offset));
                    new_word.fill.position0 = bit_offset + 1;
                    qv_insert(plwah64, &map->bits, i, new_word);
                }
            }
            map->generation++;
            return;
        }
    } else {
        if (!set) {
            word->word &= ~(UINT64_C(1) << pos);
        } else {
            word->word |= UINT64_C(1) << pos;
        }
        plwah64_normalize(&map->bits, i, i + 2);
        map->generation++;
        return;
    }
}

/* }}} */
/* Public API {{{ */

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

static inline __must_check__
bool plwah64_get(const plwah64_map_t *map, uint64_t pos)
{
    plwah64_path_t path = plwah64_find(map, pos);
    return plwah64_get_(map, path);
}

static inline
void plwah64_set(plwah64_map_t *map, uint64_t pos)
{
    plwah64_path_t path = plwah64_find(map, pos);
    plwah64_set_(map, path, true);
}

static inline
void plwah64_reset(plwah64_map_t *map, uint64_t pos)
{
    plwah64_path_t path = plwah64_find(map, pos);
    plwah64_set_(map, path, false);
}

static inline
void plwah64_not(plwah64_map_t *map)
{
    for (int i = 0; i < map->bits.len; i++) {
        plwah64_t *word = &map->bits.tab[i];
        if (word->is_fill) {
            word->fill.val = ~word->fill.val;
        } else {
            word->bits = ~word->bits;
        }
    }
    if (map->remain > 0) {
        plwah64_t *last = qv_last(plwah64, &map->bits);
        uint64_t   mask = (UINT64_C(1) << (PLWAH64_WORD_BITS - map->remain)) - 1;
        if (last->is_fill && last->fillp.positions) {
            plwah64_t new_word = APPLY_POSITIONS(last->fill);
            new_word.word &= mask;
            last->fillp.positions = 0;
            qv_append(plwah64, &map->bits, new_word);
        } else
        if (last->is_fill) {
            assert (last->fill.val);
            last->fill.counter--;
            qv_append(plwah64, &map->bits, (plwah64_t){ mask });
        } else {
            last->bits &= mask;
        }
        plwah64_normalize(&map->bits, map->bits.len - 2, map->bits.len);
    }
    map->generation++;
}

static inline __must_check__
uint64_t plwah64_bit_count(const plwah64_map_t *map)
{
#define CASE(...)
    uint64_t count = 0;
    for (int i = 0; i < map->bits.len; i++) {
        plwah64_t word = map->bits.tab[i];
        if (word.is_fill && word.fill.val) {
            count += word.fill.counter * PLWAH64_WORD_BITS;
            if (word.fillp.positions) {
                count += PLWAH64_WORD_BITS - READ_POSITIONS(word.fill, CASE);
            }
        } else
        if (word.is_fill) {
            count += READ_POSITIONS(word.fill, CASE);
        } else {
            count += bitcount64(word.word);
        }
    }
#undef CASE
    return count;
}

static inline
void plwah64_trim(plwah64_map_t *map)
{
    while (map->bits.len > 0) {
        const plwah64_t *word = qv_last(plwah64, &map->bits);
        if (word->is_fill && !word->fill.val && !word->fillp.positions) {
            uint64_t count = word->fill.counter * PLWAH64_WORD_BITS;
            map->bit_len -= count - map->remain;
            map->remain   = 0;
            --map->bits.len;
            continue;
        }
        return;
    }
}

/* }}} */
/* Enumeration {{{ */

typedef struct plwah64_enum_t {
    const plwah64_map_t *map;
    int        word_offset;
    plwah64_t  current_word;
    uint64_t   remain_in_word;
    uint64_t   key;
    uint32_t   generation;
} plwah64_enum_t;


static inline
void plwah64_enum_fetch(plwah64_enum_t *en, bool first);

static inline
void plwah64_enum_next(plwah64_enum_t *en, bool first)
{
    if (en->current_word.is_fill) {
        if (en->current_word.fill.val && en->remain_in_word) {
            if (!first) {
                en->key++;
            }
            en->remain_in_word--;
            return;
        } else {
            en->key += en->remain_in_word;
            if (en->current_word.fillp.positions) {
                en->current_word   = APPLY_POSITIONS(en->current_word.fill);
                en->remain_in_word = PLWAH64_WORD_BITS;
            } else {
                goto read_next;
            }
        }
    }
    if (!en->current_word.word) {
        en->key += en->remain_in_word;
        goto read_next;
    } else {
        uint8_t next_bit = bsf64(en->current_word.word);
        en->current_word.word >>= (next_bit + 1);
        if (first) {
            en->key += next_bit;
        } else {
            en->key += next_bit + 1;
        }
        en->remain_in_word -= next_bit + 1;
    }
    return;

  read_next:
    if (en->generation == en->map->generation) {
        en->word_offset++;
        if (en->word_offset >= en->map->bits.len) {
            en->word_offset = -1;
            return;
        }
        en->current_word = en->map->bits.tab[en->word_offset];
        if (en->current_word.is_fill) {
            en->remain_in_word
                = en->current_word.fill.counter * PLWAH64_WORD_BITS;
        } else {
            en->remain_in_word = PLWAH64_WORD_BITS;
        }
        plwah64_enum_next(en, false);
        return;
    } else {
        plwah64_enum_fetch(en, false);
        return;
    }
}

static inline
void plwah64_enum_fetch(plwah64_enum_t *en, bool first)
{
    plwah64_path_t path = plwah64_find(en->map, en->key);
    en->word_offset    = path.word_offset;
    en->generation     = en->map->generation;
    if (path.word_offset < 0) {
        return;
    }
    en->current_word = en->map->bits.tab[en->word_offset];
    if (path.in_pos) {
        en->current_word = APPLY_POSITIONS(en->current_word.fill);
    }
    if (en->current_word.is_fill) {
        en->remain_in_word  = en->current_word.fill.counter * PLWAH64_WORD_BITS;
        en->remain_in_word -= path.bit_in_word;
    } else
    if (path.bit_in_word) {
        en->current_word.word >>= path.bit_in_word - 1;
        en->remain_in_word      = PLWAH64_WORD_BITS - path.bit_in_word;
    } else {
        en->remain_in_word = PLWAH64_WORD_BITS;
    }
    plwah64_enum_next(en, first);
}


static inline
plwah64_enum_t plwah64_enum_start(const plwah64_map_t *map, uint64_t at)
{
    plwah64_enum_t en;
    p_clear(&en, 1);
    en.map = map;
    en.key = at;
    en.remain_in_word = 0;
    plwah64_enum_fetch(&en, true);
    return en;
}

#define plwah64_for_each_1(en, map)                                          \
    for (plwah64_enum_t en = plwah64_enum_start(map, 0);                     \
         en.word_offset >= 0;                                                \
         plwah64_enum_next(&en, false))

/* }}} */
/* Allocation {{{ */

static inline
void plwah64_reset_map(plwah64_map_t *map)
{
    map->bits.len   = 0;
    map->bit_len    = 0;
    map->remain     = 0;
    map->generation++;
}

static inline
plwah64_map_t *plwah64_init(plwah64_map_t *map)
{
    p_clear(map, 1);
    return map;
}

GENERIC_NEW(plwah64_map_t, plwah64);

static inline
void plwah64_wipe(plwah64_map_t *map)
{
    if (map) {
        qv_wipe(plwah64, &map->bits);
    }
}
GENERIC_DELETE(plwah64_map_t, plwah64);

static inline
void plwah64_copy(plwah64_map_t *map, const plwah64_map_t *src)
{
    map->bit_len = src->bit_len;
    map->remain  = src->remain;
    map->bits.len = 0;
    qv_splice(plwah64, &map->bits, 0, 0, src->bits.tab, src->bits.len);
    map->generation++;
}

static inline
plwah64_map_t *plwah64_dup(const plwah64_map_t *map)
{
    plwah64_map_t *m = plwah64_new();
    plwah64_copy(m, map);
    return m;
}

/* }}} */
/* Debug {{{ */

static inline
void plwah64_debug_print(const plwah64_map_t *map)
{
    for (int i = 0; i < map->bits.len; i++) {
        plwah64_t m = map->bits.tab[i];
        if (m.is_fill) {
            fprintf(stderr, "* fill word with %d, counter %"PRIi64
                    ", pos %"PRIx64"\n", (int)m.fill.val,
                    (uint64_t)m.fill.counter, (uint64_t)m.fillp.positions);
        } else {
            fprintf(stderr, "* literal word %"PRIx64"\n", (uint64_t)m.lit.bits);
        }
    }
}

#undef APPLY_POS
#undef APPLY_POSITIONS

#undef BUILD_POS
#undef BUILD_POSITIONS

#undef READ_POSITIONS

/* }}} */
#endif
