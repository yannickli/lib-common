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

#include "core.h"

int main(void)
{
    char in[PATH_MAX];

    *in = '\0';
    assert (path_simplify(in) < 0);

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

    pstrcpy(in, sizeof(in), "a/..");
    path_simplify(in);
    assert (strequal(in, "."));

    pstrcpy(in, sizeof(in), "a/../../..");
    path_simplify(in);
    assert (strequal(in, "../.."));

    pstrcpy(in, sizeof(in), "a/../../b/../c");
    path_simplify(in);
    assert (strequal(in, "../c"));

    return 0;
}
