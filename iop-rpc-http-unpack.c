/**************************************************************************/
/*                                                                        */
/*  Copyright (C) 2004-2011 INTERSEC SAS                                  */
/*                                                                        */
/*  Should you receive a copy of this source code, you must check you     */
/*  have a proper, written authorization of INTERSEC to hold it. If you   */
/*  don't have such an authorization, you must DELETE all source code     */
/*  files in your possession, and inform INTERSEC of the fact you obtain  */
/*  these files. Should you not comply to these terms, you can be         */
/*  prosecuted in the extent permitted by applicable law.                 */
/*                                                                        */
/**************************************************************************/

#include "xmlr.h"
#include "iop-rpc.h"

#define XML_CNAME(xr) \
    (const char *)xmlTextReaderConstLocalName(xr)

void ichttp_cb_wipe(ichttp_cb_t *rpc)
{
    p_delete(&rpc->name);
    p_delete(&rpc->name_uri);
    p_delete(&rpc->name_res);
    p_delete(&rpc->name_exn);
}

static void ichttp_query_wipe(ichttp_query_t *q)
{
    ichttp_cb_delete(&q->cbe);
}

OBJ_VTABLE(ichttp_query)
    ichttp_query.wipe = ichttp_query_wipe;
OBJ_VTABLE_END()

static int t_parse_json(ichttp_query_t *iq, ichttp_cb_t *cbe, void **vout)
{
    const iop_struct_t  *st = cbe->fun->args;
    pstream_t      ps;
    iop_json_lex_t jll;
    int res = 0;
    SB_8k(buf);
    void *v;

    v = t_new_raw(char, st->size);
    iop_jlex_init(t_pool(), &jll);
    ps = ps_initsb(&iq->payload);
    iop_jlex_attach(&jll, &ps);

    if (iop_junpack(&jll, st, v, true)) {
        sb_reset(&buf);
        iop_jlex_write_error(&jll, &buf);
        httpd_reject(obj_vcast(httpd_query, iq), BAD_REQUEST, "%s", buf.data);
        res = -1;
        goto end;
    }
    iop_jlex_detach(&jll);
    *vout = v;

  end:
    iop_jlex_wipe(&jll);
    return res;
}

static int t_parse_soap(ichttp_query_t *iq, ic__simple_hdr__t *hdr,
                        ichttp_cb_t **cbout, void **vout)
{
#define xr  xmlr_g
#define XCHECK(expr)  XMLR_CHECK(expr, goto xmlerror)

    const char *buf = iq->payload.data;
    int         len = iq->payload.len;

    ichttp_trigger_cb_t *tcb = container_of(iq->trig_cb, ichttp_trigger_cb_t, cb);
    ichttp_cb_t *cbe;
    char *xval;
    int pos;

    /* Initialize the xmlReader object */
    XCHECK(xmlr_setup(&xr, buf, len));
    XCHECK(xmlr_node_open_s(xr, "Envelope"));
    if (XCHECK(xmlr_node_try_open_s(xr, "Header"))) {
        do {
            int res = xmlr_node_enter_s(xr, "callerIdentity",
                                        XMLR_ENTER_MISSING_OK);

            if (res == 1) {
                XCHECK(xmlr_node_want_s(xr, "login"));
                XCHECK(t_xmlr_get_str(xr, false, &xval, &hdr->login.len));
                hdr->login.s = xval;

                XCHECK(xmlr_node_want_s(xr, "password"));
                XCHECK(t_xmlr_get_str(xr, false, &xval, &hdr->password.len));
                hdr->password.s = xval;

                XCHECK(xmlr_node_close(xr)); /* </callerIdentity> */
            } else
            if (res == 0 || res == XMLR_NOCHILD) {
                XCHECK(xmlr_next_sibling(xr));
            } else {
                XCHECK(res);
            }
        } while (!xmlr_node_is_closing(xr));
        XCHECK(xmlr_node_close(xr)); /* </Header> */
    }

    XCHECK(xmlr_node_open_s(xr, "Body"));

    pos = qm_find(ichttp_cbs, &tcb->impl, XML_CNAME(xr));
    if (pos < 0) {
        __ichttp_reply_soap_err(ichttp_query_to_slot(iq), false, "unknown rpc");
        goto error;
    }
    iq->cbe = *cbout = cbe = ichttp_cb_dup(tcb->impl.values[pos]);

    *vout = t_new(byte, cbe->fun->args->size);
    XCHECK(iop_xunpack(xr, t_pool(), cbe->fun->args, *vout));
    /* Close opened elements */

    XCHECK(xmlr_node_close(xr)); /* </Body>     */
    XCHECK(xmlr_node_close(xr)); /* </Envelope> */
    xmlr_close(&xr);
    return 0;

  xmlerror:
    __ichttp_reply_soap_err(ichttp_query_to_slot(iq), false,
                            xmlr_get_err() ?: "parsing error");
  error:
    xmlr_close(&xr);
    return -1;

#undef XCHECK
#undef xr
}

