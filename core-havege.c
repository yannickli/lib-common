/*
 *  HAVEGE: HArdware Volatile Entropy Gathering and Expansion
 *
 *  Copyright (C) 2006-2007  Christophe Devine
 *
 *  Redistribution and use in source and binary forms, with or without
 *  modification, are permitted provided that the following conditions
 *  are met:
 *
 *    * Redistributions of source code must retain the above copyright
 *      notice, this list of conditions and the following disclaimer.
 *    * Redistributions in binary form must reproduce the above copyright
 *      notice, this list of conditions and the following disclaimer in the
 *      documentation and/or other materials provided with the distribution.
 *    * Neither the name of XySSL nor the names of its contributors may be
 *      used to endorse or promote products derived from this software
 *      without specific prior written permission.
 *
 *  THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 *  "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 *  LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS
 *  FOR A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 *  OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 *  SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED
 *  TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 *  PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF
 *  LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING
 *  NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
 *  SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */
/*
 *  The HAVEGE RNG was designed by Andre Seznec in 2002.
 *
 *  http://www.irisa.fr/caps/projects/hipsor/publi.php
 *
 *  Contact: seznec(at)irisa_dot_fr - orocheco(at)irisa_dot_fr
 */

#include "hash.h"
#include "datetime.h"

#define COLLECT_SIZE  1024

/**
 * \brief          HAVEGE state structure
 */
typedef struct {
    int PT1, PT2, offset[2];
    int pool[COLLECT_SIZE];
    int WALK[8192];
} havege_state_t;

