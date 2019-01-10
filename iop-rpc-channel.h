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

#if !defined(IS_LIB_COMMON_IOP_RPC_H) || defined(IS_LIB_COMMON_IOP_RPC_CHANNEL_H)
#  error "you must include <lib-common/iop-rpc.h> instead"
#else
#define IS_LIB_COMMON_IOP_RPC_CHANNEL_H

#include <lib-common/container-htlist.h>

#if 0
#define IC_DEBUG_REPLIES
#endif
#ifdef IC_DEBUG_REPLIES
qh_k64_t(ic_replies);
#endif

typedef struct ichannel_t    ichannel_t;
typedef struct ic_msg_t      ic_msg_t;
typedef struct ic_hook_ctx_t ic_hook_ctx_t;

typedef enum ic_event_t {
    IC_EVT_CONNECTED,
    IC_EVT_DISCONNECTED,
    IC_EVT_ACT,   /* used to notify of first activity when using soft wa */
    IC_EVT_NOACT, /* used to notify no activity when using soft wa       */
} ic_event_t;


#define IC_MSG_HDR_LEN             12
#define IC_MSG_CMD_OFFSET           4
#define IC_MSG_DLEN_OFFSET          8
#define IC_PKT_MAX              65536

#define IC_ID_MAX               BITMASK_LE(uint32_t, 30)
#define IC_MSG_SLOT_MASK        (0xffffff)
#define IC_MSG_HAS_FD           (1U << 24)
#define IC_MSG_HAS_HDR          (1U << 25)
#define IC_MSG_IS_TRACED        (1U << 26)
#define IC_MSG_PRIORITY_SHIFT   27
#define IC_MSG_PRIORITY_MASK    (BITMASK_LT(uint32_t,                        \
                                            2) << IC_MSG_PRIORITY_SHIFT)

#define IC_PROXY_MAGIC_CB       ((ic_msg_cb_f *)-1)

typedef struct ic_creds_t {
    uid_t uid;
    gid_t gid;
    int   pid;
} ic_creds_t;

typedef void (ic_hook_f)(ichannel_t *, ic_event_t evt);
typedef void (ic_pre_hook_f)(ichannel_t *, uint64_t,
                             const ic__hdr__t *, data_t);
typedef void (ic_post_hook_f)(ichannel_t *, ic_status_t,
                              ic_hook_ctx_t *, data_t);
typedef int (ic_creds_f)(ichannel_t *, const ic_creds_t *creds);
typedef void (ic_msg_cb_f)(ichannel_t *, ic_msg_t *,
                           ic_status_t, void *, void *);

struct ic_msg_t {
    htnode_t      msg_link;        /**< private field used by ichannel_t */
    int           fd         : 24; /**< the fd to send */
    flag_t        async      :  1; /**< whether the RPC is async */
    flag_t        raw        :  1; /**< whether the answer should be decoded
                                        or not. */
    flag_t        force_pack :  1; /**< if set then msg is packed even if it
                                        is used with a local ic */
    flag_t        force_dup  :  1; /**< if set when ic is local and force_pack
                                        is false then hdr and arg are
                                        duplicated before being used in rpc
                                        implementation */
    flag_t        trace      :  1; /**< Activate tracing for this message. */
    flag_t        canceled   :  1; /**< Is the query canceled ? */
    ev_priority_t priority   :  2; /**< Priority of the message. */
    int32_t  cmd;                  /**< automatically filled by ic_query/reply
                                        */
    uint32_t slot;                 /**< automatically filled by ic_query/reply
                                        */
    uint32_t timeout;              /**< max lifetime of the query */
    ichannel_t *ic;                /**< the ichannel_t used for the query */
    el_t     timeout_timer;
    unsigned dlen;
    void    *data;
    pstream_t raw_res;

    /* user provided fields */
    const iop_rpc_t  *rpc;
    const ic__hdr__t *hdr;
    ic_msg_cb_f      *cb;
    byte              priv[];
};
ic_msg_t *ic_msg_new(int len);
#define ic_msg_p(_t, _v)                                                     \
    ({                                                                       \
        ic_msg_t *_msg = ic_msg_new(sizeof(_t));                             \
        *acast(_t, _msg->priv) = *(_v);                                      \
        _msg;                                                                \
    })
#define ic_msg(_t, ...)  ic_msg_p(_t, (&(_t){ __VA_ARGS__ }))

ic_msg_t *ic_msg_new_fd(int fd, int len);
ic_msg_t *ic_msg_proxy_new(int fd, uint64_t slot, const ic__hdr__t *hdr);
void ic_msg_delete(ic_msg_t **);

/** Cancel an ic message.
 *
 * Flag an ic_msg_t as "canceled". The message will not be sent or, if it is
 * already sent, the potential answer will be dropped.
 *
 * \param[in]  msg the message to cancel.
 */
void ic_msg_cancel(ic_msg_t *msg);

/** Set a timeout for ic_msg_t.
 *
 *  When sending a query with such a ic_msg_t, the query will fail with the
 *  IC_MSG_TIMEDOUT error if it is not entirely processed after 'timeout'
 *  milliseconds.
 *
 * \param[in]  msg      the ic_msg_t on which the timeout should be set.
 * \param[in]  timeout  the maximum lifetime, in milliseconds, of the query in
 *                      the ichannel.
 *                      Upon expiration of this period, the query will fail
 *                      with IC_MSG_TIMEDOUT if it hasn't been processed yet.
 *
 * \return the ic_msg_t with the timeout set.
 */
ic_msg_t *ic_msg_set_timeout(ic_msg_t *msg, uint32_t timeout);

/** Set a priority for ic_msg_t.
 *
 * When adding a message to the queue, messages with a high priority level
 * will be at the head, just after the other messages having the same priority
 * level, while messages with a low priority level will be at the tail. As a
 * consequence, if there is a lot of messages with a high priority level,
 * messages with a low or normal priority will never be pop'ed from the queue.
 *
 * \param[in]  msg       the ic_msg_t on which the priority should be set.
 * \param[in]  priority  the priority to set.
 *
 * \return the ic_msg_t with the priority set.
 */
ic_msg_t *ic_msg_set_priority(ic_msg_t *msg, ev_priority_t priority);

qm_k32_t(ic_msg, ic_msg_t *);

struct ic_hook_ctx_t {
    uint64_t         slot;
    ic_post_hook_f  *post_hook;
    const iop_rpc_t *rpc;
    data_t           post_hook_args;
    byte             data[];  /* data to pass through RPC workflow */
};

int ic_hook_ctx_save(ic_hook_ctx_t *ctx);
ic_hook_ctx_t *ic_hook_ctx_new(uint64_t slot, ssize_t extra);
ic_hook_ctx_t *ic_hook_ctx_get(uint64_t slot);
void ic_hook_ctx_delete(ic_hook_ctx_t **pctx);

enum ic_cb_entry_type {
    IC_CB_NORMAL,
    IC_CB_PROXY_P,
    IC_CB_PROXY_PP,
    IC_CB_DYNAMIC_PROXY,
    IC_CB_WS_SHARED,
};

typedef struct ic_dynproxy_t {
    ichannel_t *ic;
    ic__hdr__t *hdr;
} ic_dynproxy_t;

/** Callback to fetch a dynamic proxy (pair of ichannel + header).
 *
 * This function is allowed to return an header allocated on the t_pool() just
 * like a t_ function.
 */
typedef ic_dynproxy_t (ic_dynproxy_f)(ic__hdr__t *hdr, void *priv);

#define IC_DYNPROXY_NULL    ((ic_dynproxy_t){ .ic = NULL })
#define IC_DYNPROXY(_ic)    ((ic_dynproxy_t){ .ic = (_ic) })
#define IC_DYNPROXY_HDR(_ic, _hdr) \
    ((ic_dynproxy_t){ .ic = (_ic), .hdr = (_hdr) })

