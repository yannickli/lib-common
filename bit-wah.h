/**************************************************************************/
/*                                                                        */
/*  Copyright (C) INTERSEC SA                                             */
/*                                                                        */
/*  Should you receive a copy of this source code, you must check you     */
/*  have a proper, written authorization of INTERSEC to hold it. If you   */
/*  don't have such an authorization, you must DELETE all source code     */
/*  files in your possession, and inform INTERSEC of the fact you obtain  */
/*  these files. Should you not comply to these terms, you can be         */
/*  prosecuted in the extent permitted by applicable law.                 */
/*                                                                        */
/**************************************************************************/

#if !defined(IS_LIB_COMMON_BIT_H) || defined(IS_LIB_COMMON_BIT_WAH_H)
#  error "you must include bit.h instead"
#else
#define IS_LIB_COMMON_BIT_WAH_H

/** \defgroup qkv__ll__wah Word Aligned Hybrid bitmaps.
 * \ingroup qkv__ll
 * \brief Word Aligned Hybrid bitmaps.
 *
 * \{
 */

/* Structures {{{ */

typedef struct wah_header_t {
#if __BYTE_ORDER == __BIG_ENDIAN
    unsigned  bit   : 1;
    unsigned  words : 31;
#else
    unsigned  words : 31;
    unsigned  bit   : 1;
#endif
} wah_header_t;

typedef union wah_word_t {
    wah_header_t head;
    uint32_t     count;
    uint32_t     literal;
} wah_word_t;
qvector_t(wah_word, wah_word_t);

typedef struct wah_t {
    uint64_t  len;
    uint64_t  active;

    wah_header_t   first_run_head;
    uint32_t       first_run_len;

    int            previous_run_pos;
    int            last_run_pos;
    qv_t(wah_word) data;
    uint32_t       pending;
} wah_t;
qvector_t(wah, wah_t);

#define WAH_BIT_IN_WORD  bitsizeof(wah_word_t)

/* }}} */
/* Public API {{{ */


wah_t *wah_init(wah_t *map) __leaf;
void wah_wipe(wah_t *map) __leaf;
GENERIC_DELETE(wah_t, wah);
GENERIC_NEW(wah_t, wah);

wah_t *t_wah_new(int expected_size) __leaf;
wah_t *t_wah_dup(const wah_t *src) __leaf;
void wah_copy(wah_t *map, const wah_t *src) __leaf;

void wah_add0s(wah_t *map, uint64_t count) __leaf;
void wah_add1s(wah_t *map, uint64_t count) __leaf;
void wah_add(wah_t *map, const void *data, uint64_t count) __leaf;

void wah_and(wah_t *map, const wah_t *other) __leaf;
void wah_or(wah_t *map, const wah_t *other) __leaf;
void wah_not(wah_t *map) __leaf;

__must_check__ __leaf
bool wah_get(const wah_t *map, uint64_t pos);

static inline
void wah_reset_map(wah_t *map)
{
    map->len                  = 0;
    map->active               = 0;
    map->first_run_head.words = 0;
    map->first_run_head.bit   = 0;
    map->first_run_len        = 0;
    map->previous_run_pos     = -1;
    map->last_run_pos         = -1;
    map->data.len             = 0;
    map->pending              = 0;
}

/* }}} */
/* Enumeration {{{ */

typedef enum wah_enum_state_t {
    WAH_ENUM_END,
    WAH_ENUM_PENDING,
    WAH_ENUM_LITERAL,
    WAH_ENUM_RUN,
} wah_enum_state_t;

typedef struct wah_word_enum_t {
    const wah_t     *map;
    wah_enum_state_t state;
    int              pos;
    uint32_t         remain_words;
    uint32_t         current;
} wah_word_enum_t;

