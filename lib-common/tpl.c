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

#include "string_is.h"
#include "tpl.h"

static tpl_t *tpl_new_op(tpl_op op)
{
    tpl_t *n = p_new(tpl_t, 1);
    n->refcnt = 1;
    switch (op) {
      case TPL_OP_BLOB:
        blob_init(&n->u.blob);
        break;
      default:
        break;
    }
    tpl_array_init(&n->blocks);
    return n;
}

tpl_t *tpl_new(void)
{
    return tpl_new_op(TPL_OP_BLOCK);
}

tpl_t *tpl_dup(const tpl_t *tpl)
{
    tpl_t *res = (tpl_t *)tpl;

    /* tpl can be NULL (the NULL template is a real value) */
    if (res) {
        res->refcnt++;
    }
    return res;
}

static void tpl_wipe(tpl_t *n)
{
    switch (n->op) {
      case TPL_OP_DATA:
        tpl_data_wipe(&n->u.data);
        break;
      case TPL_OP_BLOB:
        blob_wipe(&n->u.blob);
        break;
      default:
        break;
    }
    tpl_array_wipe(&n->blocks);
}
void tpl_delete(tpl_t **tpl)
{
    if (*tpl) {
        if (--(*tpl)->refcnt > 0) {
            *tpl = NULL;
        } else {
            tpl_wipe(*tpl);
            p_delete(tpl);
        }
    }
}

/****************************************************************************/
/* Build the AST                                                            */
/****************************************************************************/

#define tpl_can_append(t)  \
        (  ((t)->op & TPL_OP_BLOCK)                                     \
        && ((t)->op != TPL_OP_IFDEF || (t)->blocks.len < 2)             \
        && ((t)->op != TPL_OP_APPLY_DELAYED || (t)->blocks.len < 2)     \
        )

void tpl_add_data(tpl_t *tpl, const byte *data, int len)
{
    struct tpl_data *td;
    tpl_t *buf;

    assert (tpl_can_append(tpl));

    if (!tpl->blocks.len || tpl->blocks.tab[tpl->blocks.len - 1]->op != TPL_OP_DATA) {
        tpl_array_append(&tpl->blocks, buf = tpl_new_op(TPL_OP_DATA));
    } else {
        buf = tpl->blocks.tab[tpl->blocks.len - 1];
    }
    td = &buf->u.data;
    p_allocgrow(&td->iov, td->n + 1, &td->sz);
    td->iov[td->n++] = (struct iovec){
        .iov_base = (void *)data,
        .iov_len  = len,
    };
}

void tpl_copy_data(tpl_t *tpl, const byte *data, int len)
{
    tpl_t *buf;

    assert (tpl_can_append(tpl));

    buf = tpl->blocks.len > 0 ? tpl->blocks.tab[tpl->blocks.len - 1] : NULL;
    if (buf->op != TPL_OP_BLOB || buf->refcnt > 1) {
        tpl_array_append(&tpl->blocks, buf = tpl_new_op(TPL_OP_BLOB));
    }
    blob_append_data(&buf->u.blob, data, len);
}

void tpl_add_var(tpl_t *tpl, uint16_t array, uint16_t index)
{
    tpl_t *var;

    assert (tpl_can_append(tpl));
    tpl_array_append(&tpl->blocks, var = tpl_new_op(TPL_OP_VAR));
    var->u.varidx = ((uint32_t)array << 16) | index;
}

void tpl_add_tpl(tpl_t *out, const tpl_t *tpl)
{
    assert (tpl_can_append(tpl));
    tpl_array_append(&out->blocks, tpl_dup(tpl));
}

tpl_t *tpl_add_ifdef(tpl_t *tpl, uint16_t array, uint16_t index)
{
    tpl_t *var;

    assert (tpl_can_append(tpl));
    tpl_array_append(&tpl->blocks, var = tpl_new_op(TPL_OP_IFDEF));
    var->u.varidx = ((uint32_t)array << 16) | index;
    return var;
}