typedef struct ic_cb_entry_t {
    int cb_type;
    const iop_rpc_t *rpc;

    ic_pre_hook_f  *pre_hook;
    ic_post_hook_f *post_hook;
    data_t          pre_hook_args;
    data_t          post_hook_args;
    union {
        struct {
            void (*cb)(ichannel_t *, uint64_t, void *, const ic__hdr__t *);
        } cb;

        struct {
            ichannel_t *ic_p;
            ic__hdr__t *hdr_p;
        } proxy_p;

        struct {
            ichannel_t **ic_pp;
            ic__hdr__t **hdr_pp;
        } proxy_pp;

        struct {
            ic_dynproxy_f *get_ic;
            void *priv;
        } dynproxy;

        struct {
            void (*cb)(void *, uint64_t, void *, const ic__hdr__t *);
        } iws_cb;
    } u;
} ic_cb_entry_t;
qm_k32_t(ic_cbs, ic_cb_entry_t);
extern qm_t(ic_cbs) const ic_no_impl;

struct ichannel_t {
    uint32_t id;

    flag_t is_closing   :  1;
    flag_t is_spawned   :  1;   /**< auto delete if true                    */
    flag_t no_autodel   :  1;   /**< disable autodelete feature             */
    flag_t is_stream    :  1;   /**< true if socket is SOCK_STREAMed        */
    flag_t auto_reconn  :  1;
    flag_t do_el_unref  :  1;
    flag_t is_wiped     :  1;
    flag_t cancel_guard :  1;
    flag_t queuable     :  1;
    flag_t is_local     :  1;
    flag_t is_trusted   :  1;   /**< set to true for internal ichannels     */

    unsigned nextslot;          /**< next slot id to try                    */

    el_t              elh;
    el_t              timer;
    ichannel_t      **owner;    /**< content set to NULL on deletion        */
    void             *priv;     /**< user private data                      */
    void             *peer;     /**< user field to identify the peer        */
    const iop_rpc_t  *desc;     /**< desc of the current unpacked RPC       */
    int               cmd;      /**< cmd of the current unpacked structure  */
    ev_priority_t     priority; /**< priority of the channel                */

    el_t wa_soft_timer;
    int  wa_soft;             /**< to be notified when no activity          */
    int  wa_hard;             /**< to close the connection when no activity */

    int          protocol;     /**< transport layer protocol (0 = default) */
    int          retry_delay;  /**< delay before a reconnection attempt (ms) */
    sockunion_t  su;
    const qm_t(ic_cbs) *impl;
    ic_hook_f   *on_event;
    ic_creds_f  *on_creds;
    void        (*on_wipe)(ichannel_t *ic);

    /* private */
    qm_t(ic_msg) queries;      /**< hash of queries waiting for an answer  */
    htlist_t     iov_list;     /**< list of messages to send, in iov       */
    htlist_t     msg_list;     /**< list of messages to send               */
    htnode_t    *last_normal_prio_msg;  /**< last message of msg_list having
                                             the priority NORMAL */
    int          current_fd;   /**< used to store the current fd           */
    int          pending;

    /* Buffers */
    qv_t(iovec)  iov;
    int          iov_total_len;
    int          wpos;
    sb_t         rbuf;

    lstr_t       peer_address;
#ifdef IC_DEBUG_REPLIES
    qh_t(ic_replies) dbg_replies;
#endif
#ifndef NDEBUG
    int pending_max;
#endif
};

void ic_drop_ans_cb(ichannel_t *, ic_msg_t *,
                    ic_status_t, void *, void *);

MODULE_DECLARE(ic);

/*----- ichannel handling -----*/

static inline bool ic_is_local(const ichannel_t *ic) {
    return ic->is_local;
}

static inline void ic_set_local(ichannel_t *ic) {
    ic->is_local = true;
    ic->peer_address = LSTR("127.0.0.1");
}

static inline int ic_get_fd(ichannel_t *ic) {
    int res = ic->current_fd;
    ic->current_fd = -1;
    return res;
}
static inline int ic_queue_len(ichannel_t *ic) {
    return qm_len(ic_msg, &ic->queries);
}
static inline bool ic_is_empty(ichannel_t *ic) {
    return htlist_is_empty(&ic->msg_list)
        && htlist_is_empty(&ic->iov_list)
        && ic_queue_len(ic) == 0
        && !ic->pending;
}

/* XXX be carefull, this function do not mean that the ichannel is actually
 * connected, just that you are allowed to queue some queries */
static inline bool ic_is_ready(const ichannel_t *ic) {
    return (ic_is_local(ic) && ic->impl)
        || (ic->elh && ic->queuable && !ic->is_closing);
}

static inline bool ic_slot_is_async(uint64_t slot) {
    return !(slot & IC_MSG_SLOT_MASK);
}

/** \brief watch the incoming activity of an ichannel.
 *
 * If a positive soft timeout is given, the on_event callback will be called
 * with IC_EVT_NOACT when an inactivity period of timeout_soft milliseconds
 * is detected, and with IC_EVT_ACT when an activity occurs after a period
 * of inactivity.
 *
 * If a positive hard timeout is given, the ichannel connection will be
 * automatically closed if an inactivity period of timeout_hard milliseconds
 * is detected.
 *
 * If one of the two given timeouts is positive, some outgoing traffic will
 * be generated each timeout / 3 milliseconds.
 *
 * In general, this function should be called with the same arguments on both
 * client and server side.
 */
void ic_watch_activity(ichannel_t *ic, int timeout_soft, int timeout_hard);

ev_priority_t ic_set_priority(ichannel_t *ic, ev_priority_t prio);
ichannel_t *ic_get_by_id(uint32_t id);
ichannel_t *ic_init(ichannel_t *);
void ic_wipe(ichannel_t *);
GENERIC_NEW(ichannel_t, ic);

__attr_nonnull__((1))
static inline void ic_delete(ichannel_t **icp)
{
    ichannel_t *ic = *icp;

    if (ic) {
        ic_wipe(ic);
        /* XXX never delete icp which may be set to NULL by ic_wipe() through
         * ic->owner. */
        p_delete(&ic);
        *icp = NULL;
    }
}

int  ic_connect(ichannel_t *);
int  ic_connect_blocking(ichannel_t *ic);
void ic_disconnect(ichannel_t *ic);
void ic_spawn(ichannel_t *, int fd, ic_creds_f *fn);
void ic_bye(ichannel_t *);
void ic_nop(ichannel_t *);

el_t ic_listento(const sockunion_t *su, int type, int proto,
                 int (*on_accept)(el_t ev, int fd));
void ic_flush(ichannel_t *ic);
lstr_t ic_get_client_addr(ichannel_t *ic);

/** Mark an ichannel_t as disconnected.
 *
 * This function ensures that the ichannel_t is properly disconnected and
 * restart the connection procedure if needed. This is useful when having a
 * connected but not properly started ichannel (handshake failure for
 * example).
 *
 * \param[in]  ic The ichannel_t to mark as disconnected.
 */
void ic_mark_disconnected(ichannel_t *ic);

/*----- rpc handling / registering -----*/

/** \brief builds the typed argument list of the implementation of an rpc.
 *
 * This macro builds the arguments of an rpc implementing the server-side of
 * an IC RPC.
 *
 * For example, for this iop:
 * <code>
 * package pkg;
 *
 * interface If {
 *     funCall in ... out ... throw ...;
 * }
 *
 * module mod {
 *     If  myIface;
 * }
 * </code>
 *
 * Then one can define the following suitable implementation callback for
 * pkg.mod.myIface.funCall:
 *
 * <code>
 * void rpc_impl(IOP_RPC_IMPL_ARGS(pkg__mod, my_iface, fun_call))
 * {
 *     ic_reply_err(ic, slot, IC_MSG_UNIMPLEMENTED);
 * }
 * </code>
 *
 * \param[in]  _mod   name of the package+module of the RPC
 * \param[in]  _i     name of the interface of the RPC
 * \param[in]  _r     name of the rpc
 * \return
 *   typed arguments of the implementation callback: (ic, slot, arg, hdr).
 *   - \v ic is the name of the calling iop-channel (to be used for
 *     synchronous replies).
 *   - \v slot is the slot of the query, to be ised in the ic_reply call.
 *   - \v arg the properly typed unfolded argument of the query. Note that the
 *     argument is allocated on the stack, hence should be duplicated in the
 *     implementation callback if needed.
 *   - \v hdr the IC query header (if any).
 */
#define IOP_RPC_IMPL_ARGS(_mod, _i, _r) \
    ichannel_t *ic, uint64_t slot, IOP_RPC_T(_mod, _i, _r, args) *arg,       \
    const ic__hdr__t *hdr

