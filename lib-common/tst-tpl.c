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

#include "all.h"

int main(int argc, const char **argv)
{
    tpl_t *tpl = tpl_new();
    tpl_add_cstr(tpl, "asdalskdjalskdjalskdjasldkjasdfoo");
    tpl_add_cstr(tpl, "foo");
    tpl_add_cstr(tpl, "foo");
    tpl_add_cstr(tpl, "foo");
    tpl_copy_cstr(tpl, "foo");
    tpl_copy_cstr(tpl, "foo");
    tpl_copy_cstr(tpl, "foo");
    tpl_copy_cstr(tpl, "foo");
    tpl_dump(0, tpl);
    return 0;
}
