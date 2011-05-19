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
qvector_t(wah, wah_word_t);

typedef struct wah_t {
    uint64_t  len;
    uint64_t  active;

    wah_header_t first_run_head;
    uint32_t     first_run_len;

    int       previous_run_pos;
    int       last_run_pos;
    qv_t(wah) data;
    uint32_t  pending;
} wah_t;
#define WAH_BIT_IN_WORD  bitsizeof(wah_word_t)

/* }}} */
/* Public API {{{ */

wah_t *wah_init(wah_t *map);
void wah_wipe(wah_t *map);
GENERIC_DELETE(wah_t, wah);
GENERIC_NEW(wah_t, wah);

void wah_add0s(wah_t *map, uint64_t count);
void wah_add1s(wah_t *map, uint64_t count);

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
    map->first_run_len        = 0;
    map->previous_run_pos     = -1;
    map->last_run_pos         = -1;
    map->data.len             = 0;
    map->pending              = 0;
}

/* }}} */
/* Enumeration {{{ */

typedef struct wah_word_enum_t {
    const wah_t *map;

    int      pos;
    bool     in_pending;
    bool     in_header;
    bool     header_bit;
    bool     end;
    uint32_t remain_words;
} wah_word_enum_t;

static inline
wah_word_enum_t wah_word_enum_start(const wah_t *map)
{
    wah_word_enum_t en = { map, -1, false, false, false, false, 0 };
    if (map->len == 0) {
        en.end = true;
        return en;
    }
    if (map->first_run_head.words > 0) {
        en.in_header  = true;
        en.in_pending = false;
        en.header_bit = map->first_run_head.bit;
        en.remain_words = map->first_run_head.words;
    } else
    if (map->first_run_len > 0) {
        en.in_header    = false;
        en.in_pending   = false;
        en.pos          = map->first_run_len;
        en.remain_words = map->first_run_len;
    } else {
        en.in_pending   = true;
        en.in_header    = false;
        en.remain_words = 1;
    }
    return en;
}

static inline
void wah_word_enum_next(wah_word_enum_t *en)
{
    if (en->end) {
        return;
    }
    en->remain_words--;
    if (en->remain_words != 0) {
        return;
    }
    if (en->in_pending) {
        en->end = true;
        return;
    } else
    if (en->in_header) {
        if (en->pos == -1) {
            en->remain_words = en->map->first_run_len;
            en->pos          = en->remain_words;
        } else {
            en->pos++;
            en->remain_words = en->map->data.tab[en->pos++].count;
            en->pos         += en->remain_words;
        }
        en->in_header = false;
    }

    if (en->remain_words != 0) {
        return;
    }

    if (en->pos == en->map->data.len) {
        if ((en->map->len % WAH_BIT_IN_WORD)) {
            en->in_pending   = true;
            en->remain_words = 1;
            return;
        } else {
            en->end = true;
            return;
        }
    }
    en->in_header    = true;
    en->header_bit   = en->map->data.tab[en->pos].head.bit;
    en->remain_words = en->map->data.tab[en->pos].head.words;
}

static inline
uint32_t wah_word_enum_current(const wah_word_enum_t *en)
{
    if (en->end) {
        return 0;
    } else
    if (en->in_header) {
        if (en->header_bit) {
            return UINT32_MAX;
        } else {
            return 0;
        }
    } else
    if (en->in_pending) {
        return en->map->pending;
    } else {
        return en->map->data.tab[en->pos - en->remain_words].literal;
    }
}

/* }}} */
#endif
