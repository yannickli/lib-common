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

#include "lib.h"

//#define WAH_CHECK_NORMALIZED  1

wah_t *wah_init(wah_t *map)
{
    qv_init(wah_word, &map->data);
    wah_reset_map(map);
    return map;
}

void wah_wipe(wah_t *map)
{
    qv_wipe(wah_word, &map->data);
}

void wah_copy(wah_t *map, const wah_t *src)
{
    p_copy(map, src, 1);
    qv_init(wah_word, &map->data);
    qv_splice(wah_word, &map->data, 0, 0, src->data.tab, src->data.len);
}

wah_t *t_wah_new(int size)
{
    wah_t *map = t_new(wah_t, 1);
    wah_reset_map(map);
    t_qv_init(wah_word, &map->data, size);
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
        return &map->data.tab[map->last_run_pos + 1].count;
    }
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
void wah_check_normalized(wah_t *map)
{
    uint32_t pos;
    uint32_t prev_word = 0xcafebabe;

    assert (map->first_run_head.words == 0 || map->first_run_head.words >= 2
         || map->first_run_len == 0);
    if (map->first_run_head.words > 0) {
        prev_word = map->first_run_head.bit ? UINT32_MAX : 0;
    }

    for (pos = 0; pos < map->first_run_len; pos++) {
        if (prev_word == UINT32_MAX || prev_word == 0) {
            assert (prev_word != map->data.tab[pos].literal);
        }
        prev_word = map->data.tab[pos].literal;
    }

    while (pos < (uint32_t)map->data.len) {
        wah_header_t *head  = &map->data.tab[pos++].head;
        uint32_t      count = map->data.tab[pos++].count;

        assert (head->words >= 2 || pos == (uint32_t)map->data.len);
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
void wah_check_invariant(wah_t *map)
{
    if (map->last_run_pos < 0) {
        assert ((int)*wah_last_run_count(map) == map->data.len);
    } else {
        assert ((int)*wah_last_run_count(map) + map->last_run_pos + 2 == map->data.len);
    }
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

    if (map->last_run_pos > 0) {
        assert (map->data.len == map->last_run_pos + 2);
        map->data.len -= 2;
    } else {
        head->words = 0;
    }
    wah_append_literal(map, head->bit ? UINT32_MAX : 0);

    if (map->previous_run_pos < 0) {
        map->first_run_len++;
    } else {
        map->data.tab[map->previous_run_pos + 1].count++;
    }
    map->last_run_pos     = map->previous_run_pos;
    map->previous_run_pos = -2;
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
    wah_check_invariant(map);
    map->pending = 0;
}

void wah_add0s(wah_t *map, uint64_t count)
{
    uint32_t remain = map->len % WAH_BIT_IN_WORD;

    if (remain + count < WAH_BIT_IN_WORD) {
        map->len += count;
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
}

void wah_add1s(wah_t *map, uint64_t count)
{
    uint64_t remain = map->len % WAH_BIT_IN_WORD;

    if (remain + count < WAH_BIT_IN_WORD) {
        map->pending |= BITMASK_LT(uint32_t, count) << remain;
        map->len     += count;
        map->active  += count;
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
    wah_check_invariant(map);
}

void wah_add(wah_t *map, const void *data, uint64_t count)
{
    uint32_t remain = WAH_BIT_IN_WORD - (map->len % WAH_BIT_IN_WORD);
    if (remain != WAH_BIT_IN_WORD) {
        if (remain >= count || (remain % 8) != 0) {
            wah_add_unaligned(map, data, count);
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
        map->len    += count * WAH_BIT_IN_WORD;
        map->active += membitcount(words, count * sizeof(wah_word_t));
    }
}

#define PUSH_COPY(Run, Data)  wah_copy_run(map, &(Run), &(Data))


void wah_and(wah_t *map, const wah_t *other)
{
    t_scope;
    const wah_t *src = t_wah_dup(map);
    wah_word_enum_t src_en   = wah_word_enum_start(src);
    wah_word_enum_t other_en = wah_word_enum_start(other);

    wah_reset_map(map);
    while (src_en.state != WAH_ENUM_END || other_en.state != WAH_ENUM_END) {
        switch (src_en.state | (other_en.state << 2)) {
          case WAH_ENUM_END     | (WAH_ENUM_PENDING << 2):
          case WAH_ENUM_PENDING | (WAH_ENUM_END     << 2):
          case WAH_ENUM_PENDING | (WAH_ENUM_PENDING << 2):
            map->len += MAX(other->len % WAH_BIT_IN_WORD,
                            src->len % WAH_BIT_IN_WORD);
            map->pending = src_en.current & other_en.current;
            map->active += bitcount32(map->pending);
            wah_word_enum_next(&src_en);
            wah_word_enum_next(&other_en);
            break;

          case WAH_ENUM_RUN     | (WAH_ENUM_LITERAL << 2):
            if (src_en.current) {
                PUSH_COPY(src_en, other_en);
            } else {
                PUSH_0RUN(src_en.remain_words);
            }
            break;

          case WAH_ENUM_LITERAL | (WAH_ENUM_RUN     << 2):
            if (other_en.current) {
                PUSH_COPY(other_en, src_en);
            } else {
                PUSH_0RUN(other_en.remain_words);
            }
            break;

          case WAH_ENUM_RUN     | (WAH_ENUM_RUN     << 2):
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
    assert (map->active <= MIN(src->active, other->active));
}

void wah_or(wah_t *map, const wah_t *other)
{
    t_scope;
    const wah_t *src = t_wah_dup(map);
    wah_word_enum_t src_en   = wah_word_enum_start(src);
    wah_word_enum_t other_en = wah_word_enum_start(other);

    wah_reset_map(map);
    while (src_en.state != WAH_ENUM_END || other_en.state != WAH_ENUM_END) {
        switch (src_en.state | (other_en.state << 2)) {
          case WAH_ENUM_END     | (WAH_ENUM_PENDING << 2):
          case WAH_ENUM_PENDING | (WAH_ENUM_END     << 2):
          case WAH_ENUM_PENDING | (WAH_ENUM_PENDING << 2):
            map->len += MAX(other->len % WAH_BIT_IN_WORD,
                            src->len % WAH_BIT_IN_WORD);
            map->pending = src_en.current | other_en.current;
            map->active += bitcount32(map->pending);
            wah_word_enum_next(&src_en);
            wah_word_enum_next(&other_en);
            break;

          case WAH_ENUM_RUN     | (WAH_ENUM_LITERAL << 2):
            if (!src_en.current) {
                PUSH_COPY(src_en, other_en);
            } else {
                PUSH_1RUN(src_en.remain_words);
            }
            break;

          case WAH_ENUM_LITERAL | (WAH_ENUM_RUN     << 2):
            if (!other_en.current) {
                PUSH_COPY(other_en, src_en);
            } else {
                PUSH_1RUN(other_en.remain_words);
            }
            break;

          case WAH_ENUM_RUN     | (WAH_ENUM_RUN     << 2):
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
    if ((map->len % WAH_BIT_IN_WORD) != 0) {
        map->pending = ~map->pending & BITMASK_LT(uint32_t, map->len);
    }
    wah_check_invariant(map);
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
    uint64_t pos = wah_debug_print_run(0, wah->first_run_head);
    uint32_t len = wah->first_run_len;
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
