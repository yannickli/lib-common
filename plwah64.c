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

#include "plwah64.h"

/* Helpers {{{ */

static inline
void plwah64_normalize_fill_(qv_t(plwah64) *map, int at, int *into,
                             bool *touched)
{
    plwah64_t * restrict prev = &map->tab[*into];
    plwah64_t * restrict cur  = &map->tab[at];
    if (*touched && prev->is_fill && prev->fill.val == cur->fill.val
    && !prev->fillp.positions) {
        prev->fill.counter   += cur->fill.counter;
        prev->fillp.positions = cur->fillp.positions;
        *touched = true;
    } else {
        if (*touched) {
            (*into)++;
        }
        if (*into != at) {
            map->tab[*into] = *cur;
        }
        *touched = true;
    }
}

static inline
void plwah64_normalize_literal_(qv_t(plwah64) *map, int at, int *into,
                                bool *touched)
{
    plwah64_t * restrict prev = &map->tab[*into];
    plwah64_t * restrict cur  = &map->tab[at];
    if (*touched && prev->is_fill && prev->fillp.positions == 0) {
        PLWAH64_BUILD_POSITIONS(prev->fill, cur->bits, return,);
    }
    if (*touched) {
        (*into)++;
    }
    if (*into != at) {
        map->tab[*into] = *cur;
    }
    *touched = true;
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
            *cur = PLWAH64_APPLY_POSITIONS(cur->fill);
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
    int pos;
    bool touched = false;
    if (to >= map->len) {
        to = map->len;
    }
    from--;
    if (from < 0) {
        from = 0;
    }
    pos = from;
    for (int i = from; i < to; i++) {
        if (!plwah64_normalize_cleanup_(map, i)) {
            continue;
        }
        if (i == from) {
            /* We preserve the first word */
            touched = true;
            continue;
        }
        if (map->tab[i].is_fill) {
            plwah64_normalize_fill_(map, i, &pos, &touched);
        } else {
            plwah64_normalize_literal_(map, i, &pos, &touched);
        }
    }
    if (touched) {
        pos++;
    }
    if (pos != to) {
        qv_splice(plwah64, map, pos, to - pos, NULL, 0);
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
        if (V##pos <= 0) {                                                   \
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
                V##word = PLWAH64_APPLY_POSITIONS(V##word.fill);             \
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
void plwah64_or_(qv_t(plwah64) *map, const plwah64_t *x, int xlen,
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
                plwah64_append_literal(map, xword.lit);
            }
            xpos--;
            ypos--;
        }
        NEXT(x);
        NEXT(y);
    }
    if (xlen == 0) {
        if (ylen == 0) {
            return;
        }
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
void plwah64_and_(qv_t(plwah64) *map, const plwah64_t *x, int xlen,
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
                plwah64_append_fill(map, PLWAH64_FILL_INIT(0, 1));
            } else
            if (xword.fill.is_fill) {
                plwah64_append_literal(map, yword.lit);
            } else
            if (yword.fill.is_fill) {
                plwah64_append_literal(map, xword.lit);
            } else {
                xword.word &= yword.word;
                plwah64_append_literal(map, xword.lit);
            }
            xpos--;
            ypos--;
        }
        NEXT(x);
        NEXT(y);
    }
    if (xlen == 0) {
        if (ylen == 0) {
            return;
        }
        SWAP_VARS();
    }
    while (xlen > 0) {
        READ(x);
        plwah64_append_fill(map, PLWAH64_FILL_INIT(0, xpos));
        xpos = 0;
        NEXT(x);
    }
}

#undef SWAP_VARS
#undef NEXT
#undef READ