#ifdef __cplusplus
extern "C" {
#endif

/* ------------------------------------------------------------------------
 * On average, one iteration accesses two 8-word blocks in the havege WALK
 * table, and generates 16 words in the RES array.
 *
 * The data read in the WALK table is updated and permuted after each use.
 * The result of the hardware clock counter read is used  for this update.
 *
 * 25 conditional tests are present.  The conditional tests are grouped in
 * two nested  groups of 12 conditional tests and 1 test that controls the
 * permutation; on average, there should be 6 tests executed and 3 of them
 * should be mispredicted.
 * ------------------------------------------------------------------------
 */

#define TST1_ENTER if (PTEST & 1) { PTEST ^= 3; PTEST >>= 1;
#define TST2_ENTER if (PTEST & 1) { PTEST ^= 3; PTEST >>= 1;

#define TST1_LEAVE U1++; }
#define TST2_LEAVE U2++; }

#define ONE_ITERATION                                   \
                                                        \
    PTEST = PT1 >> 20;                                  \
                                                        \
    TST1_ENTER  TST1_ENTER  TST1_ENTER  TST1_ENTER      \
    TST1_ENTER  TST1_ENTER  TST1_ENTER  TST1_ENTER      \
    TST1_ENTER  TST1_ENTER  TST1_ENTER  TST1_ENTER      \
                                                        \
    TST1_LEAVE  TST1_LEAVE  TST1_LEAVE  TST1_LEAVE      \
    TST1_LEAVE  TST1_LEAVE  TST1_LEAVE  TST1_LEAVE      \
    TST1_LEAVE  TST1_LEAVE  TST1_LEAVE  TST1_LEAVE      \
                                                        \
    PTX = (PT1 >> 18) & 7;                              \
    PT1 &= 0x1FFF;                                      \
    PT2 &= 0x1FFF;                                      \
    CLK = (int) hardclock();                            \
                                                        \
    i = 0;                                              \
    A = &WALK[PT1    ]; RES[i++] ^= *A;                 \
    B = &WALK[PT2    ]; RES[i++] ^= *B;                 \
    C = &WALK[PT1 ^ 1]; RES[i++] ^= *C;                 \
    D = &WALK[PT2 ^ 4]; RES[i++] ^= *D;                 \
                                                        \
    IN = (*A >> (1)) ^ (*A << (31)) ^ CLK;              \
    *A = (*B >> (2)) ^ (*B << (30)) ^ CLK;              \
    *B = IN ^ U1;                                       \
    *C = (*C >> (3)) ^ (*C << (29)) ^ CLK;              \
    *D = (*D >> (4)) ^ (*D << (28)) ^ CLK;              \
                                                        \
    A = &WALK[PT1 ^ 2]; RES[i++] ^= *A;                 \
    B = &WALK[PT2 ^ 2]; RES[i++] ^= *B;                 \
    C = &WALK[PT1 ^ 3]; RES[i++] ^= *C;                 \
    D = &WALK[PT2 ^ 6]; RES[i++] ^= *D;                 \
                                                        \
    if (PTEST & 1) SWAP(int *, A, C);                  \
                                                        \
    IN = (*A >> (5)) ^ (*A << (27)) ^ CLK;              \
    *A = (*B >> (6)) ^ (*B << (26)) ^ CLK;              \
    *B = IN; CLK = (int) hardclock();                   \
    *C = (*C >> (7)) ^ (*C << (25)) ^ CLK;              \
    *D = (*D >> (8)) ^ (*D << (24)) ^ CLK;              \
                                                        \
    A = &WALK[PT1 ^ 4];                                 \
    B = &WALK[PT2 ^ 1];                                 \
                                                        \
    PTEST = PT2 >> 1;                                   \
                                                        \
    PT2 = (RES[(i - 8) ^ PTY] ^ WALK[PT2 ^ PTY ^ 7]);   \
    PT2 = ((PT2 & 0x1FFF) & (~8)) ^ ((PT1 ^ 8) & 0x8);  \
    PTY = (PT2 >> 10) & 7;                              \
                                                        \
    TST2_ENTER  TST2_ENTER  TST2_ENTER  TST2_ENTER      \
    TST2_ENTER  TST2_ENTER  TST2_ENTER  TST2_ENTER      \
    TST2_ENTER  TST2_ENTER  TST2_ENTER  TST2_ENTER      \
                                                        \
    TST2_LEAVE  TST2_LEAVE  TST2_LEAVE  TST2_LEAVE      \
    TST2_LEAVE  TST2_LEAVE  TST2_LEAVE  TST2_LEAVE      \
    TST2_LEAVE  TST2_LEAVE  TST2_LEAVE  TST2_LEAVE      \
                                                        \
    C = &WALK[PT1 ^ 5];                                 \
    D = &WALK[PT2 ^ 5];                                 \
                                                        \
    RES[i++] ^= *A;                                     \
    RES[i++] ^= *B;                                     \
    RES[i++] ^= *C;                                     \
    RES[i++] ^= *D;                                     \
                                                        \
    IN = (*A >> (9)) ^ (*A << (23)) ^ CLK;             \
    *A = (*B >> (10)) ^ (*B << (22)) ^ CLK;             \
    *B = IN ^ U2;                                       \
    *C = (*C >> (11)) ^ (*C << (21)) ^ CLK;             \
    *D = (*D >> (12)) ^ (*D << (20)) ^ CLK;             \
                                                        \
    A = &WALK[PT1 ^ 6]; RES[i++] ^= *A;                 \
    B = &WALK[PT2 ^ 3]; RES[i++] ^= *B;                 \
    C = &WALK[PT1 ^ 7]; RES[i++] ^= *C;                 \
    D = &WALK[PT2 ^ 7]; RES[i++] ^= *D;                 \
                                                        \
    IN = (*A >> (13)) ^ (*A << (19)) ^ CLK;             \
    *A = (*B >> (14)) ^ (*B << (18)) ^ CLK;             \
    *B = IN;                                            \
    *C = (*C >> (15)) ^ (*C << (17)) ^ CLK;             \
    *D = (*D >> (16)) ^ (*D << (16)) ^ CLK;             \
                                                        \
    PT1 = (RES[(i - 8) ^ PTX] ^                        \
            WALK[PT1 ^ PTX ^ 7]) & (~1);               \
    PT1 ^= (PT2 ^ 0x10) & 0x10;                         \
                                                        \
    for (n++, i = 0; i < 16; i++)                      \
        hs->pool[n % COLLECT_SIZE] ^= RES[i];

/*
 * Entropy gathering function
 */
static void havege_fill(havege_state_t *hs)
{
    int i, n = 0;
    int  U1,  U2, *A, *B, *C, *D;
    int PT1, PT2, *WALK, RES[16];
    int PTX, PTY, CLK, PTEST, IN;

    WALK = hs->WALK;
    PT1  = hs->PT1;
    PT2  = hs->PT2;

    PTX  = U1 = 0;
    PTY  = U2 = 0;

    memset(RES, 0, sizeof(RES));

    while (n < COLLECT_SIZE * 4)
    {
        ONE_ITERATION
        ONE_ITERATION
        ONE_ITERATION
        ONE_ITERATION
    }

    hs->PT1 = PT1;
    hs->PT2 = PT2;

    hs->offset[0] = 0;
    hs->offset[1] = COLLECT_SIZE / 2;
}

/*
 * HAVEGE initialization
 */
static void havege_init(havege_state_t *hs)
{
    p_clear(hs, 1);
    havege_fill(hs);
}

/*
 * HAVEGE rand function
 */
static int havege_rand(havege_state_t *hs)
{
    int ret;

    if (hs->offset[1] >= COLLECT_SIZE)
        havege_fill(hs);

    ret  = hs->pool[hs->offset[0]++];
    ret ^= hs->pool[hs->offset[1]++];

    return ret;
}

static havege_state_t hs_g;

void ha_srand(void)
{
    havege_init(&hs_g);
}

uint32_t ha_rand(void)
{
    return havege_rand(&hs_g);
}

int ha_rand_range(int first, int last)
{
    uint64_t res;

    res = (uint64_t)ha_rand() * ((uint64_t)last - first + 1);
    return first + (res >> 32);
}

double ha_rand_ranged(double first, double last)
{
    return first + (ha_rand() * (last - first + 1)) / (1ULL << 32);
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
void ha_uuid_generate_v4(ha_uuid_t uuid)
{
    ((uint32_t *)uuid)[0] = ha_rand();
    ((uint32_t *)uuid)[1] = ha_rand();
    ((uint32_t *)uuid)[2] = ha_rand();
    ((uint32_t *)uuid)[3] = ha_rand();

    uuid[6] = (uuid[6] & 0x0f) | 0x40;
    uuid[8] = (uuid[8] & 0x3f) | 0x80;
}

void __ha_uuid_fmt(char buf[static HA_UUID_HEX_LEN], ha_uuid_t uuid)
{
    char *p = buf;

    for (int i = 0; i < 16; i++) {
        if (i == 4 || i == 6 || i == 8 || i == 10)
            *p++ = '-';
        *p++ = __str_digits_lower[uuid[i] >> 4];
        *p++ = __str_digits_lower[uuid[i] & 15];
    }
}

void sb_add_uuid(sb_t *sb, ha_uuid_t uuid)
{
    __ha_uuid_fmt(sb_growlen(sb, HA_UUID_HEX_LEN), uuid);
}

#if defined(XYSSL_RAND_TEST)

int main(int argc, char *argv[])
{
    FILE *f;
    time_t t;
    int i, j, k;
    havege_state_t hs;
    byte buf[1024];

    if (argc < 2)
    {
        fprintf(stderr, "usage: %s <output filename>\n", argv[0]);
        return 1;
    }

    if ((f = fopen(argv[1], "wb+")) == NULL)
    {
        printf("failed to open '%s' for writing.\n", argv[0]);
        return 1;
    }

    havege_init(&hs);

    t = time(NULL);

    for (i = 0, k = 32768; i < k; i++)
    {
        for (j = 0; j < sizeof(buf); j++)
            buf[j] = havege_rand(&hs);

        fwrite(buf, sizeof(buf), 1, f);

        printf("Generating 32Mb of data in file '%s'... %04.1f" \
                "%% done\r", argv[1], (100 * (float) (i + 1)) / k);
        fflush(stdout);
    }

    if (t == time(NULL))
        t--;

    fclose(f);
    return 0;
}

#endif

/*{{{ Tests */

/* LCOV_EXCL_START */

#include <lib-common/z.h>

Z_GROUP_EXPORT(core_havege) {
    Z_TEST(havege_range, "havege_range") {
        int number1;
        int numbers[10000];
        bool is_different_than_int_min = false;

        /* Test bug that existed in ha_rand_rage when INT_MIN was given, the
         * results were always INT_MIN.
         */
        for (int i = 0; i < 10000; i++) {
            numbers[i] = ha_rand_range(INT_MIN, INT_MAX);
        }
        for (int i = 0; i < 10000; i++) {
            if (numbers[i] != INT_MIN) {
                is_different_than_int_min = true;
            }
        }
        Z_ASSERT(is_different_than_int_min);

        number1 = ha_rand_range(INT_MIN + 1, INT_MAX - 1);
        Z_ASSERT(number1 > INT_MIN && number1 < INT_MAX);
        number1 = ha_rand_range(-10, 10);
        Z_ASSERT(number1 >= -10 && number1 <= 10);

    } Z_TEST_END;
} Z_GROUP_END;

/* LCOV_EXCL_STOP */

/*}}} */