tpl_t *tpl_add_apply(tpl_t *tpl, tpl_op op, tpl_apply_f *f)
{
    tpl_t *app;

    assert (tpl_can_append(tpl));
    tpl_array_append(&tpl->blocks, app = tpl_new_op(op));
    app->u.f = f;
    return app;
}

static char const pad[] = "| | | | | | | | | | | | | | | | | | | | | | | | ";

static void tpl_dump2(int dbg, const tpl_t *tpl, int lvl)
{
    switch (tpl->op) {
      case TPL_OP_DATA:
        e_trace(dbg, "%*s DATA %d vectors (embeds %zd tpls)", 2 * lvl, pad,
                tpl->u.data.n, tpl->blocks.len);
        return;

      case TPL_OP_BLOB:
        e_trace(dbg, "%*s BLOB %zd bytes", 2 * lvl, pad, tpl->u.blob.len);
        return;

      case TPL_OP_VAR:
        e_trace(dbg, "%*s VAR  q=%02x, v=%02x", 2 * lvl, pad,
                tpl->u.varidx >> 16, tpl->u.varidx & 0xffff);
        return;

      case TPL_OP_BLOCK:
        e_trace(dbg, "%*s + BLOC %zd tpls", 2 * lvl, pad, tpl->blocks.len);
        for (int i = 0; i < tpl->blocks.len; i++) {
            tpl_dump2(dbg, tpl->blocks.tab[i], lvl + 1);
        }
        break;

      case TPL_OP_IFDEF:
        e_trace(dbg, "%*s + DEF? q=%02x, v=%02x", 2 * lvl, pad,
                tpl->u.varidx >> 16, tpl->u.varidx & 0xffff);
        if (tpl->blocks.len > 0) {
            if (tpl->blocks.tab[0]) {
                e_trace(dbg, "%*s NULL", 2 + 2 * lvl, pad);
            } else {
                tpl_dump2(dbg, tpl->blocks.tab[0], lvl + 1);
            }
        }
        if (tpl->blocks.len > 1) {
            if (tpl->blocks.tab[1]) {
                e_trace(dbg, "%*s NULL", 2 + 2 * lvl, pad);
            } else {
                tpl_dump2(dbg, tpl->blocks.tab[1], lvl + 1);
            }
        }
        break;

      case TPL_OP_APPLY_DELAYED:
        e_trace(dbg, "%*s + DELA %p", 2 * lvl, pad, tpl->u.f);
        if (tpl->blocks.len > 0) {
            tpl_dump2(dbg, tpl->blocks.tab[0], lvl + 1);
        }
        if (tpl->blocks.len > 1) {
            tpl_dump2(dbg, tpl->blocks.tab[1], lvl + 1);
        }
        break;

      case TPL_OP_APPLY:
      case TPL_OP_APPLY_PURE:
      case TPL_OP_APPLY_PURE_ASSOC:
        e_trace(dbg, "%*s + FUNC %zd tpls", 2 * lvl, pad, tpl->blocks.len);
        for (int i = 0; i < tpl->blocks.len; i++) {
            tpl_dump2(dbg, tpl->blocks.tab[i], lvl + 1);
        }
        break;
    }
}

void tpl_dump(int dbg, const tpl_t *tpl)
{
    tpl_dump2(dbg, tpl, 0);
}

/****************************************************************************/
/* Substitution and optimization                                            */
/****************************************************************************/

enum tplcode {
    TPL_ERR   = -1,
    TPL_CONST = 0,
    TPL_VAR   = 1,
};

