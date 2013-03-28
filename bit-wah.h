/**************************************************************************/
/*                                                                        */
/*  Copyright (C) 2004-2013 INTERSEC SA                                   */
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

    int            previous_run_pos;
    int            last_run_pos;

    qv_t(wah_word) data;

    uint32_t       pending;
    wah_word_t     padding[3]; /* Ensure sizeof(wah_t) == 64 */
} wah_t;
qvector_t(wah, wah_t);

#define WAH_BIT_IN_WORD  bitsizeof(wah_word_t)

/* }}} */
/* Public API {{{ */


wah_t *wah_init(wah_t *map) __leaf;
wah_t *wah_new(void) __leaf;
void wah_wipe(wah_t *map) __leaf;
GENERIC_DELETE(wah_t, wah);

wah_t *t_wah_new(int expected_size) __leaf;
wah_t *t_wah_dup(const wah_t *src) __leaf;
void wah_copy(wah_t *map, const wah_t *src) __leaf;
wah_t *wah_dup(const wah_t *src) __leaf;

/* Create a wah structure from existing wah-encoded bitmap
 *
 * This generates read-only wah_t structures.
 */
wah_t *wah_init_from_data(wah_t *wah, const uint32_t *data,
                          int data_len, bool scan);
wah_t *wah_new_from_data(const uint32_t *data, int data_len, bool scan);


void wah_add0s(wah_t *map, uint64_t count) __leaf;
void wah_add1s(wah_t *map, uint64_t count) __leaf;
void wah_add1_at(wah_t *map, uint64_t pos) __leaf;
void wah_add(wah_t *map, const void *data, uint64_t count) __leaf;
void wah_pad32(wah_t *map) __leaf;

void wah_and(wah_t *map, const wah_t *other) __leaf;
void wah_and_not(wah_t *map, const wah_t *other) __leaf;
void wah_not_and(wah_t *map, const wah_t *other) __leaf;
void wah_or(wah_t *map, const wah_t *other) __leaf;
void wah_not(wah_t *map) __leaf;

__must_check__ __leaf
bool wah_get(const wah_t *map, uint64_t pos);

static inline
void wah_reset_map(wah_t *map)
{
    map->len                  = 0;
    map->active               = 0;
    map->previous_run_pos     = -1;
    map->last_run_pos         = 0;
    qv_clear(wah_word, &map->data);
    p_clear(qv_growlen(wah_word, &map->data, 2), 2);
    map->pending              = 0;
}

/* }}} */
/* WAH pools {{{ */

wah_t *wah_pool_acquire(void);
void wah_pool_release(wah_t **wah);

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
    uint32_t         reverse;
} wah_word_enum_t;

wah_word_enum_t wah_word_enum_start(const wah_t *map, bool reverse);
bool wah_word_enum_next(wah_word_enum_t *en);
uint32_t wah_word_enum_skip0(wah_word_enum_t *en);

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
} wah_bit_enum_t;

bool wah_bit_enum_scan_word(wah_bit_enum_t *en);

static ALWAYS_INLINE void wah_bit_enum_scan(wah_bit_enum_t *en)
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

static ALWAYS_INLINE void wah_bit_enum_next(wah_bit_enum_t *en)
{
    en->key++;
    if (en->remain_bits <= WAH_BIT_IN_WORD)
        en->current_word >>= 1;
    en->remain_bits--;
    wah_bit_enum_scan(en);
}

wah_bit_enum_t wah_bit_enum_start(const wah_t *wah, bool reverse);

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
