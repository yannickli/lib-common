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

#include <stdio.h>

#include "btree.h"

/**************************************************************************/
/* Test module init functions                                             */
/**************************************************************************/

static btree_t *bt = NULL;

static void test_initialize(void)
{
    const char *path = "/tmp/test.ibt";

    if (bt)
        btree_close(&bt);

    unlink(path);
    bt = btree_creat(path);
    if (!bt)
        e_panic("Cannot create %s", path);
}

static void test_shutdown(void)
{
    btree_close(&bt);
}


/**************************************************************************/
/* helpers                                                                */
/**************************************************************************/

int main(void)
{
    int64_t num_keys = 5000000;
    int64_t num_data = 4;
    int64_t n, start = 0;
    int64_t max = start + num_keys;
    int32_t d;

    test_initialize();

    for (n = start; n < max; n++) {
        for (d = 0; d < 1024 * num_data; d += 1024) {
            btree_push(bt, n, (void*)&d, 4);
        }
    }

    test_shutdown();
    return 0;
}
