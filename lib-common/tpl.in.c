static enum tplcode
NS(tpl_combine)(tpl_t *, const tpl_t *, uint16_t, VAL_TYPE*, int, int);

static enum tplcode
NS(tpl_combine_block)(tpl_t *out, const tpl_t *tpl,
                      uint16_t envid, VAL_TYPE *vals, int nb, int flags)
{
    enum tplcode res = TPL_CONST;

    assert (tpl->op & TPL_OP_BLOCK);
    for (int i = 0; i < tpl->u.blocks.len; i++) {
        res |= NS(tpl_combine)(out, tpl->u.blocks.tab[i], envid, vals, nb, flags);
    }
    return res;
}

static enum tplcode
NS(tpl_combine)(tpl_t *out, const tpl_t *tpl,
                uint16_t envid, VAL_TYPE *vals, int nb, int flags)
{
    const tpl_t *ctmp;
    tpl_t *tmp, *tmp2;

    switch (tpl->op) {
        enum tplcode res;

      case TPL_OP_DATA:
        if (tpl->u.data.len > 0)
            tpl_add_tpl(out, tpl);
        return TPL_CONST;
      case TPL_OP_BLOB:
        if (tpl->u.blob.len > 0)
            tpl_add_tpl(out, tpl);
        return TPL_CONST;

      case TPL_OP_VAR:
        if (tpl->u.varidx >> 16 == envid) {
            VAL_TYPE vtmp = getvar(tpl->u.varidx, vals, nb);
            if (!vtmp)
                return TPL_ERR;
            return DEAL_WITH_VAR(out, vtmp, envid, vals, nb, flags);
        }
        tpl_add_tpl(out, tpl);
        out->no_subst = false;
        return TPL_VAR;

      case TPL_OP_BLOCK:
        return NS(tpl_combine_block)(out, tpl, envid, vals, nb, flags);

      case TPL_OP_IFDEF:
        if (tpl->u.varidx >> 16 == envid) {
            int branch = getvar(tpl->u.varidx, vals, nb) == NULL;
            if (tpl->u.blocks.len <= branch)
                return TPL_CONST;
            return NS(tpl_combine)(out, tpl->u.blocks.tab[branch], envid,
                                   vals, nb, flags);
        }
        out->no_subst = false;
        if (tpl->no_subst) {
            tpl_add_tpl(out, tpl);
            return TPL_VAR;
        }
        tpl_array_append(&out->u.blocks, tmp = tpl_new_op(TPL_OP_IFDEF));
        tmp->no_subst = true;
        for (int i = 0; i < tpl->u.blocks.len; i++) {
            tmp2 = TPL_SUBST(tpl->u.blocks.tab[i], envid,
                             vals, nb, flags | TPL_KEEPVAR);
            if (!tmp2)
                return TPL_ERR;
            tmp->no_subst &= tmp2->no_subst;
            tpl_array_append(&tmp->u.blocks, tmp2);
        }
        return TPL_VAR;

      case TPL_OP_APPLY_DELAYED:
        switch (tpl->u.blocks.len) {
          case 0:
            return (*tpl->u.f)(out, NULL, NULL) < 0 ? TPL_ERR : TPL_CONST;

          case 1:
            if ((*tpl->u.f)(out, NULL, NULL) < 0)
                return TPL_ERR;
            return NS(tpl_combine)(out, tpl->u.blocks.tab[0], envid, vals, nb, flags);

          case 2:
            ctmp = tpl->u.blocks.tab[0];
            tmp2 = TPL_SUBST(tpl->u.blocks.tab[1], envid, vals, nb, flags | TPL_KEEPVAR);
            if (!tmp2)
                return TPL_ERR;
            if (tmp2->no_subst) {
                if ((*tpl->u.f)(out, NULL, tmp2) < 0) {
                    res = TPL_ERR;
                }
                if (ctmp) {
                    res |= NS(tpl_combine)(out, ctmp, envid, vals, nb, flags);
                }
                res |= NS(tpl_combine)(out, tmp2, envid, vals, nb, flags);
                tpl_delete(&tmp2);
                return res;
            }
            tpl_array_append(&out->u.blocks, tmp = tpl_new_op(TPL_OP_APPLY_DELAYED));
            tpl_array_append(&tmp->u.blocks, NULL);
            tpl_array_append(&tmp->u.blocks, tmp2);
            if (ctmp) {
                tmp2 = TPL_SUBST(ctmp, envid, vals, nb, flags | TPL_KEEPVAR);
                if (!tmp2)
                    return TPL_ERR;
                tmp->u.blocks.tab[0] = tmp2;
            }
            tmp->no_subst = out->no_subst = false;
            return TPL_VAR;
          default:
            return TPL_ERR;
        }

      case TPL_OP_APPLY_PURE:
      case TPL_OP_APPLY_PURE_ASSOC:
        tmp = tpl_new();
        tmp->no_subst = true;
        res = NS(tpl_combine_block)(tmp, tpl, envid, vals, nb, flags);
        if (res == TPL_CONST) {
            if ((*tpl->u.f)(out, NULL, tmp) < 0) {
                res = TPL_ERR;
            }
            tpl_delete(&tmp);
        } else {
            tmp->op  = tpl->op;
            tmp->u.f = tpl->u.f;
            out->no_subst = false;
            tpl_array_append(&out->u.blocks, tmp);
        }
        return res;
    }

    return TPL_ERR;
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
        if (tpl->u.blocks.len <= branch)
            return 0;
        return NS(tpl_fold_blob)(out, tpl->u.blocks.tab[branch], envid,
                                 vals, nb, flags);

      case TPL_OP_APPLY_DELAYED:
        switch (tpl->u.blocks.len) {
          case 0:
            return (*tpl->u.f)(NULL, out, NULL);

          case 1:
            if ((*tpl->u.f)(NULL, out, NULL) < 0)
                return -1;
            return NS(tpl_fold_block)(out, tpl, envid, vals, nb, flags);

          case 2:
            ctmp = tpl->u.blocks.tab[0];
            tmp = TPL_SUBST(tpl->u.blocks.tab[1], envid, vals, nb,
                            flags | TPL_KEEPVAR | TPL_LASTSUBST);
            if (!tmp)
                return -1;
            if (((*tpl->u.f)(NULL, out, tmp) < 0)
            ||  (ctmp && NS(tpl_fold_block)(out, ctmp, envid, vals, nb, flags))
            ||  NS(tpl_fold_block)(out, tmp, envid, vals, nb, flags))
            {
                tpl_delete(&tmp);
                return -1;
            }
            tpl_delete(&tmp);
            return 0;

          default:
            return TPL_ERR;
        }

      case TPL_OP_APPLY_PURE:
      case TPL_OP_APPLY_PURE_ASSOC:
        tmp = TPL_SUBST(tpl, envid, vals, nb, flags | TPL_KEEPVAR | TPL_LASTSUBST);
        if (!tmp)
            return -1;
        if ((*tpl->u.f)(NULL, out, tmp) < 0) {
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
