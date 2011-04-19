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

    plwah64_reset(&map);
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


