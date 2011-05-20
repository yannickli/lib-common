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
    if (count == 0) {
        return;
    }
    if (remain > 0) {
        remain = 32 - remain;
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
        remain = 32 - remain;
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

void wah_add(wah_t *map, const void *data, uint64_t count)
{
    const uint8_t *src = data;
    while (count > 0) {
        uint64_t word;
        int      bits = 0;
        bool     on_0 = true;

        if (count > 64) {
            word = get_unaligned_le64(src);
            bits = 64;
            src += 8;
        } else
        if (count > 32) {
            word = get_unaligned_le32(src);
            bits = 32;
            src += 4;
        } else {
            word = *src;
            src++;
            if (count > 8) {
                bits = 8;
            } else {
                bits = count;
            }
        }
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
    while (src_en.state != WAH_ENUM_END || other_en.state != WAH_ENUM_END) {
        map->pending = wah_word_enum_current(&src_en)
                     & wah_word_enum_current(&other_en);
        switch (src_en.state | (other_en.state << 16)) {
          case WAH_ENUM_END | (WAH_ENUM_PENDING << 16):
          case WAH_ENUM_PENDING | (WAH_ENUM_END << 16):
          case WAH_ENUM_PENDING | (WAH_ENUM_PENDING << 16):
            map->len += MAX(other->len % WAH_BIT_IN_WORD,
                            src->len % WAH_BIT_IN_WORD);
            map->active += bitcount32(map->pending);
            break;

          default:
            map->len    += WAH_BIT_IN_WORD;
            map->active += bitcount32(map->pending);
            wah_push_pending(map, 1);
            break;
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
    while (src_en.state != WAH_ENUM_END || other_en.state != WAH_ENUM_END) {
        map->pending = wah_word_enum_current(&src_en)
                     | wah_word_enum_current(&other_en);
        switch (src_en.state | (other_en.state << 16)) {
          case WAH_ENUM_END | (WAH_ENUM_PENDING << 16):
          case WAH_ENUM_PENDING | (WAH_ENUM_END << 16):
          case WAH_ENUM_PENDING | (WAH_ENUM_PENDING << 16):
            map->len += MAX(other->len % WAH_BIT_IN_WORD,
                            src->len % WAH_BIT_IN_WORD);
            map->active += bitcount32(map->pending);
            break;

          default:
            map->len    += WAH_BIT_IN_WORD;
            map->active += bitcount32(map->pending);
            wah_push_pending(map, 1);
            break;
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
    if ((map->len % WAH_BIT_IN_WORD) != 0) {
        uint32_t mask = UINT32_MAX >> (32 - (map->len % WAH_BIT_IN_WORD));
        map->pending  = ~map->pending;
        map->pending &= mask;
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


/* Tests {{{ */
#define WAH_TEST(name)  TEST_DECL("wah: " name, 0)

WAH_TEST("simple")
{
    wah_t map;
    wah_init(&map);
    wah_add0s(&map, 3);
    for (int i = 0; i < 3; i++) {
        if (wah_get(&map, i)) {
            TEST_FAIL_IF(true, "bad bit at offset %d", i);
        }
    }
    if (wah_get(&map, 3)) {
        TEST_FAIL_IF(true, "bad bit at offset 3");
    }

    wah_not(&map);
    for (int i = 0; i < 3; i++) {
        if (!wah_get(&map, i)) {
            TEST_FAIL_IF(true, "bad bit at offset %d", i);
        }
    }
    if (wah_get(&map, 3)) {
        TEST_FAIL_IF(true, "bad bit at offset 3");
    }
    wah_wipe(&map);
    TEST_DONE();
}

WAH_TEST("fill")
{
    wah_t map;
    wah_init(&map);

    STATIC_ASSERT(sizeof(wah_word_t) == sizeof(uint32_t));
    STATIC_ASSERT(sizeof(wah_header_t) == sizeof(uint32_t));

    wah_add0s(&map, 63);
    for (int i = 0; i < 2 * 63; i++) {
        if (wah_get(&map, i)) {
            TEST_FAIL_IF(true, "bad bit at %d", i);
        }
    }

    wah_add0s(&map, 3 * 63);
    for (int i = 0; i < 5 * 63; i++) {
        if (wah_get(&map, i)) {
            TEST_FAIL_IF(true, "bad bit at %d", i);
        }
    }

    wah_reset_map(&map);
    wah_add1s(&map, 63);
    for (int i = 0; i < 2 * 63; i++) {
        bool bit = wah_get(&map, i);
        if ((i < 63 && !bit) || (i >= 63 && bit)) {
            TEST_FAIL_IF(true, "bad bit at %d", i);
        }
    }
    wah_add1s(&map, 3 * 63);
    for (int i = 0; i < 5 * 63; i++) {
        bool bit = wah_get(&map, i);
        if ((i < 4 * 63 && !bit) || (i >= 4 * 63 && bit)) {
            TEST_FAIL_IF(true, "bad bit at %d", i);
        }
    }

    wah_wipe(&map);
    TEST_DONE();
}

WAH_TEST("set bitmap")
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
    wah_t map;
    wah_init(&map);
    wah_add(&map, data, bitsizeof(data));
    bc = membitcount(data, sizeof(data));

    TEST_FAIL_IF(map.active != bc,
                 "invalid bit count: %d, expected %d",
                 (int)map.active, (int)bc);
    for (int i = 0; i < countof(data); i++) {
#define CHECK_BIT(p)  (!!(data[i] & (1 << p)) == !!wah_get(&map, i * 8 + p))
        if (!CHECK_BIT(0) || !CHECK_BIT(1) || !CHECK_BIT(2) || !CHECK_BIT(3)
        ||  !CHECK_BIT(4) || !CHECK_BIT(5) || !CHECK_BIT(6) || !CHECK_BIT(7)) {
            TEST_FAIL_IF(true, "invalid byte %d", i);
        }
#undef CHECK_BIT
    }

    wah_not(&map);
    TEST_FAIL_IF(map.active != bitsizeof(data) - bc,
                 "invalid bit count: %d, expected %d",
                 (int)map.active, (int)(bitsizeof(data) - bc));
    for (int i = 0; i < countof(data); i++) {
#define CHECK_BIT(p)  (!!(data[i] & (1 << p)) != !!wah_get(&map, i * 8 + p))
        if (!CHECK_BIT(0) || !CHECK_BIT(1) || !CHECK_BIT(2) || !CHECK_BIT(3)
        ||  !CHECK_BIT(4) || !CHECK_BIT(5) || !CHECK_BIT(6) || !CHECK_BIT(7)) {
            TEST_FAIL_IF(true, "invalid byte %d", i);
        }
#undef CHECK_BIT
    }

    wah_wipe(&map);
    TEST_DONE();
}

WAH_TEST("for_each")
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
        0x00, 0x00, 0x00, 0x21, /* 280, 285                  (288)*/
        0x12, 0x00, 0x10,       /* 289, 292, 308 */
    };

    uint64_t bc;
    uint64_t nbc;
    wah_t    map;
    uint64_t c;
    uint64_t previous;
    wah_init(&map);
    wah_add(&map, data, bitsizeof(data));
    bc  = membitcount(data, sizeof(data));
    nbc = bitsizeof(data) - bc;

    TEST_FAIL_IF(map.active != bc,
                 "invalid bit count: %d, expected %d",
                 (int)map.active, (int)bc);

    c = 0;
    previous = 0;
    wah_for_each_1(en, &map) {
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
    wah_for_each_0(en, &map) {
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

#if 0
    for (int i = 0; i < (int)bitsizeof(data) + 100; i++) {
        wah_path_t path = wah_find(&map, i);
        if (!wah_check_path(&map, &path)) {
            TEST_FAIL_IF(true, "invalid path for bit %d", i);
        }
    }
    {
        wah_path_t last = wah_last(&map);
        TEST_FAIL_IF(!wah_check_path(&map, &last),
                     "bad pos for last %d", (int)last.bit_in_map);
    }
#endif

    wah_wipe(&map);
    TEST_DONE();
}

WAH_TEST("binop")
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

    wah_t map1;
    wah_t map2;
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
#define CHECK_BIT(p)  (!!(b & (1 << p)) == !!wah_get(&map1, i * 8 + p))
        if (!CHECK_BIT(0) || !CHECK_BIT(1) || !CHECK_BIT(2) || !CHECK_BIT(3)
        ||  !CHECK_BIT(4) || !CHECK_BIT(5) || !CHECK_BIT(6) || !CHECK_BIT(7)) {
            TEST_FAIL_IF(true, "invalid byte %d", i);
        }
#undef CHECK_BIT
    }

    wah_reset_map(&map1);
    wah_add(&map1, data1, bitsizeof(data1));
    wah_or(&map1, &map2);
    for (int i = 0; i < countof(data1); i++) {
        byte b = data1[i];
        if (i < countof(data2)) {
            b |= data2[i];
        }
#define CHECK_BIT(p)  (!!(b & (1 << p)) == !!wah_get(&map1, i * 8 + p))
        if (!CHECK_BIT(0) || !CHECK_BIT(1) || !CHECK_BIT(2) || !CHECK_BIT(3)
        ||  !CHECK_BIT(4) || !CHECK_BIT(5) || !CHECK_BIT(6) || !CHECK_BIT(7)) {
            TEST_FAIL_IF(true, "invalid byte %d", i);
        }
#undef CHECK_BIT
    }

    wah_wipe(&map1);
    wah_wipe(&map2);
    TEST_DONE();
}

/* }}} */
