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

static void key_format(FILE *out, const byte *k, int n)
{
    if (n == 0) {
        fprintf(out, "--->");
        return;
    }

    if (n != 8) {
        fprintf(out, "@#()*!@#^");
    } else {
        union {
            long long v;
            char s[8];
        } cast;

        memcpy(cast.s, k, n);
        fprintf(out, "%04llx", cast.v);
    }
}

int main(void)
{
    int64_t n = 0;
    int64_t max = n + 100000;

    test_initialize();

    for (; n < max; n++) {
        int32_t d;

        for (d = 0; d < 1024 * 256; d += 1024) {
            btree_push(bt, (void*)&n, 8, (void*)&d, 4);
        }
        // fprintf(stdout, "%c[1J%c[0;0f", 27, 27);
        // btree_dump(stdout, bt, &key_format);
        // fprintf(stdout, "last insert: %llx\n", (long long)n);
        // getc(stdin);
    }

    test_shutdown();
    return 0;
}