static inline
int plwah64_build_bit(plwah64_t data[2], uint32_t pos)
{
    uint32_t counter = pos / PLWAH64_WORD_BITS;
    if (counter != 0) {
        pos = pos % PLWAH64_WORD_BITS;
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
                uint8_t __read;                                              \
                uint8_t __mask;                                              \
                uint8_t __val;                                               \
                __read = MIN(8 - (bits_pos & 7), __count);                   \
                assert (__read <= 8);                                        \
                __mask = ((1 << __read) - 1);                                \
                __val  = bits[bits_pos >> 3] >> (bits_pos & 7);              \
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
        word.word <<= PLWAH64_WORD_BITS - map->remain;
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
            plwah64_t old_bits = PLWAH64_APPLY_POSITIONS(last->fill);
            assert ((old_bits.word & (UINT64_MAX << (PLWAH64_WORD_BITS - map->remain))) == 0);
            word.word |= old_bits.word;
            last->fillp.positions = 0;
            plwah64_append_literal(&map->bits, word.lit);
        } else {
            assert ((last->word & (UINT64_MAX << (PLWAH64_WORD_BITS - map->remain))) == 0);
            last->word |= word.word;
        }
        map->bit_len += take;
        map->remain  -= take;
    }
    while (bits_pos < bit_len) {
        int take = MIN(PLWAH64_WORD_BITS, bit_len - bits_pos);
        plwah64_t word = { READ_BITS(take) };
        assert (map->remain == 0);
        assert (!word.is_fill);
        plwah64_append_literal(&map->bits, word.lit);
        map->bit_len += take;
        if (take != PLWAH64_WORD_BITS) {
            map->remain = PLWAH64_WORD_BITS - take;
        }
    }
    map->generation++;
#undef READ_BITS
}

