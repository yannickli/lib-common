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

#ifndef QKV_WAH_H
#define QKV_WAH_H

#include <lib-common/container.h>

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

wah_t *wah_init(wah_t *map);
void wah_wipe(wah_t *map);
GENERIC_DELETE(wah_t, wah);
GENERIC_NEW(wah_t, wah);

void wah_copy(wah_t *map, const wah_t *src);

void wah_add0s(wah_t *map, uint64_t count);
void wah_add1s(wah_t *map, uint64_t count);
void wah_add(wah_t *map, const void *data, uint64_t count);

void wah_and(wah_t *map, const wah_t *other);
void wah_or(wah_t *map, const wah_t *other);
void wah_not(wah_t *map);

__must_check__
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
    WAH_ENUM_HEADER_0,
    WAH_ENUM_HEADER_1,
    WAH_ENUM_LITERAL,
} wah_enum_state_t;

typedef struct wah_word_enum_t {
    const wah_t     *map;
    wah_enum_state_t state;
    int              pos;
    uint32_t         remain_words;
} wah_word_enum_t;

static inline
wah_word_enum_t wah_word_enum_start(const wah_t *map)
{
    wah_word_enum_t en = { map, WAH_ENUM_END, -1, 0 };
    if (map->len == 0) {
        en.state = WAH_ENUM_END;
        return en;
    }
    if (map->first_run_head.words > 0) {
        en.state        = (wah_enum_state_t)(WAH_ENUM_HEADER_0 + map->first_run_head.bit);
        en.remain_words = map->first_run_head.words;
    } else
    if (map->first_run_len > 0) {
        en.state        = WAH_ENUM_LITERAL;
        en.pos          = map->first_run_len;
        en.remain_words = map->first_run_len;
    } else {
        en.state        = WAH_ENUM_PENDING;
        en.remain_words = 1;
    }
    return en;
}

static inline
void wah_word_enum_next(wah_word_enum_t *en)
{
    en->remain_words--;
    if (en->remain_words != 0) {
        return;
    }
    switch (en->state) {
      case WAH_ENUM_END:
        return;

      case WAH_ENUM_PENDING:
        en->state = WAH_ENUM_END;
        return;

      case WAH_ENUM_HEADER_1:
      case WAH_ENUM_HEADER_0:
        if (en->pos == -1) {
            en->remain_words = en->map->first_run_len;
            en->pos          = en->remain_words;
        } else {
            en->pos++;
            en->remain_words = en->map->data.tab[en->pos++].count;
            en->pos         += en->remain_words;
        }
        en->state = WAH_ENUM_LITERAL;
        if (en->remain_words != 0) {
            return;
        }

        /* Transition to literal, so don't break here */

      case WAH_ENUM_LITERAL:
        if (en->pos == en->map->data.len) {
            if ((en->map->len % WAH_BIT_IN_WORD)) {
                en->state = WAH_ENUM_PENDING;
                en->remain_words = 1;
                return;
            } else {
                en->state = WAH_ENUM_END;
                return;
            }
        }
        en->state = (wah_enum_state_t)(WAH_ENUM_HEADER_0 + en->map->data.tab[en->pos].head.bit);
        en->remain_words = en->map->data.tab[en->pos].head.words;
        return;
    }
}

static inline
uint32_t wah_word_enum_current(const wah_word_enum_t *en)
{
    switch (en->state) {
      case WAH_ENUM_END:
        return 0;

      case WAH_ENUM_PENDING:
        return en->map->pending;

      case WAH_ENUM_HEADER_0:
        return 0;

      case WAH_ENUM_HEADER_1:
        return UINT32_MAX;

      case WAH_ENUM_LITERAL:
        return en->map->data.tab[en->pos - en->remain_words].literal;
    }
    e_panic("This should not happen");
}


typedef struct wah_bit_enum_t {
    wah_word_enum_t word_en;
    uint64_t        key;
    uint64_t        current_word;
    uint64_t        remain_bits;
    bool            all_one;
    bool            reverse;
    bool            end;
} wah_bit_enum_t;

