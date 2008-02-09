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
    n->op     = op;
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
    tpl_t *buf;

    assert (tpl_can_append(tpl));

    if (len <= TPL_COPY_LIMIT_HARD) {
        tpl_copy_data(tpl, data, len);
        return;
    }

    if (tpl->blocks.len > 0 && tpl->blocks.tab[tpl->blocks.len - 1]->refcnt == 1) {
        buf = tpl->blocks.tab[tpl->blocks.len - 1];
        if (buf->op == TPL_OP_BLOB && len <= TPL_COPY_LIMIT_SOFT) {
            blob_append_data(&buf->u.blob, data, len);
            return;
        }
    }
    tpl_array_append(&tpl->blocks, buf = tpl_new_op(TPL_OP_DATA));
    buf->u.data = (struct tpl_data){ .data = data, .len = len };
}

void tpl_copy_data(tpl_t *tpl, const byte *data, int len)
{
    tpl_t *buf;

    assert (tpl_can_append(tpl));

    buf = tpl->blocks.len > 0 ? tpl->blocks.tab[tpl->blocks.len - 1] : NULL;
    if (!buf || buf->op != TPL_OP_BLOB || buf->refcnt > 1) {
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
    assert (tpl_can_append(out));
    if (tpl->op == TPL_OP_BLOB && out->blocks.len > 0
    &&  tpl->u.blob.len <= TPL_COPY_LIMIT_SOFT)
    {
        tpl_t *buf = out->blocks.tab[out->blocks.len - 1];
        if (buf->op == TPL_OP_BLOB && buf->refcnt == 1) {
            blob_append(&buf->u.blob, &tpl->u.blob);
            return;
        }
    }
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
#define HAS_SUBST(tpl) \
    ((tpl->op & TPL_OP_BLOCK && !tpl->no_subst) || tpl->op == TPL_OP_VAR)
#define TRACE(fmt, c, ...) \
    e_trace(dbg, "%.*s%c%c "fmt, 1 + 2 * lvl, pad, c, \
            HAS_SUBST(tpl) ? '*' : ' ', ##__VA_ARGS__)
#define TRACE_NULL() \
    e_trace(dbg, "%*s NULL", 3 + 2 * lvl, pad)


    switch (tpl->op) {
      case TPL_OP_DATA:
        TRACE("DATA %zd bytes", ' ', tpl->u.data.len);
        return;

      case TPL_OP_BLOB:
        TRACE("BLOB %zd bytes", ' ', tpl->u.blob.len);
        return;

      case TPL_OP_VAR:
        TRACE("VAR  q=%02x, v=%02x", ' ', tpl->u.varidx >> 16,
              tpl->u.varidx & 0xffff);
        return;

      case TPL_OP_BLOCK:
        TRACE("BLOC %zd tpls", '\\', tpl->blocks.len);
        for (int i = 0; i < tpl->blocks.len; i++) {
            tpl_dump2(dbg, tpl->blocks.tab[i], lvl + 1);
        }
        break;

      case TPL_OP_IFDEF:
        TRACE("DEF? q=%02x, v=%02x", '\\', tpl->u.varidx >> 16,
              tpl->u.varidx & 0xffff);
        if (tpl->blocks.len <= 0 || !tpl->blocks.tab[0]) {
            TRACE_NULL();
        } else {
            tpl_dump2(dbg, tpl->blocks.tab[0], lvl + 1);
        }
        if (tpl->blocks.len <= 1 || !tpl->blocks.tab[1]) {
            TRACE_NULL();
        } else {
            tpl_dump2(dbg, tpl->blocks.tab[1], lvl + 1);
        }
        break;

      case TPL_OP_APPLY_DELAYED:
        TRACE("DELA %p", '\\', tpl->u.f);
        if (tpl->blocks.len <= 0 || !tpl->blocks.tab[0]) {
            TRACE_NULL();
        } else {
            tpl_dump2(dbg, tpl->blocks.tab[0], lvl + 1);
        }
        if (tpl->blocks.len > 1) {
            tpl_dump2(dbg, tpl->blocks.tab[1], lvl + 1);
        }
        break;

      case TPL_OP_APPLY:
      case TPL_OP_APPLY_PURE:
      case TPL_OP_APPLY_PURE_ASSOC:
        TRACE("FUNC %zd tpls", '\\', tpl->blocks.len);
        for (int i = 0; i < tpl->blocks.len; i++) {
            tpl_dump2(dbg, tpl->blocks.tab[i], lvl + 1);
        }
        break;
    }
}

void tpl_dump(int dbg, const tpl_t *tpl, const char *s)
{
    e_trace(dbg, " ,--[ %s ]--", s);
    if (tpl) {
        tpl_dump2(dbg, tpl, 0);
    } else {
        e_trace(dbg, " | NULL");
    }
    e_trace(dbg, " '-----------------");
}

/****************************************************************************/
/* Substitution and optimization                                            */
/****************************************************************************/

enum tplcode {
    TPL_ERR   = -1,
    TPL_CONST = 0,
    TPL_VAR   = 1,
};

#define getvar(id, vals, nb) \
    (((vals) && ((id) & 0xffff) < (uint16_t)(nb)) ? (vals)[(id) & 0xffff] : NULL)

static enum tplcode
tpl_combine(tpl_t *, const tpl_t *, uint16_t, const tpl_t **, int);

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
            if (tpl->blocks.len <= branch)
                return TPL_CONST;
            return tpl_combine(out, tpl->blocks.tab[branch], envid, vals, nb);
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
        tmp->no_subst = true;
        tmp->u.f = tpl->u.f;
        return tpl_combine_block(tmp, tpl, envid, vals, nb);

      case TPL_OP_APPLY_DELAYED:
        switch (tpl->blocks.len) {
          case 0:
            if ((*tpl->u.f)(out, NULL) < 0)
                return TPL_ERR;
            return TPL_CONST;

          case 1:
            if ((*tpl->u.f)(out, NULL) < 0)
                return TPL_ERR;
            if (tpl->blocks.tab[0]->no_subst) {
                tpl_add_tpl(out, tpl->blocks.tab[0]);
                return TPL_CONST;
            }
            return tpl_combine(out, tpl->blocks.tab[0], envid, vals, nb);

          case 2:
            ctmp  = tpl->blocks.tab[0];
            ctmp2 = tpl->blocks.tab[1];
            if ((!ctmp || ctmp->no_subst) && ctmp2->no_subst) {
                /* cannot optimize, because of some TPL_OP_APPLY */
                tpl_add_tpl(out, tpl);
                return TPL_VAR;
            }
            if (!ctmp2->no_subst) {
                tmp2 = tpl_new();
                tmp2->no_subst = true;
                res = tpl_combine(tmp2, ctmp2, envid, vals, nb);
                if (res == TPL_CONST) {
                    if ((*tpl->u.f)(out, tmp2) < 0) {
                        res = TPL_ERR;
                    }
                    if (ctmp) {
                        res |= tpl_combine(out, ctmp, envid, vals, nb);
                    }
                    res |= tpl_combine(out, tmp2, envid, vals, nb);
                    tpl_delete(&tmp2);
                    return res;
                }
            } else {
                tmp2 = tpl_dup(ctmp2);
                res  = TPL_VAR;
            }
            tpl_array_append(&out->blocks, tmp = tpl_new_op(TPL_OP_APPLY_DELAYED));
            tmp->no_subst = tmp2->no_subst;
            if (ctmp) {
                if (ctmp->no_subst) {
                    tpl_add_tpl(tmp, ctmp);
                } else {
                    tpl_t *tmp3;
                    tpl_array_append(&tmp->blocks, tmp3 = tpl_new());
                    tmp3->no_subst = true;
                    res |= tpl_combine(tmp3, ctmp, envid, vals, nb);
                    tmp->no_subst &= tmp3->no_subst;
                }
            }
            tpl_array_append(&tmp->blocks, tmp2);
            out->no_subst &= tmp->no_subst;
            return res;
          default:
            return TPL_ERR;
        }

      case TPL_OP_APPLY_PURE:
      case TPL_OP_APPLY_PURE_ASSOC:
        tmp = tpl_new();
        tmp->no_subst = true;
        res = tpl_combine_block(tmp, tpl, envid, vals, nb);
        if (res == TPL_CONST) {
            if ((*tpl->u.f)(out, tmp) < 0) {
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
    if (tpl_combine_block(out, tpl, envid, vals, nb) < 0)
        tpl_delete(&out);
    return out;
}

static tpl_t *tpl_to_blob(tpl_t **orig)
{
    tpl_t *res = tpl_new_op(TPL_OP_BLOB);
    assert ((*orig)->op == TPL_OP_DATA || (*orig)->op == TPL_OP_BLOB);
    if ((*orig)->op == TPL_OP_DATA) {
        blob_set_data(&res->u.blob, (*orig)->u.data.data, (*orig)->u.data.len);
    } else {
        blob_set(&res->u.blob, &(*orig)->u.blob);
    }
    tpl_delete(orig);
    return *orig = res;
}

int tpl_optimize(tpl_t **tplp)
{
    tpl_t *tpl = *tplp;

    if (tpl->blocks.len < 1)
        return 0;

    if (tpl->refcnt > 1) {
        tpl_add_tpl(*tplp = tpl_new(), tpl);
        return tpl_optimize(tplp);
    }

    for (int i = 0; i < tpl->blocks.len; ) {
        tpl_t *cur = tpl->blocks.tab[i];

        if (cur->op == TPL_OP_BLOCK) {
            tpl_array_splice(&tpl->blocks, i, 1, cur->blocks.tab,
                             cur->blocks.len);
            for (int j = i; j < i + cur->blocks.len; j++) {
                tpl->blocks.tab[j]->refcnt++;
            }
            tpl_delete(&cur);
            continue;
        }
        i++;
    }

    for (int i = 0; i < tpl->blocks.len - 1; ) {
        tpl_t *cur = tpl->blocks.tab[i];
        tpl_t *nxt = tpl->blocks.tab[i + 1];

        if (nxt->op != TPL_OP_BLOB) {
            if (nxt->op != TPL_OP_DATA || nxt->u.data.len >= TPL_DATA_LIMIT_KEEP) {
                i += 2;
                continue;
            }
        }

        if (cur->op != TPL_OP_BLOB) {
            if (cur->op != TPL_OP_DATA || cur->u.data.len >= TPL_DATA_LIMIT_KEEP) {
                i++;
                continue;
            }
            if (nxt->op == TPL_OP_DATA) {
                if (nxt->u.data.len >= TPL_DATA_LIMIT_KEEP) {
                    i += 2;
                    continue;
                }
            } else
            if (nxt->refcnt == 1) {
                blob_insert_data(&nxt->u.blob, 0, cur->u.data.data,
                                 cur->u.data.len);
                tpl_array_take(&tpl->blocks, i);
                tpl_delete(&cur);
                continue;
            }
            cur = tpl_to_blob(&tpl->blocks.tab[i]);
        }

        if (nxt->op == TPL_OP_DATA) {
            if (cur->refcnt > 1)
                cur = tpl_to_blob(&tpl->blocks.tab[i]);
            blob_append_data(&cur->u.blob, nxt->u.data.data, nxt->u.data.len);
            tpl_array_take(&tpl->blocks, i + 1);
            tpl_delete(&nxt);
            continue;
        }

        if (cur->refcnt > 1 && nxt->refcnt > 1) {
            cur = tpl_to_blob(&tpl->blocks.tab[i]);
        }
        if (cur->refcnt > 1) {
            blob_insert(&nxt->u.blob, 0, &cur->u.blob);
            tpl_array_take(&tpl->blocks, i);
            tpl_delete(&cur);
        } else {
            blob_append(&cur->u.blob, &nxt->u.blob);
            tpl_array_take(&tpl->blocks, i + 1);
            tpl_delete(&nxt);
        }
    }

    return 0;
}
