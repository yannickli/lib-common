static int
NS(tpl_combine)(tpl_t *, const tpl_t *, uint16_t, VAL_TYPE*, int, int);

static int
NS(tpl_combine_block)(tpl_t *out, const tpl_t *tpl,
                      uint16_t envid, VAL_TYPE *vals, int nb, int flags)
{
    assert (tpl->op & TPL_OP_BLOCK);
    for (int i = 0; i < tpl->u.blocks.len; i++) {
        if (NS(tpl_combine)(out, tpl->u.blocks.tab[i], envid, vals, nb, flags))
            return -1;
    }
    return 0;
}

static int
NS(tpl_combine)(tpl_t *out, const tpl_t *tpl,
                uint16_t envid, VAL_TYPE *vals, int nb, int flags)
{
    const tpl_t *ctmp;
    tpl_t *tmp, *tmp2;

    switch (tpl->op) {
      case TPL_OP_DATA:
        if (tpl->u.data.len > 0)
            tpl_add_tpl(out, tpl);
        return 0;
      case TPL_OP_BLOB:
        if (tpl->u.blob.len > 0)
            tpl_add_tpl(out, tpl);
        return 0;

      case TPL_OP_VAR:
        if (tpl->u.varidx >> 16 == envid) {
            VAL_TYPE vtmp = getvar(tpl->u.varidx, vals, nb);
            if (!vtmp) {
                e_trace(2, "cound not find var %x (in env %d)", tpl->u.varidx,
                        envid);
                return -1;
            }
            return DEAL_WITH_VAR(out, vtmp, envid, vals, nb, flags);
        }
        tpl_add_tpl(out, tpl);
        out->is_const = false;
        return 0;

      case TPL_OP_BLOCK:
        return NS(tpl_combine_block)(out, tpl, envid, vals, nb, flags);

      case TPL_OP_IFDEF:
        if (tpl->u.varidx >> 16 == envid) {
            int branch = getvar(tpl->u.varidx, vals, nb) == NULL;
            if (tpl->u.blocks.len <= branch || !tpl->u.blocks.tab[branch])
                return 0;
            return NS(tpl_combine)(out, tpl->u.blocks.tab[branch], envid,
                                   vals, nb, flags);
        }
        out->is_const = false;
        if (tpl->is_const) {
            tpl_add_tpl(out, tpl);
            return 0;
        }
        tpl_array_append(&out->u.blocks, tmp = tpl_new_op(TPL_OP_IFDEF));
        tmp->is_const = true;
        for (int i = 0; i < tpl->u.blocks.len; i++) {
            tmp2 = tpl_dup(tpl->u.blocks.tab[i]);
            if (TPL_SUBST(&tmp2, envid, vals, nb, flags | TPL_KEEPVAR)) {
                e_trace(2, "cound not subst block %d", i);
                return -1;
            }
            tmp->is_const &= tmp2->is_const;
            tpl_array_append(&tmp->u.blocks, tmp2);
        }
        return 0;

      case TPL_OP_APPLY_DELAYED:
        switch (tpl->u.blocks.len) {
          case 0:
            return (*tpl->u.f)(out, NULL, NULL, 0);

          case 1:
            if ((*tpl->u.f)(out, NULL, NULL, 0) < 0)
                return -1;
            return NS(tpl_combine)(out, tpl->u.blocks.tab[0], envid, vals, nb, flags);

          case 2:
            ctmp = tpl->u.blocks.tab[0];
            tmp2 = tpl_dup(tpl->u.blocks.tab[1]);
            if (TPL_SUBST(&tmp2, envid, vals, nb, flags | TPL_KEEPVAR))
                return -1;
            if (tmp2->is_const) {
                if (tpl_apply(tpl->u.f, out, NULL, tmp2) < 0
                ||  (ctmp && NS(tpl_combine)(out, ctmp, envid, vals, nb, flags))
                ||  NS(tpl_combine)(out, tmp2, envid, vals, nb, flags))
                {
                    tpl_delete(&tmp2);
                    return -1;
                }
                tpl_delete(&tmp2);
                return 0;
            }

            tpl_array_append(&out->u.blocks, tmp = tpl_new_op(TPL_OP_APPLY_DELAYED));
            tpl_array_append(&tmp->u.blocks, NULL);
            tpl_array_append(&tmp->u.blocks, tmp2);
            if (ctmp) {
                tmp2 = tpl_dup(ctmp);
                if (TPL_SUBST(&tmp2, envid, vals, nb, flags | TPL_KEEPVAR))
                    return -1;
                tmp->u.blocks.tab[0] = tmp2;
            }
            tmp->is_const = out->is_const = false;
            return 0;
          default:
            return -1;
        }

      case TPL_OP_APPLY_PURE:
      case TPL_OP_APPLY_PURE_ASSOC:
        tmp = tpl_new();
        tmp->is_const = true;
        if (NS(tpl_combine_block)(tmp, tpl, envid, vals, nb, flags)) {
            tpl_delete(&tmp);
            return -1;
        }
        if (tmp->is_const) {
            int res = tpl_apply(tpl->u.f, out, NULL, tmp);
            tpl_delete(&tmp);
            if (res) {
                e_trace(2, "apply func %p failed", tpl->u.f);
            }
            return res;
        }
        tmp->op  = tpl->op;
        tmp->u.f = tpl->u.f;
        out->is_const = false;
        tpl_array_append(&out->u.blocks, tmp);
        return 0;
    }

    e_trace(2, "broke from switch");
    return -1;
}