/** \brief builds the typed argument list of the reply callback of an rpc.
 *
 * This macro builds the arguments of a reply callback (client side) of an IC
 * RPC.
 *
 * For example, for this iop:
 * <code>
 * package pkg;
 *
 * interface If {
 *     funCall in ... out ... throw ...;
 * }
 *
 * module mod {
 *     If  myIface;
 * }
 * </code>
 *
 * Then one can define the following suitable reply callback for
 * pkg.mod.myIface.funCall:
 *
 * <code>
 * void rpc_answer_cb(IOP_RPC_CB_ARGS(pkg__mod, my_iface, fun_call))
 * {
 *     if (status == IC_MSG_OK) {
 *         // one can use res here;
 *     } else
 *     if (status == IC_MSG_EXN) {
 *         // one can use exn here;
 *     } else {
 *         // deal with transport error
 *     }
 * }
 * </code>
 *
 * \param[in]  _mod   name of the package+module of the RPC
 * \param[in]  _i     name of the interface of the RPC
 * \param[in]  _r     name of the rpc
 * \return
 *   typed arguments of the reply callback: (ic, msg, status, res, exn).
 *   - \v ic is the called iop-channel (shouldn't actually be used!).
 *   - \v msg is the original message sent with ic_query. It doesn't need to
 *     be freed, though if data is put in the "priv" pointer it must be
 *     reclaimed in the reply callback.
 *   - \v status is the status of the reply.
 *   - \v res is the value of the callback result when \v status is
 *     #IC_MSG_OK, and should not be accessed otherwise.
 *   - \v exn is the value of the callback result when \v status is
 *     #IC_MSG_EXN, and should not be accessed otherwise.
 */
#define IOP_RPC_CB_ARGS(_mod, _i, _r) \
    ichannel_t *ic, ic_msg_t *msg, ic_status_t status, \
    IOP_RPC_T(_mod, _i, _r, res) *res,                 \
    IOP_RPC_T(_mod, _i, _r, exn) *exn

/* some useful macros to define IOP rpcs and callbacks */

/** \brief builds an RPC name.
 * \param[in]  _m     name of the package+module of the RPC
 * \param[in]  _i     name of the interface of the RPC
 * \param[in]  _r     name of the rpc
 * \param[in]  sfx    a unique suffix to distinguish usages (cb, impl, ...)
 */
#define IOP_RPC_NAME(_m, _i, _r, sfx)  _m##__##_i##__##_r##__##sfx

/** \brief builds an RPC Implementation prototype.
 * \param[in]  _m     name of the package+module of the RPC
 * \param[in]  _i     name of the interface of the RPC
 * \param[in]  _r     name of the rpc
 */
#define IOP_RPC_IMPL(_m, _i, _r) \
    IOP_RPC_NAME(_m, _i, _r, impl)(IOP_RPC_IMPL_ARGS(_m, _i, _r))

/** \brief builds an RPC Callback prototype.
 * \param[in]  _m     name of the package+module of the RPC
 * \param[in]  _i     name of the interface of the RPC
 * \param[in]  _r     name of the rpc
 */
#define IOP_RPC_CB(_m, _i, _r) \
    IOP_RPC_NAME(_m, _i, _r, cb)(IOP_RPC_CB_ARGS(_m, _i, _r))

/*
 * XXX this is an ugly piece of preprocessing because we lack templates and
 *     because __builtin_choose_expr generates syntaxic errors *sigh*.
 *
 * First IOP_RPC_CB_REF calls IOP_RPC_CB_REF_ with the async-ness of the rpc
 * as a macro.
 *
 * We recurse into IOP_RPC_CB_REF__ so that 'async' evaluates to 0 or 1.
 *
 * We catenate it to IOP_RPC_CB_REF__ which means it will either:
 * - evaluate to IOP_RPC_CB_REF__0(...) -> build cb name
 * - evaluate to IOP_RPC_CB_REF__1(...) -> NULL
 */
#define IOP_RPC_CB_REF__0(_m, _i, _r)        IOP_RPC_NAME(_m, _i, _r, cb)
#define IOP_RPC_CB_REF__1(_m, _i, _r)        NULL
#define IOP_RPC_CB_REF__(async, _m, _i, _r)  IOP_RPC_CB_REF__##async(_m, _i, _r)
#define IOP_RPC_CB_REF_(async, _m, _i, _r)   IOP_RPC_CB_REF__(async, _m, _i, _r)

/** \brief builds an RPC callback reference (NULL if the RPC is async).
 * \param[in]  _m     name of the package+module of the RPC
 * \param[in]  _i     name of the interface of the RPC
 * \param[in]  _r     name of the rpc
 */
#define IOP_RPC_CB_REF(_m, _i, _r) \
    IOP_RPC_CB_REF_(_m##__##_i(_r##__rpc__async), _m, _i, _r)

/** \brief register local callback and pre/post hooks for an rpc.
 * \param[in]  h
 *    the qm_t(ic_cbs) of implementation to register the rpc
 *    implementation into.
 * \param[in]  _mod   name of the package+module of the RPC
 * \param[in]  _i     name of the interface of the RPC
 * \param[in]  _r     name of the rpc
 * \param[in]  _cb
 *    the implementation callback. Its type should be:
 *    <tt>void (*)(IOP_RPC_IMPL_ARGS(_mod, _if, _rpc))</tt>
 * \param[in]  _pre_cb
 *    the pre_hook callback. Its type should be:
 *    <tt>void (*)(ichannel_t *, uint64_t, const ic__hdr__t *, void *)</tt>
 * \param[in]  _post_cb
 *    the post_hook callback. Its type should be:
 *    <tt>void (*)(ichannel_t *, ic_status_t, ic_hook_ctx_t *, void *)</tt>
 * \param[in]  _pre_arg   argument we want to pass to pre_hook
 * \param[in]  _post_arg  argument we want to pass to post_hook
 */
#define ic_register_pre_post_hook_(h, _mod, _if, _rpc, _cb,                  \
                                   _pre_cb, _post_cb, _pre_arg, _post_arg)   \
    do {                                                                     \
        void (*__cb)(IOP_RPC_IMPL_ARGS(_mod, _if, _rpc)) = _cb;              \
        uint32_t cmd    = IOP_RPC_CMD(_mod, _if, _rpc);                      \
        ic_cb_entry_t e = {                                                  \
            .cb_type = IC_CB_NORMAL,                                         \
            .rpc = IOP_RPC(_mod, _if, _rpc),                                 \
            .pre_hook = _pre_cb,                                             \
            .post_hook = _post_cb,                                           \
            .pre_hook_args = _pre_arg,                                       \
            .post_hook_args = _post_arg,                                     \
            .u = { .cb = {                                                   \
                .cb  = (void *)__cb,                                         \
            } },                                                             \
        };                                                                   \
        e_assert_n(panic, qm_add(ic_cbs, h, cmd, e),                         \
                   "collision in RPC registering");                          \
    } while (0)

/** \brief same as #ic_register_pre_post_hook_ but _pre and _post args
 *    will be transform into data_t ptr.
 */
#define ic_register_pre_post_hook_p_(h, _mod, _if, _rpc, _cb,                \
                                     _pre_cb, _post_cb, _pre_arg, _post_arg) \
    ic_register_pre_post_hook_(h, _mod, _if, _rpc, _cb,                      \
                               _pre_cb, _post_cb,                            \
                               { .ptr = _pre_arg }, { .ptr = _post_arg })

/** \brief same as #ic_register_pre_post_hook_ but auto-computes the
 *    rpc name.
 */
#define ic_register_pre_post_hook(h, _m, _i, _r, _pre_cb,                    \
                                  _post_cb, _pre_arg, _post_arg)             \
    ic_register_pre_post_hook_(h, _m, _i, _r,                                \
                               IOP_RPC_NAME(_m, _i, _r, impl),               \
                               _pre_cb, _post_cb, _pre_arg, _post_arg)
/** \brief same as #ic_register_pre_post_hook_p_ but auto-computes the
 *    rpc name.
 */
#define ic_register_pre_post_hook_p(h, _m, _i, _r, _pre_cb,                  \
                                    _post_cb, _pre_arg, _post_arg)           \
    ic_register_pre_post_hook(h, _m, _i, _r, _pre_cb, _post_cb,              \
                              { .ptr = _pre_arg }, { .ptr = _post_arg })

