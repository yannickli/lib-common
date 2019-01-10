/**************************************************************************/
/*                                                                        */
/*  Copyright (C) INTERSEC SA                                             */
/*                                                                        */
/*  Should you receive a copy of this source code, you must check you     */
/*  have a proper, written authorization of INTERSEC to hold it. If you   */
/*  don't have such an authorization, you must DELETE all source code     */
/*  files in your possession, and inform INTERSEC of the fact you obtain  */
/*  these files. Should you not comply to these terms, you can be         */
/*  prosecuted in the extent permitted by applicable law.                 */
/*                                                                        */
/**************************************************************************/

#ifndef NDEBUG
#  include <valgrind/valgrind.h>
#else
#  define RUNNING_ON_VALGRIND  false
#endif

#include "unix.h"
#include "iop-rpc.h"

/* IC_MSG_STREAM_CONTROL messages uses the "slot" to encode the command.
 */
enum ic_msg_sc_slots {
    IC_SC_BYE = 1,
    IC_SC_NOP,
};

qm_k32_t(ic, ichannel_t *);

static mem_pool_t *ic_mp_g;
static QM(ic,    ic_h_g,     false);
const QM(ic_cbs, ic_no_impl, false);

/*----- messages stuff -----*/

void ic_initialize(void)
{
    assert (!ic_mp_g);
    ic_mp_g = mem_fifo_pool_new(1 << 20);
}
void ic_shutdown(void)
{
    qm_wipe(ic, &ic_h_g);
    mem_fifo_pool_delete(&ic_mp_g);
}

ic_msg_t *ic_msg_new_fd(int fd, int len)
{
    ic_msg_t *msg;

    assert (len >= 0);
    msg = mp_new_extra(ic_mp_g, ic_msg_t, len);
    msg->fd = fd;

    return msg;
}

ic_msg_t *ic_msg_new(int len)
{
    ic_msg_t *res;

    assert (len >= 0);
    res = mp_new_extra(ic_mp_g, ic_msg_t, len);
    res->fd = -1;

    return res;
}

ic_msg_t *ic_msg_proxy_new(int fd, uint64_t slot, const ic__hdr__t *hdr)
{
    ic_msg_t *msg = mp_new_extra(ic_mp_g, ic_msg_t, sizeof(slot));

    *(uint64_t *)msg->priv = slot;
    msg->fd  = fd;
    msg->hdr = hdr;
    return msg;
}

void ic_msg_delete(ic_msg_t **msgp)
{
    ic_msg_t *msg = *msgp;
    if (unlikely(!msg))
        return;
    if (msg->fini)
        (*msg->fini)(msg->priv);
    if (msg->fd >= 0)
        close(msg->fd);
    mp_delete(ic_mp_g, &msg->data);
    mp_delete(ic_mp_g, msgp);
}

static void ic_watch_act_soft(el_t ev, el_data_t priv);

static void ic_reply_err2(ichannel_t *ic, uint64_t slot, int err,
                          const lstr_t *err_str);
static ichannel_t *ic_get_from_slot(uint64_t slot);
static void ic_queue(ichannel_t *ic, ic_msg_t *msg, uint32_t flags);
static void ___ic_query_flags(ichannel_t *ic, ic_msg_t *msg, uint32_t flags);
static void ic_proxify(ichannel_t *pxy_ic, ic_msg_t *msg, int cmd,
                       const void *data, int dlen)
{
    uint64_t slot = *(uint64_t *)msg->priv;
    ichannel_t *ic;

    if (unlikely(ic_slot_is_http(slot))) {
        __ichttp_proxify(slot, -cmd, data, dlen);
        return;
    }
    ic = ic_get_from_slot(slot);
    if (unlikely(!ic))
        return;
    if (likely(ic->elh) && likely(ic->id == slot >> 32)) {
        ic_msg_t *tmp = ic_msg_new_fd(pxy_ic ? ic_get_fd(pxy_ic) : -1, 0);
        void *buf;

#ifdef IC_DEBUG_REPLIES
        int32_t pos = qh_del_key(ic_replies, &ic->dbg_replies, slot);
        if (pos < 0)
            e_panic("answering to slot %d twice or unexpected answer!",
                    (uint32_t)slot);
#endif
        assert (ic->pending > 0);
        ic->pending--;
        tmp->slot = slot & IC_MSG_SLOT_MASK;
        tmp->cmd  = cmd;
        buf = __ic_get_buf(tmp, dlen);
        if (data) {
            memcpy(buf, data, dlen);
        } else {
            assert (dlen == 0);
        }
        ic_queue(ic, tmp, 0);
    }
}

void __ic_forward_reply_to(ichannel_t *pxy_ic, uint64_t slot, int cmd,
                           const void *res, const void *exn)
{
    ichannel_t *ic;

    if (unlikely(ic_slot_is_http(slot))) {
        __ichttp_forward_reply(slot, cmd, res, exn);
        return;
    }

    if (cmd != IC_MSG_OK && cmd != IC_MSG_EXN) {
        assert (!res);
        ic_reply_err2(NULL, slot, cmd, exn);
        return;
    }

    if (unlikely(!(ic = ic_get_from_slot(slot))))
        return;
    if (likely(ic->elh) && likely(ic->id == slot >> 32)) {
        ic_msg_t *tmp = ic_msg_new_fd(pxy_ic ? ic_get_fd(pxy_ic) : -1, 0);
        pstream_t *ps = ((pstream_t **)(cmd == IC_MSG_OK ? res : exn))[-1];

#ifdef IC_DEBUG_REPLIES
        int32_t pos = qh_del_key(ic_replies, &ic->dbg_replies, slot);
        if (pos < 0)
            e_panic("answering to slot %d twice or unexpected answer!",
                    (uint32_t)slot);
#endif
        assert (ic->pending > 0);
        ic->pending--;
        tmp->slot = slot & IC_MSG_SLOT_MASK;
        tmp->cmd  = -cmd;
        memcpy(__ic_get_buf(tmp, ps_len(ps)), ps->p, ps_len(ps));
        ic_queue(ic, tmp, 0);
    }
}

