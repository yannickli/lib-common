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
#include "wah.h"

/* Structures {{{ */

#define PLWAH64_WITH_PL  1

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
    uint64_t bit_in_map;
    uint64_t bit_in_word;
    int      word_offset;
    bool     in_pos;
} plwah64_path_t;

typedef struct plwah64_map_t {
    uint64_t      bit_len;
    uint64_t      bit_count;
    uint8_t       remain;
    qv_t(plwah64) bits;
} plwah64_map_t;
qvector_t(plwah64_map, plwah64_map_t);

#define PLWAH64_WORD_BITS         (bitsizeof(plwah64_t) - 1)
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

#define PLWAH64_APPLY_POS(i, Dest, Src)                                      \
    if ((Src).position##i) {                                                 \
        Dest |= UINT64_C(1) << ((Src).position##i - 1);                      \
    }

#define PLWAH64_APPLY_POSITIONS(Src)  ({                                     \
        plwah64_t __word = { 0 };                                            \
        PLWAH64_APPLY_POS(0, __word.word, Src);                              \
        PLWAH64_APPLY_POS(1, __word.word, Src);                              \
        PLWAH64_APPLY_POS(2, __word.word, Src);                              \
        PLWAH64_APPLY_POS(3, __word.word, Src);                              \
        PLWAH64_APPLY_POS(4, __word.word, Src);                              \
        PLWAH64_APPLY_POS(5, __word.word, Src);                              \
        if ((Src).val) {                                                     \
            __word.bits = ~__word.bits;                                      \
        }                                                                    \
        __word;                                                              \
    })

#define PLWAH64_BUILD_POS(i, Dest, Src)                                      \
      case i + 1: {                                                          \
        size_t __bit = bsf64(Src);                                           \
        (Dest).position##i = __bit + 1;                                      \
        (Src) &= ~(UINT64_C(1) << __bit);                                    \
      }

#define PLWAH64_BUILD_POSITIONS(Dest, Src, DONE_CASE, ERR_CASE)              \
    do {                                                                     \
        uint64_t __word = (Src);                                             \
        if ((Dest).val) {                                                    \
            __word |= UINT64_C(1) << PLWAH64_WORD_BITS;                      \
            __word  = ~__word;                                               \
        }                                                                    \
        switch (bitcount64(__word)) {                                        \
          PLWAH64_BUILD_POS(5, Dest, __word);                                \
          PLWAH64_BUILD_POS(4, Dest, __word);                                \
          PLWAH64_BUILD_POS(3, Dest, __word);                                \
          PLWAH64_BUILD_POS(2, Dest, __word);                                \
          PLWAH64_BUILD_POS(1, Dest, __word);                                \
          PLWAH64_BUILD_POS(0, Dest, __word);                                \
          DONE_CASE;                                                         \
          break;                                                             \
         default:                                                            \
          ERR_CASE;                                                          \
          break;                                                             \
        }                                                                    \
    } while (0)

#define PLWAH64_READ_POSITIONS(Src, CASE)  ({                                \
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
#   define restrict __restrict
#endif

static inline
plwah64_path_t plwah64_find(const plwah64_map_t *map, uint64_t pos)
{
    const uint64_t global_pos = pos;
    if (pos >= map->bit_len) {
        return (plwah64_path_t){
            /* .bit_in_map  = */ global_pos,
            /* .bit_in_word = */ pos - map->bit_len,
            /* .word_offset = */ -1,
            /* .in_pos      = */ false
        };
    }
    for (int i = 0; i < map->bits.len; i++) {
        plwah64_t word = map->bits.tab[i];
        if (word.is_fill) {
            uint64_t count = word.fill.counter * PLWAH64_WORD_BITS;
            if (pos < count) {
                return (plwah64_path_t){
                    /* .bit_in_map  = */ global_pos,
                    /* .bit_in_word = */ pos,
                    /* .word_offset = */ i,
                    /* .in_pos      = */ 0,
                };
            }
            pos -= count;
            if (word.fillp.positions != 0) {
                if (pos < PLWAH64_WORD_BITS) {
                    return (plwah64_path_t){
                        /* .bit_in_map  = */ global_pos,
                        /* .bit_in_word = */ pos,
                        /* .word_offset = */ i,
                        /* .in_pos      = */ true,
                    };
                }
                pos -= PLWAH64_WORD_BITS;
            }
        } else
        if (pos < PLWAH64_WORD_BITS) {
            return (plwah64_path_t){
                /* .bit_in_map  = */ global_pos,
                /* .bit_in_word = */ pos,
                /* .word_offset = */ i,
                /* .in_pos      = */ false,
            };
        } else {
            pos -= PLWAH64_WORD_BITS;
        }
    }
    e_panic("This should not happen");
}

