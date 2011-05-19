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

#include "wah.h"

static ALWAYS_INLINE
wah_header_t *wah_last_run_header(wah_t *map)
{
    if (map->last_run_pos < 0) {
        return &map->first_run_head;
    } else {
        return &map->data.tab[map->last_run_pos].head;
    }
}

static ALWAYS_INLINE
uint32_t *wah_last_run_count(wah_t *map)
{
    if (map->last_run_pos < 0) {
        return &map->first_run_len;
    } else {
        return &map->data.tab[map->last_run_pos].count;
    }
}

static ALWAYS_INLINE
void wah_append_header(wah_t *map, wah_header_t head)
{
    wah_word_t word;
    word.head = head;
    qv_append(wah, &map->data, word);
    word.count = 0;
    qv_append(wah, &map->data, word);
}

static ALWAYS_INLINE
void wah_append_literal(wah_t *map, uint32_t val)
{
    wah_word_t word;
    word.literal = val;
    qv_append(wah, &map->data, word);
}

static inline
void wah_flatten_last_run(wah_t *map)
{
    wah_header_t *head;
    if (map->last_run_pos < 0) {
        return;
    }
    assert (*wah_last_run_count(map) == 0);
    head = wah_last_run_header(map);
    assert (head->words == 1);
    assert (map->data.len == map->last_run_pos + 2);
    map->data.len -= 2;
    wah_append_literal(map, head->bit ? UINT32_MAX : 0);

    if (map->previous_run_pos < 0) {
        map->first_run_len++;
    } else {
        map->data.tab[map->previous_run_pos].count++;
    }
    map->last_run_pos     = map->previous_run_pos;
    map->previous_run_pos = -2;
}

static inline
void wah_push_pending(wah_t *map, uint32_t words)
{
    const bool is_trivial = map->pending == UINT32_MAX || map->pending == 0;
    if (!is_trivial) {
        if (wah_last_run_header(map)->words < 2) {
            wah_flatten_last_run(map);
        }
        *wah_last_run_count(map) += 1;
        wah_append_literal(map, map->pending);
    } else {
        wah_header_t *head = wah_last_run_header(map);
        if (*wah_last_run_count(map) == 0
        && (!head->bit == !map->pending || head->words == 0)) {
            /* Merge with previous */
            head->words += words;
            head->bit    = !map->pending;
        } else {
            /* Create a new run */
            wah_header_t new_head;
            if (head->words < 2) {
                wah_flatten_last_run(map);
            }
            new_head.bit          = !!map->pending;
            new_head.words        = words;
            map->previous_run_pos = map->last_run_pos;
            map->last_run_pos     = map->data.len;
            wah_append_header(map, new_head);
        }
    }
    map->pending = 0;
}

void wah_add0s(wah_t *map, uint64_t count)
{
    uint32_t remain = map->len % WAH_BIT_IN_WORD;
    if (count == 0) {
        return;
    }
    if (remain > 0) {
        if (count < remain) {
            map->len += count;
            return;
        }
        count    -= remain;
        map->len += remain;
        wah_push_pending(map, 1);
    }

    if (count == 0) {
        return;
    }
    if (count >= WAH_BIT_IN_WORD) {
        wah_push_pending(map, count / WAH_BIT_IN_WORD);
    }
    map->len += count;
}

void wah_add1s(wah_t *map, uint64_t count)
{
    uint64_t remain = map->len % WAH_BIT_IN_WORD;
    if (count == 0) {
        return;
    }
    if (remain > 0) {
        if (count < remain) {
            uint32_t mask = UINT32_MAX >> (32 - count);
            mask <<= (32 - remain);
            map->pending |= mask;
            map->len     += count;
            map->active  += count;
            return;
        } else {
            uint32_t mask = UINT32_MAX << (32 - remain);
            map->pending |= mask;
            map->len     += remain;
            map->active  += remain;
            count -= remain;
            wah_push_pending(map, 1);
        }
    }
    if (count == 0) {
        return;
    }
    if (count >= WAH_BIT_IN_WORD) {
        map->pending = UINT32_MAX;
        wah_push_pending(map, count / WAH_BIT_IN_WORD);
        map->pending = 0;
    }
    if (count % WAH_BIT_IN_WORD != 0) {
        map->pending = UINT32_MAX >> (32 - (count % WAH_BIT_IN_WORD));
    }
    map->len    += count;
    map->active += count;
}

