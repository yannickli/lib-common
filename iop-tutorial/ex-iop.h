/**************************************************************************/
/*                                                                        */
/*  Copyright (C) 2004-2010 INTERSEC SAS                                  */
/*                                                                        */
/*  Should you receive a copy of this source code, you must check you     */
/*  have a proper, written authorization of INTERSEC to hold it. If you   */
/*  don't have such an authorization, you must DELETE all source code     */
/*  files in your possession, and inform INTERSEC of the fact you obtain  */
/*  these files. Should you not comply to these terms, you can be         */
/*  prosecuted in the extent permitted by applicable law.                 */
/*                                                                        */
/**************************************************************************/

#ifndef IS_HELLO_HELLO_IOP_H
#define IS_HELLO_HELLO_IOP_H

#include <lib-common/iop-rpc.h>
#include "exiop.iop.h"

/* ---- Global to the product ---- */

/* Define several macros to ease IOP implementations and callbacks declaration */
#define HELLO_RPC_NAME(_i, _r, sfx)  _i##__##_r##__##sfx
#define HELLO_RPC_IMPL(_p, _m, _i, _r) \
    HELLO_RPC_NAME(_i, _r, impl)(IOP_RPC_IMPL_ARGS(_p##__##_m, _i, _r))

#define HELLO_RPC_CB(_p, _m, _i, _r) \
    HELLO_RPC_NAME(_i, _r, cb)(IOP_RPC_CB_ARGS(_p##__##_m, _i, _r))
#define HELLO_REGISTER(h, _p, _m, _i, _r) \
    ic_register(h, _p##__##_m, _i, _r, HELLO_RPC_NAME(_i, _r, impl))


/* ---- Module HelloMod helpers ---- */

/* Standard implementations and callbacks helpers */
#define HELLO_MOD_RPC_IMPL(_i, _r)     HELLO_RPC_IMPL(exiop, hello_mod, _i, _r)
#define HELLO_MOD_RPC_CB(_i, _r)       HELLO_RPC_CB(exiop, hello_mod, _i, _r)
#define HELLO_MOD_RPC_CB_ARGS(_i, _r)  IOP_RPC_CB_ARGS(exiop__hello_mod, _i, _r)
#define HELLO_MOD_REGISTER(h, _i, _r)  HELLO_REGISTER(h, exiop, hello_mod, _i, _r)

/* Send queries */
#define ic_hello_mod_query2(ic, msg, cb, _i, _r, ...) \
    ic_query(ic, msg, cb, exiop__hello_mod, _i, _r, ##__VA_ARGS__)
#define ic_hello_mod_query(ic, msg, _i, _r, ...) \
    ({ STATIC_ASSERT(exiop__hello_mod##__##_i(_r##__rpc__async) == 0); \
       ic_hello_mod_query2(ic, msg, &HELLO_RPC_NAME(_i, _r, cb), _i, \
                           _r, ##__VA_ARGS__); })
#define ic_hello_mod_async(ic, _i, _r, ...) \
    ({ STATIC_ASSERT(exiop__hello_mod##__##_i(_r##__rpc__async) == 1); \
       ic_hello_mod_query2(ic, ic_msg_new(0), NULL, _i, _r, ##__VA_ARGS__); })

#define ic_hello_mod_query2_p(ic, msg, cb, _i, _r, v) \
    ic_query_p(ic, msg, cb, exiop__hello_mod, _i, _r, v)
#define ic_hello_mod_query_p(ic, msg, _i, _r, v) \
    ({ STATIC_ASSERT(exiop__hello_mod##__##_i(_r##__rpc__async) == 0); \
       ic_hello_mod_query2_p(ic, msg, &HELLO_RPC_NAME(_i, _r, cb), _i, \
                             _r, v); })
#define ic_hello_mod_async_p(ic, _i, _r, v) \
    ({ STATIC_ASSERT(exiop__hello_mod##__##_i(_r##__rpc__async) == 1); \
       ic_hello_mod_query2_p(ic, ic_msg_new(0), NULL, _i, _r, v); })

/* Reply and throw exception at queries */
#define ic_hello_mod_reply(ic, slot, _if, _rpc, ...) \
    ic_reply(ic, slot, exiop__hello_mod, _if, _rpc, ##__VA_ARGS__)
#define ic_hello_mod_reply_fd(ic, slot, fd, _if, _rpc, ...) \
    ic_reply_fd(ic, slot, fd, exiop__hello_mod, _if, _rpc, ##__VA_ARGS__)
#define ic_hello_mod_reply_throw(ic, slot, _if, _rpc, ...) \
    ic_reply_throw(ic, slot, exiop__hello_mod, _if, _rpc, ##__VA_ARGS__)

#define ic_hello_mod_reply_p(ic, slot, _if, _rpc, v) \
    ic_reply_p(ic, slot, exiop__hello_mod, _if, _rpc, v)
#define ic_hello_mod_reply_fd_p(ic, slot, fd, _if, _rpc, v) \
    ic_reply_fd_p(ic, slot, fd, exiop__hello_mod, _if, _rpc, v)
#define ic_hello_mod_reply_throw_p(ic, slot, _if, _rpc, v) \
    ic_reply_throw_p(ic, slot, exiop__hello_mod, _if, _rpc, v)


/* Proxyfication */
#define HELLO_MOD_REGISTER_PROXY_HDR(h, _i, _r, ic, hdr) \
    ic_register_proxy_hdr(h, exiop__hello_mod, _i, _r, ic, hdr)
#define HELLO_MOD_REGISTER_PROXY(h, _i, _r, ic) \
    ic_register_proxy(h, exiop__hello_mod, _i, _r, ic)

#define HELLO_MOD_REGISTER_PROXY_HDR_P(h, _i, _r, ic, hdr) \
    ic_register_proxy_hdr_p(h, exiop__hello_mod, _i, _r, ic, hdr)
#define HELLO_MOD_REGISTER_PROXY_P(h, _i, _r, ic) \
    ic_register_proxy_p(h, exiop__hello_mod, _i, _r, ic)

#define HELLO_MOD_REGISTER_DYNPROXY(h, _i, _r, cb, priv) \
    ic_register_dynproxy(h, exiop__hello_mod, _i, _r, cb, priv)


#define ic_hello_mod_proxy(ic, slot, _i, _r, v) \
    ic_query_proxy(ic, slot, exiop__hello_mod, _i, _r, v)
#define ic_hello_mod_proxy_hdr(ic, slot, _i, _r, hdr, v) \
    ic_query_proxy_hdr(ic, slot, exiop__hello_mod, _i, _r, hdr, v)


#endif /* IS_HELLO_HELLO_IOP_H */
