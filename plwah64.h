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
    union {
        struct {
            uint64_t position5 : 6;
            uint64_t position4 : 6;
            uint64_t position3 : 6;
            uint64_t position2 : 6;
            uint64_t position1 : 6;
            uint64_t position0 : 6;
        };
        uint64_t positions : 36;
    };
    uint64_t counter   : 26; /* Can adress at most 63 * (2^26 - 1) bits in a
                                single word. */
#else
    uint64_t counter   : 26; /* Can adress at most 63 * (2^26 - 1) bits in a
                                single word. */
    union {
        struct {
            uint64_t position0 : 6;
            uint64_t position1 : 6;
            uint64_t position2 : 6;
            uint64_t position3 : 6;
            uint64_t position4 : 6;
            uint64_t position5 : 6;
        };
        uint64_t positions : 36;
    };
    uint64_t val       : 1; /* 0 describe a sequence of 0,
                               1 describe a sequence of 1 */
    uint64_t is_fill   : 1; /* Always 1 */
#endif
} plwah64_fill_t;
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
} plwah64_t;

static inline
void plwah64_append_fill(plwah64_t *map, int *len, plwah64_fill_t fill)
{
    plwah64_t last;
    assert (fill.is_fill);
    if (*len == 0) {
        map[*len++].fill = fill;
        return;
    }
    last = map[*len - 1];
    if (last.fill.is_fill && last.fill.val == fill.val
    && last.fill.positions == 0) {
        map[*len - 1].fill.counter += fill.counter;
    } else {
        map[*len++].fill = fill;
    }
}

static inline
void plwah64_append_literal(plwah64_t *map, int *len, plwah64_literal_t lit)
{
    plwah64_t last;
    assert (!lit.is_fill);
    if (lit.bits == 0) {
        plwah64_append_fill(map, len, PLWAH64_FILL_INIT_V(0, 1));
        return;
    } else
    if (lit.bits == 0x7fffffffffffffffUL) {
        plwah64_append_fill(map, len, PLWAH64_FILL_INIT_V(1, 1));
    }
    if (*len == 0) {
        map[*len++].lit = lit;
        return;
    }
    last = map[*len - 1];
    if (last.fill.is_fill && last.fill.positions == 0) {
        uint64_t word = lit.bits;;
        if (last.fill.val == 1) {
            word = ~word;
        };
#define FIND_AND_SET_POS(i)  do {                                            \
            size_t __bit = bsf64(word);                                      \
            last.fill.position##i = __bit + 1;                               \
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
    map[*len++].lit = lit;
}

static inline
void plwah64_append_word(plwah64_t *map, int *len, plwah64_t word)
{
    STATIC_ASSERT(sizeof(plwah64_t) == sizeof(uint64_t));
    STATIC_ASSERT(sizeof(plwah64_literal_t) == sizeof(uint64_t));
    STATIC_ASSERT(sizeof(plwah64_fill_t) == sizeof(uint64_t));
    if (word.lit.is_fill) {
        plwah64_append_fill(map, len, word.fill);
    } else {
        plwah64_append_literal(map, len, word.lit);
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
            if (V##word.fill.is_fill && V##word.fill.positions) {            \
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
#define SWAP_VARS(x, y)  do {                                                \
            typeof(x) __tmp = x;                                             \
            x = y;                                                           \
            y = __tmp;                                                       \
        } while (0)
#define SWAP()  do {                                                         \
            SWAP_VARS(x, y);                                                 \
            SWAP_VARS(xlen, ylen);                                           \
            SWAP_VARS(xpos, ypos);                                           \
            SWAP_VARS(xword, yword);                                         \
        } while (0)

static inline
void plwah64_or(plwah64_t *map, int *len, const plwah64_t *x, int xlen,
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
            plwah64_append_fill(map, len, PLWAH64_FILL_INIT_V(type, words));
            xpos -= words;
            ypos -= words;
        } else {
            if ((xword.fill.is_fill && xword.fill.val)
            || (yword.fill.is_fill && yword.fill.val)) {
                plwah64_append_fill(map, len, PLWAH64_FILL_INIT_V(1, 1));
            } else
            if (xword.fill.is_fill) {
                plwah64_append_literal(map, len, yword.lit);
            } else
            if (yword.fill.is_fill) {
                plwah64_append_literal(map, len, xword.lit);
            } else {
                xword.word |= yword.word;
                plwah64_append_literal(map, len, yword.lit);
            }
            xpos--;
            ypos--;
        }
        NEXT(x);
        NEXT(y);
    }
    if (xlen == 0) {
        SWAP();
    }
    if (xpos > 0) {
        if (xword.fill.is_fill) {
            xword.fill.counter = xpos;
        }
        plwah64_append_word(map, len, xword);
    }
    for (int i = 0; i < xlen; i++) {
        plwah64_append_word(map, len, x[i]);
    }
}

static inline
void plwah64_and(plwah64_t *map, int *len, const plwah64_t *x, int xlen,
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
            plwah64_append_fill(map, len, PLWAH64_FILL_INIT_V(type, words));
            xpos -= words;
            ypos -= words;
        } else {
            if ((xword.fill.is_fill && !xword.fill.val)
            || (yword.fill.is_fill && !yword.fill.val)) {
                plwah64_append_fill(map, len, PLWAH64_FILL_INIT_V(1, 0));
            } else
            if (xword.fill.is_fill) {
                plwah64_append_literal(map, len, yword.lit);
            } else
            if (yword.fill.is_fill) {
                plwah64_append_literal(map, len, xword.lit);
            } else {
                xword.word &= yword.word;
                plwah64_append_literal(map, len, yword.lit);
            }
            xpos--;
            ypos--;
        }
        NEXT(x);
        NEXT(y);
    }
}

#undef SWAP
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
bool plwah64_get(const plwah64_t *map, int len, uint32_t pos)
{

    for (int i = 0; i < len; i++) {
        plwah64_t word = map[i];
        if (word.lit.is_fill) {
            uint64_t count = word.fill.counter * 63;
            if (pos < count) {
                return !!word.fill.val;
            }
            pos -= count;
            if (pos < 63 && word.fill.positions != 0) {
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

#endif