void wah_and(wah_t *map, const wah_t *other)
{
    t_scope;
    const wah_t *src = ({
        wah_t *m = (wah_t *)t_dup(map, 1);
        m->data.tab      = (wah_word_t *)t_dup(map->data.tab, map->data.len);
        m->data.size     = map->data.len;
        m->data.mem_pool = MEM_STACK;
        m;
    });
    wah_word_enum_t src_en   = wah_word_enum_start(src);
    wah_word_enum_t other_en = wah_word_enum_start(other);

    wah_reset_map(map);
    while (!src_en.end || !src_en.end) {
        map->pending = wah_word_enum_current(&src_en)
                     & wah_word_enum_current(&other_en);
        if ((src_en.end && other_en.in_pending)
        ||  (other_en.end && src_en.in_pending)
        ||  (other_en.in_pending && src_en.in_pending)) {
            map->len += MAX(other->len % WAH_BIT_IN_WORD,
                            src->len % WAH_BIT_IN_WORD);
            map->active += bitcount32(map->pending);
        } else {
            map->len    += WAH_BIT_IN_WORD;
            map->active += bitcount32(map->pending);
            wah_push_pending(map, 1);
        }
        wah_word_enum_next(&src_en);
        wah_word_enum_next(&other_en);
    }

    assert (map->len == MAX(src->len, other->len));
    assert (map->active <= MIN(src->active, other->active));
}

void wah_or(wah_t *map, const wah_t *other)
{
    t_scope;
    const wah_t *src = ({
        wah_t *m = (wah_t *)t_dup(map, 1);
        m->data.tab      = (wah_word_t *)t_dup(map->data.tab, map->data.len);
        m->data.size     = map->data.len;
        m->data.mem_pool = MEM_STACK;
        m;
    });
    wah_word_enum_t src_en   = wah_word_enum_start(src);
    wah_word_enum_t other_en = wah_word_enum_start(other);

    wah_reset_map(map);
    while (!src_en.end || !src_en.end) {
        map->pending = wah_word_enum_current(&src_en)
                     | wah_word_enum_current(&other_en);
        if ((src_en.end && other_en.in_pending)
        ||  (other_en.end && src_en.in_pending)
        ||  (other_en.in_pending && src_en.in_pending)) {
            map->len += MAX(other->len % WAH_BIT_IN_WORD,
                            src->len % WAH_BIT_IN_WORD);
            map->active += bitcount32(map->pending);
        } else {
            map->len    += WAH_BIT_IN_WORD;
            map->active += bitcount32(map->pending);
            wah_push_pending(map, 1);
        }
        wah_word_enum_next(&src_en);
        wah_word_enum_next(&other_en);
    }

    assert (map->len == MAX(src->len, other->len));
    assert (map->active >= MAX(src->active, other->active));
}

void wah_not(wah_t *map)
{
    uint32_t pos;
    map->first_run_head.bit = !map->first_run_head.bit;
    for (pos = 0; pos < map->first_run_len; pos++) {
        map->data.tab[pos].literal = ~map->data.tab[pos].literal;
    }

    while (pos < (uint32_t)map->data.len) {
        wah_header_t *head  = &map->data.tab[pos++].head;
        uint32_t      count = map->data.tab[pos++].count;
        head->bit = !head->bit;
        for (uint32_t i = 0; i < count; i++) {
            map->data.tab[pos].literal = ~map->data.tab[pos].literal;
            pos++;
        }
    }
    map->active = map->len - map->active;
}

bool wah_get(const wah_t *map, uint64_t pos)
{
    uint64_t count;
    uint32_t remain = map->len % WAH_BIT_IN_WORD;
    int i = 0;
    if (pos >= map->len) {
        return false;
    }
    if (pos >= map->len - remain) {
        pos %= WAH_BIT_IN_WORD;
        return map->pending & (1 << pos);
    }

    count = map->first_run_head.words * WAH_BIT_IN_WORD;
    if (pos < count) {
        return !!map->first_run_head.bit;
    }
    pos -= count;
    count = map->first_run_len * WAH_BIT_IN_WORD;
    if (pos < count) {
        i    = pos / WAH_BIT_IN_WORD;
        pos %= WAH_BIT_IN_WORD;
        return !!(map->data.tab[i].literal & (1 << pos));
    }
    pos -= count;
    i    = map->first_run_len;

    while (i < map->data.len) {
        wah_header_t head  = map->data.tab[i++].head;
        uint32_t     words = map->data.tab[i++].count;

        count = head.words * WAH_BIT_IN_WORD;
        if (pos < count) {
            return !!head.bit;
        }
        pos -= count;

        count = words * WAH_BIT_IN_WORD;
        if (pos < count) {
            i   += pos / WAH_BIT_IN_WORD;
            pos %= WAH_BIT_IN_WORD;
            return !!(map->data.tab[i].literal & (1 << pos));
        }
        pos -= count;
        i   += words;
    }
    e_panic("This should not happen");
}
