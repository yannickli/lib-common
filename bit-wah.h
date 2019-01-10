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

#ifndef IS_LIB_COMMON_BIT_WAH_H
#define IS_LIB_COMMON_BIT_WAH_H

#include "container-qvector.h"

/** \defgroup qkv__ll__wah Word Aligned Hybrid bitmaps.
 * \ingroup qkv__ll
 * \brief Word Aligned Hybrid bitmaps.
 *
 * \{
 *
 * WAH are a form of compressed bitmap highly compact in case the bitmap
 * contains a lot of successive bits at the same value (1 or 0). In that case,
 * the sequence is compressed as a single integer containing the number of
 * words (32bits) of the sequence.
 *
 * \section format WAH format
 *
 * A WAH is a sequence of chunks each one composed of two parts:
 * - a header of two words with:
 *   - a bit value (0 or 1)
 *   - the number M of successive words with that bit value (called a run)
 *   - the number N of following words that were not compressed
 * - N uncompressed words.
 *
 * As a consequence, each chunk encodes M + N words in 2 + N words. This means
 * the maximum overhead of the WAH is when M is zero, in which case, the
 * overhead is 2 words (8 bytes). As a consequence, it is always at least as
 * efficient as an uncompressed stored in term of memory usage.
 *
 * It's Hybrid because it contains both compressed and uncompressed data, and
 * aligned since everything is done at the work level: the header only
 * references a integral number of words of 32 bits.
 *
 *
 * \section usage Use cases
 *
 * A WAH does not support efficient random accesses (reading / writing at a
 * specific bit position) because the chunks encode a variable amount of words
 * in a variable amount of memory. However, it efficiently support both
 * sequential reader / writing.
 *
 * Bitwise operations are also supported but, with the exception of the
 * negation operator,  they are not in place (they always require either a
 * brand new bitmap or a copy of one of the operands). Those operations are
 * efficient since they can deal with long runs with a single word read.
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
#define WAH_MAX_WORDS_IN_RUN  ((1ull << 31) - 1)

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

    /* Do not directly access these fields unless you really know what you are
     * doing. In most cases, you'll want to use wah_get_data. */
    qv_t(wah_word) _data;
    uint32_t       _pending;
    wah_word_t     _padding[3]; /* Ensure sizeof(wah_t) == 64 */
} wah_t;

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

/* Create a wah structure from existing wah-encoded bitmap.
 *
 * This generates read-only wah_t structures
 */
wah_t *wah_init_from_data(wah_t *wah, const uint32_t *data,
                          int data_len, bool scan);
wah_t *wah_new_from_data(const uint32_t *data, int data_len, bool scan);
wah_t *wah_new_from_data_lstr(lstr_t data, bool scan);

/** Get the raw data contained in a wah_t.
 *
 * This function must be used to get the data contained by a wah_t, in order
 * to, for example, write it on disk (and then use \ref wah_new_from_data to
 * reload it).
 *
 * \warning a wah must not have pending data if you want this to properly
 *          work; use \ref wah_pad32 to ensure that.
 */
lstr_t wah_get_data(const wah_t *wah);

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

wah_t *wah_multi_or(const wah_t *src[], int len, wah_t * __restrict dest) __leaf;

__must_check__ __leaf
bool wah_get(const wah_t *map, uint64_t pos);

static inline
void wah_reset_map(wah_t *map)
{
    map->len                  = 0;
    map->active               = 0;
    map->previous_run_pos     = -1;
    map->last_run_pos         = 0;
    qv_clear(wah_word, &map->_data);
    p_clear(qv_growlen(wah_word, &map->_data, 2), 2);
    map->_pending             = 0;
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

wah_word_enum_t wah_word_enum_start(const wah_t *map, bool reverse) __leaf;
bool wah_word_enum_next(wah_word_enum_t *en) __leaf;
uint32_t wah_word_enum_skip0(wah_word_enum_t *en) __leaf;

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

bool wah_bit_enum_scan_word(wah_bit_enum_t *en) __leaf;

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

wah_bit_enum_t wah_bit_enum_start(const wah_t *wah, bool reverse) __leaf;
void wah_bit_enum_skip1s(wah_bit_enum_t *en, uint64_t to_skip) __leaf;

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