/** \brief same as #ic_register_pre_post_hook_ but doesn't register pre/post
 *    hooks.
 */
#define ic_register_(h, _mod, _if, _rpc, _cb)                                \
    ic_register_pre_post_hook_p_(h, _mod, _if, _rpc, _cb,                    \
                                 NULL, NULL, NULL, NULL)

/** \brief same as #ic_register_ but auto-computes the rpc name. */
#define ic_register(h, _m, _i, _r) \
    ic_register_(h, _m, _i, _r, IOP_RPC_NAME(_m, _i, _r, impl))

/** \brief unregister a local callback for an rpc.
 * \param[in]  h
 *    the qm_t(ic_cbs) of implementation of which you want to unregister the
 *    rpc implementation.
 * \param[in]  _mod   name of the package+module of the RPC
 * \param[in]  _i     name of the interface of the RPC
 * \param[in]  _r     name of the rpc
 */
#define ic_unregister(h, _mod, _if, _rpc) \
    do {                                                                     \
        uint32_t cmd = IOP_RPC_CMD(_mod, _if, _rpc);                         \
        qm_del_key(ic_cbs, h, cmd);                                          \
    } while (0)

/** \brief register a proxy destination for the given rpc with forced header.
 * \param[in]  h
 *    the qm_t(ic_cbs) of implementation to register the rpc
 *    implementation into.
 * \param[in]  _mod   name of the package+module of the RPC
 * \param[in]  _i     name of the interface of the RPC
 * \param[in]  _r     name of the rpc
 * \param[in]  ic
 *   the #ichannel_t to unconditionnaly forward the incoming RPCs to.
 * \param[in]  hdr    the #ic__hdr__t header to force when proxifying.
 * \param[in]  _pre_cb
 *    the pre_hook callback. Its type should be:
 *    <tt>void (*)(ichannel_t *, uint64_t, const ic__hdr__t *, void *)</tt>
 * \param[in]  _post_cb
 *    the post_hook callback. Its type should be:
 *    <tt>void (*)(ichannel_t *, ic_status_t, ic_hook_ctx_t *, void *)</tt>
 * \param[in]  _pre_arg   argument we want to pass to pre_hook
 * \param[in]  _post_arg  argument we want to pass to post_hook
 */
#define ic_register_proxy_hdr_pre_post_hook(h, _mod, _if, _rpc, ic, hdr,     \
                                            _pre_cb, _post_cb,               \
                                            _pre_arg, _post_arg)             \
    do {                                                                     \
        uint32_t cmd    = IOP_RPC_CMD(_mod, _if, _rpc);                      \
        ic_cb_entry_t e = {                                                  \
            .cb_type = IC_CB_PROXY_P,                                        \
            .rpc = IOP_RPC(_mod, _if, _rpc),                                 \
            .pre_hook = _pre_cb,                                             \
            .post_hook = _post_cb,                                           \
            .pre_hook_args = _pre_arg,                                       \
            .post_hook_args = _post_arg,                                     \
            .u = { .proxy_p = { .ic_p = ic, .hdr_p = hdr } },                \
        };                                                                   \
        qm_add(ic_cbs, h, cmd, e);                                           \
    } while (0)

/** \brief same as #ic_register_proxy_hdr_pre_post_hook but _pre and _post
 *    args will be transform into data_t ptr.
 */
#define ic_register_proxy_hdr_pre_post_hook_p(h, _mod, _if, _rpc, ic, hdr,   \
                                              _pre_cb, _post_cb,             \
                                              _pre_arg, _post_arg)           \
    ic_register_proxy_hdr_pre_post_hook(h, _mod, _if, _rpc, ic, hdr,         \
                                        _pre_cb, _post_cb,                   \
                                        { .ptr = _pre_arg },                 \
                                        { .ptr = _post_arg })

/** \brief same as #ic_register_proxy_hdr_pre_post_hook but don't set
 * the hdr.
 */
#define ic_register_proxy_pre_post_hook(h, _mod, _if, _rpc, ic, _pre_cb,     \
                                        _post_cb, _pre_arg, _post_arg)       \
    ic_register_proxy_hdr_pre_post_hook(h, _mod, _if, _rpc, ic, NULL,        \
                                        _pre_cb, _post_cb,                   \
                                        _pre_arg, _post_arg)

/** \brief same as #ic_register_proxy_hdr_pre_post_hook but _pre and _post
 *    args will be transform into data_t ptr.
 */
#define ic_register_proxy_pre_post_hook_p(h, _mod, _if, _rpc, ic, _pre_cb,   \
                                          _post_cb, _pre_arg, _post_arg)     \
    ic_register_proxy_hdr_pre_post_hook(h, _mod, _if, _rpc, ic, NULL,        \
                                        _pre_cb, _post_cb,                   \
                                        { .ptr = _pre_arg },                 \
                                        { .ptr = _post_arg })
/** \brief register a proxy destination for the given rpc with forced header.
 * \param[in]  h
 *    the qm_t(ic_cbs) of implementation to register the rpc
 *    implementation into.
 * \param[in]  _mod   name of the package+module of the RPC
 * \param[in]  _i     name of the interface of the RPC
 * \param[in]  _r     name of the rpc
 * \param[in]  ic
 *   the #ichannel_t to unconditionnaly forward the incoming RPCs to.
 * \param[in]  hdr    the #ic__hdr__t header to force when proxifying.
 */
#define ic_register_proxy_hdr(h, _mod, _if, _rpc, ic, hdr)                   \
    ic_register_proxy_hdr_pre_post_hook_p(h, _mod, _if, _rpc, ic,            \
                                          hdr, NULL, NULL, NULL, NULL)

/** \brief register a proxy destination for the given rpc.
 * \param[in]  h
 *    the qm_t(ic_cbs) of implementation to register the rpc
 *    implementation into.
 * \param[in]  _mod   name of the package+module of the RPC
 * \param[in]  _i     name of the interface of the RPC
 * \param[in]  _r     name of the rpc
 * \param[in]  ic
 *   the #ichannel_t to unconditionnaly forward the incoming RPCs to.
 */
#define ic_register_proxy(h, _mod, _if, _rpc, ic) \
    ic_register_proxy_hdr(h, _mod, _if, _rpc, ic, NULL)

/** \brief register a pointed proxy destination for the given rpc with header.
 * \param[in]  h
 *    the qm_t(ic_cbs) of implementation to register the rpc
 *    implementation into.
 * \param[in]  _mod   name of the package+module of the RPC
 * \param[in]  _i     name of the interface of the RPC
 * \param[in]  _r     name of the rpc
 * \param[in]  icp
 *   a pointer to an #ichannel_t. When this pointer points to #NULL, the
 *   request is rejected with an #IC_MSG_PROXY_ERROR status, else it's
 *   proxified to the pointed #ichannel_t.
 * \param[in]  hdr    the #ic__hdr__t header to force when proxifying.
 * \param[in]  _pre_cb
 *    the pre_hook callback. Its type should be:
 *    <tt>void (*)(ichannel_t *, uint64_t, const ic__hdr__t *, void *)</tt>
 * \param[in]  _post_cb
 *    the post_hook callback. Its type should be:
 *    <tt>void (*)(ichannel_t *, ic_status_t, ic_hook_ctx_t *, void *)</tt>
 * \param[in]  _pre_arg   argument we want to pass to pre_hook
 * \param[in]  _post_arg  argument we want to pass to post_hook
 */
#define ic_register_proxy_hdr_p_pre_post_hook(h, _mod, _if, _rpc, ic, hdr,   \
                                              _pre_cb, _post_cb,             \
                                              _pre_arg, _post_arg)           \
    do {                                                                     \
        uint32_t cmd    = IOP_RPC_CMD(_mod, _if, _rpc);                      \
        ic_cb_entry_t e = {                                                  \
            .cb_type = IC_CB_PROXY_PP,                                       \
            .rpc = IOP_RPC(_mod, _if, _rpc),                                 \
            .pre_hook = _pre_cb,                                             \
            .post_hook = _post_cb,                                           \
            .pre_hook_args = _pre_arg,                                       \
            .post_hook_args = _post_arg,                                     \
            .u = { .proxy_pp = { .ic_pp = ic, .hdr_pp = hdr } },             \
        };                                                                   \
        qm_add(ic_cbs, h, cmd, e);                                           \
    } while (0)