static inline
void wah_bit_enum_find_word(wah_bit_enum_t *en)
{
    while (en->word_en.state != WAH_ENUM_END) {
        if ((en->reverse && en->word_en.state == WAH_ENUM_HEADER_1)
        ||  (!en->reverse && en->word_en.state == WAH_ENUM_HEADER_0)) {
            en->key += en->word_en.remain_words * 32;
            en->word_en.remain_words = 1;
            wah_word_enum_next(&en->word_en);
            continue;
        } else
        if ((en->reverse && en->word_en.state == WAH_ENUM_HEADER_0)
        ||  (!en->reverse && en->word_en.state == WAH_ENUM_HEADER_1)) {
            en->current_word = UINT64_MAX;
            en->all_one      = true;
            en->remain_bits  = en->word_en.remain_words * 32;
            en->word_en.remain_words = 1;
            return;
        }

        if (en->reverse) {
            en->current_word = ~wah_word_enum_current(&en->word_en);
        } else {
            en->current_word = wah_word_enum_current(&en->word_en);
        }
        if (en->current_word != 0) {
            en->all_one = false;
            break;
        }
        assert ((en->key % WAH_BIT_IN_WORD) == 0);
        en->key += 32;
        /* Skip ranges of 0s */
        wah_word_enum_next(&en->word_en);
    }
    if (en->word_en.state == WAH_ENUM_END) {
        en->end = true;
        return;
    }
    if (en->word_en.state == WAH_ENUM_PENDING) {
        en->remain_bits = en->word_en.map->len % WAH_BIT_IN_WORD;
        return;
    } else {
        en->remain_bits = 32;
    }
    wah_word_enum_next(&en->word_en);
    if (en->reverse) {
        en->current_word |= ((uint64_t)(~wah_word_enum_current(&en->word_en))) << 32;
    } else {
        en->current_word |= ((uint64_t)(wah_word_enum_current(&en->word_en))) << 32;
    }
    if (en->word_en.state == WAH_ENUM_PENDING) {
        en->remain_bits += en->word_en.map->len % WAH_BIT_IN_WORD;
    } else
    if (en->word_en.state != WAH_ENUM_END) {
        en->remain_bits += 32;
    }
}

static inline
void wah_bit_enum_next(wah_bit_enum_t *en)
{
    bool new_word = false;
    int bit;
    if (en->end) {
        return;
    }
    if (en->remain_bits == 0 || en->current_word == 0) {
        en->key += en->remain_bits + 1;
        wah_word_enum_next(&en->word_en);
        wah_bit_enum_find_word(en);
        if (en->end) {
            return;
        }
        new_word = true;
    }
    if (en->all_one) {
        en->remain_bits--;
        if (!new_word) {
            en->key++;
        }
        return;
    }
    bit = bsf64(en->current_word);
    if (bit >= (int)en->remain_bits) {
        wah_word_enum_next(&en->word_en);
        assert (en->word_en.state == WAH_ENUM_END);
        en->end = true;
        return;
    }
    en->key += bit;
    if (!new_word) {
        en->key++;
    }
    en->remain_bits -= bit + 1;
    if (en->remain_bits != 0) {
        en->current_word >>= bit + 1;
    }
}

static inline
wah_bit_enum_t wah_bit_enum_start(const wah_t *wah, bool reverse)
{
    wah_bit_enum_t en;
    int            bit;
    en.word_en = wah_word_enum_start(wah);
    en.key     = 0;
    en.reverse = reverse;
    en.end     = false;
    en.all_one = false;
    wah_bit_enum_find_word(&en);
    if (en.end) {
        return en;
    }
    if (en.all_one) {
        en.remain_bits--;
        return en;
    }
    bit = bsf64(en.current_word);
    en.key           += bit;
    en.remain_bits   -= bit + 1;
    if (en.remain_bits != 0) {
        en.current_word >>= bit + 1;
    }
    return en;
}

#define wah_for_each_1(en, map)                                              \
    for (wah_bit_enum_t en = wah_bit_enum_start(map, false);                 \
         !en.end; wah_bit_enum_next(&en))

#define wah_for_each_0(en, map)                                              \
    for (wah_bit_enum_t en = wah_bit_enum_start(map, true);                  \
         !en.end; wah_bit_enum_next(&en))

/* }}} */
#endif
