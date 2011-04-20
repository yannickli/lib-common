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

    qv_wipe(plwah64, &map.bits);
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
    TEST_DONE();
}

PLWAH64_TEST("set bitmap")
{
    const byte data[] = {
        0x1f, 0x00, 0x00, 0x8c,
        0xff, 0xff, 0xff, 0xff,
        0xff, 0xff, 0xff, 0xff,
        0xff, 0xff, 0xff, 0x80,
        0x00, 0x10, 0x40, 0x00,
        0x00, 0x00, 0x00, 0x00,
        0x00, 0x00, 0x00, 0x00,
        0x00, 0x00, 0x00, 0x00,
        0x00, 0x00, 0x00, 0x21
    };
#if 0
    byte res[2 * countof(data)];
#endif

    plwah64_map_t map = PLWAH64_MAP_INIT;
    plwah64_add(&map, data, bitsizeof(data));

    TEST_FAIL_IF(plwah64_bit_count(&map) != membitcount(data, sizeof(data)),
                 "invalid bit count: %d, expected %d",
                 (int)plwah64_bit_count(&map),
                 (int)membitcount(data, sizeof(data)));
    for (int i = 0; i < countof(data); i++) {
#define CHECK_BIT(p)  (!!(data[i] & (1 << p)) == !!plwah64_get(&map, i * 8 + p))
        if (!CHECK_BIT(0) || !CHECK_BIT(1) || !CHECK_BIT(2) || !CHECK_BIT(3)
        ||  !CHECK_BIT(4) || !CHECK_BIT(5) || !CHECK_BIT(6) || !CHECK_BIT(7)) {
            TEST_FAIL_IF(true, "invalid byte %d", i);
        }
#undef CHECK_BIT
    }
    TEST_DONE();
}
