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

#include "thr.h"
#include "bit.h"

//#define WAH_CHECK_NORMALIZED  1

/* Word enumerator {{{ */

wah_word_enum_t wah_word_enum_start(const wah_t *map, bool reverse)
{
    wah_word_enum_t en = { map, WAH_ENUM_END, 0, 0, 0, (uint32_t)0 - reverse };

    if (map->len == 0) {
        en.state   = WAH_ENUM_END;
        en.current = en.reverse;
        return en;
    }
    if (map->data.tab[0].head.words > 0) {
        en.state        = WAH_ENUM_RUN;
        en.remain_words = map->data.tab[0].head.words;
        en.current      = 0 - map->data.tab[0].head.bit;
    } else
    if (map->data.tab[1].count > 0) {
        en.state        = WAH_ENUM_LITERAL;
        en.pos          = map->data.tab[1].count + 2;
        en.remain_words = map->data.tab[1].count;
        en.current      = map->data.tab[2].literal;
        assert (en.pos <= map->data.len);
        assert ((int)en.remain_words <= en.pos);
    } else {
        en.state        = WAH_ENUM_PENDING;
        en.remain_words = 1;
        en.current      = map->pending;
    }
    en.current ^= en.reverse;
    return en;
}

bool wah_word_enum_next(wah_word_enum_t *en)
{
    if (en->remain_words != 1) {
        en->remain_words--;
        if (en->state == WAH_ENUM_LITERAL) {
            en->current  = en->map->data.tab[en->pos - en->remain_words].literal;
            en->current ^= en->reverse;
        }
        return true;
    }
    switch (__builtin_expect(en->state, WAH_ENUM_RUN)) {
      case WAH_ENUM_END:
        return false;

      case WAH_ENUM_PENDING:
        en->state    = WAH_ENUM_END;
        en->current  = en->reverse;
        return false;

      default: /* WAH_ENUM_RUN */
        assert (en->state == WAH_ENUM_RUN);
        en->pos++;
        en->remain_words = en->map->data.tab[en->pos++].count;
        en->pos         += en->remain_words;
        assert (en->pos <= en->map->data.len);
        assert ((int)en->remain_words <= en->pos);
        en->state = WAH_ENUM_LITERAL;
        if (en->remain_words != 0) {
            en->current  = en->map->data.tab[en->pos - en->remain_words].literal;
            en->current ^= en->reverse;
            return true;
        }

        /* Transition to literal, so don't break here */

      case WAH_ENUM_LITERAL:
        if (en->pos == en->map->data.len) {
            if ((en->map->len % WAH_BIT_IN_WORD)) {
                en->state = WAH_ENUM_PENDING;
                en->remain_words = 1;
                en->current  = en->map->pending;
                en->current ^= en->reverse;
                return true;
            } else {
                en->state   = WAH_ENUM_END;
                en->current = en->reverse;
                return false;
            }
        }
        en->state = WAH_ENUM_RUN;
        en->remain_words = en->map->data.tab[en->pos].head.words;
        en->current  = 0 - en->map->data.tab[en->pos].head.bit;
        en->current ^= en->reverse;
        return true;
    }
}

static bool wah_word_enum_skip(wah_word_enum_t *en, uint32_t skip)
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

/* }}} */
/* Bit enumerator {{{ */

