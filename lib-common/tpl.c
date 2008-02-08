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

    if (!tpl->blocks.len || tpl->blocks.tab[tpl->blocks.len - 1]->op != TPL_OP_BLOB) {
        tpl_array_append(&tpl->blocks, buf = tpl_new_op(TPL_OP_BLOB));
    } else {
        buf = tpl->blocks.tab[tpl->blocks.len - 1];
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

static int tpl_apply_fun(tpl_apply_f *f, blob_t *blob, const tpl_t *tpl)
{
    struct tpl_data td = { .iov = NULL };
    if (tpl_into_iovec(&td, tpl) < 0 || (*tpl->u.f)(blob, &td) < 0) {
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
    tpl_t *tmp;

    switch (tpl->op) {
        enum tplcode res;

      case TPL_OP_DATA:
      case TPL_OP_BLOB:
        tpl_add_tpl(out, tpl);
        return TPL_CONST;

      case TPL_OP_VAR:
        if (tpl->u.varidx >> 16 == envid) {
            const tpl_t *t = getvar(tpl->u.varidx, vals, nb);
            if (!t)
                return TPL_ERR;
            return tpl_combine(out, t, envid, vals, nb);
        }
        tpl_add_tpl(out, tpl);
        return TPL_VAR;

      case TPL_OP_BLOCK:
        return tpl_combine_block(out, tpl, envid, vals, nb);

      case TPL_OP_IFDEF:
        if (tpl->u.varidx >> 16 == envid) {
            int branch = getvar(tpl->u.varidx, vals, nb) != NULL;
            tpl = tpl->blocks.len > branch ? tpl->blocks.tab[branch] : NULL;
            return tpl ? tpl_combine(out, tpl, envid, vals, nb) : TPL_CONST;
        }
        tpl_add_tpl(out, tpl);
        return TPL_VAR;

      case TPL_OP_APPLY:
        tpl_array_append(&out->blocks, tmp = tpl_new_op(TPL_OP_APPLY));
        tmp->u.f = tpl->u.f;
        return TPL_VAR | tpl_combine_block(out, tpl, envid, vals, nb);

      case TPL_OP_APPLY_DELAYED:
        switch (tpl->blocks.len) {
          case 0:
            return TPL_CONST;
          case 1:
            return tpl_combine(out, tpl->blocks.tab[0], envid, vals, nb);
          case 2:
            /* TODO */
            abort();
            return TPL_ERR;
          default:
            return TPL_ERR;
        }

      case TPL_OP_APPLY_PURE:
      case TPL_OP_APPLY_PURE_ASSOC:
        tpl_array_append(&out->blocks, tmp = tpl_new());
        res = tpl_combine_block(tmp, tpl, envid, vals, nb);
        if (res == TPL_CONST) {
            tmp->op = TPL_OP_BLOB;
            blob_init(&tmp->u.blob);
            if (tpl_apply_fun(tpl->u.f, &tmp->u.blob, tpl) < 0) {
                res = TPL_ERR;
            }
            tpl_array_wipe(&tmp->blocks);
            tpl_array_init(&tmp->blocks);
        } else {
            tmp->op  = tpl->op;
            tmp->u.f = tpl->u.f;
        }
        return res;
    }

    return TPL_ERR;
}

tpl_t *tpl_subst(const tpl_t *tpl, uint16_t envid, const tpl_t **vals, int nb)
{
    tpl_t *out = tpl_new();
    if (tpl_combine_block(out, tpl, envid, vals, nb) < 0) {
        tpl_delete(&out);
    }
    return out;
}
