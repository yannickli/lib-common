/**************************************************************************/
/*                                                                        */
/*  Copyright (C) INTERSEC SA                                             */
/*                                                                        */
/*  Should you receive a copy of this source code, you must check you     */
/*  have a proper, written authorization of INTERSEC to hold it. If you   */
/*  don't have such an authorization, you must DELETE all source code     */
/*  files in your possession, and inform INTERSEC of the fact you obtain  */
/*  these files. Should you not comply to these terms, you can be         */
/*  prosecuted in the extent permitted by applicable law.                 */
/*                                                                        */
/**************************************************************************/

#include "bfield.h"

static void bfield_optimize(bfield_t *bf)
{
    ssize_t i;

    for (i = 0; i < bf->bits.len && !bf->bits.data[i]; i++);
    sb_skip(&bf->bits, i);
    bf->offs += i;
    for (i = bf->bits.len; i > 0 && !bf->bits.data[i - i]; i--);
    sb_shrink(&bf->bits, bf->bits.len - i);
}

void bfield_set(bfield_t *bf, ssize_t pos)
{
    ssize_t octet = pos >> 3;

    if (!bf->bits.len) {
        bf->offs = octet;
    }
    if (octet < bf->offs) {
        sb_splice0s(&bf->bits, 0, 0, bf->offs - octet);
        bf->offs = octet;
    }
    if (octet >= bf->offs + bf->bits.len) {
        sb_add0s(&bf->bits, octet + 1 - bf->offs - bf->bits.len);
    }
    bf->bits.data[octet - bf->offs] |= 1 << (pos & 7);
}

void bfield_unset(bfield_t *bf, ssize_t pos)
{
    ssize_t octet = (pos >> 3) - bf->offs;

    if (octet < 0 || octet >= bf->bits.len)
        return;

    bf->bits.data[octet] &= ~(1 << (pos & 7));
    if (!octet || octet == bf->bits.len - 1)
        bfield_optimize(bf);
}

bool bfield_isset(const bfield_t *bf, ssize_t pos)
{
    ssize_t octet = (pos >> 3) - bf->offs;

    if (octet < 0 || octet >= bf->bits.len)
        return false;

    return (bf->bits.data[octet] >> (pos & 7)) & 1;
}

int bfield_count(const bfield_t *bf)
{
    int count = 0;
    for (int i = 0; i < bf->bits.len; i++) {
        count += __builtin_popcount(bf->bits.data[i]);
    }
    return count;
}


TEST_DECL("bfield: testing bfield_set", 0)
{
    bfield_t bf;
    sb_t *b = &bf.bits;

    bfield_init(&bf);

    bfield_set(&bf, 0);
    bfield_set(&bf, 2);
    bfield_set(&bf, 4);
    bfield_set(&bf, 7);

    TEST_FAIL_IF(b->len != 1,           "bfield_set");
    TEST_FAIL_IF(b->data[0] != 0x95,    "bfield_set");

    TEST_FAIL_IF(!bfield_isset(&bf, 0), "bfield_isset");
    TEST_FAIL_IF(bfield_isset(&bf, 1),  "bfield_isset");
    TEST_FAIL_IF(!bfield_isset(&bf, 2), "bfield_isset");
    TEST_FAIL_IF(bfield_isset(&bf, 3),  "bfield_isset");
    TEST_FAIL_IF(!bfield_isset(&bf, 4), "bfield_isset");
    TEST_FAIL_IF(bfield_isset(&bf, 5),  "bfield_isset");
    TEST_FAIL_IF(bfield_isset(&bf, 6),  "bfield_isset");
    TEST_FAIL_IF(!bfield_isset(&bf, 7), "bfield_isset");
    TEST_FAIL_IF(bfield_isset(&bf, 8),  "bfield_isset");
    TEST_FAIL_IF(bfield_isset(&bf, 40), "bfield_isset");

    bfield_wipe(&bf);
    TEST_DONE();
}

TEST_DECL("bfield: testing bfield_set", 0)
{
    bfield_t bf;
    int num;

    bfield_init(&bf);
    bfield_set(&bf, 3);
    TEST_FAIL_IF(!bfield_isset(&bf, 3),  "bfield_set");
    bfield_set(&bf, 30);
    TEST_FAIL_IF(!bfield_isset(&bf, 30), "bfield_set");
    bfield_set(&bf, 70);
    TEST_FAIL_IF(!bfield_isset(&bf, 70), "bfield_set");


    num = 43987;
    /* (num / 8) * 8 = 43984 */
    bfield_set(&bf, 43984);
    bfield_set(&bf, 43985);
    bfield_set(&bf, 43986);

    TEST_FAIL_IF(!bfield_isset(&bf, 43984), "bfield_isset");
    TEST_FAIL_IF(!bfield_isset(&bf, 43985), "bfield_isset");
    TEST_FAIL_IF(!bfield_isset(&bf, 43986), "bfield_isset");

    bfield_set(&bf, num);
    TEST_FAIL_IF(!bfield_isset(&bf, num), "bfield_set");

    bfield_unset(&bf, num);
    TEST_FAIL_IF(bfield_isset(&bf, num),  "bfield_unset");

    bfield_wipe(&bf);
    TEST_DONE();
}
