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

tpl_t *tpl_new_op(tpl_op op)
{
    tpl_t *n  = p_new(tpl_t, 1);
    n->op     = op;
    n->refcnt = 1;
    if (op == TPL_OP_BLOB)
        blob_init(&n->u.blob);
    if (op & TPL_OP_BLOCK)
        tpl_array_init(&n->u.blocks);
    return n;
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
    if (n->op == TPL_OP_BLOB)
        blob_wipe(&n->u.blob);
    if (n->op & TPL_OP_BLOCK)
        tpl_array_wipe(&n->u.blocks);
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
        && ((t)->op != TPL_OP_IFDEF || (t)->u.blocks.len < 2)           \
        && ((t)->op != TPL_OP_APPLY_DELAYED || (t)->u.blocks.len < 2)   \
        )

void tpl_add_data(tpl_t *tpl, const byte *data, int len)
{
    tpl_t *buf;

    assert (tpl_can_append(tpl));

    if (len <= TPL_COPY_LIMIT_HARD) {
        tpl_copy_data(tpl, data, len);
        return;
    }

    if (tpl->u.blocks.len > 0
    &&  tpl->u.blocks.tab[tpl->u.blocks.len - 1]->refcnt == 1)
    {
        buf = tpl->u.blocks.tab[tpl->u.blocks.len - 1];
        if (buf->op == TPL_OP_BLOB && len <= TPL_COPY_LIMIT_SOFT) {
            blob_append_data(&buf->u.blob, data, len);
            return;
        }
    }
    tpl_array_append(&tpl->u.blocks, buf = tpl_new_op(TPL_OP_DATA));
    buf->u.data = (struct tpl_data){ .data = data, .len = len };
}

blob_t *tpl_get_blob(tpl_t *tpl)
{
    tpl_t *buf;

    assert (tpl_can_append(tpl));

    buf = tpl->u.blocks.len > 0 ? tpl->u.blocks.tab[tpl->u.blocks.len - 1] : NULL;
    if (!buf || buf->op != TPL_OP_BLOB || buf->refcnt > 1) {
        tpl_array_append(&tpl->u.blocks, buf = tpl_new_op(TPL_OP_BLOB));
    }
    return &buf->u.blob;
}

void tpl_copy_data(tpl_t *tpl, const byte *data, int len)
{
    if (len > 0) {
        blob_append_data(tpl_get_blob(tpl), data, len);
    }
}

void tpl_add_fmt(tpl_t *tpl, const char *fmt, ...)
{
    va_list ap;
    va_start(ap, fmt);
    blob_append_vfmt(tpl_get_blob(tpl), fmt, ap);
    va_end(ap);
}

void tpl_add_var(tpl_t *tpl, uint16_t array, uint16_t index)
{
    tpl_t *var;

    assert (tpl_can_append(tpl));
    tpl_array_append(&tpl->u.blocks, var = tpl_new_op(TPL_OP_VAR));
    var->u.varidx = ((uint32_t)array << 16) | index;
}

void tpl_add_tpl(tpl_t *out, const tpl_t *tpl)
{
    assert (tpl_can_append(out));

    if (tpl->op == TPL_OP_BLOB && out->u.blocks.len > 0
    &&  tpl->u.blob.len <= TPL_COPY_LIMIT_SOFT)
    {
        tpl_t *buf = out->u.blocks.tab[out->u.blocks.len - 1];
        if (buf->op == TPL_OP_BLOB && buf->refcnt == 1) {
            blob_append(&buf->u.blob, &tpl->u.blob);
            return;
        }
    }
    tpl_array_append(&out->u.blocks, tpl_dup(tpl));
}

tpl_t *tpl_add_ifdef(tpl_t *tpl, uint16_t array, uint16_t index)
{
    tpl_t *var;

    assert (tpl_can_append(tpl));
    tpl_array_append(&tpl->u.blocks, var = tpl_new_op(TPL_OP_IFDEF));
    var->u.varidx = ((uint32_t)array << 16) | index;
    return var;
}

tpl_t *tpl_add_apply(tpl_t *tpl, tpl_op op, tpl_apply_f *f)
{
    tpl_t *app;

    assert (tpl_can_append(tpl));
    tpl_array_append(&tpl->u.blocks, app = tpl_new_op(op));
    app->u.f = f;
    return app;
}

static char const pad[] = "| | | | | | | | | | | | | | | | | | | | | | | | ";