static inline
plwah64_path_t plwah64_first(const plwah64_map_t *map)
{
    return plwah64_find(map, 0);
}

static inline
plwah64_path_t plwah64_last(const plwah64_map_t *map)
{
    const plwah64_t *last;
    if (map->bit_len == 0) {
        return (plwah64_path_t){
            /* .bit_in_map  = */ map->bit_len - 1,
            /* .bit_in_word = */ 0,
            /* .word_offset = */ -1,
            /* .in_pos      = */ false
        };
    }
    last = qv_last(plwah64, &map->bits);
    if (!last->is_fill || last->fillp.positions != 0) {
        return (plwah64_path_t){
            /* .bit_in_map  = */ map->bit_len - 1,
            /* .bit_in_word = */ PLWAH64_WORD_BITS - 1 - map->remain,
            /* .word_offset = */ map->bits.len - 1,
            /* .in_pos      = */ !!last->is_fill,
        };
    } else {
        uint64_t count = last->fill.counter * PLWAH64_WORD_BITS;
        return (plwah64_path_t){
            /* .bit_in_map  = */ map->bit_len - 1,
            /* .bit_in_word = */ count - 1 - map->remain,
            /* .word_offset = */ map->bits.len - 1,
            /* .in_pos      = */ false,
        };
    }
}

static inline
void plwah64_next(plwah64_path_t *path)
{
    path->bit_in_map++;
    path->bit_in_word++;
}

static inline __must_check__
bool plwah64_get_at(const plwah64_map_t *map, const plwah64_path_t *path)
{
    plwah64_t word;
    if (path->word_offset < 0) {
        return false;
    }

    word = map->bits.tab[path->word_offset];
    if (word.is_fill) {
        if (path->in_pos) {
#define CASE(p, Val)                                                         \
            if ((uint64_t)(Val) == path->bit_in_word) {                      \
                return !word.fill.val;                                       \
            }
            PLWAH64_READ_POSITIONS(word.fill, CASE);
#undef CASE
        }
        return !!word.fill.val;
    } else {
        return !!(word.word & (UINT64_C(1) << path->bit_in_word));
    }
}


/* }}} */
/* Public API {{{ */

void plwah64_add(plwah64_map_t *map, const byte *bits, uint64_t bit_len);
void plwah64_add0s(plwah64_map_t *map, uint64_t bit_len);
void plwah64_add1s(plwah64_map_t *map, uint64_t bit_len);
void plwah64_trim(plwah64_map_t *map);

bool plwah64_set_at(plwah64_map_t *map, plwah64_path_t *pos);
bool plwah64_reset_at(plwah64_map_t *map, plwah64_path_t *pos);

static ALWAYS_INLINE
bool plwah64_set(plwah64_map_t *map, uint64_t pos)
{
    if (pos >= map->bit_len) {
        if (pos > map->bit_len) {
            plwah64_add0s(map, pos - map->bit_len);
        }
        plwah64_add1s(map, 1);
        return false;
    } else {
        plwah64_path_t path = plwah64_find(map, pos);
        return plwah64_set_at(map, &path);
    }
}

static ALWAYS_INLINE
bool plwah64_reset(plwah64_map_t *map, uint64_t pos)
{
    if (pos >= map->bit_len) {
        plwah64_add0s(map, map->bit_len - pos + 1);
        return false;
    } else {
        plwah64_path_t path = plwah64_find(map, pos);
        return plwah64_reset_at(map, &path);
    }
}

static ALWAYS_INLINE
bool plwah64_get(const plwah64_map_t *map, uint64_t pos)
{
    plwah64_path_t path = plwah64_find(map, pos);
    return plwah64_get_at(map, &path);
}

void plwah64_scanf(const plwah64_map_t *map, plwah64_path_t *path);
void plwah64_scanzf(const plwah64_map_t *map, plwah64_path_t *path);

void plwah64_not(plwah64_map_t *map);
void plwah64_and(plwah64_map_t * restrict map,
                 const plwah64_map_t * restrict other);
void plwah64_or(plwah64_map_t * restrict map,
                const plwah64_map_t * restrict other);

static ALWAYS_INLINE __must_check__
uint64_t plwah64_bit_count(const plwah64_map_t *map)
{
    return map->bit_count;
}


/* }}} */
/* Enumeration {{{ */

typedef struct plwah64_enum_t {
    const plwah64_map_t *map;
    bool       val;
    int        word_offset;
    plwah64_t  current_word;
    uint64_t   remain_in_word;
    uint64_t   key;
} plwah64_enum_t;