static inline
wah_word_enum_t wah_word_enum_start(const wah_t *map)
{
    wah_word_enum_t en = { map, WAH_ENUM_END, -1, 0, 0 };
    if (map->len == 0) {
        en.state = WAH_ENUM_END;
        return en;
    }
    if (map->first_run_head.words > 0) {
        en.state        = WAH_ENUM_RUN;
        en.remain_words = map->first_run_head.words;
        en.current      = 0 - map->first_run_head.bit;
    } else
    if (map->first_run_len > 0) {
        en.state        = WAH_ENUM_LITERAL;
        en.pos          = map->first_run_len;
        en.remain_words = map->first_run_len;
        en.current      = map->data.tab[0].literal;
        assert (en.pos <= map->data.len);
        assert ((int)en.remain_words <= en.pos);
    } else {
        en.state        = WAH_ENUM_PENDING;
        en.remain_words = 1;
        en.current      = map->pending;
    }
    return en;
}

static ALWAYS_INLINE
bool wah_word_enum_next(wah_word_enum_t *en)
{
    if (en->remain_words != 1) {
        en->remain_words--;
        if (en->state == WAH_ENUM_LITERAL) {
            en->current = en->map->data.tab[en->pos - en->remain_words].literal;
        }
        return true;
    }
    switch (__builtin_expect(en->state, WAH_ENUM_RUN)) {
      case WAH_ENUM_END:
        return false;

      case WAH_ENUM_PENDING:
        en->state   = WAH_ENUM_END;
        en->current = 0;
        return false;

      default: /* WAH_ENUM_RUN */
        assert (en->state == WAH_ENUM_RUN);
        if (en->pos == -1) {
            en->remain_words = en->map->first_run_len;
            en->pos          = en->remain_words;
        } else {
            en->pos++;
            en->remain_words = en->map->data.tab[en->pos++].count;
            en->pos         += en->remain_words;
        }
        assert (en->pos <= en->map->data.len);
        assert ((int)en->remain_words <= en->pos);
        en->state = WAH_ENUM_LITERAL;
        if (en->remain_words != 0) {
            en->current = en->map->data.tab[en->pos - en->remain_words].literal;
            return true;
        }

        /* Transition to literal, so don't break here */

      case WAH_ENUM_LITERAL:
        if (en->pos == en->map->data.len) {
            if ((en->map->len % WAH_BIT_IN_WORD)) {
                en->state = WAH_ENUM_PENDING;
                en->remain_words = 1;
                en->current = en->map->pending;
                return true;
            } else {
                en->state   = WAH_ENUM_END;
                en->current = 0;
                return false;
            }
        }
        en->state = WAH_ENUM_RUN;
        en->remain_words = en->map->data.tab[en->pos].head.words;
        en->current = 0 - en->map->data.tab[en->pos].head.bit;
        return true;
    }
}

static ALWAYS_INLINE
bool wah_word_enum_skip(wah_word_enum_t *en, uint32_t skip)
{
    uint32_t skippable = 0;

    while (skip != 0) {
        switch (__builtin_expect(en->state, WAH_ENUM_RUN)) {
          case WAH_ENUM_END:
            return false;

          case WAH_ENUM_PENDING:
            return wah_word_enum_next(en);

          default:
            skippable = MIN(skip, en->remain_words);
            skip -= skippable;

            /* XXX: Use next to skip the last word because:
             *  - if we reach the end of a run, this will automatically select
             *    the next run
             *  - if we end within a run of literal, this will properly update
             *    'en->current' with the next literal word
             */
            en->remain_words -= skippable - 1;
            wah_word_enum_next(en);
            break;
        }
    }
    return true;
}

static ALWAYS_INLINE
uint32_t wah_word_enum_skip0(wah_word_enum_t *en)
{
    uint32_t skipped = 0;

    while (en->current == 0) {
        switch (__builtin_expect(en->state, WAH_ENUM_RUN)) {
          case WAH_ENUM_END:
            return skipped;

          case WAH_ENUM_PENDING:
            skipped++;
            wah_word_enum_next(en);
            return skipped;

          case WAH_ENUM_RUN:
            skipped += en->remain_words;
            en->remain_words = 1;
            wah_word_enum_next(en);
            break;

          case WAH_ENUM_LITERAL:
            skipped++;
            wah_word_enum_next(en);
            break;
        }
    }
    return skipped;
}

