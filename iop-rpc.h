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

#ifndef IS_LIB_COMMON_IOP_RPC_H
#define IS_LIB_COMMON_IOP_RPC_H

#include "el.h"
#include "net.h"
#include "unix.h"
#include "iop.h"
#include "http.h"
#include "ic.iop.h"

#if __has_feature(nullability)
#pragma GCC diagnostic push
#pragma GCC diagnostic error "-Wnullability-completeness"
#if __has_warning("-Wnullability-completeness-on-arrays")
#pragma GCC diagnostic ignored "-Wnullability-completeness-on-arrays"
#endif
#endif

typedef enum ic_status_t {
    IC_MSG_OK             = 0,
    IC_MSG_EXN            = 1,
    IC_MSG_RETRY          = 2,
    IC_MSG_ABORT          = 3,
    IC_MSG_INVALID        = 4,
    IC_MSG_UNIMPLEMENTED  = 5,
    IC_MSG_SERVER_ERROR   = 6,
    IC_MSG_PROXY_ERROR    = 7,
    IC_MSG_TIMEDOUT       = 8,
    IC_MSG_CANCELED       = 9,

    /* XXX: think to update ic_status_to_string too */
#define IC_MSG_STREAM_CONTROL   INT32_MIN
} ic_status_t;

#define IC_SLOT_ERROR           (~UINT64_C(0))
#define IC_SLOT_FOREIGN_MASK    BITMASK_GE(uint64_t, 62)
#define IC_SLOT_FOREIGN_IC      (UINT64_C(0) << 62)
#define IC_SLOT_FOREIGN_HTTP    (UINT64_C(1) << 62)

static ALWAYS_INLINE bool ic_slot_is_http(uint64_t slot) {
    return (slot & IC_SLOT_FOREIGN_MASK) == IC_SLOT_FOREIGN_HTTP;
}

static inline const char * nonnull ic_status_to_string(ic_status_t s)
{
#define CASE(st)  case IC_MSG_##st: return #st;
    switch (s) {
        CASE(OK);
        CASE(EXN);
        CASE(RETRY);
        CASE(ABORT);
        CASE(INVALID);
        CASE(UNIMPLEMENTED);
        CASE(SERVER_ERROR);
        CASE(PROXY_ERROR);
        CASE(TIMEDOUT);
        CASE(CANCELED);
      default:
        return "UNKNOWN";
    }
#undef CASE
}

#include "iop-rpc-channel.h"
#include "iop-rpc-http.h"


/** \brief get the description of the currently unpacked RPC.
 *
 * This low-level function allows to retrieve the description of the currently
 * unpacked RPC. You are allowed to call it only inside of the callback of
 * a RPC implementation.
 */
static inline const iop_rpc_t * nonnull
__ic_get_current_rpc_desc(const ichannel_t * nullable ic, uint64_t slot)
{
#ifndef __cplusplus
    if (ic_slot_is_http(slot)) {
        ichttp_query_t *iq = ichttp_slot_to_query(slot);

        return iq->cbe->fun;
    }
#endif
    return ic->desc;
}

/** \brief get the “command” of the currently unpacked RPC.
 *
 * This low-level function allows to retrieve the “command” value of the
 * currently unpacked RPC. You are allowed to call it only inside of the
 * callback of a RPC implementation.
 */
static inline int
__ic_get_current_rpc_cmd(const ichannel_t * nullable ic, uint64_t slot)
{
#ifndef __cplusplus
    if (ic_slot_is_http(slot)) {
        ichttp_query_t *iq = ichttp_slot_to_query(slot);

        return iq->cbe->cmd;
    }
#endif
    return ic->cmd;
}

#if __has_feature(nullability)
#pragma GCC diagnostic pop
#endif

#endif
