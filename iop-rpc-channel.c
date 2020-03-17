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

#include "unix.h"
#include "iop-rpc.h"

/* IC_MSG_STREAM_CONTROL messages uses the "slot" to encode the command.
 */
enum ic_msg_sc_slots {
    IC_SC_BYE = 1,
    IC_SC_NOP,
};

qm_k32_t(ic, ichannel_t *);
qm_k64_t(ic_hook_ctx, ic_hook_ctx_t *);

static mem_pool_t *ic_mp_g;
static qm_t(ic)    ic_h_g;
static qm_t(ic_hook_ctx) ic_ctx_h_g;
const QM(ic_cbs, ic_no_impl, false);

/*----- messages stuff -----*/


/* {{{ init/shutdown */

static int ic_initialize(void *arg)
{
    if (!ic_mp_g) {
        ic_mp_g = mem_fifo_pool_new(1 << 20);
        qm_init(ic, &ic_h_g, false);
        qm_init(ic_hook_ctx, &ic_ctx_h_g, false);
    }
    return 0;
}

static int ic_shutdown(void)
{
    qm_wipe(ic_hook_ctx, &ic_ctx_h_g);
    qm_wipe(ic, &ic_h_g);
    mem_fifo_pool_delete(&ic_mp_g);
    return 0;
}

MODULE_BEGIN(ic)
MODULE_END()

/* }}}*/


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

ic_msg_t *ic_msg_set_timeout(ic_msg_t *msg, uint32_t timeout)
{
    msg->timeout = timeout;

    return msg;
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
    if (msg->dlen) {
        assert (msg->dlen >= IC_MSG_HDR_LEN);
        mp_delete(ic_mp_g, &msg->data);
    }

    el_timer_unregister(&msg->timeout_timer);
    mp_delete(ic_mp_g, msgp);
}

static void
ic_msg_init_for_reply(ichannel_t *ic, ic_msg_t *msg, uint64_t slot, int cmd)
{
#ifdef IC_DEBUG_REPLIES
    int32_t pos = qh_del_key(ic_replies, &ic->dbg_replies, slot);
    if (pos < 0)
        e_panic("answering to slot %d twice or unexpected answer!",
                (uint32_t)slot);
#endif
    assert (ic->pending > 0);
    ic->pending--;
    msg->slot = slot & IC_MSG_SLOT_MASK;
    msg->cmd  = -cmd;
}

static struct {
    ic_hook_ctx_t   *ic_hook_ctx;
    ic_post_hook_f  *post_hook;
    const iop_rpc_t *rpc;
    data_t           post_args;
} ic_hook_flow_g;

int ic_hook_ctx_save(ic_hook_ctx_t *ctx)
{
    return qm_add(ic_hook_ctx, &ic_ctx_h_g, ctx->slot, ctx);
}

ic_hook_ctx_t *ic_hook_ctx_new(uint64_t slot, ssize_t extra)
{
    if (ic_hook_flow_g.ic_hook_ctx) {
        assert(ic_hook_flow_g.ic_hook_ctx->slot != slot);
        ic_hook_ctx_save(ic_hook_flow_g.ic_hook_ctx);
    }
    ic_hook_flow_g.ic_hook_ctx = mp_new_extra_field(ic_mp_g, ic_hook_ctx_t,
                                                    data, extra);
    ic_hook_flow_g.ic_hook_ctx->slot = slot;
    ic_hook_flow_g.ic_hook_ctx->rpc = ic_hook_flow_g.rpc;
    ic_hook_flow_g.ic_hook_ctx->post_hook = ic_hook_flow_g.post_hook;
    ic_hook_flow_g.ic_hook_ctx->post_hook_args = ic_hook_flow_g.post_args;

    return ic_hook_flow_g.ic_hook_ctx;
}

ic_hook_ctx_t *ic_hook_ctx_get(uint64_t slot)
{
    if (ic_hook_flow_g.ic_hook_ctx
    &&  ic_hook_flow_g.ic_hook_ctx->slot == slot)
    {
        return ic_hook_flow_g.ic_hook_ctx;
    } else {
        return qm_get_def(ic_hook_ctx, &ic_ctx_h_g, slot, NULL);
    }
}

void ic_hook_ctx_delete(ic_hook_ctx_t **pctx)
{
    if (*pctx) {
        ic_hook_ctx_t *ctx = *pctx;

        if (ctx == ic_hook_flow_g.ic_hook_ctx) {
            ic_hook_flow_g.ic_hook_ctx = NULL;
        } else {
            qm_del_key(ic_hook_ctx, &ic_ctx_h_g, ctx->slot);
        }
    }
    mp_delete(ic_mp_g, pctx);
}

int
ic_query_do_pre_hook(ichannel_t *ic, uint64_t slot,
                     const ic__hdr__t *hdr, const ic_cb_entry_t *e)
{
    if (e->pre_hook) {
        ic_hook_flow_g.post_hook = e->post_hook;
        ic_hook_flow_g.rpc = e->rpc;
        ic_hook_flow_g.post_args = e->post_hook_args;
        (*e->pre_hook)(ic, slot, hdr, e->pre_hook_args);
        /* XXX: if we reply to the query during pre_hook then
         *      ic_hook_flow.ic_hook_ctx will be NULL, so we mustn't
         *      call the implementation of the RPC
         */
        if (!ic_hook_flow_g.ic_hook_ctx) {
            return -1;
        }
    }

    return 0;
}

