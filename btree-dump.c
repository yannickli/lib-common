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

#include "btree.h"

int main(int argc, char **argv)
{
    btree_t *bt;

    if (argc > 1) {
        const char *indexname = argv[1];

        bt = btree_open(indexname, O_RDONLY, false);
        if (!bt) {
            fprintf(stderr, "%s: cannot open %s: %m\n",
                    "btree-dump", indexname);
            return 1;
        }
        btree_dump(bt, fprintf, stdout);
        btree_close(&bt);
    }
    return 0;
}
