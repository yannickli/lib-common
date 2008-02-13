static enum tplcode
BASE(tpl_t *, const tpl_t *, uint16_t, VAL_TYPE*, int, int);

static enum tplcode BASE_BLOCK(tpl_t *out, const tpl_t *tpl, uint16_t envid,
                               VAL_TYPE *vals, int nb, int flags)
{
    enum tplcode res = TPL_CONST;

    assert (tpl->op & TPL_OP_BLOCK);
    for (int i = 0; i < tpl->u.blocks.len; i++) {
        res |= BASE(out, tpl->u.blocks.tab[i], envid, vals, nb, flags);
    }
    return res;
}

static enum tplcode BASE(tpl_t *out, const tpl_t *tpl, uint16_t envid,
                         VAL_TYPE *vals, int nb, int flags)
{
    const tpl_t *ctmp, *ctmp2;
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
        if (tpl->no_subst) {
            for (int i = 0; i < tpl->u.blocks.len; i++) {
                tpl_add_tpl(out, tpl->u.blocks.tab[i]);
            }
            return TPL_CONST;
        }
        return BASE_BLOCK(out, tpl, envid, vals, nb, flags);

      case TPL_OP_IFDEF:
        if (tpl->u.varidx >> 16 == envid) {
            int branch = getvar(tpl->u.varidx, vals, nb) != NULL;
            if (tpl->u.blocks.len <= branch)
                return TPL_CONST;
            return BASE(out, tpl->u.blocks.tab[branch], envid,
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
            if (tpl->u.blocks.tab[i]->no_subst) {
                tpl_add_tpl(tmp, tpl->u.blocks.tab[i]);
            } else {
                tmp2 = TPL_SUBST(tpl->u.blocks.tab[i], envid,
                                 vals, nb, flags | TPL_KEEPVAR);
                if (!tmp2)
                    return TPL_ERR;
                tmp->no_subst &= tmp2->no_subst;
                tpl_array_append(&tmp->u.blocks, tmp2);
            }
        }
        return TPL_VAR;

      case TPL_OP_APPLY_DELAYED:
        switch (tpl->u.blocks.len) {
          case 0:
            return (*tpl->u.f)(out, NULL) < 0 ? TPL_ERR : TPL_CONST;

          case 1:
            if ((*tpl->u.f)(out, NULL) < 0)
                return TPL_ERR;
            if (tpl->u.blocks.tab[0]->no_subst) {
                tpl_add_tpl(out, tpl->u.blocks.tab[0]);
                return TPL_CONST;
            }
            return BASE(out, tpl->u.blocks.tab[0], envid, vals, nb, flags);

          case 2:
            ctmp  = tpl->u.blocks.tab[0];
            ctmp2 = tpl->u.blocks.tab[1];
            if (!ctmp2->no_subst) {
                tmp2 = tpl_new();
                tmp2->no_subst = true;
                res = BASE(tmp2, ctmp2, envid, vals, nb, flags);
                if (res == TPL_CONST) {
                    if ((*tpl->u.f)(out, tmp2) < 0) {
                        res = TPL_ERR;
                    }
                    if (ctmp) {
                        res |= BASE(out, ctmp, envid, vals, nb, flags);
                    }
                    res |= BASE(out, tmp2, envid, vals, nb, flags);
                    tpl_delete(&tmp2);
                    return res;
                }
            } else {
                tmp2 = tpl_dup(ctmp2);
                res  = TPL_VAR;
            }
            tpl_array_append(&out->u.blocks, tmp = tpl_new_op(TPL_OP_APPLY_DELAYED));
            tmp->no_subst = tmp2->no_subst;
            if (ctmp) {
                if (ctmp->no_subst) {
                    tpl_add_tpl(tmp, ctmp);
                } else {
                    tpl_t *tmp3;
                    tpl_array_append(&tmp->u.blocks, tmp3 = tpl_new());
                    tmp3->no_subst = true;
                    res |= BASE(tmp3, ctmp, envid, vals, nb, flags);
                    tmp->no_subst &= tmp3->no_subst;
                }
            }
            tpl_array_append(&tmp->u.blocks, tmp2);
            out->no_subst &= tmp->no_subst;
            return res;
          default:
            return TPL_ERR;
        }

      case TPL_OP_APPLY_PURE:
      case TPL_OP_APPLY_PURE_ASSOC:
        tmp = tpl_new();
        tmp->no_subst = true;
        res = BASE_BLOCK(tmp, tpl, envid, vals, nb, flags);
        if (res == TPL_CONST) {
            if ((*tpl->u.f)(out, tmp) < 0) {
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

#undef BASE
#undef BASE_BLOCK
#undef VAL_TYPE
#undef DEAL_WITH_VAR
#undef TPL_SUBST
