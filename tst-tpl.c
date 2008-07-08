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

static int identity(tpl_t *out, blob_t *b, tpl_t **arr, int nb)
{
    if (out) {
        tpl_add_tpls(out, arr, nb);
        return 0;
    }
    while (--nb >= 0) {
        tpl_t *in = *arr++;
        if (in->op == TPL_OP_BLOB) {
            blob_append(b, &in->u.blob);
        } else {
            assert (in->op == TPL_OP_DATA);
            blob_append_data(b, in->u.data.data, in->u.data.len);
        }
    }
    return 0;
}

int main(int argc, const char **argv)
{
    tpl_t *tpl, *fun, *res, *var;
    blob_t blob, b2;

    e_trace(0, "sizeof(tpl_t) = %zd", sizeof(tpl_t));
    blob_init(&blob);
    blob_init(&b2);
    blob_extend2(&blob, 4096, ' ');

    var = tpl_new();
    tpl_add_cstr(var, "var");
    tpl_add_data(var, blob.data, blob.len);
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

    res = tpl_dup(tpl);
    tpl_subst(&res, 1, NULL, 0, true);
    tpl_dump(0, res, "subst");
    tpl_delete(&res);

    res = tpl_dup(tpl);
    tpl_subst(&res, 0, &var, 1, TPL_LASTSUBST | TPL_KEEPVAR);
    tpl_dump(0, res, "subst");
    tpl_optimize(res);
    tpl_dump(0, res, "subst (opt)");
    tpl_delete(&res);

    if (tpl_fold(&b2, &tpl, 0, &var, 1, TPL_LASTSUBST)) {
        e_panic("fold failed");
    }
    e_trace(0, "b2 size: %d", b2.len);

    blob_wipe(&blob);
    blob_wipe(&b2);
    return 0;
}
