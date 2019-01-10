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

#ifndef IS_LIB_COMMON_NET_H
#define IS_LIB_COMMON_NET_H

#include "core.h"

#include <arpa/inet.h>
#include <netinet/in.h>
#ifdef HAVE_NETINET_SCTP_H
# include <netinet/sctp.h>
# ifdef SCTP_ADAPTION_LAYER
    /* see http://www1.ietf.org/mail-archive/web/tsvwg/current/msg05971.html */
#   define SCTP_ADAPTATION_LAYER         SCTP_ADAPTION_LAYER
#   define sctp_adaptation_layer_event   sctp_adaption_layer_event
# endif
#endif
#include <sys/socket.h>
#include <sys/un.h>

#include "net-addr.h"
#include "net-socket.h"
#include "net-sctp.h"
#include "net-rate.h"

#endif
