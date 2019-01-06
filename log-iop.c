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

#include "log.h"

void logger_configure(const core__log_configuration__t *conf)
{
    logger_set_level(LSTR_EMPTY_V, conf->root_level,
                     LOG_MK_FLAGS(conf->force_all, conf->is_silent));

    tab_for_each_ptr(l, &conf->specific) {
        logger_set_level(l->full_name, l->level,
                         LOG_MK_FLAGS(l->force_all, l->is_silent));
    }
}

void IOP_RPC_IMPL(core__core, log, set_root_level)
{
    unsigned flags = LOG_MK_FLAGS(arg->force_all, arg->is_silent);

    ic_reply(ic, slot, core__core, log, set_root_level,
             .level = logger_set_level(LSTR_EMPTY_V, arg->level, flags));
}

void IOP_RPC_IMPL(core__core, log, reset_root_level)
{
    ic_reply(ic, slot, core__core, log, reset_root_level,
             .level = logger_reset_level(LSTR_EMPTY_V));
}

void IOP_RPC_IMPL(core__core, log, set_logger_level)
{
    unsigned flags = LOG_MK_FLAGS(arg->force_all, arg->is_silent);

    ic_reply(ic, slot, core__core, log, set_logger_level,
             .level = logger_set_level(arg->full_name, arg->level, flags));
}

void IOP_RPC_IMPL(core__core, log, reset_logger_level)
{
    ic_reply(ic, slot, core__core, log, reset_logger_level,
             .level = logger_reset_level(arg->full_name));
}

void IOP_RPC_IMPL(core__core, log, list_loggers)
{
    t_scope;
    qv_t(logger_conf) confs;

    t_qv_init(&confs, 1024);

    logger_get_all_configurations(arg->prefix, &confs);

    ic_reply(ic, slot, core__core, log, list_loggers,
             .loggers = IOP_ARRAY_TAB(&confs));
}