/** \brief same as #ic_register_proxy_hdr_p_pre_post_hook but _pre and _post
 *    args will be transform into data_t ptr.
 */
#define ic_register_proxy_hdr_p_pre_post_hook_p(h, _mod, _if, _rpc, ic, hdr, \
                                                _pre_cb, _post_cb,           \
                                                _pre_arg, _post_arg)         \
    ic_register_proxy_hdr_p_pre_post_hook(h, _mod, _if, _rpc, ic, hdr,       \
                                          _pre_cb, _post_cb,                 \
                                          { .ptr = _pre_arg },               \
                                          { .ptr = _post_arg })

/** \brief register a pointed proxy destination for the given rpc with header.
 * \param[in]  h
 *    the qm_t(ic_cbs) of implementation to register the rpc
 *    implementation into.
 * \param[in]  _mod   name of the package+module of the RPC
 * \param[in]  _i     name of the interface of the RPC
 * \param[in]  _r     name of the rpc
 * \param[in]  icp
 *   a pointer to an #ichannel_t. When this pointer points to #NULL, the
 *   request is rejected with an #IC_MSG_PROXY_ERROR status, else it's
 *   proxified to the pointed #ichannel_t.
 * \param[in]  hdr    the #ic__hdr__t header to force when proxifying.
 */
#define ic_register_proxy_hdr_p(h, _mod, _if, _rpc, ic, hdr)                 \
    ic_register_proxy_hdr_p_pre_post_hook_p(h, _mod, _if, _rpc, ic, hdr,     \
                                            NULL, NULL, NULL, NULL)

/** \brief register a pointed proxy destination for the given rpc.
 * \param[in]  h
 *    the qm_t(ic_cbs) of implementation to register the rpc
 *    implementation into.
 * \param[in]  _mod   name of the package+module of the RPC
 * \param[in]  _i     name of the interface of the RPC
 * \param[in]  _r     name of the rpc
 * \param[in]  icp
 *   a pointer to an #ichannel_t. When this pointer points to #NULL, the
 *   request is rejected with an #IC_MSG_PROXY_ERROR status, else it's
 *   proxified to the pointed #ichannel_t.
 */
#define ic_register_proxy_p(h, _mod, _if, _rpc, icp) \
    ic_register_proxy_hdr_p(h, _mod, _if, _rpc, icp, NULL)

/** \brief register a dynamic proxy destination for the given rpc.
 * \param[in]  h
 *    the qm_t(ic_cbs) of implementation to register the rpc
 *    implementation into.
 * \param[in]  _mod   name of the package+module of the RPC
 * \param[in]  _i     name of the interface of the RPC
 * \param[in]  _r     name of the rpc
 * \param[in]  cb
 *   a callback that returns an #ic_dynproxy_t (a pair of an #ichannel_t and
 *   an #ic__hdr__t). When profixying the callback is called. When it returns
 *   a #NULL #ichannel_t the request is rejected with a #IC_MSG_PROXY_ERROR
 *   status, else it's forwarded to this #ichannel_t using the returned
 *   #ic__hdr__t.
 * \param[in]  _priv
 *   an opaque pointer passed to the callback each time it's called.
 * \param[in]  _pre_cb
 *    the pre_hook callback. Its type should be:
 *    <tt>void (*)(ichannel_t *, uint64_t, const ic__hdr__t *, void *)</tt>
 * \param[in]  _post_cb
 *    the post_hook callback. Its type should be:
 *    <tt>void (*)(ichannel_t *, ic_status_t, ic_hook_ctx_t *, void *)</tt>
 * \param[in]  _pre_arg   argument we want to pass to pre_hook
 * \param[in]  _post_arg  argument we want to pass to post_hook
 */
#define ic_register_dynproxy_pre_post_hook(h, _mod, _if, _rpc, cb, _priv,    \
                                           _pre_cb, _post_cb,                \
                                           _pre_arg, _post_arg)              \
    do {                                                                     \
        uint32_t cmd    = IOP_RPC_CMD(_mod, _if, _rpc);                      \
        ic_cb_entry_t e = {                                                  \
            .cb_type = IC_CB_DYNAMIC_PROXY,                                  \
            .rpc = IOP_RPC(_mod, _if, _rpc),                                 \
            .pre_hook = _pre_cb,                                             \
            .post_hook = _post_cb,                                           \
            .pre_hook_args = _pre_arg,                                       \
            .post_hook_args = _post_arg,                                     \
            .u = { .dynproxy = {                                             \
                .get_ic = cb,                                                \
                .priv   = _priv,                                             \
            } },                                                             \
        };                                                                   \
        qm_add(ic_cbs, h, cmd, e);                                           \
    } while (0)

/** \brief same as #ic_register_dynproxy_pre_post_hook but _pre and _post args
 *    will be transform into data_t ptr.
 */
#define ic_register_dynproxy_pre_post_hook_p(h, _mod, _if, _rpc, cb, _priv,  \
                                             _pre_cb, _post_cb,              \
                                             _pre_arg, _post_arg)            \
    ic_register_dynproxy_pre_post_hook(h, _mod, _if, _rpc, cb, _priv,        \
                                       _pre_cb, _post_cb,                    \
                                       { .ptr = _pre_arg },                  \
                                       { .ptr = _post_arg })

/** \brief register a dynamic proxy destination for the given rpc.
 * \param[in]  h
 *    the qm_t(ic_cbs) of implementation to register the rpc
 *    implementation into.
 * \param[in]  _mod   name of the package+module of the RPC
 * \param[in]  _i     name of the interface of the RPC
 * \param[in]  _r     name of the rpc
 * \param[in]  cb
 *   a callback that returns an #ic_dynproxy_t (a pair of an #ichannel_t and
 *   an #ic__hdr__t). When profixying the callback is called. When it returns
 *   a #NULL #ichannel_t the request is rejected with a #IC_MSG_PROXY_ERROR
 *   status, else it's forwarded to this #ichannel_t using the returned
 *   #ic__hdr__t.
 * \param[in]  _priv
 *   an opaque pointer passed to the callback each time it's called.
 */
#define ic_register_dynproxy(h, _mod, _if, _rpc, cb, _priv)                  \
    ic_register_dynproxy_pre_post_hook_p(h, _mod, _if, _rpc, cb, _priv,      \
                                         NULL, NULL, NULL, NULL)

/*----- message handling -----*/

/** \brief internal do not use directly, or know what you're doing. */
void *__ic_get_buf(ic_msg_t *msg, int len);
/** \brief internal do not use directly, or know what you're doing. */
void  __ic_bpack(ic_msg_t *, const iop_struct_t *, const void *);
/** \brief internal do not use directly, or know what you're doing. */
void  __ic_msg_build(ic_msg_t *, const iop_struct_t *, const void *, bool);
/** \brief internal do not use directly, or know what you're doing. */
void  __ic_msg_build_from(ic_msg_t *, const ic_msg_t *);
/** \brief internal do not use directly, or know what you're doing. */
void  __ic_query_flags(ichannel_t *, ic_msg_t *, uint32_t flags);
/** \brief internal do not use directly, or know what you're doing. */
void  __ic_query(ichannel_t *, ic_msg_t *);
/** \brief internal do not use directly, or know what you're doing. */
void  __ic_query_sync(ichannel_t *, ic_msg_t *);

/** \brief internal do not use directly, or know what you're doing. */
size_t __ic_reply(ichannel_t *, uint64_t slot, int cmd, int fd,
                  const iop_struct_t *, const void *);

/** \brief reply to a given rpc with an error.
 *
 * This function is meant to be used either:
 * - synchronously, in an rpc implementation callback (in which case \v ic and
 *   \v slot should be the \v ic and \v slot passed to the implementation.
 *   callback).
 * - asynchronously, in which case \v ic \em MUST be #NULL, and \v slot should
 *   be the slot originally passed to the implementation callback.
 *
 * \param[in] ic
 *   the #ichannel_t to send the reply to, should be #NULL if the reply is
 *   asynchronous.
 * \param[in] slot  the slot of the query we're answering to.
 * \param[in] err
 *   the error status to use, should NOT be #IC_MSG_OK nor #IC_MSG_EXN.
 */