static int tpl_into_iovec(struct tpl_data *td, const tpl_t *tpl)
{
    if (!tpl)
        return 0;
    for (int i = 0; i < tpl->blocks.len; i++) {
        tpl_t *tmp = tpl->blocks.tab[i];
        struct tpl_data *td2 = &tmp->u.data;

        switch (tmp->op) {
          case TPL_OP_DATA:
            p_allocgrow(&td->iov, td->n + td2->n, &td->sz);
            memcpy(td->iov + td->n, td2->iov, td2->n * sizeof(struct iovec));
            td->n += td2->n;
            break;

          case TPL_OP_BLOB:
            p_allocgrow(&td->iov, td->n + 1, &td->sz);
            td->iov[td->n++] = (struct iovec){
                .iov_base = (void *)tmp->u.blob.data,
                .iov_len  = tmp->u.blob.len,
            };
            break;

          case TPL_OP_BLOCK:
            if (tpl_into_iovec(td, tmp) < 0)
                return -1;
            break;

          default:
            return -1;
        }
    }
    return 0;
}

#define getvar(id, vals, nb) \
    ((((id) & 0xffff) >= (uint16_t)(nb)) ? NULL : (vals)[(id) & 0xffff])

static enum tplcode
tpl_combine(tpl_t *, const tpl_t *, uint16_t, const tpl_t **, int);

static int tpl_apply_fun(tpl_apply_f *f, tpl_t *out, const tpl_t *tpl)
{
    struct tpl_data td = { .iov = NULL };
    if (tpl_into_iovec(&td, tpl) < 0 || (*tpl->u.f)(out, &td) < 0) {
        tpl_data_wipe(&td);
        return -1;
    }
    tpl_data_wipe(&td);
    return 0;
}

static enum tplcode tpl_combine_block(tpl_t *out, const tpl_t *tpl, uint16_t envid,
                                      const tpl_t **vals, int nb)
{
    enum tplcode res = TPL_CONST;

    assert (tpl->op & TPL_OP_BLOCK);
    for (int i = 0; i < tpl->blocks.len; i++) {
        res |= tpl_combine(out, tpl->blocks.tab[i], envid, vals, nb);
    }
    return res;
}

