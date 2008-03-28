/**************************************************************************/
/*                                                                        */
/*  Copyright (C) 2004-2008 INTERSEC SAS                                  */
/*                                                                        */
/*  Should you receive a copy of this source code, you must check you     */
/*  have a proper, written authorization of INTERSEC to hold it. If you   */
/*  don't have such an authorization, you must DELETE all source code     */
/*  files in your possession, and inform INTERSEC of the fact you obtain  */
/*  these files. Should you not comply to these terms, you can be         */
/*  prosecuted in the extent permitted by applicable law.                 */
/*                                                                        */
/**************************************************************************/

#include "str-path.h"

int main(void)
{
    char out[PATH_MAX], in[PATH_MAX];

    pstrcpy(in, sizeof(in), "/../test//foo///");
    path_simplify(in);
    assert (strequal(in, "/test/foo"));

    pstrcpy(in, sizeof(in), "/test/..///foo/./");
    path_simplify(in);
    assert (strequal(in, "/foo"));

    pstrcpy(in, sizeof(in), "./test/bar");
    path_simplify(in);
    assert (strequal(in, "test/bar"));

    pstrcpy(in, sizeof(in), "./test/../bar");
    path_simplify(in);
    assert (strequal(in, "bar"));

    pstrcpy(in, sizeof(in), "./../test");
    path_simplify(in);
    assert (strequal(in, "../test"));

    pstrcpy(in, sizeof(in), ".//test");
    path_simplify(in);
    assert (strequal(in, "test"));

    return 0;
}