void ic_reply_err(ichannel_t *ic, uint64_t slot, int err);

/** \brief helper to set ctx and execute the pre hook of the query.
 *
 * \param[in]  ic    the #ichannel_t to send the query to.
 * \param[in]  slot  the slot of the received query.
 * \param[in]  hdr   the #ic__hdr__t of the query.
 * \param[in]  e     the #ic_cb_entry_t of the rpc called.
 *
 * return -1 if the pre_hook has replied to the query, 0 otherwise.
 */
int
ic_query_do_pre_hook(ichannel_t *ic, uint64_t slot,
                     const ic__hdr__t *hdr, const ic_cb_entry_t *e);
/** \brief helper to get and execute the post hook of the query.
 *
 * \param[in]  ic      the #ichannel_t to send the query to.
 * \param[in]  status  the received answer status parameter.
 * \param[in]  slot    the slot of the received query.
 */
void ic_query_do_post_hook(ichannel_t *ic, ic_status_t status, uint64_t slot);
#ifndef NDEBUG
bool __ic_rpc_is_traced(const iop_iface_t *iface, const iop_rpc_t *rpc);

/** Check if the given RPC is traced.
 *
 * Traces are automatically enabled for RPCs listed in IC_TRACE. That
 * environment variable is a space-separate list of interface and RPC names.
 * The names are provided in the camel-case form (same os .iop files),
 * including the package name:
 *
 *  IC_TRACE="core.Log" will trace all logging RPCs
 *  IC_TRACE="core.Log.setRootLevel" will trace the setRootLevel RPCs
 *
 * Traces are emitted using the ic/tracing logger that is a silent logger, and
 * thus you must make sure you activated that logger in order to get the
 * traces. For example, using the IS_DEBUG environment variable, this would be
 *
 *  IS_DEBUG=+ic/tracing:0
 *
 * Packed and unpacked arguments/responses/headers are emitted at trace level
 * 1 only.
 *
 * A more complete example with mixed RPC and interface names:
 *
 *  IC_TRACE="qkv.Base qkv.Repl.push" IS_DEBUG=+ic/tracing:0 ./zchk-cluster
 *
 * \param[in]  _mod   name of the package+module of the RPC
 * \param[in]  _if    name of the interface of the RPC
 * \param[in]  _rpc   name of the rpc
 * \return true if tracing is activated for that RPC.
 */
#define ic_rpc_is_traced(_mod, _if, _rpc)  ({                               \
        static int _mod##__##_if##__##_rpc##_traced = -1;                   \
                                                                            \
        if (unlikely(_mod##__##_if##__##_rpc##_traced < 0)) {               \
            _mod##__##_if##__##_rpc##_traced                                \
                = __ic_rpc_is_traced(&_mod##__##_if(if),                    \
                                     IOP_RPC(_mod, _if, _rpc));             \
        }                                                                   \
        _mod##__##_if##__##_rpc##_traced;                                   \
    })
#else
#define ic_rpc_is_traced(_mod, _if, _rpc)  (false)
#endif

/** \brief helper to prepare a typed query message.
 * \param[in]  _msg   the #ic_msg_t to prepare.
 * \param[in]  _cb    the rpc reply callback to use
 * \param[in]  _mod   name of the package+module of the RPC
 * \param[in]  _if    name of the interface of the RPC
 * \param[in]  _rpc   name of the rpc
 */
#define __ic_prepare_msg(_msg, _cb, _mod, _if, _rpc) \
    ({                                                                      \
        ic_msg_t *__msgp = (_msg);                                          \
        void (*__cb)(IOP_RPC_CB_ARGS(_mod, _if, _rpc)) = _cb;               \
        __msgp->cb = __cb != NULL ? (ic_msg_cb_f *)__cb : &ic_drop_ans_cb;  \
        __msgp->rpc = IOP_RPC(_mod, _if, _rpc);                             \
        __msgp->async = __msgp->rpc->async;                                 \
        __msgp->cmd = IOP_RPC_CMD(_mod, _if, _rpc);                         \
        __msgp->trace = __msgp->trace || ic_rpc_is_traced(_mod, _if, _rpc); \
        __msgp;                                                             \
    })

/** \brief helper to build a typed query message.
 * \param[in]  _ich   the #ichannel_t to send the query to.
 * \param[in]  _msg   the #ic_msg_t to fill.
 * \param[in]  _cb    the rpc reply callback to use
 * \param[in]  _mod   name of the package+module of the RPC
 * \param[in]  _if    name of the interface of the RPC
 * \param[in]  _rpc   name of the rpc
 * \param[in]  v      a <tt>${_mod}__${_if}__${_rpc}_args__t *</tt> value.
 */
#define ic_build_query_p(_ich, _msg, _cb, _mod, _if, _rpc, v) \
    ({                                                                      \
        const IOP_RPC_T(_mod, _if, _rpc, args) *__v = (v);                  \
        ic_msg_t *__msg = (_msg);                                           \
        const ichannel_t *__ich = (_ich);                                   \
        __ic_prepare_msg(__msg, (_cb), _mod, _if, _rpc);                    \
        __ic_msg_build(__msg, IOP_RPC(_mod, _if, _rpc)->args, __v,          \
                       !ic_is_local(__ich) || __msg->force_pack);           \
        __msg;                                                              \
    })

/** \brief helper to build a typed query message by duplicating another.
 * \param[in]  _msg      the #ic_msg_t to fill.
 * \param[in]  _msg_src  the #ic_msg_t to duplicate.
 */
#define ic_build_query_from(_msg, _msg_src) \
    ({                                                                      \
        ic_msg_t *__msg = (_msg), *__msg_src = (_msg_src);                  \
        __msg->cb         = __msg_src->cb;                                  \
        __msg->rpc        = __msg_src->rpc;                                 \
        __msg->async      = __msg_src->async;                               \
        __msg->cmd        = __msg_src->cmd;                                 \
        __msg->trace      = __msg_src->trace;                               \
        __msg->force_pack = true;                                           \
        __ic_msg_build_from(__msg, __msg_src);                              \
        __msg;                                                              \
    })

/** \brief helper to build a typed query message.
 *
 * \param[in]  _ic    the #ichannel_t to send the query to.
 * \param[in]  _msg   the #ic_msg_t to fill.
 * \param[in]  _cb    the rpc reply callback to use
 * \param[in]  _mod   name of the package+module of the RPC
 * \param[in]  _if    name of the interface of the RPC
 * \param[in]  _rpc   name of the rpc
 * \param[in]  ...
 *   the initializers of the value on the form <tt>.field = value</tt>
 */
#define ic_build_query(_ic, _msg, _cb, _mod, _if, _rpc, ...) \
    ic_build_query_p(_ic, _msg, _cb, _mod, _if, _rpc,                       \
                     (&(IOP_RPC_T(_mod, _if, _rpc, args)){ __VA_ARGS__ }))

/** \brief helper to send a query to a given ic.
 *
 * \param[in]  _ic    the #ichannel_t to send the query to.
 * \param[in]  _msg   the #ic_msg_t to fill.
 * \param[in]  _cb    the rpc reply callback to use
 * \param[in]  _mod   name of the package+module of the RPC
 * \param[in]  _if    name of the interface of the RPC
 * \param[in]  _rpc   name of the rpc
 * \param[in]  ...
 *   the initializers of the value on the form <tt>.field = value</tt>
 */
#define ic_query(_ic, _msg, _cb, _mod, _if, _rpc, ...) \
    ({  ichannel_t *_ich = (_ic);                                         \
        __ic_query(_ich, ic_build_query(_ich, _msg, _cb, _mod, _if, _rpc, \
                                        __VA_ARGS__));                    \
    })

/** \brief helper to send a query to a given ic.
 *
 * \param[in]  _ic    the #ichannel_t to send the query to.
 * \param[in]  _msg   the #ic_msg_t to fill.
 * \param[in]  _cb    the rpc reply callback to use
 * \param[in]  _mod   name of the package+module of the RPC
 * \param[in]  _if    name of the interface of the RPC
 * \param[in]  _rpc   name of the rpc
 * \param[in]  v      a <tt>${_mod}__${_if}__${_rpc}_args__t *</tt> value.
 */