static enum tplcode tpl_combine(tpl_t *out, const tpl_t *tpl, uint16_t envid,
                                const tpl_t **vals, int nb)
{
    const tpl_t *ctmp, *ctmp2;
    tpl_t *tmp, *tmp2;

    switch (tpl->op) {
        enum tplcode res;

      case TPL_OP_DATA:
      case TPL_OP_BLOB:
        tpl_add_tpl(out, tpl);
        return TPL_CONST;

      case TPL_OP_VAR:
        if (tpl->u.varidx >> 16 == envid) {
            ctmp = getvar(tpl->u.varidx, vals, nb);
            if (!ctmp)
                return TPL_ERR;
            return tpl_combine(out, ctmp, envid, vals, nb);
        }
        tpl_add_tpl(out, tpl);
        out->no_subst = false;
        return TPL_VAR;

      case TPL_OP_BLOCK:
        if (tpl->no_subst) {
            for (int i = 0; i < tpl->blocks.len; i++) {
                tpl_add_tpl(out, tpl->blocks.tab[i]);
            }
            return TPL_CONST;
        }
        return tpl_combine_block(out, tpl, envid, vals, nb);

      case TPL_OP_IFDEF:
        if (tpl->u.varidx >> 16 == envid) {
            int branch = getvar(tpl->u.varidx, vals, nb) != NULL;
            tpl = tpl->blocks.len > branch ? tpl->blocks.tab[branch] : NULL;
            return tpl ? tpl_combine(out, tpl, envid, vals, nb) : TPL_CONST;
        }
        out->no_subst = false;
        if (tpl->no_subst) {
            tpl_add_tpl(out, tpl);
            return TPL_VAR;
        }
        tpl_array_append(&out->blocks, tmp = tpl_new_op(TPL_OP_IFDEF));
        tmp->no_subst = true;
        for (int i = 0; i < tpl->blocks.len; i++) {
            if (tpl->blocks.tab[i]->no_subst) {
                tpl_add_tpl(tmp, tpl->blocks.tab[i]);
            } else {
                tmp2 = tpl_subst(tpl->blocks.tab[i], envid, vals, nb);
                if (!tmp2)
                    return TPL_ERR;
                tmp->no_subst &= tmp2->no_subst;
                tpl_array_append(&tmp->blocks, tmp2);
            }
        }
        return TPL_VAR;

      case TPL_OP_APPLY:
        if (tpl->no_subst) {
            tpl_add_tpl(out, tpl);
            return TPL_VAR;
        }
        tpl_array_append(&out->blocks, tmp = tpl_new_op(TPL_OP_APPLY));
        tmp->u.f = tpl->u.f;
        return tpl_combine_block(tmp, tpl, envid, vals, nb);

      case TPL_OP_APPLY_DELAYED:
        switch (tpl->blocks.len) {
          case 0:
            if (tpl_apply_fun(tpl->u.f, out, NULL) < 0)
                return TPL_ERR;
            return TPL_CONST;

          case 1:
            if (tpl_apply_fun(tpl->u.f, out, NULL) < 0)
                return TPL_ERR;
            if (tpl->blocks.tab[0]->no_subst) {
                tpl_add_tpl(out, tpl->blocks.tab[0]);
                return TPL_CONST;
            }
            return tpl_combine(out, tpl->blocks.tab[0], envid, vals, nb);

          case 2:
            ctmp  = tpl->blocks.tab[0];
            ctmp2 = tpl->blocks.tab[1];
            if (ctmp->no_subst && ctmp2->no_subst) {
                /* cannot optimize, because of some TPL_OP_APPLY */
                tpl_add_tpl(out, tpl);
                return TPL_VAR;
            }
            if (!ctmp2->no_subst) {
                res = tpl_combine(tmp2 = tpl_new(), ctmp2, envid, vals, nb);
                if (res == TPL_CONST) {
                    if (tpl_apply_fun(tpl->u.f, out, tmp2) < 0) {
                        res = TPL_ERR;
                    }
                    res |= tpl_combine(out, ctmp, envid, vals, nb);
                    res |= tpl_combine(out, tmp2, envid, vals, nb);
                    tpl_delete(&tmp2);
                    return res;
                }
            } else {
                tmp2 = tpl_dup(ctmp2);
                res  = TPL_VAR;
            }
            tpl_array_append(&out->blocks, tmp = tpl_new_op(TPL_OP_APPLY_DELAYED));
            if (ctmp->no_subst) {
                tpl_add_tpl(tmp, ctmp);
                tmp->no_subst = ctmp->no_subst && tmp2->no_subst;
            } else {
                tpl_t *tmp3;
                tpl_array_append(&tmp->blocks, tmp3 = tpl_new());
                res |= tpl_combine(tmp3, ctmp, envid, vals, nb);
                tmp->no_subst = tmp3->no_subst && tmp2->no_subst;
            }
            tpl_array_append(&tmp->blocks, tmp2);
            out->no_subst &= tmp->no_subst;
            return res;
          default:
            return TPL_ERR;
        }

      case TPL_OP_APPLY_PURE:
      case TPL_OP_APPLY_PURE_ASSOC:
        res = tpl_combine_block(tmp = tpl_new(), tpl, envid, vals, nb);
        if (res == TPL_CONST) {
            if (tpl_apply_fun(tpl->u.f, out, tmp) < 0) {
                res = TPL_ERR;
            }
            tpl_delete(&tmp);
        } else {
            tmp->op  = tpl->op;
            tmp->u.f = tpl->u.f;
            out->no_subst = false;
            tpl_array_append(&out->blocks, tmp);
        }
        return res;
    }

    return TPL_ERR;
}

tpl_t *tpl_subst(const tpl_t *tpl, uint16_t envid, const tpl_t **vals, int nb)
{
    tpl_t *out;

    if (tpl->no_subst)
        return tpl_dup(tpl);
    out = tpl_new();
    out->no_subst = true;
    if (tpl_combine_block(out = tpl_new(), tpl, envid, vals, nb) < 0)
        tpl_delete(&out);
    return out;
}
