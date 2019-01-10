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

#if !defined(IS_LIB_COMMON_NET_H) || defined(IS_LIB_COMMON_NET_SCTP_H)
#  error "you must include <lib-common/net.h> instead"
#else
#define IS_LIB_COMMON_NET_SCTP_H

enum sctp_events {
    SCTP_DATA_IO_EV           = 0x01,
    SCTP_ASSOCIATION_EV       = 0x02,
    SCTP_ADDRESS_EV           = 0x04,
    SCTP_SEND_FAILURE_EV      = 0x08,
    SCTP_PEER_ERROR_EV        = 0x10,
    SCTP_SHUTDOWN_EV          = 0x20,
    SCTP_PARTIAL_DELIVERY_EV  = 0x40,
    SCTP_ADAPTATION_LAYER_EV  = 0x80,
};

#ifdef SCTP_SOCKOPT_CONNECTX_OLD
#  undef SCTP_SOCKOPT_CONNECTX
#  define SCTP_SOCKOPT_CONNECTX SCTP_SOCKOPT_CONNECTX_OLD
#endif
#define sctp_connectx  sctp_connectx_old
int sctp_connectx_old(int fd, struct sockaddr *addrs, int count);

int sctp_enable_events(int fd, int flags);

ssize_t sctp_sendv(int sd, const struct iovec *iov, int iovlen,
                   const struct sctp_sndrcvinfo *sinfo, int flags);
int sctp_addr_len(const sockunion_t *addrs, int count);
int sctp_close_assoc(int fd, int assoc_id);
int sctp_getaddrs(int fd, int optnum, sctp_assoc_t id,
                  struct sockaddr *addrs, int addr_size);

#endif