bool wah_bit_enum_scan_word(wah_bit_enum_t *en)
{
    /* realign to a word boundary */
    assert (en->current_word == 0);
    en->key += en->remain_bits;
    assert (en->word_en.state <= WAH_ENUM_PENDING ||
            (en->key % WAH_BIT_IN_WORD) == 0);

    while (wah_word_enum_next(&en->word_en)) {
        en->current_word = en->word_en.current;
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

wah_bit_enum_t wah_bit_enum_start(const wah_t *wah, bool reverse)
{
    wah_bit_enum_t en;

    p_clear(&en, 1);
    en.word_en = wah_word_enum_start(wah, reverse);
    if (en.word_en.state != WAH_ENUM_END) {
        en.current_word = en.word_en.current;
        en.remain_bits  = WAH_BIT_IN_WORD;
        if (en.word_en.state == WAH_ENUM_PENDING) {
            en.remain_bits   = en.word_en.map->len % WAH_BIT_IN_WORD;
            en.current_word &= BITMASK_LT(uint32_t, en.remain_bits);
        }
        wah_bit_enum_scan(&en);
    }
    return en;
}

/* }}} */
/* Administrativia {{{ */

wah_t *wah_init(wah_t *map)
{
    qv_init(wah_word, &map->data);
    wah_reset_map(map);
    return map;
}

wah_t *wah_new(void)
{
    wah_t *map = p_new_raw(wah_t, 1);

    __qv_init(wah_word, &map->data, map->padding, 0, 3, MEM_STATIC);
    wah_reset_map(map);
    return map;
}

void wah_wipe(wah_t *map)
{
    qv_wipe(wah_word, &map->data);
}

/* WAH copy requires an initialized wah as target
 */
void wah_copy(wah_t *map, const wah_t *src)
{
    qv_t(wah_word) data = map->data;

    p_copy(map, src, 1);
    map->data = data;
    qv_splice(wah_word, &map->data, 0, map->data.len,
              src->data.tab, src->data.len);
}

wah_t *wah_dup(const wah_t *src)
{
    wah_t *wah = wah_new();

    wah_copy(wah, src);
    return wah;
}

wah_t *t_wah_new(int size)
{
    wah_t *map = t_new(wah_t, 1);
    t_qv_init(wah_word, &map->data, size);
    wah_reset_map(map);
    return map;
}

wah_t *t_wah_dup(const wah_t *src)
{
    wah_t *map = t_new(wah_t, 1);
    p_copy(map, src, 1);
    t_qv_init(wah_word, &map->data, src->data.len);
    qv_splice(wah_word, &map->data, 0, 0, src->data.tab, src->data.len);
    return map;
}

/* }}} */
/* Operations {{{ */

static ALWAYS_INLINE
wah_header_t *wah_last_run_header(const wah_t *map)
{
    assert (map->last_run_pos >= 0);
    return &map->data.tab[map->last_run_pos].head;
}

static ALWAYS_INLINE
uint32_t *wah_last_run_count(const wah_t *map)
{
    assert (map->last_run_pos >= 0);
    return &map->data.tab[map->last_run_pos + 1].count;
}

static ALWAYS_INLINE
void wah_append_header(wah_t *map, wah_header_t head)
{
    wah_word_t word;
    word.head = head;
    qv_append(wah_word, &map->data, word);
    word.count = 0;
    qv_append(wah_word, &map->data, word);
}

static ALWAYS_INLINE
void wah_append_literal(wah_t *map, uint32_t val)
{
    wah_word_t word;
    word.literal = val;
    qv_append(wah_word, &map->data, word);
}

static inline
void wah_check_normalized(const wah_t *map)
{
    int pos = 0;
    uint32_t prev_word = 0xcafebabe;

    while (pos < map->data.len) {
        wah_header_t *head  = &map->data.tab[pos++].head;
        uint32_t      count = map->data.tab[pos++].count;

        assert (head->words >= 2 || pos == map->data.len || pos == 2);
        if (prev_word == UINT32_MAX || prev_word == 0) {
            assert (prev_word != head->bit ? UINT32_MAX : 0);
            prev_word = head->bit ? UINT32_MAX : 0;
        }

        for (uint32_t i = 0; i < count; i++) {
            if (prev_word == UINT32_MAX || prev_word == 0) {
                assert (prev_word != map->data.tab[pos].literal);
            }
            prev_word = map->data.tab[pos++].literal;
        }
    }
}

static ALWAYS_INLINE
void wah_check_invariant(const wah_t *map)
{
    assert (map->last_run_pos >= 0);
    assert (map->previous_run_pos >= -1);
    assert (map->data.len >= 2);
    assert ((int)*wah_last_run_count(map) + map->last_run_pos + 2 == map->data.len);
    assert (map->len >= map->active);
#ifdef WAH_CHECK_NORMALIZED
    wah_check_normalized(map);
#endif
}


static inline
void wah_flatten_last_run(wah_t *map)
{
    wah_header_t *head;

    head = wah_last_run_header(map);
    if (likely(head->words != 1)) {
        return;
    }
    assert (*wah_last_run_count(map) == 0);
    assert (map->data.len == map->last_run_pos + 2);

    if (map->last_run_pos > 0) {
        map->data.len -= 2;
        map->data.tab[map->previous_run_pos + 1].count++;
        map->last_run_pos     = map->previous_run_pos;
        map->previous_run_pos = -1;
    } else {
        head->words = 0;
        map->data.tab[1].count = 1;
    }

    wah_append_literal(map, head->bit ? UINT32_MAX : 0);
    wah_check_invariant(map);
}

static inline
void wah_push_pending(wah_t *map, uint32_t words)
{
    const bool is_trivial = map->pending == UINT32_MAX || map->pending == 0;
    if (!is_trivial) {
        wah_flatten_last_run(map);
        *wah_last_run_count(map) += words;
        while (words > 0) {
            wah_append_literal(map, map->pending);
            words--;
        }
    } else {
        wah_header_t *head = wah_last_run_header(map);
        if (*wah_last_run_count(map) == 0
        && (!head->bit == !map->pending || head->words == 0)) {
            /* Merge with previous */
            head->words += words;
            head->bit    = !!map->pending;
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

    wah_check_invariant(map);
    if (remain + count < WAH_BIT_IN_WORD) {
        map->len += count;
        wah_check_invariant(map);
        return;
    }
    if (remain > 0) {
        count    -= WAH_BIT_IN_WORD - remain;
        map->len += WAH_BIT_IN_WORD - remain;
        wah_push_pending(map, 1);
    }
    if (count >= WAH_BIT_IN_WORD) {
        wah_push_pending(map, count / WAH_BIT_IN_WORD);
    }
    map->len += count;
    wah_check_invariant(map);
}

void wah_pad32(wah_t *map)
{
    uint32_t padding = WAH_BIT_IN_WORD - (map->len % WAH_BIT_IN_WORD);

    if (padding) {
        wah_add0s(map, padding);
    }
}

void wah_add1s(wah_t *map, uint64_t count)
{
    uint64_t remain = map->len % WAH_BIT_IN_WORD;

    wah_check_invariant(map);
    if (remain + count < WAH_BIT_IN_WORD) {
        map->pending |= BITMASK_LT(uint32_t, count) << remain;
        map->len     += count;
        map->active  += count;
        wah_check_invariant(map);
        return;
    }
    if (remain > 0) {
        map->pending |= BITMASK_GE(uint32_t, remain);
        map->len     += WAH_BIT_IN_WORD - remain;
        map->active  += WAH_BIT_IN_WORD - remain;
        count        -= WAH_BIT_IN_WORD - remain;
        wah_push_pending(map, 1);
    }
    if (count >= WAH_BIT_IN_WORD) {
        map->pending = UINT32_MAX;
        wah_push_pending(map, count / WAH_BIT_IN_WORD);
    }
    map->pending = BITMASK_LT(uint32_t, count);
    map->len    += count;
    map->active += count;
    wah_check_invariant(map);
}

void wah_add1_at(wah_t *map, uint64_t pos)
{
    assert (pos >= map->len);

    if (unlikely(pos < map->len)) {
        t_scope;
        wah_t *tmp = t_wah_new(4);

        wah_add1_at(tmp, pos);
        wah_or(map, tmp);
        return;
    }

    if (pos != map->len) {
        wah_add0s(map, pos - map->len);
    }
    wah_add1s(map, 1);
}

static
const void *wah_read_word(const uint8_t *src, uint64_t count,
                          uint64_t *res, int *bits)
{
    uint64_t mask;
    if (count >= 64) {
        *res  = get_unaligned_le64(src);
        *bits = 64;
        return src + 8;
    }

    *res  = 0;
    *bits = 0;
    mask  = BITMASK_LT(uint64_t, count);
#define get_unaligned_le8(src)  (*src)
#define READ_SIZE(Size)                                                      \
    if (count > (Size - 8)) {                                                \
        const uint64_t to_read = MIN(count, Size);                           \
        *res   |= ((uint64_t)get_unaligned_le##Size(src)) << *bits;          \
        *bits  += to_read;                                                   \
        src    += Size / 8;                                                  \
        if (to_read == count) {                                              \
            *res &= mask;                                                    \
            return src;                                                      \
        }                                                                    \
        count -= to_read;                                                    \
    }
    READ_SIZE(32);
    READ_SIZE(24);
    READ_SIZE(16);
    READ_SIZE(8);
#undef READ_SIZE
    *res &= mask;
    return src;
}

static
const void *wah_add_unaligned(wah_t *map, const uint8_t *src, uint64_t count)
{
    while (count > 0) {
        uint64_t word;
        int      bits = 0;
        bool     on_0 = true;

        src    = wah_read_word(src, count, &word, &bits);
        count -= bits;

        while (bits > 0) {
            if (word == 0) {
                if (on_0) {
                    wah_add0s(map, bits);
                } else {
                    wah_add1s(map, bits);
                }
                bits = 0;
            } else {
                int first = bsf64(word);
                if (first > bits) {
                    first = bits;
                }
                if (first != 0) {
                    if (on_0) {
                        wah_add0s(map, first);
                    } else {
                        wah_add1s(map, first);
                    }
                    bits  -= first;
                    word >>= first;
                }
                word = ~word;
                on_0 = !on_0;
            }
        }
    }
    wah_check_invariant(map);
    return src;
}

static
void wah_add_aligned(wah_t *map, const uint8_t *src, uint64_t count)
{
    map->len += count;
    while (count > 0) {
        int bits;
        union {
            struct {
#if __BYTE_ORDER == __BIG_ENDIAN
                uint32_t high;
                uint32_t low;
#else
                uint32_t low;
                uint32_t high;
#endif
            };
            uint64_t val;
        } word;
        src = wah_read_word(src, count, &word.val, &bits);
        count -= bits;
        map->active += bitcount64(word.val);
        map->pending = word.low;
        if (bits == 64) {
            if (word.low == word.high) {
                wah_push_pending(map, 2);
            } else {
                wah_push_pending(map, 1);
                map->pending = word.high;
                wah_push_pending(map, 1);
            }
        } else
        if (bits >= 32) {
            wah_push_pending(map, 1);
            map->pending = word.high;
        } else {
            assert (count == 0);
        }
    }
}

void wah_add(wah_t *map, const void *data, uint64_t count)
{
    uint32_t remain = WAH_BIT_IN_WORD - (map->len % WAH_BIT_IN_WORD);

    wah_check_invariant(map);
    if (remain != WAH_BIT_IN_WORD) {
        if (remain >= count || (remain % 8) != 0) {
            wah_add_unaligned(map, data, count);
            wah_check_invariant(map);
            return;
        } else {
            data   = wah_add_unaligned(map, data, remain);
            count -= remain;
        }
    }
    assert (map->len % WAH_BIT_IN_WORD == 0);
    wah_add_aligned(map, data, count);
    wah_check_invariant(map);
}

#define PUSH_1RUN(Count) ({                                                  \
            uint32_t __run = (Count);                                        \
                                                                             \
            map->pending  = UINT32_MAX;                                      \
            map->len     += __run * WAH_BIT_IN_WORD;                         \
            map->active  += __run * WAH_BIT_IN_WORD;                         \
            wah_push_pending(map, __run);                                    \
            wah_word_enum_skip(&other_en, __run);                            \
            wah_word_enum_skip(&src_en, __run);                              \
        })

#define PUSH_0RUN(Count) ({                                                  \
            uint32_t __run = (Count);                                        \
                                                                             \
            map->pending  = 0;                                               \
            map->len     += __run * WAH_BIT_IN_WORD;                         \
            wah_push_pending(map, __run);                                    \
            wah_word_enum_skip(&src_en, __run);                              \
            wah_word_enum_skip(&other_en, __run);                            \
        })

static
void wah_copy_run(wah_t *map, wah_word_enum_t *run, wah_word_enum_t *data)
{
    uint32_t count = MIN(run->remain_words, data->remain_words);

    assert (count > 0);
    wah_word_enum_skip(run, count);

    if (unlikely(!data->current || data->current == UINT32_MAX)) {
        map->pending  = data->current;
        map->active  += bitcount32(map->pending);
        map->len     += WAH_BIT_IN_WORD;
        wah_push_pending(map, 1);
        wah_word_enum_next(data);
        count--;
    }
    if (likely(count > 0)) {
        const wah_word_t *words;

        words = &data->map->data.tab[data->pos - data->remain_words];
        wah_word_enum_skip(data, count);

        wah_flatten_last_run(map);
        *wah_last_run_count(map) += count;
        qv_splice(wah_word, &map->data, map->data.len, 0, words, count);
        if (data->reverse) {
            for (int i = map->data.len - count; i < map->data.len; i++) {
                map->data.tab[i].literal = ~map->data.tab[i].literal;
            }
        }
        map->len    += count * WAH_BIT_IN_WORD;
        map->active += membitcount(&map->data.tab[map->data.len - count],
                                   count * sizeof(wah_word_t));
    }
}

#define PUSH_COPY(Run, Data)  wah_copy_run(map, &(Run), &(Data))

#define REMAIN_WORDS(Long, Map) (((Long)->len - (Map)->len) / WAH_BIT_IN_WORD)

static
void wah_and_(wah_t *map, const wah_t *other, bool map_not, bool other_not)
{
    t_scope;
    const wah_t *src = t_wah_dup(map);
    wah_word_enum_t src_en   = wah_word_enum_start(src, map_not);
    wah_word_enum_t other_en = wah_word_enum_start(other, other_not);

    wah_check_invariant(map);
    wah_reset_map(map);
    while (src_en.state != WAH_ENUM_END || other_en.state != WAH_ENUM_END) {
        if (src_en.state == WAH_ENUM_END) {
            src_en.remain_words = REMAIN_WORDS(other, map);
        } else
        if (other_en.state == WAH_ENUM_END) {
            other_en.remain_words = REMAIN_WORDS(src, map);
        }

        switch (src_en.state | (other_en.state << 2)) {
          case WAH_ENUM_END     | (WAH_ENUM_PENDING << 2):
          case WAH_ENUM_PENDING | (WAH_ENUM_END     << 2):
          case WAH_ENUM_PENDING | (WAH_ENUM_PENDING << 2):
            map->len     = MAX(other->len, src->len);
            map->pending = src_en.current & other_en.current;
            map->active += bitcount32(map->pending);
            wah_word_enum_next(&src_en);
            wah_word_enum_next(&other_en);
            break;

          case WAH_ENUM_RUN     | (WAH_ENUM_LITERAL << 2):
          case WAH_ENUM_END     | (WAH_ENUM_LITERAL << 2):
            if (src_en.current) {
                PUSH_COPY(src_en, other_en);
            } else {
                PUSH_0RUN(src_en.remain_words);
            }
            break;

          case WAH_ENUM_LITERAL | (WAH_ENUM_RUN     << 2):
          case WAH_ENUM_LITERAL | (WAH_ENUM_END     << 2):
            if (other_en.current) {
                PUSH_COPY(other_en, src_en);
            } else {
                PUSH_0RUN(other_en.remain_words);
            }
            break;

          case WAH_ENUM_RUN     | (WAH_ENUM_RUN     << 2):
          case WAH_ENUM_END     | (WAH_ENUM_RUN     << 2):
          case WAH_ENUM_RUN     | (WAH_ENUM_END     << 2):
            if (!other_en.current || !src_en.current) {
                uint32_t run = 0;

                if (!other_en.current) {
                    run = other_en.remain_words;
                }
                if (!src_en.current) {
                    run = MAX(run, src_en.remain_words);
                }
                PUSH_0RUN(run);
            } else {
                PUSH_1RUN(MIN(other_en.remain_words, src_en.remain_words));
            }
            break;

          default:
            map->len    += WAH_BIT_IN_WORD;
            map->pending = src_en.current & other_en.current;
            map->active += bitcount32(map->pending);
            wah_push_pending(map, 1);
            wah_word_enum_next(&src_en);
            wah_word_enum_next(&other_en);
            break;
        }
    }
    wah_check_invariant(map);

    assert (map->len == MAX(src->len, other->len));
    {
        uint64_t src_active = src->active;
        uint64_t other_active = other->active;

        if (map_not) {
            src_active = MAX(other->len, src->len) - src->active;
        }
        if (other_not) {
            other_active = MAX(other->len, src->len) - other->active;
        }
        assert (map->active <= MIN(src_active, other_active));
    }
}

void wah_and(wah_t *map, const wah_t *other)
{
    wah_and_(map, other, false, false);
}

void wah_and_not(wah_t *map, const wah_t *other)
{
    wah_and_(map, other, false, true);
}

void wah_not_and(wah_t *map, const wah_t *other)
{
    wah_and_(map, other, true, false);
}

void wah_or(wah_t *map, const wah_t *other)
{
    t_scope;
    const wah_t *src = t_wah_dup(map);
    wah_word_enum_t src_en   = wah_word_enum_start(src, false);
    wah_word_enum_t other_en = wah_word_enum_start(other, false);

    wah_check_invariant(map);
    wah_reset_map(map);
    while (src_en.state != WAH_ENUM_END || other_en.state != WAH_ENUM_END) {
        if (src_en.state == WAH_ENUM_END) {
            src_en.remain_words = REMAIN_WORDS(other, map);
        } else
        if (other_en.state == WAH_ENUM_END) {
            other_en.remain_words = REMAIN_WORDS(src, map);
        }

        switch (src_en.state | (other_en.state << 2)) {
          case WAH_ENUM_END     | (WAH_ENUM_PENDING << 2):
          case WAH_ENUM_PENDING | (WAH_ENUM_END     << 2):
          case WAH_ENUM_PENDING | (WAH_ENUM_PENDING << 2):
            map->len     = MAX(other->len, src->len);
            map->pending = src_en.current | other_en.current;
            map->active += bitcount32(map->pending);
            wah_word_enum_next(&src_en);
            wah_word_enum_next(&other_en);
            break;

          case WAH_ENUM_RUN     | (WAH_ENUM_LITERAL << 2):
          case WAH_ENUM_END     | (WAH_ENUM_LITERAL << 2):
            if (!src_en.current) {
                PUSH_COPY(src_en, other_en);
            } else {
                PUSH_1RUN(src_en.remain_words);
            }
            break;

          case WAH_ENUM_LITERAL | (WAH_ENUM_RUN     << 2):
          case WAH_ENUM_LITERAL | (WAH_ENUM_END     << 2):
            if (!other_en.current) {
                PUSH_COPY(other_en, src_en);
            } else {
                PUSH_1RUN(other_en.remain_words);
            }
            break;

          case WAH_ENUM_RUN     | (WAH_ENUM_RUN     << 2):
          case WAH_ENUM_END     | (WAH_ENUM_RUN     << 2):
          case WAH_ENUM_RUN     | (WAH_ENUM_END     << 2):
            if (other_en.current || src_en.current) {
                uint32_t run = 0;

                if (other_en.current) {
                    run = other_en.remain_words;
                }
                if (src_en.current) {
                    run = MAX(run, src_en.remain_words);
                }
                PUSH_1RUN(run);
            } else {
                PUSH_0RUN(MIN(other_en.remain_words, src_en.remain_words));
            }
            break;

          default:
            map->len    += WAH_BIT_IN_WORD;
            map->pending = src_en.current | other_en.current;
            map->active += bitcount32(map->pending);
            wah_push_pending(map, 1);
            wah_word_enum_next(&src_en);
            wah_word_enum_next(&other_en);
            break;
        }
    }
    wah_check_invariant(map);

    assert (map->len == MAX(src->len, other->len));
    assert (map->active >= MAX(src->active, other->active));
}

#undef PUSH_COPY
#undef PUSH_0RUN
#undef PUSH_1RUN

void wah_not(wah_t *map)
{
    uint32_t pos = 0;

    wah_check_invariant(map);
    while (pos < (uint32_t)map->data.len) {
        wah_header_t *head  = &map->data.tab[pos++].head;
        uint32_t      count = map->data.tab[pos++].count;
        head->bit = !head->bit;
        for (uint32_t i = 0; i < count; i++) {
            map->data.tab[pos].literal = ~map->data.tab[pos].literal;
            pos++;
        }
    }
    if ((map->len % WAH_BIT_IN_WORD) != 0) {
        map->pending = ~map->pending & BITMASK_LT(uint32_t, map->len);
    }
    map->active = map->len - map->active;
    wah_check_invariant(map);
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
    e_panic("this should not happen");
}

/* }}} */
/* Open existing WAH {{{ */

wah_t *wah_init_from_data(wah_t *map, const uint32_t *data, int data_len,
                          bool scan)
{
    int pos = 0;

    p_clear(map, 1);
    __qv_init(wah_word, &map->data, (wah_word_t *)data,
              data_len, data_len, MEM_STATIC);
    map->previous_run_pos = -1;
    map->last_run_pos = -1;

    THROW_NULL_IF(data_len < 2);
    if (!scan) {
        return map;
    }

    while (pos < map->data.len - 1) {
        wah_header_t head  = map->data.tab[pos++].head;
        uint32_t     words = map->data.tab[pos++].count;

        THROW_NULL_IF(words > (uint32_t)map->data.len
                    || (uint32_t)pos > map->data.len - words);
        map->previous_run_pos = map->last_run_pos;
        map->last_run_pos     = pos - 2;
        if (head.bit) {
            map->active += WAH_BIT_IN_WORD * head.words;
        }
        if (words) {
            map->active += membitcount(map->data.tab + pos,
                                       words * sizeof(wah_word_t));
        }
        map->len += WAH_BIT_IN_WORD * (head.words + words);
        pos += words;
    }
    THROW_NULL_IF(pos != map->data.len);

    return map;
}

wah_t *wah_new_from_data(const uint32_t *data, int data_len, bool scan)
{
    wah_t *map = p_new_raw(wah_t, 1);
    wah_t *ret;

    ret = wah_init_from_data(map, data, data_len, scan);
    if (!ret) {
        wah_delete(&map);
    }
    return ret;
}

/* }}} */
/* Pool {{{ */

static __thread struct {
    wah_t    *pool[16];
    int       count;
} pool_g;

__attribute__((constructor))
static void wah_pool_initialize(void)
{
}

static void wah_pool_shutdown(void)
{
    for (int i = 0; i < pool_g.count; i++) {
        wah_delete(&pool_g.pool[i]);
    }
}
thr_hooks(wah_pool_initialize, wah_pool_shutdown);


wah_t *wah_pool_acquire(void)
{
    if (pool_g.count == 0) {
        return wah_new();
    }
    return pool_g.pool[--pool_g.count];
}

void wah_pool_release(wah_t **pmap)
{
    wah_t *map = *pmap;

    if (!map) {
        return;
    }
    if (pool_g.count == countof(pool_g.pool)
    || (map->data.mem_pool != MEM_LIBC && map->data.tab != map->padding))
    {
        wah_delete(pmap);
        return;
    }

    wah_reset_map(map);
    pool_g.pool[pool_g.count++] = map;
    *pmap = NULL;
}

/* }}} */
/* Printer {{{ */

static
uint64_t wah_debug_print_run(uint64_t pos, const wah_header_t head)
{
    if (head.words != 0) {
        fprintf(stderr, "\e[1;30m[%08x] \e[33mRUN %d \e[0m%d words (%d bits)\n",
                (uint32_t)pos, head.bit, head.words, head.words * 32);
    }
    return head.words * 32;
}

static
void wah_debug_print_literal(uint64_t pos, const uint32_t lit)
{
    fprintf(stderr, "\e[1;30m[%08x] \e[33mLITERAL \e[0m%08x\n",
            (uint32_t)pos, lit);
}

static
uint64_t wah_debug_print_literals(uint64_t pos, uint32_t len)
{
    if (len != 0) {
        fprintf(stderr, "\e[1;30m[%08x] \e[33mLITERAL \e[0m%u words\n",
                (uint32_t)pos, len);
    }
    return len * 32;
}

static
void wah_debug_print_pending(uint64_t pos, const uint32_t pending, int len)
{
    if (len > 0) {
        fprintf(stderr, "\e[1;30m[%08x] \e[33mPENDING \e[0m%d bits: %08x\n",
                (uint32_t)pos, len, pending);
    }
}


void wah_debug_print(const wah_t *wah, bool print_content)
{
    uint64_t pos = 0;
    uint32_t len = 0;
    int      off = 0;

    for (;;) {
        if (print_content) {
            for (uint32_t i = 0; i < len; i++) {
                wah_debug_print_literal(pos, wah->data.tab[off++].literal);
                pos += 32;
            }
        } else {
            off += len;
            pos += wah_debug_print_literals(pos, len);
        }
        if (off < wah->data.len) {
            pos += wah_debug_print_run(pos, wah->data.tab[off++].head);
            len  = wah->data.tab[off++].count;
        } else {
            break;
        }
    }
    wah_debug_print_pending(pos, wah->pending, wah->len % 32);
}

/* }}} */
/* Tests {{{ */

#include "z.h"

Z_GROUP_EXPORT(wah)
{
    wah_t map, map1, map2;

    Z_TEST(simple, "") {
        wah_init(&map);
        wah_add0s(&map, 3);
        for (int i = 0; i < 3; i++) {
            Z_ASSERT(!wah_get(&map, i), "bad bit at offset %d", i);
        }
        Z_ASSERT(!wah_get(&map, 3), "bad bit at offset 3");

        wah_not(&map);
        for (int i = 0; i < 3; i++) {
            Z_ASSERT(wah_get(&map, i), "bad bit at offset %d", i);
        }
        Z_ASSERT(!wah_get(&map, 3), "bad bit at offset 3");
        wah_wipe(&map);
    } Z_TEST_END;

    Z_TEST(fill, "") {
        wah_init(&map);

        STATIC_ASSERT(sizeof(wah_word_t) == sizeof(uint32_t));
        STATIC_ASSERT(sizeof(wah_header_t) == sizeof(uint32_t));

        wah_add0s(&map, 63);
        for (int i = 0; i < 2 * 63; i++) {
            Z_ASSERT(!wah_get(&map, i), "bad bit at %d", i);
        }

        wah_add0s(&map, 3 * 63);
        for (int i = 0; i < 5 * 63; i++) {
            Z_ASSERT(!wah_get(&map, i), "bad bit at %d", i);
        }

        wah_reset_map(&map);
        wah_add1s(&map, 63);
        for (int i = 0; i < 2 * 63; i++) {
            bool bit = wah_get(&map, i);

            Z_ASSERT(!(i < 63 && !bit), "bad bit at %d", i);
            Z_ASSERT(!(i >= 63 && bit), "bad bit at %d", i);
        }
        wah_add1s(&map, 3 * 63);
        for (int i = 0; i < 5 * 63; i++) {
            bool bit = wah_get(&map, i);

            Z_ASSERT(!(i < 4 * 63 && !bit), "bad bit at %d", i);
            Z_ASSERT(!(i >= 4 * 63 && bit), "bad bit at %d", i);
        }

        wah_wipe(&map);
    } Z_TEST_END;

    Z_TEST(set_bitmap, "") {
        const byte data[] = {
            0x1f, 0x00, 0x00, 0x8c, /* 0, 1, 2, 3, 4, 26, 27, 31 (32)  */
            0xff, 0xff, 0xff, 0xff, /* 32 -> 63                  (64)  */
            0xff, 0xff, 0xff, 0xff, /* 64 -> 95                  (96)  */
            0xff, 0xff, 0xff, 0x80, /* 96 -> 119, 127            (128) */
            0x00, 0x10, 0x40, 0x00, /* 140, 150                  (160) */
            0x00, 0x00, 0x00, 0x00, /*                           (192) */
            0x00, 0x00, 0x00, 0x00, /*                           (224) */
            0x00, 0x00, 0x00, 0x00, /*                           (256) */
            0x00, 0x00, 0x00, 0x21  /* 280, 285                  (288) */
        };

        uint64_t      bc;

        wah_init(&map);
        wah_add(&map, data, bitsizeof(data));
        bc = membitcount(data, sizeof(data));

        Z_ASSERT_EQ(map.len, bitsizeof(data));

        Z_ASSERT_P(wah_init_from_data(&map2, (uint32_t *)map.data.tab,
                                      map.data.len, true));
        Z_ASSERT_EQ(map.len, map2.len);

        Z_ASSERT_EQ(map.active, bc, "invalid bit count");
        Z_ASSERT_EQ(map2.active, bc, "invalid bit count");
        for (int i = 0; i < countof(data); i++) {
            for (int j = 0; j < 8; j++) {
                Z_ASSERT_EQ(!!(data[i] & (1 << j)), !!wah_get(&map, i * 8 + j),
                            "invalid byte %d, bit %d", i, j);
                Z_ASSERT_EQ(!!(data[i] & (1 << j)), !!wah_get(&map2, i * 8 + j),
                            "invalid byte %d, bit %d", i, j);
            }
        }

        wah_wipe(&map2);

        wah_not(&map);
        Z_ASSERT_EQ(map.active, bitsizeof(data) - bc, "invalid bit count");
        for (int i = 0; i < countof(data); i++) {
            for (int j = 0; j < 8; j++) {
                Z_ASSERT_EQ(!(data[i] & (1 << j)), !!wah_get(&map, i * 8 + j),
                            "invalid byte %d, bit %d", i, j);
            }
        }

        wah_wipe(&map);
    } Z_TEST_END;

    Z_TEST(for_each, "") {
        const byte data[] = {
            0x1f, 0x00, 0x00, 0x8c, /* 0, 1, 2, 3, 4, 26, 27, 31 (32) */
            0xff, 0xff, 0xff, 0xff, /* 32 -> 63                  (64) */
            0xff, 0xff, 0xff, 0xff, /* 64 -> 95                  (96) */
            0xff, 0xff, 0xff, 0x80, /* 96 -> 119, 127            (128)*/
            0x00, 0x10, 0x40, 0x00, /* 140, 150                  (160)*/
            0x00, 0x00, 0x00, 0x00, /*                           (192)*/
            0x00, 0x00, 0x00, 0x00, /*                           (224)*/
            0x00, 0x00, 0x00, 0x00, /*                           (256)*/
            0x00, 0x00, 0x00, 0x21, /* 280, 285                  (288)*/
            0x12, 0x00, 0x10,       /* 289, 292, 308 */
        };

        uint64_t bc;
        uint64_t nbc;
        uint64_t c;
        uint64_t previous;

        wah_init(&map);
        wah_add(&map, data, bitsizeof(data));
        bc  = membitcount(data, sizeof(data));
        nbc = bitsizeof(data) - bc;

        Z_ASSERT_EQ(map.active, bc, "invalid bit count");
        c = 0;
        previous = 0;
        wah_for_each_1(en, &map) {
            if (c != 0) {
                Z_ASSERT_CMP(previous, <, en.key, "misordered enumeration");
            }
            previous = en.key;
            c++;
            Z_ASSERT_CMP(en.key, <, bitsizeof(data), "enumerate too far");
            Z_ASSERT(data[en.key >> 3] & (1 << (en.key & 0x7)),
                     "bit %d is not set", (int)en.key);
        }
        Z_ASSERT_EQ(c, bc, "bad number of enumerated entries");

        c = 0;
        previous = 0;
        wah_for_each_0(en, &map) {
            if (c != 0) {
                Z_ASSERT_CMP(previous, <, en.key, "misordered enumeration");
            }
            previous = en.key;
            c++;
            Z_ASSERT_CMP(en.key, <, bitsizeof(data), "enumerate too far");
            Z_ASSERT(!(data[en.key >> 3] & (1 << (en.key & 0x7))),
                     "bit %d is set", (int)en.key);
        }
        Z_ASSERT_EQ(c, nbc, "bad number of enumerated entries");
        wah_wipe(&map);
    } Z_TEST_END;

    Z_TEST(binop, "") {
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

        /* And-Not result
         * 0, 1, 2, 3, 4, 26, 27, 31                                       (32)
         * 32 -> 62                                                        (64)
         * 64 -> 75, 77 -> 84, 86 -> 95                                    (96)
         *                                                                 (128)
         *                                                                 (160)
         *                                                                 (192)
         *                                                                 (224)
         *                                                                 (256)
         * 280, 285                                                        (288)
         */

        /* Not-And result
         *                                                                 (32)
         *                                                                 (64)
         *                                                                 (96)
         * 125                                                             (128)
         * 128 -> 135, 138, 139, 141 -> 149, 151, 153, 156                 (160)
         */

        wah_init(&map1);
        wah_init(&map2);

        wah_add(&map1, data1, bitsizeof(data1));
        wah_add(&map2, data2, bitsizeof(data2));
        wah_and(&map1, &map2);
        for (int i = 0; i < countof(data1); i++) {
            byte b = data1[i];
            if (i < countof(data2)) {
                b &= data2[i];
            } else {
                b = 0;
            }
            for (int j = 0; j < 8; j++) {
                Z_ASSERT_EQ(!!(b & (1 << j)), !!wah_get(&map1, i * 8 + j),
                            "invalid byte %d, bit %d", i, j);
            }
        }

        wah_reset_map(&map1);
        wah_add(&map1, data1, bitsizeof(data1));
        wah_or(&map1, &map2);
        for (int i = 0; i < countof(data1); i++) {
            byte b = data1[i];
            if (i < countof(data2)) {
                b |= data2[i];
            }
            for (int j = 0; j < 8; j++) {
                Z_ASSERT_EQ(!!(b & (1 << j)), !!wah_get(&map1, i * 8 + j),
                            "invalid byte %d, bit %d", i, j);
            }
        }

        wah_reset_map(&map1);
        wah_add(&map1, data1, bitsizeof(data1));
        wah_and_not(&map1, &map2);
        for (int i = 0; i < countof(data1); i++) {
            byte b = data1[i];
            if (i < countof(data2)) {
                b &= ~data2[i];
            }
            for (int j = 0; j < 8; j++) {
                Z_ASSERT_EQ(!!(b & (1 << j)), !!wah_get(&map1, i * 8 + j),
                            "invalid byte %d, bit %d", i, j);
            }
        }

        wah_reset_map(&map1);
        wah_add(&map1, data1, bitsizeof(data1));
        wah_not_and(&map1, &map2);
        for (int i = 0; i < countof(data1); i++) {
            byte b = ~data1[i];
            if (i < countof(data2)) {
                b &= data2[i];
            } else {
                b  = 0;
            }
            for (int j = 0; j < 8; j++) {
                Z_ASSERT_EQ(!!(b & (1 << j)), !!wah_get(&map1, i * 8 + j),
                            "invalid byte %d, bit %d", i, j);
            }
        }

        wah_wipe(&map1);
        wah_wipe(&map2);
    } Z_TEST_END;

    Z_TEST(redmine_4576, "") {
        const byte data[] = {
            0x1f, 0x00, 0x1f, 0x1f,
            0x00, 0x00, 0x00, 0x00,
            0x00, 0x00, 0x00, 0x00,
            0x00, 0x00, 0x00, 0x00,
            0x1f, 0x1f, 0x1f, 0x1f,
            0x00, 0x00, 0x00, 0x00,
            0x1f, 0x1f, 0x1f, 0x1f,
            0x00, 0x00, 0x00, 0x00
        };

        wah_init(&map);
        wah_add(&map, data, bitsizeof(data));

        for (int i = 0; i < countof(data); i++) {
            for (int j = 0; j < 8; j++) {
                Z_ASSERT_EQ(!!(data[i] & (1 << j)), !!wah_get(&map, i * 8 + j),
                            "invalid byte %d, bit %d", i, j);
            }
        }
        wah_wipe(&map);
    } Z_TEST_END;

    Z_TEST(redmine_9437, "") {
        const uint32_t data = 0xbfffffff;

        wah_init(&map);

        wah_add0s(&map, 626 * 32);
        wah_add1s(&map, 32);
        wah_add(&map, &data, 32);

        for (int i = 0; i < 626; i++) {
            for (int j = 0; j < 32; j++) {
                Z_ASSERT(!wah_get(&map, i * 32 + j));
            }
        }
        for (int i = 626 * 32; i < 628 * 32; i++) {
            if (i != 628 * 32 - 2) {
                Z_ASSERT(wah_get(&map, i));
            } else {
                Z_ASSERT(!wah_get(&map, i));
            }
        }
        wah_wipe(&map);
    } Z_TEST_END;

    Z_TEST(non_reg_and, "") {
        uint32_t src_data[] = { 0x00000519,      0x00000000,      0x80000101,      0x00000000 };
        uint32_t other_data[] = { 0x00000000,      0x00000002,      0x80000010,      0x00000003,
                                  0x0000001d,      0x00000001,      0x00007e00,      0x0000001e,
                                  0x00000000 };

        wah_t src;
        wah_t other;
        wah_t res;

        wah_init_from_data(&src, src_data, countof(src_data), true);
        src.pending = 0x1ffff;
        src.active  = 8241;
        src.len     = 50001;

        wah_init_from_data(&other, other_data, countof(other_data), true);
        other.pending = 0x600000;
        other.active  = 12;
        other.len     = 2007;

        wah_init(&res);
        wah_copy(&res, &src);
        wah_and(&res, &other);

        Z_ASSERT_EQ(res.len, 50001u);
        Z_ASSERT_LE(res.active, 12u);

        wah_wipe(&res);
    } Z_TEST_END;
} Z_GROUP_END;

/* }}} */