static void tpl_dump2(int dbg, const tpl_t *tpl, int lvl)
{
#define HAS_SUBST(tpl) \
    ((tpl->op & TPL_OP_BLOCK && !tpl->is_const) || tpl->op == TPL_OP_VAR)
#define TRACE(fmt, c, ...) \
    e_trace(dbg, "%.*s%c%c "fmt, 1 + 2 * lvl, pad, c, \
            HAS_SUBST(tpl) ? '*' : ' ', ##__VA_ARGS__)
#define TRACE_NULL() \
    e_trace(dbg, "%.*s NULL", 3 + 2 * lvl, pad)


    switch (tpl->op) {
      case TPL_OP_DATA:
        TRACE("DATA %5d bytes (%.*s...)", ' ', tpl->u.data.len,
              MIN(tpl->u.data.len, 16), tpl->u.data.data);
        return;

      case TPL_OP_BLOB:
        TRACE("BLOB %5zd bytes (%.*s...)", ' ', tpl->u.blob.len,
              MIN((int)tpl->u.blob.len, 16), tpl->u.blob.data);
        return;

      case TPL_OP_VAR:
        TRACE("VAR  q=%02x, v=%02x", ' ', tpl->u.varidx >> 16,
              tpl->u.varidx & 0xffff);
        return;

      case TPL_OP_BLOCK:
        TRACE("BLOC %d tpls", '\\', tpl->u.blocks.len);
        for (int i = 0; i < tpl->u.blocks.len; i++) {
            tpl_dump2(dbg, tpl->u.blocks.tab[i], lvl + 1);
        }
        break;

      case TPL_OP_IFDEF:
        TRACE("DEF? q=%02x, v=%02x", '\\', tpl->u.varidx >> 16,
              tpl->u.varidx & 0xffff);
        if (tpl->u.blocks.len <= 0 || !tpl->u.blocks.tab[0]) {
            TRACE_NULL();
        } else {
            tpl_dump2(dbg, tpl->u.blocks.tab[0], lvl + 1);
        }
        if (tpl->u.blocks.len <= 1 || !tpl->u.blocks.tab[1]) {
            TRACE_NULL();
        } else {
            tpl_dump2(dbg, tpl->u.blocks.tab[1], lvl + 1);
        }
        break;

      case TPL_OP_APPLY_DELAYED:
        TRACE("DELA %p", '\\', tpl->u.f);
        if (tpl->u.blocks.len <= 0 || !tpl->u.blocks.tab[0]) {
            TRACE_NULL();
        } else {
            tpl_dump2(dbg, tpl->u.blocks.tab[0], lvl + 1);
        }
        if (tpl->u.blocks.len > 1) {
            tpl_dump2(dbg, tpl->u.blocks.tab[1], lvl + 1);
        }
        break;

      case TPL_OP_APPLY_PURE:
      case TPL_OP_APPLY_PURE_ASSOC:
        TRACE("FUNC %p (%d tpls)", '\\', tpl->u.f, tpl->u.blocks.len);
        for (int i = 0; i < tpl->u.blocks.len; i++) {
            tpl_dump2(dbg, tpl->u.blocks.tab[i], lvl + 1);
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
/* Substitution helpers                                                     */
/****************************************************************************/

int tpl_get_short_data(tpl_t *tpl, const byte **data, int *len)
{
    for (;;) {
        switch (tpl->op) {
          case TPL_OP_BLOB:
            *data = tpl->u.blob.data;
            *len  = tpl->u.blob.len;
            return 0;
          case TPL_OP_DATA:
            *data = tpl->u.data.data;
            *len  = tpl->u.data.len;
            return 0;
          case TPL_OP_BLOCK:
            tpl_optimize(tpl);
            if (tpl->u.blocks.len != 1) {
                e_trace(2, "blocks.len: %zd != 1", tpl->u.blocks.len);
                return -1;
            }
            tpl = tpl->u.blocks.tab[0];
            break;
          default:
            e_trace(2, "unsupported op: %d", tpl->op);
            return -1;
        }
    }
}

/****************************************************************************/
/* Substitution and optimization                                            */
/****************************************************************************/

#define getvar(id, vals, nb) \
    (((vals) && ((id) & 0xffff) < (uint16_t)(nb)) ? (vals)[(id) & 0xffff] : NULL)

#define NS(x)          x##_tpl
#define VAL_TYPE       tpl_t *
#define DEAL_WITH_VAR  tpl_combine_tpl
#define DEAL_WITH_VAR2 tpl_fold_blob_tpl
#define TPL_SUBST      tpl_subst
#include "tpl.in.c"
int tpl_subst(tpl_t **tplp, uint16_t envid, tpl_t **vals, int nb, int flags)
{
    tpl_t *out = *tplp;

    if (!out->is_const) {
        out = tpl_new();
        out->is_const = true;
        if (tpl_combine_tpl(out, *tplp, envid, vals, nb, flags) < 0)
            tpl_delete(&out);
        if ((flags & TPL_LASTSUBST) && !out->is_const)
            tpl_delete(&out);
        tpl_delete(tplp);
        *tplp = out;
    }
    if (!(flags & TPL_KEEPVAR)) {
        for (int i = 0; i < nb; i++) {
            tpl_delete(&vals[i]);
        }
    }
    return out ? 0 : -1;
}

int tpl_fold(blob_t *out, tpl_t **tplp, uint16_t envid, tpl_t **vals, int nb,
             int flags)
{
    int pos = out->len;
    int res = 0;

    if (tpl_fold_blob_tpl(out, *tplp, envid, vals, nb, flags) < 0) {
        blob_resize(out, pos);
        res = -1;
    }
    if (!(flags & TPL_KEEPVAR)) {
        for (int i = 0; i < nb; i++) {
            tpl_delete(&vals[i]);
        }
    }
    tpl_delete(tplp);
    return res;
}

#define NS(x)          x##_str
#define VAL_TYPE       const char *
#define DEAL_WITH_VAR(t, v, ...)   (tpl_copy_cstr((t), (v)), 0)
#define DEAL_WITH_VAR2(t, v, ...)  (blob_append_cstr((t), (v)), 0)
#define TPL_SUBST      tpl_subst_str
#include "tpl.in.c"
int tpl_subst_str(tpl_t **tplp, uint16_t envid,
                  const char **vals, int nb, int flags)
{
    tpl_t *out = *tplp;
    if (!out->is_const) {
        out = tpl_new();
        out->is_const = true;
        if (tpl_combine_str(out, *tplp, envid, vals, nb, flags) < 0)
            tpl_delete(&out);
        if ((flags & TPL_LASTSUBST) && !out->is_const)
            tpl_delete(&out);
        tpl_delete(tplp);
        *tplp = out;
    }
    return out ? 0 : -1;
}

int tpl_fold_str(blob_t *out, tpl_t **tplp, uint16_t envid,
                 const char **vals, int nb, int flags)
{
    int pos = out->len;
    if (tpl_fold_blob_str(out, *tplp, envid, vals, nb, flags) < 0) {
        blob_resize(out, pos);
        tpl_delete(tplp);
        return -1;
    }
    tpl_delete(tplp);
    return 0;
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

void tpl_optimize(tpl_t *tpl)
{
    /* OG: Should not assume tpl->op & TPL_OP_BLOCK */
    if (!tpl || tpl->u.blocks.len < 1)
        return;

    /* XXX: tpl->u.blocks.len likely to be modified in the loop */
    for (int i = 0; i < tpl->u.blocks.len; ) {
        tpl_t *cur = tpl->u.blocks.tab[i];

        if (cur->op == TPL_OP_BLOCK && tpl->op == TPL_OP_BLOCK) {
            tpl_array_splice(&tpl->u.blocks, i, 1,
                             cur->u.blocks.tab, cur->u.blocks.len);
            for (int j = i; j < i + cur->u.blocks.len; j++) {
                tpl->u.blocks.tab[j]->refcnt++;
            }
            tpl_delete(&cur);
        } else {
            if (cur->op & TPL_OP_BLOCK) {
                tpl_optimize(tpl->u.blocks.tab[i]);
            }
            i++;
        }
    }

    for (int i = 0; i < tpl->u.blocks.len - 1; ) {
        tpl_t *cur = tpl->u.blocks.tab[i];
        tpl_t *nxt = tpl->u.blocks.tab[i + 1];

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
            if (nxt->op == TPL_OP_BLOB && nxt->refcnt == 1) {
                blob_insert_data(&nxt->u.blob, 0, cur->u.data.data,
                                 cur->u.data.len);
                tpl_array_remove(&tpl->u.blocks, i);
                tpl_delete(&cur);
                continue;
            }
            cur = tpl_to_blob(&tpl->u.blocks.tab[i]);
        }

        assert (cur->op == TPL_OP_BLOB);
        if (nxt->op == TPL_OP_DATA) {
            if (cur->refcnt > 1) {
                cur = tpl_to_blob(&tpl->u.blocks.tab[i]);
            }
            blob_append_data(&cur->u.blob, nxt->u.data.data, nxt->u.data.len);
            tpl_array_remove(&tpl->u.blocks, i + 1);
            tpl_delete(&nxt);
            continue;
        }

        assert (nxt->op == TPL_OP_BLOB);
        if (cur->refcnt > 1 && nxt->refcnt > 1) {
            cur = tpl_to_blob(&tpl->u.blocks.tab[i]);
        }
        if (cur->refcnt > 1) {
            blob_insert(&nxt->u.blob, 0, &cur->u.blob);
            tpl_array_remove(&tpl->u.blocks, i);
            tpl_delete(&cur);
        } else {
            blob_append(&cur->u.blob, &nxt->u.blob);
            tpl_array_remove(&tpl->u.blocks, i + 1);
            tpl_delete(&nxt);
        }
    }
}

int tpl_to_iov(struct iovec *iov, int nr, tpl_t *tpl)
{
    switch (tpl->op) {
        int n;
      case TPL_OP_DATA:
        if (nr > 0) {
            iov->iov_base = (void *)tpl->u.data.data;
            iov->iov_len  = tpl->u.data.len;
        }
        return 1;
      case TPL_OP_BLOB:
        if (nr > 0) {
            iov->iov_base = (void *)tpl->u.blob.data;
            iov->iov_len  = tpl->u.blob.len;
        }
        return 1;
      case TPL_OP_BLOCK:
        for (int i = n = 0; i < tpl->u.blocks.len; i++) {
            int res = tpl_to_iov(iov + n, n < nr ? nr - n : 0,
                                 tpl->u.blocks.tab[i]);
            if (res < 0)
                return -1;
            n += res;
        }
        return n;
      default:
        return -1;
    }
}

int tpl_to_iovec_vector(iovec_vector *iov, tpl_t *tpl)
{
    int oldlen = iov->len;

    switch (tpl->op) {
      case TPL_OP_DATA:
        iovec_vector_append(iov, MAKE_IOVEC(tpl->u.data.data,
                                            tpl->u.data.len));
        return 0;

      case TPL_OP_BLOB:
        iovec_vector_append(iov, MAKE_IOVEC(tpl->u.blob.data,
                                            tpl->u.blob.len));
        return 0;

      case TPL_OP_BLOCK:
        for (int i = 0; i < tpl->u.blocks.len; i++) {
            if (tpl_to_iovec_vector(iov, tpl->u.blocks.tab[i])) {
                iov->len = oldlen;
                return -1;
            }
        }
        return 0;

      default:
        return -1;
    }
}

int tpl_encode_xml_string(tpl_t *out, blob_t *blob, tpl_t *args)
{
    const byte *data;
    int len;

    if (!blob) {
        assert(out);
        blob = tpl_get_blob(out);
    }
    if (args->u.blocks.len != 1)
        return -1;

    if (tpl_get_short_data(args, &data, &len))
        return -1;

    blob_append_xml_escape(blob, data, len);
    return 0;
}

int tpl_encode_plmn(tpl_t *out, blob_t *blob, tpl_t *args)
{
    const byte *data;
    int len;
    int64_t num;

    if (!blob) {
        assert(out);
        blob = tpl_get_blob(out);
    }
    if (args->u.blocks.len != 1)
        return -1;

    if (tpl_get_short_data(args, &data, &len))
        return -1;

    num = msisdn_canonify((const char*)data, len, -1);

    if (num < 0) {
        blob_append_data(blob, data, len);
    } else {
        blob_append_fmt(blob, "+%lld/TYPE=PLMN", (long long)num);
    }
    return 0;
}

int tpl_encode_expiration(tpl_t *out, blob_t *blob, tpl_t *args)
{
    const byte *data;
    int len;
    time_t expiration;
    struct tm t;

    if (!blob) {
        assert(out);
        blob = tpl_get_blob(out);
    }
    if (args->u.blocks.len != 1)
        return -1;

    if (tpl_get_short_data(args, &data, &len))
        return -1;

    expiration = memtoip(data, len, NULL);
    localtime_r(&expiration, &t);

    blob_ensure_avail(blob, 20);
    blob_append_fmt(blob, "%04d-%02d-%02dT%02d:%02d:%02d",
                    t.tm_year + 1900, t.tm_mon + 1, t.tm_mday,
                    t.tm_hour, t.tm_min, t.tm_sec);
    return 0;
}