static void ic_cancel_all(ichannel_t *ic)
{
    ic_msg_t *msg;
    qm_t(ic_msg) h;

#ifndef NDEBUG
    ic->cancel_guard = true;
#endif

    while (!htlist_is_empty(&ic->iov_list)) {
        msg = htlist_pop_entry(&ic->iov_list, ic_msg_t, msg_link);
        if (msg->cmd <= 0 || msg->slot == 0)
            ic_msg_delete(&msg);
    }
    while (!htlist_is_empty(&ic->msg_list)) {
        msg = htlist_pop_entry(&ic->msg_list, ic_msg_t, msg_link);
        if (msg->cmd <= 0 || msg->slot == 0)
            ic_msg_delete(&msg);
    }

    h = ic->queries;
    qm_init(ic_msg, &ic->queries, false);
    qm_for_each_pos(ic_msg, pos, &h) {
        qm_del_at(ic_msg, &h, pos);

        msg = h.values[pos];
        if (msg->cb == IC_PROXY_MAGIC_CB) {
            ic_proxify(ic, msg, -IC_MSG_ABORT, NULL, 0);
        } else {
            (*msg->cb)(ic, msg, IC_MSG_ABORT, NULL, NULL);
        }
        ic_msg_delete(&msg);
    }
    qm_wipe(ic_msg, &h);

    /* if that crashes, one of the IC_MSG_ABORT callback reenqueues directly in
     * that ichannel_t which is forbidden, fix the code
     */
    assert (qm_len(ic_msg, &ic->queries) == 0);

#ifndef NDEBUG
    ic->cancel_guard = false;
#endif
}

static void ic_choose_id(ichannel_t *ic)
{
    static uint32_t nextid = 0;
    do {
        if (unlikely(nextid == IC_ID_MAX))
            nextid = 0;
    } while (unlikely(qm_add(ic, &ic_h_g, ++nextid, ic) < 0));
    ic->id = nextid;
}

static void ic_drop_id(ichannel_t *ic)
{
    qm_del_key(ic, &ic_h_g, ic->id);
    ic->id = 0;
}


/*----- binary protocol handling -----*/

static ic_msg_t *ic_query_take(ichannel_t *ic, uint32_t slot)
{
    int32_t pos = qm_del_key(ic_msg, &ic->queries, slot);
    return pos < 0 ? NULL : ic->queries.values[pos];
}

static void ic_msg_abort(ichannel_t *ic, ic_msg_t *msg)
{
    if (msg->slot) {
        __unused__ ic_msg_t *tmp = ic_query_take(ic, msg->slot);

        assert (tmp == msg);
        if (msg->cb == IC_PROXY_MAGIC_CB) {
            ic_proxify(ic, msg, -IC_MSG_RETRY, NULL, 0);
        } else {
            (*msg->cb)(ic, msg, IC_MSG_ABORT, NULL, NULL);
        }
    }
    ic_msg_delete(&msg);
}

static void ic_msg_filter_on_bye(ichannel_t *ic)
{
    htlist_t l;

    htlist_move(&l, &ic->msg_list);
    if (!ic->is_stream && ic->wpos)
        htlist_add_tail(&ic->msg_list, htlist_pop(&l));

    while (!htlist_is_empty(&l)) {
        ic_msg_t *msg = htlist_pop_entry(&l, ic_msg_t, msg_link);

        if (msg->cmd > 0) {
            ic_msg_abort(ic, msg);
        } else {
            htlist_add_tail(&ic->msg_list, &msg->msg_link);
        }
    }
}

static int ic_write_stream(ichannel_t *ic, int fd)
{
    bool timer_restarted = false;

    do {
        while (!htlist_is_empty(&ic->msg_list) && ic->iov_total_len < IC_PKT_MAX) {
            ic_msg_t *msg = htlist_pop_entry(&ic->msg_list, ic_msg_t, msg_link);

            htlist_add_tail(&ic->iov_list, &msg->msg_link);
            qv_append(iovec, &ic->iov, MAKE_IOVEC(msg->data, msg->dlen));
            ic->iov_total_len += msg->dlen;
        }

        if (ic->iov_total_len) {
            ssize_t res = writev(fd, ic->iov.tab, MIN(ic->iov.len, IOV_MAX));
            int killed;

            if (res < 0)
                return ERR_RW_RETRIABLE(errno) ? 0 : -1;
            if (ic->timer && !timer_restarted) {
                el_timer_restart(ic->timer, 0);
                timer_restarted = true;
            }
            killed = iovec_vector_kill_first(&ic->iov, res);
            ic->iov_total_len -= res;

            while (killed-- > 0) {
                ic_msg_t *tmp = htlist_pop_entry(&ic->iov_list, ic_msg_t, msg_link);

                if (tmp->cmd <= 0 || tmp->slot == 0) {
                    ic_msg_delete(&tmp);
                }
            }
        }
    } while (ic->iov_total_len || !htlist_is_empty(&ic->msg_list));

    el_fd_set_mask(ic->elh, POLLIN);
    return 0;
}

static void ic_prepare_cmsg(ichannel_t *ic, struct msghdr *msgh,
                            void *cbuf, int clen, int fdc, int *fdv)
{
    struct cmsghdr *cmsg;

    msgh->msg_control    = cbuf;
    msgh->msg_controllen = clen;

    cmsg = CMSG_FIRSTHDR(msgh);
    if (fdc) {
        cmsg->cmsg_level = SOL_SOCKET;
        cmsg->cmsg_type  = SCM_RIGHTS;
        cmsg->cmsg_len   = CMSG_LEN(sizeof(int) * fdc);
        memcpy(CMSG_DATA(cmsg), fdv, sizeof(int) * fdc);

        /* XXX: Erase padding to avoid Valgrind warning */
        if (RUNNING_ON_VALGRIND) {
            p_clear(CMSG_DATA(cmsg) + sizeof(int) * fdc,
                    CMSG_ALIGN(cmsg->cmsg_len) - sizeof(int) * fdc);
        }
        cmsg = (struct cmsghdr *)((char *)cmsg + CMSG_ALIGN(cmsg->cmsg_len));
    }

    msgh->msg_controllen = (char *)cmsg - (char *)cbuf;
}