/*
 * invariants for an enumerator not a WAH_ENUM_END:
 *  - current_word is non 0 and its last bit is set
 *  - if remain_bits <= WAH_BIT_IN_WORD we're consuming "current_word" else
 *    we're streaming ones.
 *  - remain_bits is the number of yet to stream bits including the current
 *    one.
 */
typedef struct wah_bit_enum_t {
    wah_word_enum_t word_en;
    uint64_t        key;
    uint64_t        remain_bits;
    uint32_t        current_word;
    uint32_t        reverse;
} wah_bit_enum_t;

static inline
bool wah_bit_enum_scan_word(wah_bit_enum_t *en)
{
    /* realign to a word boundary */
    assert (en->current_word == 0);
    en->key += en->remain_bits;
    assert (en->word_en.state <= WAH_ENUM_PENDING ||
            (en->key % WAH_BIT_IN_WORD) == 0);

    while (wah_word_enum_next(&en->word_en)) {
        en->current_word = en->reverse ^ en->word_en.current;
        if (en->word_en.state == WAH_ENUM_RUN) {
            if (en->current_word) {
                en->remain_bits  = en->word_en.remain_words * WAH_BIT_IN_WORD;
                en->word_en.remain_words = 1;
                return true;
            }
            en->key += en->word_en.remain_words * WAH_BIT_IN_WORD;
            en->word_en.remain_words = 1;
        } else {
            if (unlikely(en->word_en.state == WAH_ENUM_PENDING)) {
                en->remain_bits  = en->word_en.map->len % WAH_BIT_IN_WORD;
                en->current_word &= BITMASK_LT(uint32_t, en->remain_bits);
            } else {
                en->remain_bits  = WAH_BIT_IN_WORD;
            }
            if (likely(en->current_word)) {
                return true;
            }
            en->key += WAH_BIT_IN_WORD;
        }
    }
    return false;
}

static ALWAYS_INLINE
void wah_bit_enum_scan(wah_bit_enum_t *en)
{
    if (en->current_word == 0 && !wah_bit_enum_scan_word(en))
        return;

    assert (en->current_word);
    if (en->remain_bits <= WAH_BIT_IN_WORD) {
        uint32_t bit = bsf32(en->current_word);

        assert (bit < en->remain_bits);
        en->key           += bit;
        en->current_word >>= bit;
        en->remain_bits   -= bit;
    }
}

static ALWAYS_INLINE
void wah_bit_enum_next(wah_bit_enum_t *en)
{
    en->key++;
    if (en->remain_bits <= WAH_BIT_IN_WORD)
        en->current_word >>= 1;
    en->remain_bits--;
    wah_bit_enum_scan(en);
}

static inline
wah_bit_enum_t wah_bit_enum_start(const wah_t *wah, bool reverse)
{
    wah_bit_enum_t en;

    p_clear(&en, 1);
    en.word_en = wah_word_enum_start(wah);
    en.reverse = 0 - reverse;
    if (en.word_en.state != WAH_ENUM_END) {
        en.current_word = en.reverse ^ en.word_en.current;
        en.remain_bits  = WAH_BIT_IN_WORD;
        if (en.word_en.state == WAH_ENUM_PENDING) {
            en.remain_bits   = en.word_en.map->len % WAH_BIT_IN_WORD;
            en.current_word &= BITMASK_LT(uint32_t, en.remain_bits);
        }
        wah_bit_enum_scan(&en);
    }
    return en;
}

#define wah_for_each_1(en, map)                                              \
    for (wah_bit_enum_t en = wah_bit_enum_start(map, false);                 \
         en.word_en.state != WAH_ENUM_END; wah_bit_enum_next(&en))

#define wah_for_each_0(en, map)                                              \
    for (wah_bit_enum_t en = wah_bit_enum_start(map, true);                  \
         en.word_en.state != WAH_ENUM_END; wah_bit_enum_next(&en))

/* }}} */
/* Debugging {{{ */

__cold
void wah_debug_print(const wah_t *wah, bool print_content);

/* }}} */
/** \} */
#endif