static inline
void plwah64_enum_next(plwah64_enum_t *en, bool first)
{
    if (en->current_word.is_fill) {
        if (en->current_word.fill.val && en->remain_in_word) {
            if (!first) {
                en->key++;
                if (en->key >= en->map->bit_len) {
                    en->word_offset = -1;
                    return;
                }
            }
            en->remain_in_word--;
            return;
        } else {
            en->key += en->remain_in_word;
            if (en->key >= en->map->bit_len) {
                en->word_offset = -1;
                return;
            }
            if (en->current_word.fillp.positions) {
                en->current_word = PLWAH64_APPLY_POSITIONS(en->current_word.fill);
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
        if (en->key >= en->map->bit_len) {
            en->word_offset = -1;
            return;
        }
        en->remain_in_word -= next_bit + 1;
    }
    return;

  read_next:
    en->word_offset++;
    if (en->word_offset >= en->map->bits.len) {
        en->word_offset = -1;
        return;
    }
    en->current_word = en->map->bits.tab[en->word_offset];
    if (en->current_word.is_fill) {
        en->remain_in_word
            = en->current_word.fill.counter * PLWAH64_WORD_BITS;
        if (!en->val) {
            en->current_word.fill.val = ~en->current_word.fill.val;
        }
    } else {
        en->remain_in_word = PLWAH64_WORD_BITS;
        if (!en->val) {
            en->current_word.bits = ~en->current_word.bits;
        }
    }
    plwah64_enum_next(en, false);
}

static inline
void plwah64_enum_fetch(plwah64_enum_t *en, bool first)
{
    plwah64_path_t path = plwah64_find(en->map, en->key);
    en->word_offset    = path.word_offset;
    if (path.word_offset < 0) {
        return;
    }
    en->current_word = en->map->bits.tab[en->word_offset];
    if (path.in_pos) {
        en->current_word = PLWAH64_APPLY_POSITIONS(en->current_word.fill);
    }
    if (en->current_word.is_fill) {
        en->remain_in_word  = en->current_word.fill.counter * PLWAH64_WORD_BITS;
        en->remain_in_word -= path.bit_in_word;
        if (!en->val) {
            en->current_word.fill.val = ~en->current_word.fill.val;
        }
    } else {
        if (!en->val) {
            en->current_word.bits = ~en->current_word.bits;
        }
        if (path.bit_in_word) {
            en->current_word.word >>= path.bit_in_word - 1;
            en->remain_in_word      = PLWAH64_WORD_BITS - path.bit_in_word;
        } else {
            en->remain_in_word = PLWAH64_WORD_BITS;
        }
    }
    plwah64_enum_next(en, first);
}


static inline
plwah64_enum_t plwah64_enum_start(const plwah64_map_t *map, uint64_t at,
                                  bool val)
{
    plwah64_enum_t en;
    p_clear(&en, 1);
    en.val = val;
    en.map = map;
    en.key = at;
    en.remain_in_word = 0;
    plwah64_enum_fetch(&en, true);
    return en;
}

#define plwah64_for_each_1(en, map)                                          \
    for (plwah64_enum_t en = plwah64_enum_start(map, 0, true);               \
         en.word_offset >= 0;                                                \
         plwah64_enum_next(&en, false))

#define plwah64_for_each_0(en, map)                                          \
    for (plwah64_enum_t en = plwah64_enum_start(map, 0, false);              \
         en.word_offset >= 0;                                                \
         plwah64_enum_next(&en, false))

/* }}} */
/* Allocation {{{ */

static inline
void plwah64_reset_map(plwah64_map_t *map)
{
    map->bits.len   = 0;
    map->bit_count  = 0;
    map->bit_len    = 0;
    map->remain     = 0;
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

__cold
static inline
void plwah64_debug_print(const plwah64_map_t *map)
{
    for (int i = 0; i < map->bits.len; i++) {
        plwah64_t m = map->bits.tab[i];
        if (m.is_fill) {
            fprintf(stderr, "* fill word with %d, counter %"PRIi64,
                    (int)m.fill.val, (uint64_t)m.fill.counter);
#define CASE(p, val)  fprintf(stderr, ", pos%d: %d", (int)p, (int)val);
            PLWAH64_READ_POSITIONS(m.fill, CASE);
#undef CASE
            fprintf(stderr, "\n");
        } else {
            fprintf(stderr, "* literal word %016"PRIx64"\n", (uint64_t)m.lit.bits);
        }
    }
}

/* }}} */
#endif
