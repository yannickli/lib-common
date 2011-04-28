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

#include "plwah64.h"

#define PLWAH64_TEST(name)  TEST_DECL("plwah64: " name, 0)

PLWAH64_TEST("fill")
{
    plwah64_map_t map = PLWAH64_MAP_INIT;

    STATIC_ASSERT(sizeof(plwah64_t) == sizeof(uint64_t));
    STATIC_ASSERT(sizeof(plwah64_fill_t) == sizeof(uint64_t));
    STATIC_ASSERT(sizeof(plwah64_literal_t) == sizeof(uint64_t));

    plwah64_add0s(&map, 63);
    TEST_FAIL_IF(map.bits.len != 1, "bad bitmap length: %d", map.bits.len);
    for (int i = 0; i < 2 * 63; i++) {
        if (plwah64_get(&map, i)) {
            TEST_FAIL_IF(true, "bad bit at %d", i);
        }
    }
    plwah64_add0s(&map, 3 * 63);
    TEST_FAIL_IF(map.bits.len != 1, "bad bitmap length: %d", map.bits.len);
    for (int i = 0; i < 5 * 63; i++) {
        if (plwah64_get(&map, i)) {
            TEST_FAIL_IF(true, "bad bit at %d", i);
        }
    }

    plwah64_trim(&map);
    TEST_FAIL_IF(map.bits.len != 0, "bad bitmap length: %d", map.bits.len);

    plwah64_reset_map(&map);
    plwah64_add1s(&map, 63);
    TEST_FAIL_IF(map.bits.len != 1, "bad bitmap length: %d", map.bits.len);
    for (int i = 0; i < 2 * 63; i++) {
        bool bit = plwah64_get(&map, i);
        if ((i < 63 && !bit) || (i >= 63 && bit)) {
            TEST_FAIL_IF(true, "bad bit at %d", i);
        }
    }
    plwah64_add1s(&map, 3 * 63);
    TEST_FAIL_IF(map.bits.len != 1, "bad bitmap length: %d", map.bits.len);
    for (int i = 0; i < 5 * 63; i++) {
        bool bit = plwah64_get(&map, i);
        if ((i < 4 * 63 && !bit) || (i >= 4 * 63 && bit)) {
            TEST_FAIL_IF(true, "bad bit at %d", i);
        }
    }

    plwah64_wipe(&map);
    TEST_DONE();
}

PLWAH64_TEST("set and reset")
{
    plwah64_map_t map = PLWAH64_MAP_INIT;

    plwah64_set(&map, 135);
    TEST_FAIL_IF(plwah64_bit_count(&map) != 1, "invalid bit count: %d",
                 (int)plwah64_bit_count(&map));
    TEST_FAIL_IF(!plwah64_get(&map, 135), "bad bit");

    plwah64_set(&map, 136);
    TEST_FAIL_IF(plwah64_bit_count(&map) != 2, "invalid bit count: %d",
                 (int)plwah64_bit_count(&map));
    TEST_FAIL_IF(!plwah64_get(&map, 135), "bad bit");
    TEST_FAIL_IF(!plwah64_get(&map, 136), "bad bit");

    plwah64_set(&map, 134);
    TEST_FAIL_IF(plwah64_bit_count(&map) != 3, "invalid bit count: %d",
                 (int)plwah64_bit_count(&map));
    TEST_FAIL_IF(!plwah64_get(&map, 134), "bad bit");
    TEST_FAIL_IF(!plwah64_get(&map, 135), "bad bit");
    TEST_FAIL_IF(!plwah64_get(&map, 136), "bad bit");

    plwah64_reset(&map, 135);
    TEST_FAIL_IF(plwah64_bit_count(&map) != 2, "invalid bit count: %d",
                 (int)plwah64_bit_count(&map));
    TEST_FAIL_IF(!plwah64_get(&map, 134), "bad bit");
    TEST_FAIL_IF(plwah64_get(&map, 135), "bad bit");
    TEST_FAIL_IF(!plwah64_get(&map, 136), "bad bit");
    plwah64_wipe(&map);
    TEST_DONE();
}