static inline
bool plwah64_set_(plwah64_map_t *map, plwah64_path_t *path, bool set)
{
    plwah64_t *word;
    const int i = path->word_offset;
    const uint64_t pos = path->bit_in_word;
    if (path->word_offset < 0) {
        if (set) {
            plwah64_add_(map, NULL, path->bit_in_word, false);
            plwah64_add_(map, NULL, 1, true);
        } else {
            plwah64_add_(map, NULL, path->bit_in_word + 1, false);
        }
        *path = plwah64_last(map);
        return false;
    }

    word = &map->bits.tab[path->word_offset];
    if (word->is_fill) {
        if (path->in_pos) {
#define CASE(i, Val)                                                         \
            if ((uint64_t)(Val) == pos) {                                    \
                if (!!word->fill.val != !!set) {                             \
                    /* Bit is already at the expected value */               \
                    return set;                                              \
                }                                                            \
                switch (i) {                                                 \
                  case 0: word->fill.position0 = word->fill.position1;       \
                  case 1: word->fill.position1 = word->fill.position2;       \
                  case 2: word->fill.position2 = word->fill.position3;       \
                  case 3: word->fill.position3 = word->fill.position4;       \
                  case 4: word->fill.position4 = word->fill.position5;       \
                  case 5: word->fill.position5 = 0;                          \
                }                                                            \
                return !set;                                                 \
            }
#define SET_POS(p)                                                           \
              case p:                                                        \
                word->fill.position##p = pos + 1;                            \
                map->generation++;                                           \
                return !set;

            switch (PLWAH64_READ_POSITIONS(word->fill, CASE)) {
              SET_POS(1);
              SET_POS(2);
              SET_POS(3);
              SET_POS(4);
              SET_POS(5);
              case 0:
                e_panic("This should not happen");
              default: {
                plwah64_t new_word = PLWAH64_APPLY_POSITIONS(word->fill);
                if (set) {
                    new_word.word |= UINT64_C(1) << pos;
                } else {
                    new_word.word &= ~(UINT64_C(1) << pos);
                }
                word->fillp.positions = 0;
                qv_insert(plwah64, &map->bits, i + 1, new_word);
                map->generation++;
                return !set;
              } break;
            }
#undef SET_POS
#undef CASE
        } else {
            uint64_t bit_offset;
            uint64_t word_offset;
            if (!!word->fill.val == !!set) {
                /* The bit is already at the good value */
                return set;
            }
            bit_offset  = pos % PLWAH64_WORD_BITS;
            word_offset = pos / PLWAH64_WORD_BITS;

            /* Split the fill word */
            if (word_offset == (uint64_t)(word->fill.counter - 1)) {
                if (word->fillp.positions) {
                    plwah64_t new_word = PLWAH64_APPLY_POSITIONS(word->fill);
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
            return !set;
        }
    } else {
        if (!set) {
            word->word &= ~(UINT64_C(1) << pos);
        } else {
            word->word |= UINT64_C(1) << pos;
        }
        plwah64_normalize(&map->bits, i, i + 2);
        map->generation++;
        return !set;
    }
    return set;
}

/* }}} */
/* Public API {{{ */

void plwah64_add(plwah64_map_t *map, const byte *bits, uint64_t bit_len)
{
    plwah64_add_(map, bits, bit_len, false);
}

void plwah64_add0s(plwah64_map_t *map, uint64_t bit_len)
{
    plwah64_add_(map, NULL, bit_len, false);
}

void plwah64_add1s(plwah64_map_t *map, uint64_t bit_len)
{
    plwah64_add_(map, NULL, bit_len, true);
}

bool plwah64_get(const plwah64_map_t *map, uint64_t pos)
{
    plwah64_path_t path = plwah64_find(map, pos);
    return plwah64_get_(map, path);
}

bool plwah64_set_at(plwah64_map_t *map, plwah64_path_t *path)
{
    return plwah64_set_(map, path, true);
}

bool plwah64_reset_at(plwah64_map_t *map, plwah64_path_t *path)
{
    return plwah64_set_(map, path, false);
}

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
            plwah64_t new_word = PLWAH64_APPLY_POSITIONS(last->fill);
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

void plwah64_and(plwah64_map_t * restrict map,
                 const plwah64_map_t * restrict other)
{
    const plwah64_t *src;
    int src_len;
    t_scope;

    src     = (const plwah64_t *)t_dup(map->bits.tab, map->bits.len);
    src_len = map->bits.len;
    map->bits.len = 0;
    plwah64_and_(&map->bits, src, src_len, other->bits.tab, other->bits.len);
    if (map->bit_len < other->bit_len) {
        map->bit_len = other->bit_len;
        map->remain  = other->remain;
    }
    map->generation++;
}

void plwah64_or(plwah64_map_t * restrict map,
                const plwah64_map_t * restrict other)
{
    const plwah64_t *src;
    int src_len;
    t_scope;

    src     = (const plwah64_t *)t_dup(map->bits.tab, map->bits.len);
    src_len = map->bits.len;
    map->bits.len = 0;
    plwah64_or_(&map->bits, src, src_len, other->bits.tab, other->bits.len);
    if (map->bit_len < other->bit_len) {
        map->bit_len = other->bit_len;
        map->remain  = other->remain;
    }
    map->generation++;
}

uint64_t plwah64_bit_count(const plwah64_map_t *map)
{
#define CASE(...)
    uint64_t count = 0;
    for (int i = 0; i < map->bits.len; i++) {
        plwah64_t word = map->bits.tab[i];
        if (word.is_fill && word.fill.val) {
            count += word.fill.counter * PLWAH64_WORD_BITS;
            if (word.fillp.positions) {
                count += PLWAH64_WORD_BITS - PLWAH64_READ_POSITIONS(word.fill, CASE);
            }
        } else
        if (word.is_fill) {
            count += PLWAH64_READ_POSITIONS(word.fill, CASE);
        } else {
            count += bitcount64(word.word);
        }
    }
#undef CASE
    return count;
}

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
/* Unit tests {{{ */

#define PLWAH64_TEST(name)  TEST_DECL("plwah64: " name, 0)

PLWAH64_TEST("simple")
{
    plwah64_map_t map = PLWAH64_MAP_INIT;
    plwah64_add0s(&map, 3);
    TEST_FAIL_IF(map.bits.len != 1, "bad bitmap length: %d", map.bits.len);
    plwah64_not(&map);
    TEST_FAIL_IF(map.bits.len != 1, "bad bitmap length: %d", map.bits.len);
    for (int i = 0; i < 3; i++) {
        if (!plwah64_get(&map, i)) {
            TEST_FAIL_IF(true, "bad bit at offset %d", i);
        }
    }
    if (plwah64_get(&map, 3)) {
        TEST_FAIL_IF(true, "bad bit at offset 3");
    }
    plwah64_wipe(&map);
    TEST_DONE();
}

PLWAH64_TEST("fill")
{
    plwah64_map_t map = PLWAH64_MAP_INIT;

    STATIC_ASSERT(sizeof(plwah64_t) == sizeof(uint64_t));
    STATIC_ASSERT(sizeof(plwah64_fill_t) == sizeof(uint64_t));
    STATIC_ASSERT(sizeof(plwah64_literal_t) == sizeof(uint64_t));

    plwah64_add0s(&map, 63);
    TEST_FAIL_IF(map.bits.len != 1, "bad bitmap length: %d", map.bits.len);
    for (int i = 0; i < 2 * 63; i++) {
        if (plwah64_get(&map, i)) {
            TEST_FAIL_IF(true, "bad bit at %d", i);
        }
    }
    plwah64_add0s(&map, 3 * 63);
    TEST_FAIL_IF(map.bits.len != 1, "bad bitmap length: %d", map.bits.len);
    for (int i = 0; i < 5 * 63; i++) {
        if (plwah64_get(&map, i)) {
            TEST_FAIL_IF(true, "bad bit at %d", i);
        }
    }

    plwah64_trim(&map);
    TEST_FAIL_IF(map.bits.len != 0, "bad bitmap length: %d", map.bits.len);

    plwah64_reset_map(&map);
    plwah64_add1s(&map, 63);
    TEST_FAIL_IF(map.bits.len != 1, "bad bitmap length: %d", map.bits.len);
    for (int i = 0; i < 2 * 63; i++) {
        bool bit = plwah64_get(&map, i);
        if ((i < 63 && !bit) || (i >= 63 && bit)) {
            TEST_FAIL_IF(true, "bad bit at %d", i);
        }
    }
    plwah64_add1s(&map, 3 * 63);
    TEST_FAIL_IF(map.bits.len != 1, "bad bitmap length: %d", map.bits.len);
    for (int i = 0; i < 5 * 63; i++) {
        bool bit = plwah64_get(&map, i);
        if ((i < 4 * 63 && !bit) || (i >= 4 * 63 && bit)) {
            TEST_FAIL_IF(true, "bad bit at %d", i);
        }
    }

    plwah64_wipe(&map);
    TEST_DONE();
}

PLWAH64_TEST("set and reset")
{
    plwah64_map_t map = PLWAH64_MAP_INIT;

    plwah64_set(&map, 135);
    TEST_FAIL_IF(plwah64_bit_count(&map) != 1, "invalid bit count: %d",
                 (int)plwah64_bit_count(&map));
    TEST_FAIL_IF(!plwah64_get(&map, 135), "bad bit");

    plwah64_set(&map, 136);
    TEST_FAIL_IF(plwah64_bit_count(&map) != 2, "invalid bit count: %d",
                 (int)plwah64_bit_count(&map));
    TEST_FAIL_IF(!plwah64_get(&map, 135), "bad bit");
    TEST_FAIL_IF(!plwah64_get(&map, 136), "bad bit");

    plwah64_set(&map, 134);
    TEST_FAIL_IF(plwah64_bit_count(&map) != 3, "invalid bit count: %d",
                 (int)plwah64_bit_count(&map));
    TEST_FAIL_IF(!plwah64_get(&map, 134), "bad bit");
    TEST_FAIL_IF(!plwah64_get(&map, 135), "bad bit");
    TEST_FAIL_IF(!plwah64_get(&map, 136), "bad bit");

    plwah64_reset(&map, 135);
    TEST_FAIL_IF(plwah64_bit_count(&map) != 2, "invalid bit count: %d",
                 (int)plwah64_bit_count(&map));
    TEST_FAIL_IF(!plwah64_get(&map, 134), "bad bit");
    TEST_FAIL_IF(plwah64_get(&map, 135), "bad bit");
    TEST_FAIL_IF(!plwah64_get(&map, 136), "bad bit");
    plwah64_wipe(&map);
    TEST_DONE();
}

PLWAH64_TEST("set bitmap")
{
    const byte data[] = {
        0x1f, 0x00, 0x00, 0x8c, /* 0, 1, 2, 3, 4, 26, 27, 31 (32) */
        0xff, 0xff, 0xff, 0xff, /* 32 -> 63                  (64) */
        0xff, 0xff, 0xff, 0xff, /* 64 -> 95                  (96) */
        0xff, 0xff, 0xff, 0x80, /* 96 -> 119, 127            (128)*/
        0x00, 0x10, 0x40, 0x00, /* 140, 150                  (160)*/
        0x00, 0x00, 0x00, 0x00, /*                           (192)*/
        0x00, 0x00, 0x00, 0x00, /*                           (224)*/
        0x00, 0x00, 0x00, 0x00, /*                           (256)*/
        0x00, 0x00, 0x00, 0x21  /* 280, 285                  (288)*/
    };

    uint64_t      bc;
    plwah64_map_t map = PLWAH64_MAP_INIT;
    plwah64_add(&map, data, bitsizeof(data));
    bc = membitcount(data, sizeof(data));

    TEST_FAIL_IF(plwah64_bit_count(&map) != bc,
                 "invalid bit count: %d, expected %d",
                 (int)plwah64_bit_count(&map), (int)bc);
    for (int i = 0; i < countof(data); i++) {
#define CHECK_BIT(p)  (!!(data[i] & (1 << p)) == !!plwah64_get(&map, i * 8 + p))
        if (!CHECK_BIT(0) || !CHECK_BIT(1) || !CHECK_BIT(2) || !CHECK_BIT(3)
        ||  !CHECK_BIT(4) || !CHECK_BIT(5) || !CHECK_BIT(6) || !CHECK_BIT(7)) {
            TEST_FAIL_IF(true, "invalid byte %d", i);
        }
#undef CHECK_BIT
    }

    plwah64_not(&map);
    TEST_FAIL_IF(plwah64_bit_count(&map) != bitsizeof(data) - bc,
                 "invalid bit count: %d, expected %d",
                 (int)plwah64_bit_count(&map), (int)(bitsizeof(data) - bc));
    for (int i = 0; i < countof(data); i++) {
#define CHECK_BIT(p)  (!!(data[i] & (1 << p)) != !!plwah64_get(&map, i * 8 + p))
        if (!CHECK_BIT(0) || !CHECK_BIT(1) || !CHECK_BIT(2) || !CHECK_BIT(3)
        ||  !CHECK_BIT(4) || !CHECK_BIT(5) || !CHECK_BIT(6) || !CHECK_BIT(7)) {
            TEST_FAIL_IF(true, "invalid byte %d", i);
        }
#undef CHECK_BIT
    }

    plwah64_wipe(&map);
    TEST_DONE();
}

PLWAH64_TEST("for_each")
{
    const byte data[] = {
        0x1f, 0x00, 0x00, 0x8c, /* 0, 1, 2, 3, 4, 26, 27, 31 (32) */
        0xff, 0xff, 0xff, 0xff, /* 32 -> 63                  (64) */
        0xff, 0xff, 0xff, 0xff, /* 64 -> 95                  (96) */
        0xff, 0xff, 0xff, 0x80, /* 96 -> 119, 127            (128)*/
        0x00, 0x10, 0x40, 0x00, /* 140, 150                  (160)*/
        0x00, 0x00, 0x00, 0x00, /*                           (192)*/
        0x00, 0x00, 0x00, 0x00, /*                           (224)*/
        0x00, 0x00, 0x00, 0x00, /*                           (256)*/
        0x00, 0x00, 0x00, 0x21  /* 280, 285                  (288)*/
    };

    uint64_t      bc;
    uint64_t      nbc;
    plwah64_map_t map = PLWAH64_MAP_INIT;
    uint64_t      c;
    uint64_t      previous;
    plwah64_add(&map, data, bitsizeof(data));
    bc  = membitcount(data, sizeof(data));
    nbc = bitsizeof(data) - bc;

    TEST_FAIL_IF(plwah64_bit_count(&map) != bc,
                 "invalid bit count: %d, expected %d",
                 (int)plwah64_bit_count(&map), (int)bc);

    c = 0;
    previous = 0;
    plwah64_for_each_1(en, &map) {
        if (c != 0) {
            if (previous >= en.key) {
                TEST_FAIL_IF(true, "misordered enumeration: %d after %d",
                             (int)en.key, (int)previous);
            }
        }
        previous = en.key;
        c++;
        if (en.key >= bitsizeof(data)) {
            TEST_FAIL_IF(true, "enumerate too far: %d", (int)en.key);
        }
        if (!(data[en.key >> 3] & (1 << (en.key & 0x7)))) {
            TEST_FAIL_IF(true, "bit %d is not set", (int)en.key);
        }
    }
    TEST_FAIL_IF(c != bc, "bad number of enumerated entries %d, expected %d",
                 (int)c, (int)bc);

    c = 0;
    previous = 0;
    plwah64_for_each_0(en, &map) {
        if (c != 0) {
            if (previous >= en.key) {
                TEST_FAIL_IF(true, "misordered enumeration: %d after %d",
                             (int)en.key, (int)previous);
            }
        }
        previous = en.key;
        c++;
        if (en.key >= bitsizeof(data)) {
            TEST_FAIL_IF(true, "enumerate too far: %d", (int)en.key);
        }
        if ((data[en.key >> 3] & (1 << (en.key & 0x7)))) {
            TEST_FAIL_IF(true, "bit %d is set", (int)en.key);
        }
    }
    TEST_FAIL_IF(c != nbc, "bad number of enumerated entries %d, expected %d",
                 (int)c, (int)nbc);


    for (int i = 0; i < (int)bitsizeof(data) + 100; i++) {
        plwah64_path_t path = plwah64_find(&map, i);
        if (plwah64_get_pos(&map, &path) != (uint64_t)i) {
            TEST_FAIL_IF(true, "invalid path for bit %d", i);
        }
    }
    {
        plwah64_path_t last = plwah64_last(&map);
        uint64_t pos = plwah64_get_pos(&map, &last);
        TEST_FAIL_IF(pos != map.bit_len - 1, "bad pos for last %d", (int)pos);
    }

    plwah64_wipe(&map);
    TEST_DONE();
}


PLWAH64_TEST("binop")
{
    const byte data1[] = {
        0x1f, 0x00, 0x00, 0x8c, /* 0, 1, 2, 3, 4, 26, 27, 31 (32) */
        0xff, 0xff, 0xff, 0xff, /* 32 -> 63                  (64) */
        0xff, 0xff, 0xff, 0xff, /* 64 -> 95                  (96) */
        0xff, 0xff, 0xff, 0x80, /* 96 -> 119, 127            (128)*/
        0x00, 0x10, 0x40, 0x00, /* 140, 150                  (160)*/
        0x00, 0x00, 0x00, 0x00, /*                           (192)*/
        0x00, 0x00, 0x00, 0x00, /*                           (224)*/
        0x00, 0x00, 0x00, 0x00, /*                           (256)*/
        0x00, 0x00, 0x00, 0x21  /* 280, 285                  (288)*/
    };

    const byte data2[] = {
        0x00, 0x00, 0x00, 0x00, /*                                     (32) */
        0x00, 0x00, 0x00, 0x80, /* 63                                  (64) */
        0x00, 0x10, 0x20, 0x00, /* 76, 85                              (96) */
        0x00, 0x00, 0xc0, 0x20, /* 118, 119, 125                       (128)*/
        0xff, 0xfc, 0xff, 0x12  /* 128 -> 135, 138 -> 151, 153, 156    (160)*/
    };

    /* And result:
     *                                                                 (32)
     * 63                                                              (64)
     * 76, 85                                                          (96)
     * 118, 119                                                        (128)
     * 140, 150                                                        (160)
     */

    /* Or result:
     * 0 -> 4, 26, 27, 31                                              (32)
     * 32 -> 63                                                        (64)
     * 64 -> 95                                                        (96)
     * 96 -> 119, 125, 127                                             (128)
     * 128 -> 135, 138 -> 151, 153, 156                                (160)
     *                                                                 (192)
     *                                                                 (224)
     *                                                                 (256)
     * 280, 285                                                        (288)
     */

    plwah64_map_t map1 = PLWAH64_MAP_INIT;
    plwah64_map_t map2 = PLWAH64_MAP_INIT;

    plwah64_add(&map1, data1, bitsizeof(data1));
    plwah64_add(&map2, data2, bitsizeof(data2));
    plwah64_and(&map1, &map2);
    for (int i = 0; i < countof(data1); i++) {
        byte b = data1[i];
        if (i < countof(data2)) {
            b &= data2[i];
        } else {
            b = 0;
        }
#define CHECK_BIT(p)  (!!(b & (1 << p)) == !!plwah64_get(&map1, i * 8 + p))
        if (!CHECK_BIT(0) || !CHECK_BIT(1) || !CHECK_BIT(2) || !CHECK_BIT(3)
        ||  !CHECK_BIT(4) || !CHECK_BIT(5) || !CHECK_BIT(6) || !CHECK_BIT(7)) {
            TEST_FAIL_IF(true, "invalid byte %d", i);
        }
#undef CHECK_BIT
    }

    plwah64_reset_map(&map1);
    plwah64_add(&map1, data1, bitsizeof(data1));
    plwah64_or(&map1, &map2);
    for (int i = 0; i < countof(data1); i++) {
        byte b = data1[i];
        if (i < countof(data2)) {
            b |= data2[i];
        }
#define CHECK_BIT(p)  (!!(b & (1 << p)) == !!plwah64_get(&map1, i * 8 + p))
        if (!CHECK_BIT(0) || !CHECK_BIT(1) || !CHECK_BIT(2) || !CHECK_BIT(3)
        ||  !CHECK_BIT(4) || !CHECK_BIT(5) || !CHECK_BIT(6) || !CHECK_BIT(7)) {
            TEST_FAIL_IF(true, "invalid byte %d", i);
        }
#undef CHECK_BIT
    }

    plwah64_wipe(&map1);
    plwah64_wipe(&map2);
    TEST_DONE();
}

/* }}} */