void ic_query_do_post_hook(ichannel_t *ic, ic_status_t status, uint64_t slot)
{
    ic_hook_ctx_t *ctx = ic_hook_ctx_get(slot);

    if (!ctx) {
        return;
    }
    if (ctx->post_hook) {
        (*ctx->post_hook)(ic, status, ctx, ctx->post_hook_args);
    }
    ic_hook_ctx_delete(&ctx);

    return;
}

static inline bool ic_can_reply(const ichannel_t *ic, uint64_t slot) {
    return likely(ic->elh || ic_is_local(ic)) && likely(ic->id == slot >> 32);
}

static void ic_watch_act_soft(el_t ev, data_t priv);

static void ic_reply_err2(ichannel_t *ic, uint64_t slot, int err,
                          const lstr_t *err_str);
static ichannel_t *ic_get_from_slot(uint64_t slot);
static void ic_queue(ichannel_t *ic, ic_msg_t *msg, uint32_t flags);
static void ic_queue_for_reply(ichannel_t *ic, ic_msg_t *msg);
static void ___ic_query_flags(ichannel_t *ic, ic_msg_t *msg, uint32_t flags);
static void ic_proxify(ichannel_t *pxy_ic, ic_msg_t *msg, int cmd,
                       const void *data, int dlen,
                       const ic_msg_t *unpacked_msg)
{
    uint64_t slot = *(uint64_t *)msg->priv;
    ichannel_t *ic;

    cmd = -cmd;

    if (unlikely(ic_slot_is_http(slot))) {
        __ichttp_proxify(slot, cmd, data, dlen);
        return;
    }
    ic = ic_get_from_slot(slot);
    if (unlikely(!ic))
        return;
    if (likely(ic_can_reply(ic, slot))) {
        ic_msg_t *tmp = ic_msg_new_fd(pxy_ic ? ic_get_fd(pxy_ic) : -1, 0);

        ic_query_do_post_hook(ic, cmd, slot);
        ic_msg_init_for_reply(ic, tmp, slot, cmd);
        if (unpacked_msg && (likely(cmd == IC_MSG_OK) || cmd == IC_MSG_EXN)) {
            const iop_struct_t *st;

            assert (msg->rpc);
            st = likely(cmd == IC_MSG_OK) ? msg->rpc->result : msg->rpc->exn;
            tmp->force_dup = msg->force_dup;
            __ic_msg_build(tmp, st, unpacked_msg->data, !ic_is_local(ic));
        } else {
            void *buf;

            assert (!unpacked_msg || dlen == 0 || cmd == IC_MSG_INVALID);

            if (ic_is_local(ic))
                tmp->force_pack = !unpacked_msg;

            buf = __ic_get_buf(tmp, dlen);
            if (data) {
                memcpy(buf, data, dlen);
            } else {
                assert (dlen == 0);
            }
        }
        ic_queue_for_reply(ic, tmp);
    }
}

void __ic_msg_reply_err(ichannel_t *ic, ic_msg_t *msg, ic_status_t status)
{
    if (msg->canceled) {
        return;
    }

    if (msg->cb == IC_PROXY_MAGIC_CB) {
        ic_proxify(ic, msg, -status, NULL, 0, NULL);
    } else {
        (*msg->cb)(ic, msg, status, NULL, NULL);
    }
}