#define ic_query_p(_ic, _msg, _cb, _mod, _if, _rpc, v) \
    ({  ichannel_t *_ich = (_ic);                                           \
        __ic_query(_ich, ic_build_query_p(_ich, _msg, _cb, _mod, _if, _rpc, \
                                          v));                              \
    })

/** \brief helper to send a query to a given ic, computes callback name.
 *
 * \param[in]  _ic    the #ichannel_t to send the query to.
 * \param[in]  _msg   the #ic_msg_t to fill.
 * \param[in]  _mod   name of the package+module of the RPC
 * \param[in]  _if    name of the interface of the RPC
 * \param[in]  _rpc   name of the rpc
 * \param[in]  ...
 *   the initializers of the value on the form <tt>.field = value</tt>
 */
#define ic_query2(_ic, _msg, _mod, _if, _rpc, ...) \
    ({  ichannel_t *_ich = (_ic);                                            \
        __ic_query(_ich, ic_build_query(_ich, _msg,                          \
            IOP_RPC_CB_REF(_mod, _if, _rpc), _mod, _if, _rpc, __VA_ARGS__)); \
    })

/** \brief helper to send a query to a given ic, computes callback name.
 *
 * \param[in]  _ic    the #ichannel_t to send the query to.
 * \param[in]  _msg   the #ic_msg_t to fill.
 * \param[in]  _cb    the rpc reply callback to use
 * \param[in]  _mod   name of the package+module of the RPC
 * \param[in]  _if    name of the interface of the RPC
 * \param[in]  _rpc   name of the rpc
 * \param[in]  v      a <tt>${_mod}__${_if}__${_rpc}_args__t *</tt> value.
 */
#define ic_query2_p(_ic, _msg, _mod, _if, _rpc, v) \
    ({  ichannel_t *_ich = (_ic);                                  \
        __ic_query(_ich, ic_build_query_p(_ich, _msg,              \
            IOP_RPC_CB_REF(_mod, _if, _rpc), _mod, _if, _rpc, v)); \
    })

/** \brief helper to send a query to a given ic.
 *
 * Same as #ic_query but waits for the query to be sent before the call
 * returns. DO NOT USE unless you have a really good reason.
 *
 * \param[in]  _ic    the #ichannel_t to send the query to.
 * \param[in]  _msg   the #ic_msg_t to fill.
 * \param[in]  _cb    the rpc reply callback to use
 * \param[in]  _mod   name of the package+module of the RPC
 * \param[in]  _if    name of the interface of the RPC
 * \param[in]  _rpc   name of the rpc
 * \param[in]  ...
 *   the initializers of the value on the form <tt>.field = value</tt>
 */
#define ic_query_sync(_ic, _msg, _cb, _mod, _if, _rpc, ...) \
    ({  ichannel_t *_ich = (_ic);                                         \
        __ic_query_sync(_ich, ic_build_query(_ich, _msg, _cb, _mod, _if,  \
                                              _rpc, __VA_ARGS__));        \
    })

/** \brief helper to send a query to a given ic.
 *
 * Same as #ic_query_p but waits for the query to be sent before the call
 * returns. DO NOT USE unless you have a really good reason.
 *
 * \param[in]  _ic    the #ichannel_t to send the query to.
 * \param[in]  _msg   the #ic_msg_t to fill.
 * \param[in]  _cb    the rpc reply callback to use
 * \param[in]  _mod   name of the package+module of the RPC
 * \param[in]  _if    name of the interface of the RPC
 * \param[in]  _rpc   name of the rpc
 * \param[in]  v      a <tt>${_mod}__${_if}__${_rpc}_args__t *</tt> value.
 */
#define ic_query_sync_p(_ic, _msg, _cb, _mod, _if, _rpc, v) \
    ({  ichannel_t *_ich = (_ic);                                          \
        __ic_query_sync(_ich, ic_build_query_p(_ich, _msg, _cb, _mod, _if, \
                                               _rpc, v));                  \
    })

/** \brief helper to proxy a query to a given ic with header.
 *
 * It setups the message automatically so that when the reply is received it's
 * proxied back to the caller without any "human" intervention.
 *
 * \param[in]  ic     the #ichannel_t to proxy the query to.
 * \param[in]  slot   the slot of the received query.
 * \param[in]  _mod   name of the package+module of the RPC
 * \param[in]  _if    name of the interface of the RPC
 * \param[in]  _rpc   name of the rpc
 * \param[in]  hdr    the #ic__hdr__t to use.
 * \param[in]  v      a <tt>${_mod}__${_if}__${_rpc}_args__t *</tt> value.
 */
#define ic_query_proxy_hdr(ic, slot, _mod, _if, _rpc, hdr, v) \
    ic_query_p(ic, ic_msg_proxy_new(-1, slot, hdr),                         \
               (void *)IC_PROXY_MAGIC_CB, _mod, _if, _rpc, v);

/** \brief helper to proxy a query to a given ic.
 *
 * It setups the message automatically so that when the reply is received it's
 * proxied back to the caller without any "human" intervention.
 *
 * \param[in]  ic     the #ichannel_t to proxy the query to.
 * \param[in]  slot   the slot of the received query.
 * \param[in]  _mod   name of the package+module of the RPC
 * \param[in]  _if    name of the interface of the RPC
 * \param[in]  _rpc   name of the rpc
 * \param[in]  v      a <tt>${_mod}__${_if}__${_rpc}_args__t *</tt> value.
 */
#define ic_query_proxy(ic, slot, _mod, _if, _rpc, v) \
    ic_query_proxy_hdr(ic, slot, _mod, _if, _rpc, NULL, v)

/** \brief helper to proxy a query to a given ic with an fd.
 *
 * It setups the message automatically so that when the reply is received it's
 * proxied back to the caller without any "human" intervention.
 *
 * \param[in]  ic     the #ichannel_t to proxy the query to.
 * \param[in]  fd     the fd to send.
 * \param[in]  slot   the slot of the received query.
 * \param[in]  _mod   name of the package+module of the RPC
 * \param[in]  _if    name of the interface of the RPC
 * \param[in]  _rpc   name of the rpc
 * \param[in]  v      a <tt>${_mod}__${_if}__${_rpc}_args__t *</tt> value.
 */
#define ic_query_proxy_fd(ic, fd, slot, _mod, _if, _rpc, v) \
    ic_query_p(ic, ic_msg_proxy_new(fd, slot, hdr),         \
               (void *)IC_PROXY_MAGIC_CB, _mod, _if, _rpc, v);

/** \brief helper to reply to a given query (server-side).
 *
 * \param[in]  ic
 *   the #ichannel_t to send the reply to, must be #NULL if the reply isn't
 *   done in the implementation callback synchronously.
 * \param[in]  slot   the slot of the query we're answering to.
 * \param[in]  _cb    the rpc reply callback to use
 * \param[in]  _mod   name of the package+module of the RPC
 * \param[in]  _if    name of the interface of the RPC
 * \param[in]  _rpc   name of the rpc
 * \param[in]  v      a <tt>${_mod}__${_if}__${_rpc}_res__t *</tt> value.
 */
