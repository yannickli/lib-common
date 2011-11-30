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

#include "xmlpp.h"
#include "iop-rpc.h"

void
__ichttp_reply(uint64_t slot, int cmd, const iop_struct_t *st, const void *v)
{
    ichttp_query_t *iq = ichttp_slot_to_query(slot);
    httpd_query_t  *q  = obj_vcast(httpd_query, iq);
    ichttp_trigger_cb_t *tcb;
    outbuf_t *ob;
    sb_t *out;
    int oldlen;
    xmlpp_t pp;
    http_code_t code;
    size_t oblen;

    if (cmd == IC_MSG_OK) {
        ob = httpd_reply_hdrs_start(q, code = HTTP_CODE_OK, true);
    } else {
        ob = httpd_reply_hdrs_start(q, code = HTTP_CODE_INTERNAL_SERVER_ERROR, true);
    }
    if (iq->json) {
        ob_adds(ob, "Content-Type: application/json; charset=utf-8\r\n");
    } else {
        ob_adds(ob, "Content-Type: text/xml; charset=utf-8\r\n");
    }
    httpd_reply_hdrs_done(q, -1, false);
    oblen = ob->length;

    out = outbuf_sb_start(ob, &oldlen);
    tcb = container_of(iq->trig_cb, ichttp_trigger_cb_t, cb);

    if (iq->json) {
        iop_jpack(st, v, iop_sb_write, out, true);
    } else {
        xmlpp_open_banner(&pp, out);
        pp.nospace = true;
        xmlpp_opentag(&pp, "s:Envelope");
        xmlpp_putattr(&pp, "xmlns:s", "http://schemas.xmlsoap.org/soap/envelope/");
        xmlpp_putattr(&pp, "xmlns:n", tcb->schema);

        xmlpp_opentag(&pp, "s:Body");
        if (cmd == IC_MSG_OK) {
            ichttp_cb_t *cbe = iq->cbe;

            if (v) {
                sb_addf(out, "<n:%s>", cbe->name_res);
                iop_xpack(out, st, v, false, true);
                sb_addf(out, "</n:%s>", cbe->name_res);
            } else {
                sb_addf(out, "<n:%s />", cbe->name_res);
            }
        } else {
            ichttp_cb_t *cbe = iq->cbe;

            xmlpp_opentag(&pp, "s:Fault");
            xmlpp_opentag(&pp, "faultcode");
            xmlpp_puts(&pp,    "s:Server");
            xmlpp_opensib(&pp, "faultstring");
            xmlpp_opensib(&pp, "detail");

            /* FIXME handle union of exceptions which are an array of exceptions */
            if (v) {
                sb_addf(out, "<n:%s>", cbe->name_exn);
                iop_xpack(out, st, v, false, true);
                sb_addf(out, "</n:%s>", cbe->name_exn);
            } else {
                sb_addf(out, "<n:%s />", cbe->name_exn);
            }
        }
        pp.can_do_attr = false;
        xmlpp_close(&pp);
    }
    outbuf_sb_end(ob, oldlen);

    oblen = ob->length - oblen;
    if (tcb->on_reply)
        (*tcb->on_reply)(tcb, iq, oblen, code);
    httpd_reply_done(q);
}

void __ichttp_reply_soap_err(uint64_t slot, bool serverfault, const char *err)
{
    ichttp_query_t *iq = ichttp_slot_to_query(slot);
    httpd_query_t  *q  = obj_vcast(httpd_query, iq);
    ichttp_trigger_cb_t *tcb;
    outbuf_t *ob;
    sb_t *out;
    int oldlen;
    size_t oblen;
    xmlpp_t pp;

    assert (!iq->json);

    ob = httpd_reply_hdrs_start(q, HTTP_CODE_INTERNAL_SERVER_ERROR, true);
    ob_adds(ob, "Content-Type: text/xml; charset=utf-8\r\n");
    httpd_reply_hdrs_done(q, -1, false);
    oblen = ob->length;

    out = outbuf_sb_start(ob, &oldlen);
    tcb = container_of(iq->trig_cb, ichttp_trigger_cb_t, cb);

    xmlpp_open_banner(&pp, out);
    pp.nospace = true;
    xmlpp_opentag(&pp, "s:Envelope");
    xmlpp_putattr(&pp, "xmlns:s",
                  "http://schemas.xmlsoap.org/soap/envelope/");

    xmlpp_opentag(&pp, "s:Body");
    xmlpp_opentag(&pp, "s:Fault");
    xmlpp_opentag(&pp, "s:faultcode");
    if (serverfault) {
        xmlpp_puts(&pp,"s:Server");
    } else {
        xmlpp_puts(&pp,"s:Client");
    }
    xmlpp_opensib(&pp, "s:faultstring");
    xmlpp_puts(&pp,    err);
    xmlpp_close(&pp);
    outbuf_sb_end(ob, oldlen);

    oblen = ob->length - oblen;
    if (tcb->on_reply)
        (*tcb->on_reply)(tcb, iq, oblen, HTTP_CODE_INTERNAL_SERVER_ERROR);
    httpd_reply_done(q);
}

void __ichttp_reply_err(uint64_t slot, int err)
{
    ichttp_query_t *iq = ichttp_slot_to_query(slot);

    switch (err) {
      case IC_MSG_OK:
      case IC_MSG_EXN:
        e_panic("should not happen");
      case IC_MSG_RETRY:
      case IC_MSG_ABORT:
      case IC_MSG_PROXY_ERROR:
        if (iq->json) {
            httpd_reject(obj_vcast(httpd_query, iq), GATEWAY_TIMEOUT, "");
        } else {
            __ichttp_reply_soap_err(slot, true, "query temporary refused");
        }
        break;
      case IC_MSG_INVALID:
      case IC_MSG_SERVER_ERROR:
        if (iq->json) {
            httpd_reject(obj_vcast(httpd_query, iq), INTERNAL_SERVER_ERROR, "");
        } else {
            __ichttp_reply_soap_err(slot, true, "query refused by server");
        }
        break;
      case IC_MSG_UNIMPLEMENTED:
        if (iq->json) {
            httpd_reject(obj_vcast(httpd_query, iq), INTERNAL_SERVER_ERROR, "");
        } else {
            __ichttp_reply_soap_err(slot, true, "query not implemented by server");
        }
        break;
    }
}

void __ichttp_proxify(uint64_t slot, int cmd, const void *data, int dlen)
{
    ichttp_query_t  *iq  = ichttp_slot_to_query(slot);
    const iop_rpc_t *rpc = iq->cbe->fun;
    const iop_struct_t *st;
    pstream_t ps;
    void *v;

    iq->iop_res_size = IC_MSG_HDR_LEN + dlen;
    switch (cmd) {
      case IC_MSG_OK:
        st = rpc->result;
        break;
      case IC_MSG_EXN:
        st = rpc->exn;
        break;
      default:
        __ichttp_reply_err(slot, cmd);
        return;
    }

    {
        t_scope;

        v  = t_new_raw(char, st->size);
        ps = ps_init(data, dlen);
        if (unlikely(iop_bunpack(t_pool(), st, v, ps, false) < 0)) {
            e_trace(0, "%s: answer with invalid encoding", rpc->name.s);
            __ichttp_reply_err(slot, IC_MSG_INVALID);
        } else {
            __ichttp_reply(slot, cmd, st, v);
        }
    }
}
