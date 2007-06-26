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

int main(int argc, char **argv)
{
    btree_t *bt;

    if (argc > 1) {
        bt = btree_open(argv[1], O_RDONLY | BT_O_NOCHECK);
        assert (bt);

        btree_dump(bt, fprintf, stdout);

        btree_close(&bt);
    }
    return 0;
}
