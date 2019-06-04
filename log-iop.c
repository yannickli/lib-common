/***************************************************************************/
/*                                                                         */
/* Copyright 2019 INTERSEC SA                                              */
/*                                                                         */
/* Licensed under the Apache License, Version 2.0 (the "License");         */
/* you may not use this file except in compliance with the License.        */
/* You may obtain a copy of the License at                                 */
/*                                                                         */
/*     http://www.apache.org/licenses/LICENSE-2.0                          */
/*                                                                         */
/* Unless required by applicable law or agreed to in writing, software     */
/* distributed under the License is distributed on an "AS IS" BASIS,       */
/* WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.*/
/* See the License for the specific language governing permissions and     */
/* limitations under the License.                                          */
/*                                                                         */
/***************************************************************************/

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
