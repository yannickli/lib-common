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

#ifndef IS_LIB_COMMON_LOG_IOP_H
#define IS_LIB_COMMON_LOG_IOP_H

#include "log.h"
#include "core.iop.h"
#include "iop-rpc.h"

/* {{{ Configuration */

/** Set the configuration of the logging system.
 */
void logger_configure(const core__log_configuration__t * nonnull conf);

/** IOP configuration interface.
 */

void IOP_RPC_IMPL(core__core, log, set_root_level);
void IOP_RPC_IMPL(core__core, log, reset_root_level);
void IOP_RPC_IMPL(core__core, log, set_logger_level);
void IOP_RPC_IMPL(core__core, log, reset_logger_level);

/* }}} */
/* {{{ Accessors */

qvector_t(logger_conf, core__logger_configuration__t);

/** Returns information about a set of loggers, which can be filtered using
 *  a prefixed name.
 *
 * \param[in] prefix A string that the loggers' full names must start with.
 *                   LSTR_NULL_V can be used to avoid filtering.
 * \param[out] confs The vector holding the information about each logger.
 */
void logger_get_all_configurations(lstr_t prefix,
                                   qv_t(logger_conf) * nonnull confs);

void IOP_RPC_IMPL(core__core, log, list_loggers);

/* }}} */

#endif
