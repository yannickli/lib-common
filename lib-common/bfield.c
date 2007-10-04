/**************************************************************************/
/*                                                                        */
/*  Copyright (C) 2004-2007 INTERSEC SAS                                  */
/*                                                                        */
/*  Should you receive a copy of this source code, you must check you     */
/*  have a proper, written authorization of INTERSEC to hold it. If you   */
/*  don't have such an authorization, you must DELETE all source code     */
/*  files in your possession, and inform INTERSEC of the fact you obtain  */
/*  these files. Should you not comply to these terms, you can be         */
/*  prosecuted in the extent permitted by applicable law.                 */
/*                                                                        */
/**************************************************************************/

#include <ctype.h>
#include <string.h>
#include "bfield.h"
#include "blob.h"

static void bfield_optimize(bfield_t *bf)
{
    ssize_t i;

    for (i = 0; i < bf->bits.len && !bf->bits.data[i]; i++);
    blob_kill_first(&bf->bits, i);
    bf->offs += i;
    for (i = bf->bits.len; i > 0 && !bf->bits.data[i - i]; i--);
    blob_kill_last(&bf->bits, bf->bits.len - i);
}

void bfield_set(bfield_t *bf, ssize_t pos)
{
    ssize_t octet = pos >> 3;

    if (!bf->bits.len) {
        bf->offs = octet;
    }
    if (octet < bf->offs) {
        ssize_t oldlen = bf->bits.len;
        blob_resize(&bf->bits, bf->bits.len + bf->offs - octet);
        p_move(bf->bits.data, bf->offs - octet, 0, oldlen);
        p_clear(bf->bits.data, bf->offs - octet);
        bf->offs = octet;
    }
    if (octet >= bf->offs + bf->bits.len) {
        blob_extend2(&bf->bits, octet + 1 - bf->offs - bf->bits.len, 0);
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

bool bfield_isset(bfield_t *bf, ssize_t pos)
{
    ssize_t octet = (pos >> 3) - bf->offs;

    if (octet < 0 || octet >= bf->bits.len)
        return 0;

    return (bf->bits.data[octet] >> (pos & 7)) & 1;
}

/*[ CHECK ]::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::{{{*/
#ifdef CHECK
/* {{{*/
#include <check.h>

START_TEST(check_bfield)
{
    int num;
    bfield_t bf;
    blob_t *b = &bf.bits;

    bfield_init(&bf);

    bfield_set(&bf, 0);
    bfield_set(&bf, 2);
    bfield_set(&bf, 4);
    bfield_set(&bf, 7);

    fail_if(b->len != 1, "bfield_set failed");
    fail_if(b->data[0] != 0x95, "bfield_set failed");

    fail_if(!bfield_isset(&bf, 0),
            "bfield_isset failed");
    fail_if(!bfield_isset(&bf, 2),
            "bfield_isset failed");
    fail_if(!bfield_isset(&bf, 4),
            "bfield_isset failed");
    fail_if(!bfield_isset(&bf, 7),
            "bfield_isset failed");

    bfield_set(&bf, 3);
    fail_if(!bfield_isset(&bf, 3),
            "bfield_set failed");
    bfield_set(&bf, 30);
    fail_if(!bfield_isset(&bf, 30),
            "bfield_set failed");
    bfield_set(&bf, 70);
    fail_if(!bfield_isset(&bf, 70),
            "bfield_set failed");

    /* 10011101 -> 9D */
    fail_if(b->data[0] != 0x9D,
            "bfield_set failed");

    num = 43987;
    /* (num / 8) * 8 = 43984 */
    bfield_set(&bf, 43984);
    bfield_set(&bf, 43985);
    bfield_set(&bf, 43986);

    fail_if(!bfield_isset(&bf, 43984),
            "bfield_isset failed");
    fail_if(!bfield_isset(&bf, 43985),
            "bfield_isset failed");
    fail_if(!bfield_isset(&bf, 43986),
            "bfield_isset failed");

    bfield_set(&bf, num);
    fail_if(!bfield_isset(&bf, num),
            "bfield_set failed");

    bfield_unset(&bf, num);
    fail_if(bfield_isset(&bf, num),
            "bfield_unset failed");

    bfield_wipe(&bf);
}
END_TEST

Suite *check_bfield_suite(void)
{
    Suite *s  = suite_create("bfield");
    TCase *tc = tcase_create("Core");

    suite_add_tcase(s, tc);
    tcase_add_test(tc, check_bfield);
    return s;
}
#endif
