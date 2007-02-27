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
#include <lib-common/mem.h>
#include "bfield.h"

CONVERSION_FUNCTIONS(bfield_t, blob_t);

bfield_t *bfield_init(bfield_t *blob)
{
    return blob_t_to_bfield_t(blob_init(bfield_t_to_blob_t(blob)));
}

void bfield_mark(bfield_t *blob, int pos, bool val)
{
    blob_t *b = bfield_t_to_blob_t(blob);
    int idx = pos / 8;
    unsigned int bidx = pos & 0x7;

    if (idx >= b->len) {
        int oldlen = b->len;

        blob_extend(b, idx - b->len + 1);
        p_clear(b->data + oldlen, b->len - oldlen);
    }
    if (val) {
        b->data[idx] |= 0x1 << bidx;
    } else {
        b->data[idx] &= !(0x1 << bidx);
    }
}

bool bfield_isset(bfield_t *blob, int pos)
{
    blob_t *b = bfield_t_to_blob_t(blob);
    int idx = pos / 8;
    unsigned int bidx = pos & 0x7;
    
    if (idx >= b->len) {
        return false;
    }
    return (blob->data[idx] & (0x1 << bidx));
}

void bfield_reset(bfield_t *blob)
{
    blob_reset(bfield_t_to_blob_t(blob));
}
void bfield_wipe(bfield_t *blob)
{
    blob_wipe(bfield_t_to_blob_t(blob));
}

/*[ CHECK ]::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::{{{*/
#ifdef CHECK
/* {{{*/
#include <check.h>

START_TEST(check_bfield)
{
    int i;
    bfield_t bf;
    blob_t *b = bfield_t_to_blob_t(&bf);

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

    /* 10011101 -> 9D */
    fail_if(b->data[0] != 0x9D,
            "bfield_set failed");

    i = 43987;
    /* (i / 8) * 8 = 43984 */
    bfield_set(&bf, 43984);
    bfield_set(&bf, 43985);
    bfield_set(&bf, 43986);

    fail_if(!bfield_isset(&bf, 43984),
            "bfield_isset failed");
    fail_if(!bfield_isset(&bf, 43985),
            "bfield_isset failed");
    fail_if(!bfield_isset(&bf, 43986),
            "bfield_isset failed");

    bfield_set(&bf, i);
    fail_if(!bfield_isset(&bf, i),
            "bfield_set failed");

    bfield_unset(&bf, i);
    fail_if(bfield_isset(&bf, i),
            "bfield_unset failed");

    bfield_reset(&bf);
    fail_if(bfield_isset(&bf, i),
            "bfield_reset failed");
    fail_if(bfield_isset(&bf, 4),
            "bfield_reset failed");
    fail_if(bfield_isset(&bf, 0),
            "bfield_reset failed");

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