#define ic_reply_p(ic, slot, _mod, _if, _rpc, v) \
    ({  const IOP_RPC_T(_mod, _if, _rpc, res) *__v = (v);                   \
        STATIC_ASSERT(_mod##__##_if(_rpc##__rpc__async) == 0);              \
        __ic_reply(ic, slot, IC_MSG_OK, -1,                                 \
                   IOP_RPC(_mod, _if, _rpc)->result, __v); })
/** \brief helper to reply to a given query (server-side).
 *
 * \param[in]  ic
 *   the #ichannel_t to send the reply to, must be #NULL if the reply isn't
 *   done in the implementation callback synchronously.
 * \param[in]  slot   the slot of the query we're answering to.
 * \param[in]  _cb    the rpc reply callback to use
 * \param[in]  _mod   name of the package+module of the RPC
 * \param[in]  _if    name of the interface of the RPC
 * \param[in]  _rpc   name of the rpc
 * \param[in]  ...
 *   the initializers of the value on the form <tt>.field = value</tt>
 */
#define ic_reply(ic, slot, _mod, _if, _rpc, ...) \
    ic_reply_p(ic, slot, _mod, _if, _rpc,                                   \
               (&(IOP_RPC_T(_mod, _if, _rpc, res)){ __VA_ARGS__ }))

/** \brief helper to reply to a given query (server-side), with fd.
 *
 * \param[in]  ic
 *   the #ichannel_t to send the reply to, must be #NULL if the reply isn't
 *   done in the implementation callback synchronously.
 * \param[in]  slot   the slot of the query we're answering to.
 * \param[in]  fd     the file descriptor to use in the reply.
 * \param[in]  _cb    the rpc reply callback to use
 * \param[in]  _mod   name of the package+module of the RPC
 * \param[in]  _if    name of the interface of the RPC
 * \param[in]  _rpc   name of the rpc
 * \param[in]  v      a <tt>${_mod}__${_if}__${_rpc}_res__t *</tt> value.
 */
#define ic_reply_fd_p(ic, slot, fd, _mod, _if, _rpc, v) \
    ({  const IOP_RPC_T(_mod, _if, _rpc, res) *__v = (v);                   \
        STATIC_ASSERT(_mod##__##_if(_rpc##__rpc__async) == 0);              \
        __ic_reply(ic, slot, IC_MSG_OK, fd,                                 \
                   IOP_RPC(_mod, _if, _rpc)->result, __v); })
/** \brief helper to reply to a given query (server-side), with fd.
 *
 * \param[in]  ic
 *   the #ichannel_t to send the reply to, must be #NULL if the reply isn't
 *   done in the implementation callback synchronously.
 * \param[in]  slot   the slot of the query we're answering to.
 * \param[in]  fd     the file descriptor to use in the reply.
 * \param[in]  _cb    the rpc reply callback to use
 * \param[in]  _mod   name of the package+module of the RPC
 * \param[in]  _if    name of the interface of the RPC
 * \param[in]  _rpc   name of the rpc
 * \param[in]  ...
 *   the initializers of the value on the form <tt>.field = value</tt>
 */
#define ic_reply_fd(ic, slot, fd, _mod, _if, _rpc, ...) \
    ic_reply_fd_p(ic, slot, fd, _mod, _if, _rpc,                            \
                  (&(IOP_RPC_T(_mod, _if, _rpc, res)){ __VA_ARGS__ }))

/** \brief helper to reply to a given query (server-side) with an exception.
 *
 * \param[in]  ic
 *   the #ichannel_t to send the reply to, must be #NULL if the reply isn't
 *   done in the implementation callback synchronously.
 * \param[in]  slot   the slot of the query we're answering to.
 * \param[in]  _cb    the rpc reply callback to use
 * \param[in]  _mod   name of the package+module of the RPC
 * \param[in]  _if    name of the interface of the RPC
 * \param[in]  _rpc   name of the rpc
 * \param[in]  v      a <tt>${_mod}__${_if}__${_rpc}_exn__t *</tt> value.
 */
#define ic_throw_p(ic, slot, _mod, _if, _rpc, v) \
    ({  const IOP_RPC_T(_mod, _if, _rpc, exn) *__v = (v);                   \
        STATIC_ASSERT(_mod##__##_if(_rpc##__rpc__async) == 0);              \
        __ic_reply(ic, slot, IC_MSG_EXN, -1,                                \
                   IOP_RPC(_mod, _if, _rpc)->exn, __v); })

/** \brief helper to reply to a given query (server-side) with an exception.
 *
 * \param[in]  ic
 *   the #ichannel_t to send the reply to, must be #NULL if the reply isn't
 *   done in the implementation callback synchronously.
 * \param[in]  slot   the slot of the query we're answering to.
 * \param[in]  _cb    the rpc reply callback to use
 * \param[in]  _mod   name of the package+module of the RPC
 * \param[in]  _if    name of the interface of the RPC
 * \param[in]  _rpc   name of the rpc
 * \param[in]  ...
 *   the initializers of the value on the form <tt>.field = value</tt>
 */
#define ic_throw(ic, slot, _mod, _if, _rpc, ...) \
    ic_throw_p(ic, slot, _mod, _if, _rpc,                                   \
               (&(IOP_RPC_T(_mod, _if, _rpc, exn)){ __VA_ARGS__ }))

/** \brief helper to reply to a query (server-side) with a forced exception.
 *   NB: This macro is means to be used only inside a pre_hook or
 *   implementation with a hook_ctx define.
 *
 * \param[in]  ic
 *   the #ichannel_t to send the reply to, must be #NULL if the reply isn't
 *   done in the pre_hook/implementation callback synchronously.
 * \param[in]  slot   the slot of the query we're answering to.
 * \param[in]  ctx    the context of the query we're answering to.
 * \param[in]  _exn   the type of the exception to throw
 * \param[in]  v      a <tt>_exn *</tt> value.
 *   the initializers of the value on the form <tt>.field = value</tt>
 */
#define ic_throw_exn_p(ic, slot, ctx, _exn, v)                              \
    ({  const _exn##__t *__v = (v);                                         \
        assert(ctx && ctx->rpc && ctx->rpc->exn == &_exn##__s);             \
        __ic_reply(ic, slot, IC_MSG_EXN, -1,                                \
                   ctx->rpc->exn, __v); })

/** \brief helper to reply to a query (server-side) with a forced exception.
 *   NB: This macro is means to be used only inside a pre_hook or
 *   implementation with a hook_ctx define.
 *
 * \param[in]  ic
 *   the #ichannel_t to send the reply to, must be #NULL if the reply isn't
 *   done in the pre_hook/implementation callback synchronously.
 * \param[in]  slot   the slot of the query we're answering to.
 * \param[in]  _exn   the type of the exception to throw
 * \param[in]  ...
 *   the initializers of the value on the form <tt>.field = value</tt>
 */
#define ic_throw_exn(ic, slot, ctx, _exn, ...)                               \
    ic_throw_exn_p(ic, slot, ctx, _exn, (&(_exn##__t){ __VA_ARGS__ }))

/** \brief Bounce an IOP answer to reply to another slot.
 *
 * This function may be used to forward an answer to another slot when
 * implementing a manual proxy. It saves the reply data packing.
 *
 * WARNINGS:
 *  - this function MUST be used before leaving the RPC callback scope.
 *  - you cannot use this function to forward a webservices answer; this is
 *    not implemented.
 *  - be really careful because this function suppose that the answer has
 *    been left unmodified since its reception. If you want to modify it
 *    before the forwarding, then *don't* use this function and use instead
 *    ic_reply_p/ic_throw_p. If you try to do this, in the best scenario
 *    your changes will be ignored, in the worst you will have a crash…
 *
 * Here an example of how to use this function:
 * <code>
 *  RPC_IMPL(pkg, foo, bar)
 *  {
 *      CHECK_ID_OK(arg->id);
 *
 *      // manual proxy
 *      ic_msg_t *imsg = ic_msg_new(sizeof(uint64_t));
 *      *(uint64_t *)imsg->priv = slot;
 *      ic_query_p(remote_ic, imsg, pkg, foo, bar, arg);
 *  }
 *
 *  RPC_CB(pkg, foo, bar)
 *  {
 *      uint64_t origin_slot = *(uint64_t *)msg->priv;
 *
 *      // automatic and efficient answer forwarding
 *      __ic_forward_reply_to(ic, origin_slot, status, res, exn);
 *  }
 * </code>
 *
 * \param[in]  ic     the ichannel_t the "thing" we proxy comes from.
 * \param[in]  slot   the slot of the query we're answering to.
 * \param[in]  cmd    the received answer status parameter.
 * \param[in]  res    the received answer result parameter.
 * \param[in]  exn    the received answer exception parameter.
 */
void __ic_forward_reply_to(ichannel_t *ic, uint64_t slot,
                           int cmd, const void *res, const void *exn);


/** \brief Manually reply to a message with an error code.
 *
 * This function may be used in case you are manually dealing with ic_msg_t in
 * order to implement some routing layer.
 *
 * \param[in]  ic     the ichannel_t the "thing" the message come from (NULL
 *                    if unknown)
 * \param[in]  msg    the message that get replied.
 * \param[in]  status the reply status.
 */
void __ic_msg_reply_err(ichannel_t *ic, ic_msg_t *msg, ic_status_t status);

/* Compatibility aliases */
#define ic_reply_throw_p(...)  ic_throw_p(__VA_ARGS__)
#define ic_reply_throw(...)    ic_throw(__VA_ARGS__)

#endif