PLWAH64_TEST("set bitmap")
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
    plwah64_map_t map = PLWAH64_MAP_INIT;
    plwah64_add(&map, data, bitsizeof(data));
    bc = membitcount(data, sizeof(data));

    TEST_FAIL_IF(plwah64_bit_count(&map) != bc,
                 "invalid bit count: %d, expected %d",
                 (int)plwah64_bit_count(&map), (int)bc);
    for (int i = 0; i < countof(data); i++) {
#define CHECK_BIT(p)  (!!(data[i] & (1 << p)) == !!plwah64_get(&map, i * 8 + p))
        if (!CHECK_BIT(0) || !CHECK_BIT(1) || !CHECK_BIT(2) || !CHECK_BIT(3)
        ||  !CHECK_BIT(4) || !CHECK_BIT(5) || !CHECK_BIT(6) || !CHECK_BIT(7)) {
            TEST_FAIL_IF(true, "invalid byte %d", i);
        }
#undef CHECK_BIT
    }

    plwah64_not(&map);
    TEST_FAIL_IF(plwah64_bit_count(&map) != bitsizeof(data) - bc,
                 "invalid bit count: %d, expected %d",
                 (int)plwah64_bit_count(&map), (int)(bitsizeof(data) - bc));
    for (int i = 0; i < countof(data); i++) {
#define CHECK_BIT(p)  (!!(data[i] & (1 << p)) != !!plwah64_get(&map, i * 8 + p))
        if (!CHECK_BIT(0) || !CHECK_BIT(1) || !CHECK_BIT(2) || !CHECK_BIT(3)
        ||  !CHECK_BIT(4) || !CHECK_BIT(5) || !CHECK_BIT(6) || !CHECK_BIT(7)) {
            TEST_FAIL_IF(true, "invalid byte %d", i);
        }
#undef CHECK_BIT
    }

    plwah64_wipe(&map);
    TEST_DONE();
}

PLWAH64_TEST("for_each")
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
    uint64_t      nbc;
    plwah64_map_t map = PLWAH64_MAP_INIT;
    uint64_t      c;
    uint64_t      previous;
    plwah64_add(&map, data, bitsizeof(data));
    bc  = membitcount(data, sizeof(data));
    nbc = bitsizeof(data) - bc;

    TEST_FAIL_IF(plwah64_bit_count(&map) != bc,
                 "invalid bit count: %d, expected %d",
                 (int)plwah64_bit_count(&map), (int)bc);

    c = 0;
    previous = 0;
    plwah64_for_each_1(en, &map) {
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
    plwah64_for_each_0(en, &map) {
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

    plwah64_wipe(&map);
    TEST_DONE();
}


PLWAH64_TEST("binop")
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

    plwah64_map_t map1 = PLWAH64_MAP_INIT;
    plwah64_map_t map2 = PLWAH64_MAP_INIT;

    plwah64_add(&map1, data1, bitsizeof(data1));
    plwah64_add(&map2, data2, bitsizeof(data2));
    plwah64_and(&map1, &map2);
    for (int i = 0; i < countof(data1); i++) {
        byte b = data1[i];
        if (i < countof(data2)) {
            b &= data2[i];
        } else {
            b = 0;
        }
#define CHECK_BIT(p)  (!!(b & (1 << p)) == !!plwah64_get(&map1, i * 8 + p))
        if (!CHECK_BIT(0) || !CHECK_BIT(1) || !CHECK_BIT(2) || !CHECK_BIT(3)
        ||  !CHECK_BIT(4) || !CHECK_BIT(5) || !CHECK_BIT(6) || !CHECK_BIT(7)) {
            TEST_FAIL_IF(true, "invalid byte %d", i);
        }
#undef CHECK_BIT
    }

    plwah64_reset_map(&map1);
    plwah64_add(&map1, data1, bitsizeof(data1));
    plwah64_or(&map1, &map2);
    for (int i = 0; i < countof(data1); i++) {
        byte b = data1[i];
        if (i < countof(data2)) {
            b |= data2[i];
        }
#define CHECK_BIT(p)  (!!(b & (1 << p)) == !!plwah64_get(&map1, i * 8 + p))
        if (!CHECK_BIT(0) || !CHECK_BIT(1) || !CHECK_BIT(2) || !CHECK_BIT(3)
        ||  !CHECK_BIT(4) || !CHECK_BIT(5) || !CHECK_BIT(6) || !CHECK_BIT(7)) {
            TEST_FAIL_IF(true, "invalid byte %d", i);
        }
#undef CHECK_BIT
    }

    plwah64_wipe(&map1);
    plwah64_wipe(&map2);
    TEST_DONE();
}