static int
NS(tpl_fold_blob)(blob_t *, const tpl_t *, uint16_t, VAL_TYPE *, int, int);

static int
NS(tpl_fold_block)(blob_t *out, const tpl_t *tpl,
                   uint16_t envid, VAL_TYPE *vals, int nb, int flags)
{
    assert (tpl->op & TPL_OP_BLOCK);
    for (int i = 0; i < tpl->u.blocks.len; i++) {
        if (!tpl->u.blocks.tab[i])
            continue;
        if (NS(tpl_fold_blob)(out, tpl->u.blocks.tab[i], envid, vals, nb, flags))
            return -1;
    }
    return 0;
}

static int
NS(tpl_fold_blob)(blob_t *out, const tpl_t *tpl,
                  uint16_t envid, VAL_TYPE *vals, int nb, int flags)
{
    const tpl_t *ctmp;
    tpl_t *tmp;
    VAL_TYPE vtmp;
    int branch;

    switch (tpl->op) {
      case TPL_OP_DATA:
        blob_append_data(out, tpl->u.data.data, tpl->u.data.len);
        return 0;

      case TPL_OP_BLOB:
        blob_append(out, &tpl->u.blob);
        return 0;

      case TPL_OP_VAR:
        if (tpl->u.varidx >> 16 != envid)
            return -1;
        vtmp = getvar(tpl->u.varidx, vals, nb);
        if (!vtmp)
            return -1;
        return DEAL_WITH_VAR2(out, vtmp, envid, vals, nb, flags);

      case TPL_OP_BLOCK:
        return NS(tpl_fold_block)(out, tpl, envid, vals, nb, flags);

      case TPL_OP_IFDEF:
        if (tpl->u.varidx >> 16 != envid)
            return -1;
        branch = getvar(tpl->u.varidx, vals, nb) == NULL;
        if (tpl->u.blocks.len <= branch || !tpl->u.blocks.tab[branch])
            return 0;
        return NS(tpl_fold_blob)(out, tpl->u.blocks.tab[branch], envid,
                                 vals, nb, flags);

      case TPL_OP_APPLY_DELAYED:
        switch (tpl->u.blocks.len) {
          case 0:
            return tpl_apply(tpl->u.f, NULL, out, NULL);

          case 1:
            if (tpl_apply(tpl->u.f, NULL, out, NULL) < 0)
                return -1;
            return NS(tpl_fold_block)(out, tpl, envid, vals, nb, flags);

          case 2:
            ctmp = tpl->u.blocks.tab[0];
            tmp = tpl_dup(tpl->u.blocks.tab[1]);
            if (TPL_SUBST(&tmp, envid, vals, nb,
                          flags | TPL_KEEPVAR | TPL_LASTSUBST))
            {
                return -1;
            }
            if ((tpl_apply(tpl->u.f, NULL, out, tmp) < 0)
            ||  (ctmp && NS(tpl_fold_block)(out, ctmp, envid, vals, nb, flags))
            ||  NS(tpl_fold_block)(out, tmp, envid, vals, nb, flags))
            {
                tpl_delete(&tmp);
                return -1;
            }
            tpl_delete(&tmp);
            return 0;

          default:
            return -1;
        }

      case TPL_OP_APPLY_PURE:
      case TPL_OP_APPLY_PURE_ASSOC:
        if (NS(tpl_combine_block)(tmp = tpl_new(), tpl, envid, vals, nb,
                                  flags | TPL_KEEPVAR | TPL_LASTSUBST)
        ||  tpl_apply(tpl->u.f, NULL, out, tmp) < 0)
        {
            tpl_delete(&tmp);
            return -1;
        }
        tpl_delete(&tmp);
        return 0;
    }

    return -1;
}

#undef NS
#undef VAL_TYPE
#undef DEAL_WITH_VAR
#undef DEAL_WITH_VAR2
#undef TPL_SUBST
