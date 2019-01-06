/**************************************************************************/
/*                                                                        */
/*  Copyright (C) 2004-2019 INTERSEC SA                                   */
/*                                                                        */
/*  Should you receive a copy of this source code, you must check you     */
/*  have a proper, written authorization of INTERSEC to hold it. If you   */
/*  don't have such an authorization, you must DELETE all source code     */
/*  files in your possession, and inform INTERSEC of the fact you obtain  */
/*  these files. Should you not comply to these terms, you can be         */
/*  prosecuted in the extent permitted by applicable law.                 */
/*                                                                        */
/**************************************************************************/

#include "z.h"
#include "iop-rpc.h"
#include "core.iop.h"

typedef struct ctx_t {
    uint32_t u;
} ctx_t;

static struct {
    ichannel_t         *ic_aux;
    int                 mode;
    ic_status_t         status;
    core__log_level__t  level;
    ctx_t               ctx;
} z_iop_rpc_g;
#define _G  z_iop_rpc_g

static void g_result_init(void)
{
    _G.status = -1;
    _G.level = INT32_MIN;
    _G.ctx.u = 0;
}

static void IOP_RPC_IMPL(core__core, log, set_root_level)
{
    if (arg->level < LOG_LEVEL_min || LOG_LEVEL_max < arg->level) {
        ic_throw(ic, slot, core__core, log, set_root_level);
        return;
    }
    ic_reply(NULL, slot, core__core, log, set_root_level,
             .level = arg->level);
    arg->level++;
}

#define RPC_CB()                                 \
    do {                                         \
        _G.status = status;                      \
        _G.level = res ? res->level : INT32_MIN; \
        _G.ctx = *acast(ctx_t, msg->priv);       \
    } while (0)

static void IOP_RPC_CB(core__core, log, set_root_level)
{
    if (_G.mode) {
        __ic_forward_reply_to(ic, get_unaligned_cpu64(msg->priv),
                              status, res, exn);
    } else {
        RPC_CB();
    }
}

static void IOP_RPC_IMPL(core__core, log, set_logger_level)
{
    IOP_RPC_T(core__core, log, set_root_level, args) v = {
        .level = arg->level,
    };

    if (_G.mode) {
        ic_query2_p(_G.ic_aux, ic_msg(uint64_t, slot),
                    core__core, log, set_root_level, &v);
    } else {
        ic_query_proxy(_G.ic_aux, slot, core__core, log, set_root_level, &v);
    }
}

static void IOP_RPC_CB(core__core, log, set_logger_level)
{
    RPC_CB();
}

#define RPC_CALL(_ic, _rpc, _force_pack, _force_dup, _level, _u)             \
    do {                                                                     \
        ic_msg_t *ic_msg = ic_msg(ctx_t, .u = (_u));                         \
        int level = (_level);                                                \
        IOP_RPC_T(core__core, log, _rpc, args) arg = { .level = level, };    \
                                                                             \
        g_result_init();                                                     \
        ic_msg->force_pack = (_force_pack);                                  \
        ic_msg->force_dup = (_force_dup);                                    \
        ic_query2_p((_ic), ic_msg, core__core, log, _rpc, &arg);             \
        if (_force_pack || _force_dup) {                                     \
            Z_ASSERT_EQ(arg.level, level, "arg not preserved");              \
        }                                                                    \
    } while (0)

#define TEST_RPC_CALL(_ic, _rpc, _force_pack, _force_dup, _suffix)           \
    do {                                                                     \
        ichannel_t *__ic = (_ic);                                            \
        bool __force_pack = (_force_pack);                                   \
        bool __force_dup = (_force_dup);                                     \
                                                                             \
        /* call with no error */                                             \
        RPC_CALL(__ic, _rpc, __force_pack, __force_dup, 1, 0xabcdef);        \
        Z_ASSERT_EQ(_G.status, (ic_status_t)IC_MSG_OK,                       \
                    "rpc returned bad status"_suffix);                       \
        Z_ASSERT_EQ(_G.level, 1, "rpc returned bad result"_suffix);          \
        Z_ASSERT_EQ(_G.ctx.u, 0xabcdefU, "rpc returned bad msg priv"_suffix);\
                                                                             \
        /* call with throw */                                                \
        RPC_CALL(__ic, _rpc, __force_pack, __force_dup,                      \
                 LOG_LEVEL_min - 1, 0);                                      \
        Z_ASSERT_EQ(_G.status, (ic_status_t)IC_MSG_EXN,                      \
                    "rpc returned bad status"_suffix);                       \
    } while (0)


Z_GROUP_EXPORT(iop_rpc)
{
    Z_TEST(ic_local, "iop-rpc: ic local") {
        ichannel_t ic;
        qm_t(ic_cbs) impl = QM_INIT(ic_cbs, impl);
        qm_t(ic_cbs) impl_aux = QM_INIT(ic_cbs, impl_aux);

        MODULE_REQUIRE(ic);

        ic_init(&ic);
        ic_set_local(&ic);

        _G.ic_aux = ic_new();
        ic_set_local(_G.ic_aux);

        for (int force_pack = 0; force_pack <= 1; force_pack++) {
            for (int force_dup = 0; force_dup <= !force_pack; force_dup++) {

                ic.impl = NULL;
                _G.ic_aux->impl = NULL;

                /* check behavior when ic->impl is null */
                RPC_CALL(&ic, set_root_level, force_pack, force_dup, 0, 0);
                Z_ASSERT_EQ(_G.status, (ic_status_t)IC_MSG_UNIMPLEMENTED,
                            "rpc returned bad status");

                ic_register(&impl, core__core, log, set_root_level);
                ic_register(&impl, core__core, log, set_logger_level);

                ic.impl = &impl;
                _G.ic_aux->impl = &impl_aux;

                TEST_RPC_CALL(&ic, set_root_level, force_pack, force_dup, "");

                ic_register_proxy(&impl_aux, core__core, log, set_root_level,
                                  &ic);
                TEST_RPC_CALL(_G.ic_aux, set_root_level, force_pack,
                              force_dup, " for register proxy");
                ic_unregister(&impl_aux, core__core, log, set_root_level);

                /* ic:set_logger_level --query_proxy--> ic_aux:set_root_level
                 */
                ic_register(&impl_aux, core__core, log, set_root_level);
                TEST_RPC_CALL(&ic, set_logger_level, force_pack, force_dup,
                              " for query_proxy");

                /* ic:set_logger_level --query--> ic_aux:set_root_level */
                _G.mode = 1;
                TEST_RPC_CALL(&ic, set_logger_level, force_pack, force_dup,
                              " for forward reply");
                _G.mode = 0;

                ic_unregister(&impl, core__core, log, set_root_level);
                ic_unregister(&impl, core__core, log, set_logger_level);
                ic_unregister(&impl_aux, core__core, log, set_root_level);
            }
        }

        qm_wipe(ic_cbs, &impl);
        qm_wipe(ic_cbs, &impl_aux);
        ic_disconnect(&ic);
        ic_disconnect(_G.ic_aux);
        ic_wipe(&ic);
        ic_delete(&_G.ic_aux);

        MODULE_RELEASE(ic);
    } Z_TEST_END;
} Z_GROUP_END;
