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

static int identity(tpl_t *out, const tpl_t *in)
{
    tpl_add_tpl(out, in);
    return 0;
}

int main(int argc, const char **argv)
{
    tpl_t *tpl, *fun, *res, *var;

    var = tpl_new();
    tpl_add_cstr(var, "var");
    tpl_dump(0, var, "var");

    tpl = tpl_new();
    tpl_add_cstr(tpl, "asdalskdjalskdjalskdjasldkjasdfoo");
    tpl_add_cstr(tpl, "foo");
    tpl_add_cstr(tpl, "foo");
    tpl_add_cstr(tpl, "foo");
    tpl_add_var(tpl, 0, 0);
    fun = tpl_add_apply(tpl, TPL_OP_APPLY_PURE, &identity);
    tpl_add_var(fun, 0, 0);
    tpl_copy_cstr(fun, "foo");
    tpl_copy_cstr(fun, "foo");
    tpl_copy_cstr(tpl, "foo");
    tpl_copy_cstr(tpl, "foo");
    tpl_add_tpl(tpl, var);
    tpl_dump(0, tpl, "source");

    res = tpl_subst(tpl, 1, NULL, 0);
    tpl_dump(0, res, "subst");
    tpl_delete(&res);

    res = tpl_subst(tpl, 0, (const tpl_t *[]){ var }, 1);
    tpl_dump(0, res, "subst");
    tpl_delete(&res);

    tpl_delete(&tpl);
    return 0;
}