void __ic_forward_reply_to(ichannel_t *pxy_ic, uint64_t slot, int cmd,
                           const void *res, const void *exn)
{
    ichannel_t *ic;

    e_assert_p(panic, pxy_ic,
               "are you trying to forward a webservices answer?");

    if (unlikely(ic_slot_is_http(slot))) {
        __ichttp_forward_reply(pxy_ic, slot, cmd, res, exn);
        return;
    }

    if (cmd != IC_MSG_OK && cmd != IC_MSG_EXN) {
        assert (!res);
        ic_reply_err2(NULL, slot, cmd, exn);
        return;
    }

    if (unlikely(!(ic = ic_get_from_slot(slot))))
        return;
    if (likely(ic_can_reply(ic, slot))) {
        ic_msg_t *tmp = ic_msg_new_fd(ic_get_fd(pxy_ic), 0);
        const sb_t *buf = &pxy_ic->rbuf;
        const void *data = buf->data + IC_MSG_HDR_LEN;
        int dlen = get_unaligned_cpu32(buf->data + IC_MSG_DLEN_OFFSET);

        ic_query_do_post_hook(ic, cmd, slot);
        ic_msg_init_for_reply(ic, tmp, slot, cmd);
        if (ic_is_local(pxy_ic) && !get_unaligned_cpu32(buf->data)) {
            const ic_msg_t *msg_org = data;
            const iop_struct_t *st;

            assert (dlen == sizeof(ic_msg_t) && msg_org->rpc);

            if (likely(cmd == IC_MSG_OK)) {
                st = msg_org->rpc->result;
                data = res;
            } else {
                st = msg_org->rpc->exn;
                data = exn;
            }
            tmp->force_dup = msg_org->force_dup;
            __ic_msg_build(tmp, st, data, !ic_is_local(ic));
        } else {
            if (ic_is_local(ic))
                tmp->force_pack = true;

            memcpy(__ic_get_buf(tmp, dlen), data, dlen);
        }
        ic_queue_for_reply(ic, tmp);
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
        __ic_msg_reply_err(ic, msg, IC_MSG_ABORT);
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

static void ic_msg_take_and_delete(ic_msg_t *msg)
{
    if (msg->slot) {
        __unused__ ic_msg_t *tmp = ic_query_take(msg->ic, msg->slot);
        assert (tmp == msg);
    }
    ic_msg_delete(&msg);
}

static void ic_msg_abort(ichannel_t *ic, ic_msg_t *msg)
{
    if (msg->slot) {
        __unused__ ic_msg_t *tmp = ic_query_take(msg->ic, msg->slot);
        assert (tmp == msg);

        __ic_msg_reply_err(msg->ic, msg, IC_MSG_ABORT);
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

            if (msg->canceled) {
                ic_msg_take_and_delete(msg);
                continue;
            }

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
    return 1;
}

#ifndef CMSG_ALIGN
# define CMSG_ALIGN(l)  CMSG_LEN(l)
#endif

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
        if (mem_tool_is_running(MEM_TOOL_VALGRIND)) {
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

            if (msg->canceled) {
                to_drop++;
                continue;
            }

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

            if (tmp->canceled) {
                ic_msg_take_and_delete(tmp);
                continue;
            }

            if (tmp->cmd <= 0 || tmp->slot == 0) {
                ic_msg_delete(&tmp);
            }
        }
#undef iovc
    }
    el_fd_set_mask(ic->elh, POLLIN);
    return 1;
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

static int
t_get_value_of_st(const iop_struct_t *, const ic_msg_t *unpacked_msg,
                  pstream_t, void **value, bool do_copy);

static ALWAYS_INLINE int
ic_read_process_answer(ichannel_t *ic, int cmd, uint32_t slot,
                       const void *data, int dlen,
                       const ic_msg_t *unpacked_msg)
{
    ic_msg_t *tmp = ic_query_take(ic, slot);
    const iop_struct_t *st;

    if (unlikely(!tmp)) {
        errno = 0;
        return -1;
    }

    if (tmp->canceled) {
        goto wipe;
    }

    if (ic_is_local(ic)) {
        sb_reset(&ic->rbuf);
        sb_addnc(&ic->rbuf, IC_MSG_HDR_LEN, 0);
        put_unaligned_cpu32(ic->rbuf.data + IC_MSG_CMD_OFFSET, cmd);
        if (data) {
            put_unaligned_cpu32(ic->rbuf.data, slot);
            put_unaligned_cpu32(ic->rbuf.data + IC_MSG_DLEN_OFFSET, dlen);
            sb_add(&ic->rbuf, data, dlen);
        } else
        if (unpacked_msg) {
            put_unaligned_cpu32(ic->rbuf.data + IC_MSG_DLEN_OFFSET,
                                sizeof(ic_msg_t));
            sb_add(&ic->rbuf, unpacked_msg, sizeof(ic_msg_t));
            ((ic_msg_t *)(ic->rbuf.data + IC_MSG_HDR_LEN))->rpc = tmp->rpc;
        }
    }

    tmp->raw_res = ps_init(data, dlen);
    if (tmp->cb == IC_PROXY_MAGIC_CB) {
        ic_proxify(ic, tmp, cmd, data, dlen, unpacked_msg);
        goto wipe;
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
        goto wipe;
    }
    if (!tmp->raw) {
        t_scope;
        void *value = NULL;
        pstream_t ps = tmp->raw_res;

        if (unlikely(t_get_value_of_st(st, unpacked_msg, ps,
                                       &value, false) < 0))
        {
#ifndef NDEBUG
            const char *err = iop_get_err();

            if (err) {
                e_trace(0, "rpc(%04x:%04x):%s: %s",
                        (tmp->cmd >> 16) & 0x7fff, tmp->cmd & 0x7fff,
                        tmp->rpc->name.s, err);
            } else {
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
    } else {
        (*tmp->cb)(ic, tmp, cmd, NULL, NULL);
    }

  wipe:
    ic_msg_delete(&tmp);
    if (ic_is_local(ic)) {
        sb_reset(&ic->rbuf);
    }
    return 0;
}

lstr_t ic_get_client_addr(ichannel_t *ic)
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

static inline void ic_update_pending(ichannel_t *ic, uint32_t slot)
{
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
}

static int
t_get_hdr_of_query(const ic_msg_t *unpacked_msg, pstream_t *ps,
                   ic__hdr__t **hdr, bool do_copy)
{
    if (unpacked_msg) {
        RETHROW(ic__hdr__check((ic__hdr__t *)unpacked_msg->hdr));
        if (unpacked_msg->force_dup) {
            *hdr = ic__hdr__dup(t_pool(), unpacked_msg->hdr);
        } else {
            *hdr = (ic__hdr__t *)unpacked_msg->hdr;
        }
    } else {
        *hdr = t_new_raw(ic__hdr__t, 1);
        RETHROW(t_ic__hdr__bunpack_multi(*hdr, ps, do_copy));
    }
    return 0;
}

static int
t_get_value_of_st(const iop_struct_t *st, const ic_msg_t *unpacked_msg,
                  pstream_t ps, void **value, bool do_copy)
{
    if (unpacked_msg) {
        RETHROW(iop_check_constraints(st, unpacked_msg->data));
        if (unpacked_msg->force_dup) {
            *value = iop_dup(t_pool(), st, unpacked_msg->data, NULL);
        } else {
            *value = unpacked_msg->data;
        }
    } else {
        RETHROW(iop_bunpack_ptr(t_pool(), st, value, ps, do_copy));
    }
    return 0;
}

static int
t_get_hdr_value_of_query(ichannel_t *ic, int cmd, uint32_t flags,
                         const void *data, int dlen,
                         const ic_msg_t *unpacked_msg, const iop_struct_t *st,
                         ic__hdr__t **hdr, void **value)
{
    pstream_t ps = ps_init(data, dlen);
    bool do_copy = false;

#define QUERY_FMT      "query %04x:%04x, type %s: "
#define QUERY_FMT_ARG  (cmd >> 16) & 0x7fff, cmd & 0x7fff, st->fullname.s

    if (ic_is_local(ic) && !unpacked_msg) {
        /* IOP payload comes from a volatile ic_msg_t that will be destroy
         * with ic_reply(), thus we need to duplicate strings */
        do_copy = true;
    }

    if (unlikely(flags & IC_MSG_HAS_HDR)) {
        if (unlikely(t_get_hdr_of_query(unpacked_msg, &ps, hdr, do_copy) < 0))
        {
            e_trace(0, QUERY_FMT "header encoding", QUERY_FMT_ARG);
            return -1;
        }
        /* XXX on simple header we write the payload size of the iop query */
        if (unlikely((*hdr)->iop_tag == IOP_UNION_TAG(ic__hdr, simple))) {
            ic__simple_hdr__t *shdr = &(*hdr)->simple;

            if (shdr->payload < 0)
                shdr->payload = dlen;
            if (!shdr->host.len) {
                shdr->host = ic_get_client_addr(ic);
            }
        }
    }

    if (value
    &&  unlikely(t_get_value_of_st(st, unpacked_msg, ps, value, do_copy) < 0))
    {
#ifndef NDEBUG
        const char *err = iop_get_err();

        if (err) {
            e_trace(0, QUERY_FMT "%s", QUERY_FMT_ARG, err);
        } else {
            e_trace(0, QUERY_FMT "encoding", QUERY_FMT_ARG);
        }
#endif
        return -1;
    }

    return 0;
#undef QUERY_FMT
#undef QUERY_FMT_ARG
}

static ALWAYS_INLINE void
ic_read_process_query(ichannel_t *ic, int cmd, uint32_t slot,
                      uint32_t flags, const void *data, int dlen,
                      const ic_msg_t *unpacked_msg)
{
    const ic_cb_entry_t *e;
    const iop_struct_t *st;
    ichannel_t *pxy;
    ic__hdr__t *pxy_hdr = NULL;
    uint64_t query_slot = MAKE64(ic->id, slot);
    int pos;

    pos = likely(ic->impl) ? qm_find_safe(ic_cbs, ic->impl, cmd) : -1;
    if (unlikely(pos < 0)) {
        e_trace(0, "received query for unimplemented RPC (%04x:%04x)",
                (cmd >> 16) & 0x7fff, cmd & 0x7fff);
        if (slot)
            ic_reply_err(ic, MAKE64(ic->id, slot), IC_MSG_UNIMPLEMENTED);
        return;
    }
    e = ic->impl->values + pos;
    st = e->rpc ? e->rpc->args : NULL;

    switch (e->cb_type) {
      case IC_CB_NORMAL:
      case IC_CB_WS_SHARED: {
        t_scope;
        ic__hdr__t *hdr = NULL;
        void *value = NULL;

        if (t_get_hdr_value_of_query(ic, cmd, flags, data, dlen, unpacked_msg,
                                     st, &hdr, &value) >= 0)
        {
            t_seal();
            ic->desc = e->rpc;
            ic->cmd  = cmd;

            if (ic_query_do_pre_hook(ic, query_slot, hdr, e) < 0) {
                ic->desc = NULL;
                ic->cmd  = 0;
                return;
            }
            (*e->u.cb.cb)(ic, query_slot, value, hdr);
            ic->desc = NULL;
            ic->cmd  = 0;
        } else
        if (slot) {
            lstr_t err_str = iop_get_err_lstr();

            ic_reply_err2(ic, query_slot, IC_MSG_INVALID, &err_str);
        }
        return;
      }

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
            t_scope;
            ic__hdr__t *hdr = NULL;
            ic_dynproxy_t dynproxy;

            if (t_get_hdr_value_of_query(ic, cmd, flags, data, dlen,
                                         unpacked_msg, st, &hdr, NULL) < 0)
            {
                if (slot) {
                    lstr_t err_str = iop_get_err_lstr();

                    ic_reply_err2(ic, query_slot, IC_MSG_INVALID, &err_str);
                }
                return;
            }

            dynproxy = (*e->u.dynproxy.get_ic)(hdr, e->u.dynproxy.priv);
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
            ic_reply_err(ic, query_slot, IC_MSG_PROXY_ERROR);
    } else {
        ic_msg_t *tmp;
        bool take_pxy_hdr = !(flags & IC_MSG_HAS_HDR) && pxy_hdr;

        if (e->pre_hook) {
            t_scope;
            ic__hdr__t *hdr = NULL;

            if (flags & IC_MSG_HAS_HDR) {
                if (unpacked_msg) {
                    hdr = (ic__hdr__t *)unpacked_msg->hdr;
                } else
                if (t_get_hdr_value_of_query(ic, cmd, flags, data, dlen,
                                             NULL, st, &hdr, NULL) < 0)
                {
                    if (slot) {
                        lstr_t err_str = iop_get_err_lstr();
                        ic_reply_err2(ic, query_slot, IC_MSG_INVALID, &err_str);
                    }
                    return;
                }
            }
            t_seal();
            ic->cmd = cmd;
            if (ic_query_do_pre_hook(ic, query_slot, hdr, e) < 0) {
                ic->cmd = 0;
                return;
            }
            ic->cmd = 0;
        }

        tmp = ic_msg_proxy_new(ic_get_fd(ic), slot, NULL);
        if (slot) {
            tmp->cb = IC_PROXY_MAGIC_CB;
        } else {
            tmp->async = true;
        }
        tmp->cmd = cmd;
        *(uint64_t *)tmp->priv = query_slot;

        /* XXX We do not support header replacement with proxyfication */

        if (unpacked_msg) {
            tmp->hdr = take_pxy_hdr ? pxy_hdr : unpacked_msg->hdr;
            if (ic_is_local(pxy)) {
                tmp->force_dup = unpacked_msg->force_dup;
                tmp->rpc = unpacked_msg->rpc;
            }
            __ic_msg_build(tmp, st, unpacked_msg->data, !ic_is_local(pxy));
        } else {
            if (ic_is_local(pxy))
                tmp->force_pack = true;

            if (take_pxy_hdr) {
                qv_t(i32) szs;
                uint8_t *buf;
                int hlen;

                /* Pack the given header in proxyfied request */
                qv_inita(i32, &szs, 128);
                hlen = iop_bpack_size(&ic__hdr__s, pxy_hdr, &szs);
                buf = __ic_get_buf(tmp, hlen + dlen);
                iop_bpack(buf, &ic__hdr__s, pxy_hdr, szs.tab);
                qv_wipe(i32, &szs);
                memcpy(buf + hlen, data, dlen);
            } else {
                memcpy(__ic_get_buf(tmp, dlen), data, dlen);
            }
        }

        if (take_pxy_hdr) {
            flags |= IC_MSG_HAS_HDR;
        }
        ___ic_query_flags(pxy, tmp, flags);
    }
}

/* Check flag consistency.
 */
static int ic_check_msg_hdr_flags(const ichannel_t *ic, int flags)
{
    flags &= ~IC_MSG_SLOT_MASK;
    if (ic->is_stream && (flags & IC_MSG_HAS_FD)) {
        assert (!ic->is_trusted);
        return -1;
    }
    flags &= ~(IC_MSG_HAS_FD | IC_MSG_HAS_HDR);
    if (flags) {
        assert (!ic->is_trusted);
        return -1;
    }
    return 0;
}

/* Check if msg hdr seems valid.
 */
static int ic_check_msg_hdr(const ichannel_t *ic, const void *data)
{
    const byte *hdr = data;
    uint32_t slot = get_unaligned_cpu32(hdr);
    int cmd  = get_unaligned_cpu32(hdr + IC_MSG_CMD_OFFSET);
    int dlen = get_unaligned_cpu32(hdr + IC_MSG_DLEN_OFFSET);

    if (dlen < 0 || (uint32_t)dlen > MEM_ALLOC_MAX) {
        /* length is invalid */
        assert (!ic->is_trusted);
        return -1;
    }

    RETHROW(ic_check_msg_hdr_flags(ic, slot));
    if (dlen < 10 << 20) {
        /* don't lose time validating small message, the correct checks will
         * be performed when treating the message.
         */
        return 0;
    }

    slot &= IC_MSG_SLOT_MASK;
    if (unlikely(cmd == IC_MSG_STREAM_CONTROL)) {
        /* we are called because the size of header is to large, so this
         * cannot be a valid control message.
         */
        assert (!ic->is_trusted);
        return -1;
    } else
    if (cmd <= 0) {
        /* reject message that reply to unkown slot.
         */
        RETHROW(qm_find_safe(ic_msg, &ic->queries, slot));
        switch (-cmd) {
          case IC_MSG_OK:
          case IC_MSG_EXN:
          case IC_MSG_INVALID:
            break;

          case IC_MSG_RETRY:
          case IC_MSG_ABORT:
          case IC_MSG_UNIMPLEMENTED:
          case IC_MSG_SERVER_ERROR:
          case IC_MSG_PROXY_ERROR:
            /* no data is expected with those reply codes */
            assert (!ic->is_trusted);
            return -1;

          default:
            /* unkown reply code */
            assert (!ic->is_trusted);
            return -1;
        }
    } else {
        if (!ic->impl) {
            /* no implementation, so nothing to reply */
            assert (!ic->is_trusted);
            return -1;
        }
        if (!ic->is_trusted) {
            /* unimplemented command */
            RETHROW(qm_find_safe(ic_cbs, ic->impl, cmd));
        }
    }
    return 0;
}

static int ic_read(ichannel_t *ic, short events, int sock)
{
    sb_t *buf = &ic->rbuf;
    int *fdv, fdc;
    bool fd_overflow = false;
    ssize_t seqpkt_at_least = IC_PKT_MAX;
    int to_read = IC_PKT_MAX;
    bool starves = false;
    int write_errno = 0;
    char cmsgbuf[BUFSIZ];

    if (buf->len >= IC_MSG_HDR_LEN) {
        errno = 0;
        RETHROW(ic_check_msg_hdr(ic, buf->data));
        to_read  = get_unaligned_cpu32(buf->data + IC_MSG_DLEN_OFFSET);
        to_read += IC_MSG_HDR_LEN;
        assert (to_read > buf->len);
        to_read -= buf->len;
    }

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

        to_read = MAX(to_read, IC_PKT_MAX);
        if (ic->is_stream) {
            res = sb_read(buf, sock, to_read);
            starves = sb_avail(buf) == 0;
        } else {
            iov = (struct iovec){
                .iov_base = sb_grow(buf, to_read),
                .iov_len  = to_read,
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

            /* XXX fd_overflow is set when the kernel decided that it cannot
             * transfer some ancillary data. In our case, this means we didn't
             * properly receive the expected file descriptor, probably because
             * there is no file descriptor available anymore on our side of
             * the channel.
             */
            fd_overflow = msgh.msg_flags & MSG_CTRUNC;
        }
    }

    while (buf->len >= IC_MSG_HDR_LEN) {
        void *data = buf->data + IC_MSG_HDR_LEN;
        int slot, dlen, cmd;
        int flags;

        slot  = get_unaligned_cpu32(buf->data);
        flags = slot & ~IC_MSG_SLOT_MASK;
        slot &= IC_MSG_SLOT_MASK;
        cmd   = get_unaligned_cpu32(buf->data + IC_MSG_CMD_OFFSET);
        dlen  = get_unaligned_cpu32(buf->data + IC_MSG_DLEN_OFFSET);

        if (IC_MSG_HDR_LEN + dlen > buf->len) {
            errno = 0;
            RETHROW(ic_check_msg_hdr(ic, buf->data));
            to_read = IC_MSG_HDR_LEN + dlen - buf->len;
            break;
        }

        starves = true;
        errno = 0;
        RETHROW(ic_check_msg_hdr_flags(ic, flags));
        if (unlikely(flags & IC_MSG_HAS_FD)) {
            if (fdc < 1) {
                assert (fd_overflow);
                return -1;
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
            if (ic_read_process_answer(ic, cmd, slot, data, dlen, NULL) < 0)
                goto close_and_error_out;
        } else {
            /* deal with queries */
            ic_update_pending(ic, slot);

            if (unlikely(ic->is_closing)) {
                if (slot)
                    ic_reply_err(ic, MAKE64(ic->id, slot), IC_MSG_RETRY);
            } else {
                ic_read_process_query(ic, cmd, slot, flags, data, dlen, NULL);
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

        if (events & POLLOUT) {
            int ret = ic->is_stream ? ic_write_stream(ic, sock)
                                    : ic_write_seq(ic, sock);

            if (ret <= 0) {
                /* Stop writing */
                events &= ~POLLOUT;
                if (ret < 0) {
                    /* XXX don't raise an error _now_ since we want to read
                     * a potential pending IC_BYE on the channel.
                     */
                    write_errno = errno ?: EINVAL;
                }
            }
        }
    }
    if (expect(fdc == 0)) {
        if (buf->len < IC_MSG_HDR_LEN) {
            to_read = IC_MSG_HDR_LEN;
        }
        if (!ic->is_stream && seqpkt_at_least > 0) {
            goto again;
        }
        if (ic->is_stream && !starves) {
            goto again;
        }
        if (write_errno) {
            errno = write_errno;
            return -1;
        }
        return 0;
    }

    errno = 0;
close_and_error_out:
    while (fdc > 1)
        close(NEXTARG(fdc, fdv));
    return -1;
}

static void ic_reconnect(el_t ev, data_t priv)
{
    ichannel_t *ic = priv.ptr;

    /* XXX: ic->timer can be either the watch_activity or the reconnection
     * timer. This code ensure the watch_activity timer is properly
     * unregistered and that it will not be leaked in the event-loop.
     */
    el_timer_unregister(&ic->timer);
    if (ic_connect(ic) < 0) {
        ic->timer = el_timer_register(ic->retry_delay, 0, 0, ic_reconnect,
                                      ic);
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

    if (!ic->is_closing && ic->is_spawned && !ic->no_autodel) {
        ic_delete(&ic);
    } else {
        ic_choose_id(ic);
        qv_clear(iovec, &ic->iov);
        ic->is_closing = false;
        ic->wpos = 0;
        ic->iov_total_len = 0;
        sb_reset(&ic->rbuf);
    }
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

static int ic_event(el_t ev, int fd, short events, data_t priv)
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
    if ((ic->is_stream ? ic_write_stream(ic, fd) : ic_write_seq(ic, fd)) < 0) {
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
    return qm_get_def(ic, &ic_h_g, slot >> 32, NULL);
}

void *__ic_get_buf(ic_msg_t *msg, int len)
{
    msg->data = mp_new_raw(ic_mp_g, char, IC_MSG_HDR_LEN + len);
    msg->dlen = IC_MSG_HDR_LEN + len;
    return (char *)msg->data + IC_MSG_HDR_LEN;
}

static void ic_msg_update_flags(const ic_msg_t *msg, uint32_t *flags)
{
    if (msg->fd >= 0)
        *flags |= IC_MSG_HAS_FD;
    if (msg->hdr)
        *flags |= IC_MSG_HAS_HDR;
}

static void ic_queue(ichannel_t *ic, ic_msg_t *msg, uint32_t flags)
{
    uint32_t *hdr = (uint32_t *)msg->data;

    assert (!ic->is_wiped);
    assert (msg->dlen >= IC_MSG_HDR_LEN && msg->data);

    flags |= msg->slot;
    ic_msg_update_flags(msg, &flags);
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

    if (ic_is_local(ic))
        return;

    fd_unset_features(fd, O_NONBLOCK);
    __ic_flush(ic, fd);
    fd_set_features(fd, O_NONBLOCK);
}

void ic_msg_cancel(ic_msg_t *msg)
{
    if (expect(!msg->async)) {
        __ic_msg_reply_err(msg->ic, msg, IC_MSG_CANCELED);
    }
    msg->canceled = true;
}

static void ic_msg_on_timeout(el_t ev, el_data_t data)
{
    ic_msg_t *msg = data.ptr;
    htlist_t *queue = &msg->ic->msg_list;

    if (!msg->async) {
        __ic_msg_reply_err(msg->ic, msg, IC_MSG_TIMEDOUT);
    }

    msg->timeout_timer = NULL;
    msg->canceled = true;

    while (!htlist_is_empty(queue)
    &&    (htlist_first_entry(queue, ic_msg_t, msg_link)->canceled))
    {
        ic_msg_take_and_delete(htlist_pop_entry(queue, ic_msg_t, msg_link));
    }

}

static void ___ic_query_flags(ichannel_t *ic, ic_msg_t *msg, uint32_t flags)
{
    bool async = msg->async;

    /* if that crashes, one of the IC_MSG_ABORT callback reenqueues directly in
     * that ichannel_t which is forbidden, fix the code
     */
    assert (ic->cancel_guard == false);

    msg->ic = ic;

    if (msg->timeout > 0) {
        msg->timeout_timer = el_timer_register(msg->timeout, 0,
                                               EL_TIMER_LOWRES,
                                               ic_msg_on_timeout, msg);
        el_unref(msg->timeout_timer);
    }

    if (!async) {
        unsigned start = ic->nextslot;

        do {
            if (ic->nextslot == IC_MSG_SLOT_MASK) {
                ic->nextslot = 1;
            } else {
                ic->nextslot++;
            }
            if (unlikely(ic->nextslot == start)) {
                /* can't find a free slot, abort this query */
                __ic_msg_reply_err(ic, msg, IC_MSG_ABORT);
                ic_msg_delete(&msg);
                return;
            }
            msg->slot = ic->nextslot;
        } while (qm_add(ic_msg, &ic->queries, msg->slot, msg));
    }
    if (ic_is_local(ic)) {
        const void *data;
        int dlen;
        const ic_msg_t *unpacked_msg;

        ic_msg_update_flags(msg, &flags);
        ic_update_pending(ic, msg->slot);
        assert (msg->force_pack == !!msg->dlen);
        if (msg->force_pack) {
            data = (char *)msg->data + IC_MSG_HDR_LEN;
            dlen = msg->dlen - IC_MSG_HDR_LEN;
            assert (dlen >= 0);
            unpacked_msg = NULL;
        } else {
            data = NULL;
            dlen = 0;
            unpacked_msg = msg;
        }
        ic_read_process_query(ic, msg->cmd, msg->slot, flags, data, dlen,
                              unpacked_msg);
        if (async) {
            ic_msg_delete(&msg);
        }
    } else {
        ic_queue(ic, msg, flags);
    }
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

void
__ic_msg_build(ic_msg_t *msg, const iop_struct_t *st, const void *arg,
               bool do_bpack)
{
    if (do_bpack) {
        __ic_bpack(msg, st, arg);
    } else {
        msg->dlen = 0;
        msg->data = (void *)arg;
    }
}

static ic_msg_t *
ic_msg_new_for_reply(ichannel_t **ic, uint64_t slot, int cmd)
{
    assert (!(slot & IC_SLOT_FOREIGN_MASK));

    if (unlikely(!*ic)) {
        *ic = ic_get_from_slot(slot);
        if (unlikely(!*ic))
            return NULL;
    }
    if (likely(ic_can_reply(*ic, slot))) {
        ic_msg_t *msg = ic_msg_new(0);

        ic_msg_init_for_reply(*ic, msg, slot, cmd);
        return msg;
    }
    return NULL;
}

static void ic_queue_for_reply(ichannel_t *ic, ic_msg_t *msg)
{
    if (ic_is_local(ic)) {
        const void *data = NULL;
        int dlen = 0;

        if (msg->dlen) {
            assert (msg->force_pack
                    || (-msg->cmd != IC_MSG_OK && -msg->cmd != IC_MSG_EXN));

            data = (char *)msg->data + IC_MSG_HDR_LEN;
            dlen = msg->dlen - IC_MSG_HDR_LEN;
            assert (dlen >= 0);
        }
        ic_read_process_answer(ic, msg->cmd, msg->slot, data, dlen,
                               msg->force_pack ? NULL : msg);
        ic_msg_delete(&msg);
    } else {
        ic_queue(ic, msg, 0);
    }
}

size_t __ic_reply(ichannel_t *ic, uint64_t slot, int cmd, int fd,
                  const iop_struct_t *st, const void *arg)
{
    ic_msg_t *msg;
    int res;

    assert (slot & IC_MSG_SLOT_MASK);

    if (unlikely(ic_slot_is_http(slot))) {
        assert (fd < 0);
        __ichttp_reply(slot, cmd, st, arg);
        return 0;
    }

    ic_query_do_post_hook(ic, cmd, slot);

    msg = ic_msg_new_for_reply(&ic, slot, cmd);
    if (!msg)
        return 0;

    if (ic_is_local(ic)) {
        int32_t pos = qm_find(ic_msg, &ic->queries, slot & IC_MSG_SLOT_MASK);

        if (likely(pos >= 0)) {
            if (unlikely(ic->queries.values[pos]->force_pack)) {
                msg->force_pack = true;
            } else {
                msg->force_dup = unlikely(ic->queries.values[pos]->force_dup);
            }
        }
    }
    msg->fd = fd;
    __ic_msg_build(msg, st, arg, !ic_is_local(ic) || msg->force_pack);
    res = msg->dlen;
    ic_queue_for_reply(ic, msg);
    return res;
}

static void ic_reply_err2(ichannel_t *ic, uint64_t slot, int err,
                          const lstr_t *err_str)
{
    ic_msg_t *msg;

    assert (slot & IC_MSG_SLOT_MASK);

    if (unlikely(ic_slot_is_http(slot))) {
        __ichttp_reply_err(slot, err, err_str);
        return;
    }

    ic_query_do_post_hook(ic, err, slot);

    msg = ic_msg_new_for_reply(&ic, slot, err);
    if (!msg)
        return;

    if (err_str && err_str->len) {
        msg->data = mp_new_raw(ic_mp_g, char, IC_MSG_HDR_LEN + err_str->len + 1);
        msg->dlen = IC_MSG_HDR_LEN + err_str->len + 1;
        memcpyz((char *)msg->data + IC_MSG_HDR_LEN, err_str->s, err_str->len);
    } else {
        msg->data = mp_new_raw(ic_mp_g, char, IC_MSG_HDR_LEN);
        msg->dlen = IC_MSG_HDR_LEN;
    }
    ic_queue_for_reply(ic, msg);
}

void ic_reply_err(ichannel_t *ic, uint64_t slot, int err)
{
    ic_reply_err2(ic, slot, err, NULL);
}

static void ic_sc_do(ichannel_t *ic, int sc_op)
{
    if (!ic->is_closing && !ic_is_local(ic)) {
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
    return qm_get_def(ic, &ic_h_g, id, NULL);
}

ichannel_t *ic_init(ichannel_t *ic)
{
    /* ic_initialize() should be called before ic_init() */
    assert (ic_h_g.k_size);

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
    ic->retry_delay = 1000;

    return ic;
}

static int ic_mark_connected(ichannel_t *ic, int fd)
{
    ic->queuable = true;
    el_fd_set_hook(ic->elh, ic_event);
    ic->on_event(ic, IC_EVT_CONNECTED);
    if (ic->on_creds && !ic->is_stream) {
        ic_creds_t creds;
#if defined(OS_LINUX)
        struct ucred ucred;
        socklen_t len = sizeof(ucred);

        RETHROW(getsockopt(fd, SOL_SOCKET, SO_PEERCRED, &ucred, &len));
        creds = (ic_creds_t){ .uid = ucred.uid, .gid = ucred.gid, .pid = ucred.pid };
#elif defined(OS_APPLE)
        struct xucred xucred;
        socklen_t len = sizeof(xucred);

        RETHROW(getsockopt(fd, SOL_LOCAL, LOCAL_PEERCRED, &xucred, &len));
        creds = (ic_creds_t){ .uid = xucred.cr_uid, .gid = xucred.cr_gid, .pid = -1 };
#else
# error "unsupported OS"
#endif
        RETHROW((*ic->on_creds)(ic, &creds));

    }
    /* force an exchange at connect time, so force NOP if queue is empty */
    if (htlist_is_empty(&ic->msg_list))
        ic_nop(ic);
    el_fd_set_mask(ic->elh, POLLINOUT);
    return 0;
}

void ic_mark_disconnected(ichannel_t *ic)
{
    ic->is_closing = true;
    ic_disconnect(ic);
    if (!ic->is_spawned && ic->auto_reconn) {
        assert (ic->timer == NULL);
        ic->timer = el_timer_register(ic->retry_delay, 0, 0, ic_reconnect,
                                      ic);
        el_unref(ic->timer);
    }
}

static int ic_connecting(el_t ev, int fd, short events, data_t priv)
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

static void ic_watch_act_soft(el_t ev, data_t priv)
{
    ichannel_t *ic = priv.ptr;

    ic->on_event(ic, IC_EVT_NOACT);
    el_timer_unregister(&ic->wa_soft_timer);
}

static void ic_watch_act_nop(el_t ev, data_t priv)
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

    if (unlikely(ic_is_local(ic)))
        e_fatal("cannot connect a local ic");

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
    if (ic_is_local(ic))
        return;

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

    assert (!ic_is_local(ic));
    if (unlikely(ic_is_local(ic)))
        return;

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

static int ic_accept(el_t ev, int fd, short events, data_t priv)
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