static int is_ctype_json(const httpd_qinfo_t *info)
{
    for (int i = info->hdrs_len; i-- > 0; ) {
        const http_qhdr_t *hdr = info->hdrs + i;

        if (hdr->wkhdr == HTTP_WKHDR_CONTENT_TYPE) {
            pstream_t v = hdr->val;

            ps_skipspaces(&v);
            if (ps_skipcasestr(&v, "application/json"))
                return false;
            return true;
        }
    }
    return false;
}

static void ichttp_query_on_done(httpd_query_t *q)
{
    t_scope;
    ichttp_query_t *iq  = obj_vcast(ichttp_query, q);
    ichttp_trigger_cb_t *tcb = container_of(iq->trig_cb, ichttp_trigger_cb_t, cb);
    ic__hdr__t      hdr = IOP_UNION_VA(ic__hdr, simple,
       .kind = (tcb->auth_kind ? CLSTR_STR_V(tcb->auth_kind) : CLSTR_NULL_V),
       .payload = iq->payload.len,
    );
    ichttp_cb_t    *cbe = NULL;
    ic_cb_entry_t  *e;
    pstream_t       login, pw, url = iq->qinfo->query;

    uint64_t    slot = ichttp_query_to_slot(iq);
    ichannel_t *pxy;
    ic__hdr__t *pxy_hdr = NULL;
    void *value;
    int res;

    ps_skipstr(&url, "/");

    if (ps_len(&url)) {
        pstream_t ps = url;

        if (ps_skip_uptochr(&url, '/') < 0) {
          not_found:
            httpd_reject(obj_vcast(httpd_query, iq), NOT_FOUND, "");
            return;
        }
        __ps_skip(&url, 1);
        if (ps_skip_uptochr(&url, '/') < 0) {
            ps.s_end = url.s_end;
        } else {
            ps.s_end = url.s;
        }
        res = qm_find(ichttp_cbs, &tcb->impl, t_dupz(ps.s, ps_len(&ps)));
        if (res < 0)
            goto not_found;
        iq->cbe = cbe = ichttp_cb_dup(tcb->impl.values[res]);

        if (is_ctype_json(q->qinfo)) {
            iq->json = true;
            res = t_parse_json(iq, cbe, &value);
        } else {
            httpd_reject(obj_vcast(httpd_query, iq), NOT_ACCEPTABLE,
                         "Content-Type must be application/json");
            return;
        }
    } else {
        res = t_parse_soap(iq, &hdr.simple, &cbe, &value);
    }
    if (unlikely(res < 0))
        return;

    if (t_httpd_qinfo_get_basic_auth(q->qinfo, &login, &pw) == 0) {
        hdr.simple.login    = CLSTR_INIT_V(login.s, ps_len(&login));
        hdr.simple.password = CLSTR_INIT_V(pw.s,    ps_len(&pw));
    }

    switch ((e = &cbe->e)->cb_type) {
      case IC_CB_NORMAL:
      case IC_CB_WS_SHARED:
        t_seal();
        (*e->u.cb.cb)(NULL, slot, value, &hdr);
        if (cbe->fun->async)
            httpd_reply_202accepted(q);
        t_unseal();
        return;

      case IC_CB_PROXY_P:
        pxy     = e->u.proxy_p.ic_p;
        pxy_hdr = e->u.proxy_p.hdr_p;
        break;
      case IC_CB_PROXY_PP:
        pxy     = *e->u.proxy_pp.ic_pp;
        if (e->u.proxy_pp.hdr_pp)
            pxy_hdr = *e->u.proxy_pp.hdr_pp;
        break;
      case IC_CB_DYNAMIC_PROXY:
        {
            ic_dynproxy_t dynproxy;
            dynproxy = (*e->u.dynproxy.get_ic)(e->u.dynproxy.priv);
            pxy      = dynproxy.ic;
            pxy_hdr  = dynproxy.hdr;
        }
        break;
      default:
        e_panic("should not happen");
        break;
    }

    if (likely(pxy)) {
        ic_msg_t *msg = ic_msg_new(sizeof(uint64_t));

        if (!ps_len(&login) && pxy_hdr) {
            /* XXX on simple header we write the payload size of the HTTP query */
            if (unlikely(pxy_hdr->iop_tag == IOP_UNION_TAG(ic__hdr, simple)))
            {
                ic__simple_hdr__t *shdr = &pxy_hdr->simple;
                shdr->payload = iq->payload.len;
            }

            msg->hdr = pxy_hdr;
        } else {
            assert(!pxy_hdr);
            /* XXX We do not support header replacement with proxyfication */
            msg->hdr = &hdr;
        }
        msg->cmd = cbe->cmd;
        msg->rpc = cbe->fun;
        msg->async = cbe->fun->async;
        if (!msg->async) {
            msg->cb = IC_PROXY_MAGIC_CB;
            *(uint64_t *)msg->priv = slot;
        }
        __ic_bpack(msg, cbe->fun->args, value);
        __ic_query(pxy, msg);
        if (msg->async)
            httpd_reply_202accepted(q);
    } else {
        __ichttp_reply_err(slot, IC_MSG_PROXY_ERROR);
    }
}

