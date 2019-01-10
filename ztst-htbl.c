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

#include "container.h"
#include "container-htbl.h"

cstring_htbl h;

static void print_one(const char **s) {
    printf("%s\n", *s);
}

int main(void) {
    cstring_htbl_init(&h);
    cstring_htbl_insert2(&h, "test");
    cstring_htbl_insert2(&h, "toto");
    cstring_htbl_insert2(&h, "tutu");
    cstring_htbl_take2(&h, "toto");
    HTBL_STR_MAP(&h, print_one);
    cstring_htbl_wipe(&h);
    return 0;
}
