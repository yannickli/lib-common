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

#include "log.h"

void logger_configure(const core__log_configuration__t *conf)
{
    logger_set_level(LSTR_EMPTY_V, conf->root_level,
                     conf->force_all ? LOG_RECURSIVE : 0);

    tab_for_each_ptr(l, &conf->specific) {
        logger_set_level(l->full_name, l->level,
                         l->force_all ? LOG_RECURSIVE : 0);
    }
}

void IOP_RPC_IMPL(core__core, log, set_root_level)
{
    ic_reply(ic, slot, core__core, log, set_root_level,
             .level = logger_set_level(LSTR_EMPTY_V, arg->level,
                                       arg->force_all ? LOG_RECURSIVE : 0));
}

void IOP_RPC_IMPL(core__core, log, reset_root_level)
{
    ic_reply(ic, slot, core__core, log, reset_root_level,
             .level = logger_reset_level(LSTR_EMPTY_V));
}

void IOP_RPC_IMPL(core__core, log, set_logger_level)
{
    ic_reply(ic, slot, core__core, log, set_logger_level,
             .level = logger_set_level(arg->full_name, arg->level,
                                       arg->force_all ? LOG_RECURSIVE : 0));
}

void IOP_RPC_IMPL(core__core, log, reset_logger_level)
{
    ic_reply(ic, slot, core__core, log, reset_logger_level,
             .level = logger_reset_level(arg->full_name));
}