static void httpd_trigger__ichttp_destroy(httpd_trigger_cb_t *tcb)
{
    ichttp_trigger_cb_t *cb = container_of(tcb, ichttp_trigger_cb_t, cb);

    qm_for_each_pos(ichttp_cbs, pos, &cb->impl) {
        ichttp_cb_delete(&cb->impl.values[pos]);
    }
    qm_wipe(ichttp_cbs, &cb->impl);
    p_delete(&cb);
}

static void httpd_trigger__ichttp_cb(httpd_trigger_cb_t *tcb, httpd_query_t *q,
                                     const httpd_qinfo_t *req)
{
    ichttp_trigger_cb_t *cb = container_of(tcb, ichttp_trigger_cb_t, cb);

    httpd_bufferize(q, cb->query_max_size);
    q->on_done = ichttp_query_on_done;
    q->qinfo   = httpd_qinfo_dup(req);
}


ichttp_trigger_cb_t *
httpd_trigger__ichttp(const iop_mod_t *mod, const char *schema,
                      unsigned szmax)
{
    ichttp_trigger_cb_t *cb = p_new(ichttp_trigger_cb_t, 1);

    cb->cb.cb          = &httpd_trigger__ichttp_cb;
    cb->cb.query_cls   = obj_class(ichttp_query);
    cb->cb.destroy     = &httpd_trigger__ichttp_destroy;
    cb->schema         = schema;
    cb->mod            = mod->ifaces;
    cb->query_max_size = szmax;
    qm_init(ichttp_cbs, &cb->impl, true);
    return cb;
}

ichttp_cb_t *
__ichttp_register(ichttp_trigger_cb_t *tcb,
                  const iop_iface_alias_t *alias,
                  const iop_rpc_t *fun,
                  int32_t cmd)
{
    ichttp_cb_t *cb = ichttp_cb_new();
    int keysize = alias->name.len + 1 + fun->name.len;

    cb->cmd      = cmd;
    cb->fun      = fun;
    cb->name     = p_new(char, keysize + sizeof("Req"));
    cb->name_uri = p_new(char, keysize + sizeof(""));
    cb->name_res = p_new(char, keysize + sizeof("Res"));
    cb->name_exn = p_new(char, keysize + sizeof(".Fault"));
    sprintf(cb->name,     "%s.%sReq",    alias->name.s, fun->name.s);
    sprintf(cb->name_uri, "%s/%s",       alias->name.s, fun->name.s);
    sprintf(cb->name_res, "%s.%sRes",    alias->name.s, fun->name.s);
    sprintf(cb->name_exn, "%s.%s.Fault", alias->name.s, fun->name.s);
    if (qm_add(ichttp_cbs, &tcb->impl, cb->name, cb))
        e_panic("programming error");
    qm_add(ichttp_cbs, &tcb->impl, cb->name_uri, ichttp_cb_dup(cb));
    return cb;
}