static int ic_write_seq(ichannel_t *ic, int fd)
{
    bool timer_restarted = false;
    int  fdv[32];
    struct iovec iov[IOV_MAX];
    int fdc = 0, size = 0, wpos = ic->wpos;
    struct msghdr msgh = { .msg_iov = iov, };
#define iovc  msgh.msg_iovlen

    while (!htlist_is_empty(&ic->msg_list)) {
        size_t to_drop = 0;

        htlist_for_each(it, &ic->msg_list) {
            ic_msg_t *msg = htlist_entry(it, ic_msg_t, msg_link);

            if (unlikely(fdc == countof(fdv) && msg->fd >= 0))
                break;

            if (unlikely(size + msg->dlen - wpos > IC_PKT_MAX)) {
                int avail = IC_PKT_MAX - size;

                iov[iovc++] = (struct iovec){
                    .iov_base = (char *)msg->data + wpos,
                    .iov_len  = avail,
                };
                wpos += avail;
                break;
            }

            iov[iovc++] = (struct iovec){
                .iov_base = (char *)msg->data + wpos,
                .iov_len  = msg->dlen - wpos,
            };
            size += msg->dlen - wpos;
            wpos  = 0;
            if (msg->fd >= 0)
                fdv[fdc++] = msg->fd;
            to_drop++;

            if (unlikely(iovc >= countof(iov) - 1) || unlikely(size >= IC_PKT_MAX))
                break;
        }

        if (likely(iovc)) {
            char cbuf[CMSG_SPACE(sizeof(int) * countof(fdv)) +
                      CMSG_SPACE(sizeof(struct ucred))];

            ic_prepare_cmsg(ic, &msgh, cbuf, sizeof(cbuf), fdc, fdv);
            if (sendmsg(fd, &msgh, 0) < 0)
                return ERR_RW_RETRIABLE(errno) ? 0 : -1;

            if (ic->timer && !timer_restarted) {
                el_timer_restart(ic->timer, 0);
                timer_restarted = true;
            }
        }

        size = fdc = iovc = 0;
        ic->wpos = wpos;
        while (to_drop-- > 0) {
            ic_msg_t *tmp = htlist_pop_entry(&ic->msg_list, ic_msg_t, msg_link);

            if (tmp->cmd <= 0 || tmp->slot == 0) {
                ic_msg_delete(&tmp);
            }
        }
#undef iovc
    }
    el_fd_set_mask(ic->elh, POLLIN);
    return 0;
}


static void
ic_parse_cmsg(ichannel_t *ic, struct msghdr *msgh, int *fdcp, int **fdvp)
{
    struct cmsghdr *cmsg;

    for (cmsg = CMSG_FIRSTHDR(msgh); cmsg; cmsg = CMSG_NXTHDR(msgh, cmsg)) {
        if (cmsg->cmsg_level != SOL_SOCKET)
            continue;
        switch (cmsg->cmsg_type) {
          case SCM_RIGHTS:
            *fdcp = (cmsg->cmsg_len - sizeof(*cmsg)) / sizeof(int);
            *fdvp = (int *)CMSG_DATA(cmsg);
            break;
        }
    }
}

static ALWAYS_INLINE int
ic_read_process_answer(ichannel_t *ic, int cmd, uint32_t slot,
                       const void *data, int dlen)
{
    ic_msg_t *tmp = ic_query_take(ic, slot);
    const iop_struct_t *st;
    void *value;
    pstream_t ps;

    if (unlikely(!tmp)) {
        errno = 0;
        return -1;
    }

    if (tmp->cb == IC_PROXY_MAGIC_CB) {
        ic_proxify(ic, tmp, cmd, data, dlen);
        ic_msg_delete(&tmp);
        return 0;
    }

    cmd = -cmd;
    if (likely(cmd == IC_MSG_OK)) {
        st = tmp->rpc->result;
    } else
    if (cmd == IC_MSG_EXN) {
        st = tmp->rpc->exn;
    } else {
        assert (dlen == 0 || cmd == IC_MSG_INVALID);
        if (dlen > 0) {
            lstr_t err = LSTR_INIT_V(data, dlen - 1);
            iop_set_err2(&err);
            (*tmp->cb)(ic, tmp, cmd, NULL, &err);
        } else {
            iop_clear_err();
            (*tmp->cb)(ic, tmp, cmd, NULL, NULL);
        }
        ic_msg_delete(&tmp);
        return 0;
    }
    {
        t_scope;

        value = t_new_raw(char, sizeof(pstream_t *) + st->size);
        ps    = ps_init(data, dlen);
        *(pstream_t **)value = &ps;
        value = (pstream_t **)value + 1;

        if (unlikely(iop_bunpack(t_pool(), st, value, ps, false) < 0)) {
#ifndef NDEBUG
            if (!iop_get_err()) {
                e_trace(0, "rpc(%04x:%04x):%s: answer with invalid encoding",
                        (tmp->cmd >> 16) & 0x7fff, tmp->cmd & 0x7fff,
                        tmp->rpc->name.s);
            }
#endif
            t_seal();
            (*tmp->cb)(ic, tmp, IC_MSG_INVALID, NULL, NULL);
        } else
        if (cmd == IC_MSG_OK) {
            t_seal();
            (*tmp->cb)(ic, tmp, cmd, value, NULL);
        } else {
            t_seal();
            (*tmp->cb)(ic, tmp, cmd, NULL, value);
        }
    }
    ic_msg_delete(&tmp);
    return 0;
}

