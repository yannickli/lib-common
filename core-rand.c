/***************************************************************************/
/*                                                                         */
/* Copyright 2019 INTERSEC SA                                              */
/*                                                                         */
/* Licensed under the Apache License, Version 2.0 (the "License");         */
/* you may not use this file except in compliance with the License.        */
/* You may obtain a copy of the License at                                 */
/*                                                                         */
/*     http://www.apache.org/licenses/LICENSE-2.0                          */
/*                                                                         */
/* Unless required by applicable law or agreed to in writing, software     */
/* distributed under the License is distributed on an "AS IS" BASIS,       */
/* WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.*/
/* See the License for the specific language governing permissions and     */
/* limitations under the License.                                          */
/*                                                                         */
/***************************************************************************/

#include <pthread.h>
#include "core.h"

static __thread struct {
    unsigned int seed;
    bool initialized;
} thr_rand_g;

int is_rand(void)
{
    if (unlikely(!thr_rand_g.initialized)) {
        struct timeval tm;

        gettimeofday(&tm, NULL);
        thr_rand_g.seed = tm.tv_sec + tm.tv_usec + getpid()
                        + (uintptr_t)pthread_self();
        thr_rand_g.initialized = true;
    }

    return rand_r(&thr_rand_g.seed);
}

uint32_t rand32(void)
{
    STATIC_ASSERT(RAND_MAX >= INT_MAX);

    return ((uint32_t)rand())
         | (((uint32_t)rand()) << 31);
}

uint64_t rand64(void)
{
    return ((uint64_t)rand())
         | (((uint64_t)rand()) << 31)
         | (((uint64_t)rand()) << 62);
}

int64_t rand_range(int64_t first, int64_t last)
{
    const uint64_t range = last - first;
    uint64_t number;

    if (range <= RAND_MAX - 1) {
        number = rand();
    } else
    if (range < (1ull << 62) - 1) {
        number = ((uint64_t)rand())
               | (((uint64_t)rand()) << 31);
    } else {
        number = rand64();
    }

    if (range == UINT64_MAX) {
        return number;
    }

    return first + (number % (range + 1));
}

double rand_ranged(double first, double last)
{
    return first + (last - first) * (rand() / (double)RAND_MAX);
}

/*
 RFC 4122
      0                   1                   2                   3
    0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1
   +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
   |                          time_low                             |
   +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
   |       time_mid                |         time_hi_and_version   |
   +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
   |clk_seq_hi_res |  clk_seq_low  |         node (0-1)            |
   +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
   |                         node (2-5)                            |
   +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+

 §4.4:
   +--------+--------+--------+--------+
   |........|........|........|........|
   |........|........|0100....|........|
   |10......|........|........|........|
   |........|........|........|........|
   +--------+--------+--------+--------+

  IOW gives a uuid under the form:
    xxxxxxxx-xxxx-4xxx-[89ab]xxx-xxxxxxxxxxxx
*/
void rand_generate_uuid_v4(uuid_t uuid)
{
    ((uint32_t *)uuid)[0] = rand32();
    ((uint32_t *)uuid)[1] = rand32();
    ((uint32_t *)uuid)[2] = rand32();
    ((uint32_t *)uuid)[3] = rand32();

    uuid[6] = (uuid[6] & 0x0f) | 0x40;
    uuid[8] = (uuid[8] & 0x3f) | 0x80;
}

void __uuid_fmt(char buf[static UUID_HEX_LEN], uuid_t uuid)
{
    char *p = buf;

    for (int i = 0; i < 16; i++) {
        if (i == 4 || i == 6 || i == 8 || i == 10) {
            *p++ = '-';
        }
        *p++ = __str_digits_lower[uuid[i] >> 4];
        *p++ = __str_digits_lower[uuid[i] & 15];
    }
}

void sb_add_uuid(sb_t *sb, uuid_t uuid)
{
    __uuid_fmt(sb_growlen(sb, UUID_HEX_LEN), uuid);
}

/*{{{ Tests */

/* LCOV_EXCL_START */

#include <lib-common/z.h>

Z_GROUP_EXPORT(core_havege) {
    Z_TEST(havege_range, "havege_range") {
        int number1;
        int numbers[10000];
        bool is_different_than_int_min = false;

        /* Test bug that existed in rand_range when INT_MIN was given, the
         * results were always INT_MIN.
         */
        for (int i = 0; i < 10000; i++) {
            numbers[i] = rand_range(INT_MIN, INT_MAX);
        }
        for (int i = 0; i < 10000; i++) {
            if (numbers[i] != INT_MIN) {
                is_different_than_int_min = true;
            }
        }
        Z_ASSERT(is_different_than_int_min);

        number1 = rand_range(INT_MIN + 1, INT_MAX - 1);
        Z_ASSERT(number1 > INT_MIN && number1 < INT_MAX);
        number1 = rand_range(-10, 10);
        Z_ASSERT(number1 >= -10 && number1 <= 10);
    } Z_TEST_END;
} Z_GROUP_END;

/* LCOV_EXCL_STOP */

/*}}} */
