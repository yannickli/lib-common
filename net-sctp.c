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

#include "net.h"
#include "unix.h"
#include "container.h"

int sctp_enable_events(int sd, int flags)
{
    struct sctp_event_subscribe ev;

    p_clear(&ev, 1);

    if (flags & SCTP_DATA_IO_EV)
        ev.sctp_data_io_event = 1;

    if (flags & SCTP_ASSOCIATION_EV)
        ev.sctp_association_event = 1;

    if (flags & SCTP_ADDRESS_EV)
        ev.sctp_address_event = 1;

    if (flags & SCTP_SEND_FAILURE_EV)
        ev.sctp_send_failure_event = 1;

    if (flags & SCTP_PEER_ERROR_EV)
        ev.sctp_peer_error_event = 1;

    if (flags & SCTP_SHUTDOWN_EV)
        ev.sctp_shutdown_event = 1;

    if (flags & SCTP_PARTIAL_DELIVERY_EV)
        ev.sctp_partial_delivery_event = 1;

    if (flags & SCTP_ADAPTATION_LAYER_EV)
        ev.sctp_adaptation_layer_event = 1;

    return setsockopt(sd, IPPROTO_SCTP, SCTP_EVENTS, &ev, sizeof(ev));
}

ssize_t sctp_sendv(int sd, const struct iovec *iov, int iovlen,
                   const struct sctp_sndrcvinfo *sinfo, int flags)
{
    char buf[CMSG_SPACE(sizeof(*sinfo))];
    struct msghdr msg = {
        .msg_iov         = (struct iovec *)iov,
        .msg_iovlen      = iovlen,
        .msg_control     = buf,
        .msg_controllen  = sizeof(buf),
        .msg_flags       = 0,
    };

    struct cmsghdr *cmsg = CMSG_FIRSTHDR(&msg);

    cmsg->cmsg_level = IPPROTO_SCTP;
    cmsg->cmsg_type  = SCTP_SNDRCV;
    cmsg->cmsg_len   = CMSG_LEN(sizeof(*sinfo));

    msg.msg_controllen = cmsg->cmsg_len;
    memcpy(CMSG_DATA(cmsg), sinfo, sizeof(*sinfo));
    return sendmsg(sd, &msg, flags);
}

int sctp_send(int s, const void *msg, size_t len,
              const struct sctp_sndrcvinfo *sinfo, int flags)
{
    return sctp_sendv(s, &MAKE_IOVEC(msg, len), 1, sinfo, flags);
}

/* Implementation of sctp_sendmsg(3), we don't want to -lsctp */
int sctp_sendmsg(int sd, const void *_msg, size_t len,
                 struct sockaddr *to, socklen_t tolen,
                 uint32_t ppid, uint32_t flags, uint16_t stream_no,
                 uint32_t ttl, uint32_t context)
{
    struct sctp_sndrcvinfo *sinfo;
    char buf[CMSG_SPACE(sizeof(*sinfo))];
    struct iovec iov = {
        .iov_base = (void *)_msg,
        .iov_len  = len,
    };
    struct msghdr msg = {
        .msg_name        = to,
        .msg_namelen     = tolen,
        .msg_iov         = &iov,
        .msg_iovlen      = 1,
        .msg_control     = buf,
        .msg_controllen  = sizeof(buf),
        .msg_flags       = 0,
    };

    struct cmsghdr *cmsg = CMSG_FIRSTHDR(&msg);

    cmsg->cmsg_level = IPPROTO_SCTP;
    cmsg->cmsg_type  = SCTP_SNDRCV;
    cmsg->cmsg_len   = CMSG_LEN(sizeof(*sinfo));

    msg.msg_controllen = cmsg->cmsg_len;
    sinfo = (void *)CMSG_DATA(cmsg);
    memset(sinfo, 0, sizeof(*sinfo));
    sinfo->sinfo_ppid       = ppid;
    sinfo->sinfo_flags      = flags;
    sinfo->sinfo_stream     = stream_no;
    sinfo->sinfo_timetolive = ttl;
    sinfo->sinfo_context    = context;

    return sendmsg(sd, &msg, 0);
}

/* Implementation of sctp_recmsg(3), we don't want to -lsctp */
int sctp_recvmsg(int sd, void *_msg, size_t len,
                 struct sockaddr *from, socklen_t *fromlen,
                 struct sctp_sndrcvinfo *sinfo, int *msg_flags)
{
    struct iovec iov = {
        .iov_base = _msg,
        .iov_len  = len,
    };
    char buf[CMSG_SPACE(sizeof(*sinfo))];
    struct msghdr msg = {
        .msg_name       = from,
        .msg_namelen    = fromlen ? *fromlen : 0,
        .msg_iov        = &iov,
        .msg_iovlen     = 1,
        .msg_control    = buf,
        .msg_controllen = sizeof(buf),
    };

    int res = recvmsg(sd, &msg, 0);

    RETHROW(res);
    if (fromlen)
        *fromlen = msg.msg_namelen;
    if (sinfo) {
        struct cmsghdr *cmsg = CMSG_FIRSTHDR(&msg);
        while (cmsg) {
            if ((cmsg->cmsg_level == IPPROTO_SCTP)
            &&  (cmsg->cmsg_type == SCTP_SNDRCV))
            {
                memcpy(sinfo, CMSG_DATA(cmsg), sizeof(*sinfo));
                break;
            }
            cmsg = CMSG_NXTHDR(&msg, cmsg);
        }
    }
    if (msg_flags)
        *msg_flags = msg.msg_flags;

    return res;
}

int sctp_peeloff(int fd, sctp_assoc_t associd)
{
    sctp_peeloff_arg_t peeloff;
    socklen_t peeloff_size = sizeof(sctp_peeloff_arg_t);
    int err;

    peeloff.associd = associd;
    peeloff.sd = 0;

    if ((err = getsockopt(fd,
                          SOL_SCTP, SCTP_SOCKOPT_PEELOFF,
                          &peeloff, &peeloff_size)) < 0)
    {
        return err;
    }

    return peeloff.sd;
}

int sctp_addr_len(const sockunion_t *addrs, int count)
{
    const byte *buf = (const byte *)addrs;

    for (int i = 0; i < count; i++) {
        int len = sockunion_len((const sockunion_t *)buf);
        if (unlikely(len < 0)) {
            errno = EINVAL;
            return -1;
        }
        buf += len;
    }

    return buf - (const byte *)addrs;
}

int sctp_connectx_old(int fd, struct sockaddr *addrs, int count)
{
    int size = sctp_addr_len((const sockunion_t *)addrs, count);

    RETHROW(size);
    return setsockopt(fd, SOL_SCTP, SCTP_SOCKOPT_CONNECTX, addrs, size);
}

int sctp_getaddrs(int fd, int optnum, sctp_assoc_t id,
                  struct sockaddr *addrs, int addr_size)
{
    struct sctp_getaddrs *ga = (void *)addrs;
    int res;
    socklen_t len = addr_size;

    ga->assoc_id = id;
    RETHROW(getsockopt(fd, SOL_SCTP, optnum, ga, &len));
    res = ga->addr_num;
    memmove(ga, ga + 1, len);
    return res;
}

int sctp_close_assoc(int fd, int assoc_id)
{
    int res;

    for (;;) {
        struct sctp_sndrcvinfo sinfo = {
            .sinfo_assoc_id   = assoc_id,
            .sinfo_flags      = SCTP_EOF | SCTP_ABORT,
        };
        res = sctp_sendv(fd, NULL, 0, &sinfo, 0);
        if (res < 0 && ERR_RW_RETRIABLE(errno))
            continue;
        return res;
    }
}