static lstr_t ic_get_client_addr(ichannel_t *ic)
{
    if (!ic->peer_address.s) {
        t_scope;
        socklen_t saddr_len = sizeof(ic->su.ss);

        if (getpeername(el_fd_get_fd(ic->elh), (struct sockaddr *)&ic->su.ss,
                        &saddr_len) < 0)
        {
            e_error("unable to get peer name: %m");
            ic->peer_address = LSTR_EMPTY_V;
        } else {
            ic->peer_address = lstr_dup(t_addr_fmt_lstr(&ic->su));
        }
    }
    return ic->peer_address.len ? lstr_dupc(ic->peer_address) : LSTR_NULL_V;
}

static ALWAYS_INLINE void
ic_read_process_query(ichannel_t *ic, int cmd, uint32_t slot,
                      uint32_t flags, const void *data, int dlen)
{
    const ic_cb_entry_t *e;
    const iop_struct_t *st;
    ichannel_t *pxy;
    ic__hdr__t *hdr = NULL, *pxy_hdr = NULL;
    void *value;
    pstream_t ps;
    int pos;

    pos = qm_find_safe(ic_cbs, ic->impl, cmd);
    if (unlikely(pos < 0)) {
        e_trace(0, "received query for unimplemented RPC (%04x:%04x)",
                (cmd >> 16) & 0x7fff, cmd & 0x7fff);
        if (slot)
            ic_reply_err(ic, MAKE64(ic->id, slot), IC_MSG_UNIMPLEMENTED);
        return;
    }
    e = ic->impl->values + pos;

    switch (e->cb_type) {
      case IC_CB_NORMAL:
      case IC_CB_WS_SHARED:
        {
            t_scope;

            /* XXX works for both IC_CB_NORMAL & IC_CB_WS_SHARED */
            st    = e->u.cb.rpc->args;
            value = t_new_raw(char, st->size);
            ps    = ps_init(data, dlen);
            if (unlikely(flags & IC_MSG_HAS_HDR)) {
                hdr = t_new_raw(ic__hdr__t, 1);
                if (unlikely(iop_bunpack_multi(t_pool(), &ic__hdr__s, hdr, &ps, false)) < 0)
                {
                    e_trace(0, "query %04x:%04x, type %s: invalid header encoding",
                            (cmd >> 16) & 0x7fff, cmd & 0x7fff, st->fullname.s);
                    goto invalid;
                }
                /* XXX on simple header we write the payload size of the iop query */
                if (unlikely(hdr->iop_tag == IOP_UNION_TAG(ic__hdr, simple))) {
                    ic__simple_hdr__t *shdr = &hdr->simple;
                    if (shdr->payload < 0)
                        shdr->payload = dlen;
                    if (!shdr->host.len) {
                        shdr->host = ic_get_client_addr(ic);
                    }
                }
            }

            if (unlikely(iop_bunpack(t_pool(), st, value, ps, false) < 0)) {
#ifndef NDEBUG
                if (!iop_get_err()) {
                    e_trace(0, "query %04x:%04x, type %s: invalid encoding",
                            (cmd >> 16) & 0x7fff, cmd & 0x7fff, st->fullname.s);
                }
#endif
              invalid:
                t_seal();
                if (slot) {
                    lstr_t err_str = iop_get_err_lstr();

                    ic_reply_err2(ic, MAKE64(ic->id, slot), IC_MSG_INVALID,
                                  &err_str);
                }
            } else {
                t_seal();
                ic->desc = e->u.cb.rpc;
                ic->cmd  = cmd;
                (*e->u.cb.cb)(ic, MAKE64(ic->id, slot), value, hdr);
                ic->desc = NULL;
                ic->cmd  = 0;
            }
        }
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

    if (unlikely(!pxy || !ic_is_ready(pxy))) {
        if (slot)
            ic_reply_err(ic, MAKE64(ic->id, slot), IC_MSG_PROXY_ERROR);
    } else {
        ic_msg_t *tmp = ic_msg_proxy_new(ic_get_fd(ic), slot, NULL);

        if (slot) {
            tmp->cb = IC_PROXY_MAGIC_CB;
        } else {
            tmp->async = true;
        }
        tmp->cmd = cmd;
        *(uint64_t *)tmp->priv = MAKE64(ic->id, slot);

        if (!(flags & IC_MSG_HAS_HDR) && pxy_hdr) {
            qv_t(i32) szs;
            uint8_t *buf;
            int hlen;

            /* Pack the given header in proxyfied request */
            qv_inita(i32, &szs, 128);
            hlen = iop_bpack_size(&ic__hdr__s, pxy_hdr, &szs);
            buf = __ic_get_buf(tmp, hlen + dlen);
            qv_wipe(i32, &szs);

            iop_bpack(buf, &ic__hdr__s, pxy_hdr, szs.tab);
            memcpy(buf + hlen, data, dlen);
            flags |= IC_MSG_HAS_HDR;
        } else {
            /* XXX We do not support header replacement with proxyfication */
            memcpy(__ic_get_buf(tmp, dlen), data, dlen);
        }

        ___ic_query_flags(pxy, tmp, flags);
    }
}

static int ic_read(ichannel_t *ic, short events, int sock)
{
    sb_t *buf = &ic->rbuf;
    int *fdv, fdc;
    bool fd_overflow = false;
    ssize_t seqpkt_at_least = IC_PKT_MAX;
    char cmsgbuf[BUFSIZ];

  again:
    {
        struct iovec iov;
        struct msghdr msgh = {
            .msg_iov        = &iov,
            .msg_iovlen     = 1,
            .msg_control    = cmsgbuf,
            .msg_controllen = sizeof(cmsgbuf),
        };

        ssize_t res;

        if (ic->is_stream) {
            res = sb_read(buf, sock, IC_PKT_MAX);
        } else {
            iov = (struct iovec){
                .iov_base = sb_grow(buf, IC_PKT_MAX),
                .iov_len  = IC_PKT_MAX,
            };
            res = recvmsg(sock, &msgh, 0);
            seqpkt_at_least -= res;
        }

        if (res < 0) {
            /* XXX: Workaround linux < 2.6.24 bug: it returns -EAGAIN *
             * instead of 0 in the hangup case for SOCK_SEQPACKETS    */
            if ((events & POLLHUP) || !ERR_RW_RETRIABLE(errno))
                return -1;
            return 0;
        }
        if (res == 0) {
            errno = 0;
            return -1;
        }

        if (ic->wa_soft > 0) {
            if (!ic->wa_soft_timer) {
                ic->on_event(ic, IC_EVT_ACT);
                ic->wa_soft_timer = el_timer_register(ic->wa_soft, 0, 0,
                                                      ic_watch_act_soft, ic);
                el_unref(ic->wa_soft_timer);
            } else {
                el_timer_restart(ic->wa_soft_timer, 0);
            }
        }

        fdv = NULL;
        fdc = 0;
        if (!ic->is_stream) {
            __sb_fixlen(buf, buf->len + res);
            ic_parse_cmsg(ic, &msgh, &fdc, &fdv);
            fd_overflow = msgh.msg_flags & MSG_CTRUNC; /* see #664 */
        }
    }

    while (buf->len >= IC_MSG_HDR_LEN) {
        void *data = buf->data + IC_MSG_HDR_LEN;
        int slot, dlen, cmd;
        int flags;

        slot  = get_unaligned_cpu32(buf->data);
        flags = slot & ~IC_MSG_SLOT_MASK;
        slot &= IC_MSG_SLOT_MASK;
        cmd   = get_unaligned_cpu32(buf->data + 4);
        dlen  = get_unaligned_cpu32(buf->data + 8);

        if (IC_MSG_HDR_LEN + dlen > buf->len)
            break;

        if (unlikely(flags & IC_MSG_HAS_FD)) {
            if (fdc < 1) {
                assert (fd_overflow); /* see #664 */
            } else {
                ic->current_fd = NEXTARG(fdc, fdv);
            }
            if (unlikely(ic->current_fd < 0)) {
                e_warning("file descriptor table overflow");
            }
            flags &= ~IC_MSG_HAS_FD;
        }

        if (unlikely(cmd == IC_MSG_STREAM_CONTROL)) {
            ic->is_closing |= slot == IC_SC_BYE;
        } else
        if (cmd <= 0) {
            if (ic_read_process_answer(ic, cmd, slot, data, dlen) < 0)
                goto close_and_error_out;
        } else {
            /* deal with queries */
            if (slot) {
                ic->pending++;
#ifndef NDEBUG
                /*
                 * This code prints a warning when the amount of pending
                 * request peaks more than 10% higher than before.
                 *
                 * If 10% is too low, feel free to make it higher, though keep
                 * it low enough so that real bugs can be seen, even for
                 * queries made only once per second.
                 *
                 */
                if (ic->pending >= ic->pending_max * 9 / 8) {
                    e_trace(0, "warning: %p peaked at %d pending requests (cb %p)",
                            ic, ic->pending, ic->on_event);
                    ic->pending_max = ic->pending;
                }
#endif
#ifdef IC_DEBUG_REPLIES
                qh_add(ic_replies, &ic->dbg_replies, MAKE64(ic->id, slot));
#endif
            }

            if (unlikely(ic->is_closing)) {
                if (slot)
                    ic_reply_err(ic, MAKE64(ic->id, slot), IC_MSG_RETRY);
            } else {
                ic_read_process_query(ic, cmd, slot, flags, data, dlen);
            }
        }

        if (unlikely(ic->current_fd >= 0)) {
            close(ic->current_fd);
            ic->current_fd = -1;
        }

        if (unlikely(!ic->elh)) {
            assert (buf->len == 0);
            /* a problem occured and the ichannel has been closed */
            errno = 0;
            return -1;
        }
        sb_skip(buf, IC_MSG_HDR_LEN + dlen);
    }
    assert (fdc == 0);

    if (likely(fdc == 0)) {
        if (!ic->is_stream && seqpkt_at_least > 0)
            goto again;
        return 0;
    }

    errno = 0;
close_and_error_out:
    while (fdc > 1)
        close(NEXTARG(fdc, fdv));
    return -1;
}

static void ic_reconnect(el_t ev, el_data_t priv)
{
    ichannel_t *ic = priv.ptr;

    /* XXX: ic->timer can be either the watch_activity or the reconnection
     * timer. This code ensure the watch_activity timer is properly
     * unregistered and that it will not be leaked in the event-loop.
     */
    el_timer_unregister(&ic->timer);
    if (ic_connect(ic) < 0) {
        ic->timer = el_timer_register(1000, 0, 0, ic_reconnect, ic);
        el_unref(ic->timer);
    }
}

void ic_disconnect(ichannel_t *ic)
{
    ic->queuable = false;

    if (ic->elh) {
        el_fd_unregister(&ic->elh, true);
        ic_cancel_all(ic);
#ifdef IC_DEBUG_REPLIES
        qh_wipe(ic_replies, &ic->dbg_replies);
        qh_init(ic_replies, &ic->dbg_replies, false);
#endif
        ic->on_event(ic, IC_EVT_DISCONNECTED);
    }
    el_timer_unregister(&ic->timer);
    el_timer_unregister(&ic->wa_soft_timer);
    ic_drop_id(ic);
    ic_choose_id(ic);
    qv_clear(iovec, &ic->iov);
    ic->is_closing = false;
    ic->wpos = 0;
    ic->iov_total_len = 0;
    sb_reset(&ic->rbuf);
}

void ic_wipe(ichannel_t *ic)
{
    ic->is_closing = true;
    ic_disconnect(ic);
    ic_cancel_all(ic);

    if (ic->owner) {
        *ic->owner = NULL;
    }
    ic_drop_id(ic);
    qm_wipe(ic_msg, &ic->queries);
#ifdef IC_DEBUG_REPLIES
    qh_init(ic_replies, &ic->dbg_replies, false);
#endif
    qv_wipe(iovec, &ic->iov);
    sb_wipe(&ic->rbuf);
    p_close(&ic->current_fd);
    lstr_wipe(&ic->peer_address);
    ic->is_wiped   = true;
}

static void ic_mark_disconnected(ichannel_t *ic);
static int ic_event(el_t ev, int fd, short events, el_data_t priv)
{
    ichannel_t *ic = priv.ptr;

    if (events == EL_EVENTS_NOACT)
        goto close;

    if ((events & POLLIN) && ic_read(ic, events, fd)) {
        if (errno)
            e_trace(1, "ic_read error: %m");
        goto close;
    }
    if (unlikely(ic->is_closing)) {
        ic_msg_filter_on_bye(ic);
    }
    if (!ic->elh) {
        return 0;
    }
    if (ic->is_stream ? ic_write_stream(ic, fd) : ic_write_seq(ic, fd)) {
        if (errno)
            e_trace(1, "ic_write error: %m");
        goto close;
    }
    if (ic->is_closing && ic_is_empty(ic))
        goto close;
    return 0;

close:
    if (ic->is_spawned && !ic->no_autodel) {
        ic_delete(&ic);
    } else {
        ic_mark_disconnected(ic);
    }
    return 0;
}


/*----- queries and answers handling -----*/

static ichannel_t *ic_get_from_slot(uint64_t slot)
{
#ifndef NDEBUG
    if (unlikely(!(slot >> 32)))
        e_panic("slot truncated at some point: ic->id bits are missing");
#endif
    return ic_h_g.values[RETHROW_NP(qm_find(ic, &ic_h_g, slot >> 32))];
}

void *__ic_get_buf(ic_msg_t *msg, int len)
{
    msg->data = mp_new_raw(ic_mp_g, char, IC_MSG_HDR_LEN + len);
    msg->dlen = IC_MSG_HDR_LEN + len;
    return (char *)msg->data + IC_MSG_HDR_LEN;
}

static void ic_queue(ichannel_t *ic, ic_msg_t *msg, uint32_t flags)
{
    uint32_t *hdr = (uint32_t *)msg->data;

    assert (!ic->is_wiped);
    assert (msg->dlen >= IC_MSG_HDR_LEN && msg->data);

    flags |= msg->slot;
    if (msg->fd >= 0)
        flags |= IC_MSG_HAS_FD;
    if (msg->hdr)
        flags |= IC_MSG_HAS_HDR;
    hdr[0] = cpu_to_le32(flags);
    hdr[1] = cpu_to_le32(msg->cmd);
    hdr[2] = cpu_to_le32(msg->dlen - IC_MSG_HDR_LEN);
    if (htlist_is_empty(&ic->msg_list) && ic->elh)
        el_fd_set_mask(ic->elh, POLLINOUT);
    htlist_add_tail(&ic->msg_list, &msg->msg_link);
}

static void __ic_flush(ichannel_t *ic, int fd)
{
    if (ic->is_stream) {
        while (ic->iov_total_len || !htlist_is_empty(&ic->msg_list)) {
            if (ic_write_stream(ic, fd) < 0)
                goto close;
        }
    } else {
        while (!htlist_is_empty(&ic->msg_list)) {
            if (ic_write_seq(ic, fd) < 0)
                goto close;
        }
    }
    return;

close:
    if (ic->is_spawned && !ic->no_autodel) {
        ic_delete(&ic);
    } else {
        ic_mark_disconnected(ic);
    }
}

void ic_flush(ichannel_t *ic)
{
    int fd = el_fd_get_fd(ic->elh);

    fd_unset_features(fd, O_NONBLOCK);
    __ic_flush(ic, fd);
    fd_set_features(fd, O_NONBLOCK);
}

static void ___ic_query_flags(ichannel_t *ic, ic_msg_t *msg, uint32_t flags)
{
    /* if that crashes, one of the IC_MSG_ABORT callback reenqueues directly in
     * that ichannel_t which is forbidden, fix the code
     */
    assert (ic->cancel_guard == false);

    if (!msg->async) {
        unsigned start = ic->nextslot;

        do {
            if (ic->nextslot == IC_MSG_SLOT_MASK) {
                ic->nextslot = 1;
            } else {
                ic->nextslot++;
            }
            if (unlikely(ic->nextslot == start)) {
                /* can't find a free slot, abort this query */
                if (msg->cb == IC_PROXY_MAGIC_CB) {
                    ic_proxify(ic, msg, -IC_MSG_ABORT, NULL, 0);
                } else {
                    (*msg->cb)(ic, msg, IC_MSG_ABORT, NULL, NULL);
                }
                ic_msg_delete(&msg);
                return;
            }
            msg->slot = ic->nextslot;
        } while (qm_add(ic_msg, &ic->queries, msg->slot, msg));
    }
    ic_queue(ic, msg, flags);
}

void __ic_query_flags(ichannel_t *ic, ic_msg_t *msg, uint32_t flags)
{
    ___ic_query_flags(ic, msg, flags);
}

void __ic_query(ichannel_t *ic, ic_msg_t *msg)
{
    ___ic_query_flags(ic, msg, 0);
}

/**
 * XXX: __ic_query_sync() insures that the message has effectively been
 * written on the socket (and does not wait for the answer). The message is
 * queued, then all pending queries are flushed. This may causing slowdowns,
 * that's why this function may be used only in some very special cases.
 */
void __ic_query_sync(ichannel_t *ic, ic_msg_t *msg)
{
    ___ic_query_flags(ic, msg, 0);
    ic_flush(ic);
}

void __ic_bpack(ic_msg_t *msg, const iop_struct_t *st, const void *arg)
{
    qv_t(i32) szs;
    uint8_t *buf;
    int len;

    qv_inita(i32, &szs, 1024);
    if (msg->hdr) {
        int hlen, szpos;

        hlen  = iop_bpack_size(&ic__hdr__s, msg->hdr, &szs);
        szpos = szs.len;
        len   = iop_bpack_size_flags(st, arg, IOP_BPACK_SKIP_DEFVAL, &szs);
        buf   = __ic_get_buf(msg, hlen + len);

        iop_bpack(buf, &ic__hdr__s, msg->hdr, szs.tab);
        iop_bpack(buf + hlen, st, arg, szs.tab + szpos);
    } else {
        len   = iop_bpack_size_flags(st, arg, IOP_BPACK_SKIP_DEFVAL, &szs);
        buf   = __ic_get_buf(msg, len);
        iop_bpack(buf, st, arg, szs.tab);
    }
    qv_wipe(i32, &szs);
}

size_t __ic_reply(ichannel_t *ic, uint64_t slot, int cmd, int fd,
                  const iop_struct_t *st, const void *arg)
{
    assert (slot & IC_MSG_SLOT_MASK);

    if (unlikely(ic_slot_is_http(slot))) {
        assert (fd < 0);
        __ichttp_reply(slot, cmd, st, arg);
        return 0;
    }
    assert (!(slot & IC_SLOT_FOREIGN_MASK));

    if (unlikely(!ic)) {
        ic = ic_get_from_slot(slot);
        if (unlikely(!ic))
            return 0;
    }
    if (likely(ic->elh) && likely(ic->id == slot >> 32)) {
        ic_msg_t *msg = ic_msg_new(0);

#ifdef IC_DEBUG_REPLIES
        int32_t pos = qh_del_key(ic_replies, &ic->dbg_replies, slot);
        if (pos < 0)
            e_panic("answering to slot %d twice or unexpected answer!",
                    (uint32_t)slot);
#endif
        assert (ic->pending > 0);
        ic->pending--;
        msg->slot = slot & IC_MSG_SLOT_MASK;
        msg->fd   = fd;
        msg->cmd  = -cmd;
        __ic_bpack(msg, st, arg);
        ic_queue(ic, msg, 0);
        return msg->dlen;
    }
    return 0;
}

static void ic_reply_err2(ichannel_t *ic, uint64_t slot, int err,
                          const lstr_t *err_str)
{
    assert (slot & IC_MSG_SLOT_MASK);

    if (unlikely(ic_slot_is_http(slot))) {
        __ichttp_reply_err(slot, err, err_str);
        return;
    }
    assert (!(slot & IC_SLOT_FOREIGN_MASK));

    if (unlikely(!ic)) {
        ic = ic_get_from_slot(slot);
        if (unlikely(!ic))
            return;
    }
    if (likely(ic->elh) && likely(ic->id == slot >> 32)) {
        ic_msg_t *msg = ic_msg_new(0);

#ifdef IC_DEBUG_REPLIES
        int32_t pos = qh_del_key(ic_replies, &ic->dbg_replies, slot);
        if (pos < 0)
            e_panic("answering to slot %d twice or unexpected answer!",
                    (uint32_t)slot);
#endif
        assert (ic->pending > 0);
        ic->pending--;
        msg->slot = slot & IC_MSG_SLOT_MASK;
        msg->cmd  = -err;
        if (err_str && err_str->len) {
            msg->data = mp_new_raw(ic_mp_g, char, IC_MSG_HDR_LEN + err_str->len + 1);
            msg->dlen = IC_MSG_HDR_LEN + err_str->len + 1;
            memcpyz((char *)msg->data + IC_MSG_HDR_LEN, err_str->s, err_str->len);
        } else {
            msg->data = mp_new_raw(ic_mp_g, char, IC_MSG_HDR_LEN);
            msg->dlen = IC_MSG_HDR_LEN;
        }
        ic_queue(ic, msg, 0);
    }
}

void ic_reply_err(ichannel_t *ic, uint64_t slot, int err)
{
    ic_reply_err2(ic, slot, err, NULL);
}

static void ic_sc_do(ichannel_t *ic, int sc_op)
{
    if (!ic->is_closing) {
        ic_msg_t *msg = ic_msg_new(0);
        msg->data = mp_new_raw(ic_mp_g, char, IC_MSG_HDR_LEN);
        msg->dlen = IC_MSG_HDR_LEN;
        msg->cmd  = IC_MSG_STREAM_CONTROL;
        msg->slot = sc_op;
        ic_queue(ic, msg, 0);
    }
}

void ic_bye(ichannel_t *ic)
{
    if (ic->elh) {
        ic_sc_do(ic, IC_SC_BYE);
    } else {
        el_timer_unregister(&ic->timer);
        el_timer_unregister(&ic->wa_soft_timer);
    }
    ic->is_closing  = true;
    ic->auto_reconn = false;
}
void ic_nop(ichannel_t *ic)
{
    ic_sc_do(ic, IC_SC_NOP);
}

/*----- ichannel life cycle API -----*/

ichannel_t *ic_get_by_id(uint32_t id)
{
    return ic_h_g.values[RETHROW_NP(qm_find(ic, &ic_h_g, id))];
}

ichannel_t *ic_init(ichannel_t *ic)
{
    p_clear(ic, 1);
    htlist_init(&ic->msg_list);
    htlist_init(&ic->iov_list);
    ic->current_fd  = -1;
    ic->auto_reconn = true;
    ic->priority    = EV_PRIORITY_NORMAL;
    qm_init(ic_msg, &ic->queries, false);
    sb_init(&ic->rbuf);
    ic_choose_id(ic);
#ifdef IC_DEBUG_REPLIES
    qh_init(ic_replies, &ic->dbg_replies, false);
#endif
#ifndef NDEBUG
    ic->pending_max = 128;
#endif
    ic->peer_address = LSTR_NULL_V;

    return ic;
}

static int ic_mark_connected(ichannel_t *ic, int fd)
{
    ic->queuable = true;
    el_fd_set_hook(ic->elh, ic_event);
    ic->on_event(ic, IC_EVT_CONNECTED);
    if (ic->on_creds && !ic->is_stream) {
        struct ucred ucred;
        socklen_t len = sizeof(ucred);

        RETHROW(getsockopt(fd, SOL_SOCKET, SO_PEERCRED, &ucred, &len));
        RETHROW((*ic->on_creds)(ic, &ucred));
    }
    /* force an exchange at connect time, so force NOP if queue is empty */
    if (htlist_is_empty(&ic->msg_list))
        ic_nop(ic);
    el_fd_set_mask(ic->elh, POLLINOUT);
    return 0;
}

static void ic_mark_disconnected(ichannel_t *ic)
{
    ic_disconnect(ic);
    if (!ic->is_spawned && ic->auto_reconn) {
        assert (ic->timer == NULL);
        ic->timer = el_timer_register(1000, 0, 0, ic_reconnect, ic);
        el_unref(ic->timer);
    }
}

static int ic_connecting(el_t ev, int fd, short events, el_data_t priv)
{
    ichannel_t *ic = priv.ptr;
    int res;

    if (events == EL_EVENTS_NOACT) {
        ic_mark_disconnected(ic);
        return -1;
    }

    res = socket_connect_status(fd);
    if (res < 0) {
        ic_mark_disconnected(ic);
        return -1;
    }
    if (res > 0 && ic_mark_connected(ic, fd)) {
        ic_mark_disconnected(ic);
        return -1;
    }
    return 0;
}

static void ic_watch_act_soft(el_t ev, el_data_t priv)
{
    ichannel_t *ic = priv.ptr;

    ic->on_event(ic, IC_EVT_NOACT);
    el_timer_unregister(&ic->wa_soft_timer);
}

static void ic_watch_act_nop(el_t ev, el_data_t priv)
{
    ic_nop(priv.ptr);
    el_timer_restart(ev, 0);
}

static void __ic_watch_activity(ichannel_t *ic)
{
    int wa = ic->wa_hard;

    if (!ic->elh)
        return;

    el_fd_watch_activity(ic->elh, POLLIN, wa);
    el_timer_unregister(&ic->timer);
    el_timer_unregister(&ic->wa_soft_timer);

    if (ic->wa_soft > 0) {
        if (wa > 0) {
            wa = MIN(wa, ic->wa_soft);
        } else {
            wa = ic->wa_soft;
        }
    }

    if (wa <= 0)
        return;

    ic->timer = el_timer_register(wa / 3, 0, 0, ic_watch_act_nop, ic);
    el_unref(ic->timer);
}

static int __ic_connect(ichannel_t *ic, int flags)
{
    int type, sock;

    assert (!ic->elh);

    ic->is_spawned = false;
    ic->is_stream  = ic->su.family != AF_UNIX;

    if (ic->su.family == AF_UNIX) {
        flags &= ~O_NONBLOCK;
        type   = SOCK_SEQPACKET;
    } else {
        type   = SOCK_STREAM;
    }

    sock = connectx(-1, &ic->su, 1, type, ic->protocol, flags);
    if (sock < 0) {
        return -1;
    }
    if (flags & O_NONBLOCK) {
        ic->elh = el_fd_register(sock, POLLOUT, &ic_connecting, ic);
        ic->queuable = true;
    } else {
        ic->elh = el_fd_register(sock, POLLINOUT, &ic_event, ic);
        if (ic_mark_connected(ic, sock)) {
            ic_mark_disconnected(ic);
            return -1;
        }
        fd_set_features(sock, O_NONBLOCK);
    }
    (el_fd_set_priority)(ic->elh, ic->priority);
    __ic_watch_activity(ic);
    if (ic->do_el_unref) {
        el_unref(ic->elh);
    }
    return 0;
}

void ic_watch_activity(ichannel_t *ic, int timeout_soft, int timeout_hard)
{
    ic->wa_soft = timeout_soft;
    ic->wa_hard = timeout_hard;
    __ic_watch_activity(ic);
}

ev_priority_t ic_set_priority(ichannel_t *ic, ev_priority_t prio)
{
    ev_priority_t prev = ic->priority;

    ic->priority = prio;

    if (ic->elh) {
        ev_priority_t elprev = (el_fd_set_priority)(ic->elh, prio);
        assert (elprev == prev);
    }

    return prev;
}

int ic_connect(ichannel_t *ic)
{
    return __ic_connect(ic, O_NONBLOCK);
}

int ic_connect_blocking(ichannel_t *ic)
{
    return __ic_connect(ic, 0);
}

void ic_spawn(ichannel_t *ic, int fd, ic_creds_f *creds_fn)
{
    int type = 0;
    socklen_t len = sizeof(type);

    if (getsockopt(fd, SOL_SOCKET, SO_TYPE, (void *)&type, &len))
        e_panic(E_UNIXERR("getsockopt"));
    assert (type == SOCK_STREAM || type == SOCK_SEQPACKET);

    ic->is_spawned = true;
    ic->is_stream  = type == SOCK_STREAM;
    ic->on_creds   = creds_fn;

    ic->elh = el_fd_register(fd, POLLIN, &ic_event, ic);
    (el_fd_set_priority)(ic->elh, ic->priority);
    __ic_watch_activity(ic);
    if (ic->do_el_unref) {
        el_unref(ic->elh);
    }

    if (ic_mark_connected(ic, fd)) {
        ic_mark_disconnected(ic);
    }
}

static int ic_accept(el_t ev, int fd, short events, el_data_t priv)
{
    int (*on_accept)(el_t ev, int fd) = priv.ptr;
    int sock;

    while ((sock = acceptx(fd, O_NONBLOCK)) >= 0) {
        if ((*on_accept)(ev, sock))
            p_close(&sock);
    }
    return 0;
}

el_t ic_listento(const sockunion_t *su, int type, int proto,
                 int (*on_accept)(el_t ev, int fd))
{
    int sock = listenx(-1, su, 1, type, proto, O_NONBLOCK);
    if (sock < 0)
        return NULL;
    return el_unref(el_fd_register(sock, POLLIN, &ic_accept,
                                   (void *)on_accept));
}

void ic_drop_ans_cb(ichannel_t *ic, ic_msg_t *msg,
                    ic_status_t res, void *arg, void *exn)
{
}
