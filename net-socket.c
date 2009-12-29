/**************************************************************************/
/*                                                                        */
/*  Copyright (C) 2004-2009 INTERSEC SAS                                  */
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

static int sock_reuseaddr(int sock)
{
    int v = 1;
    /* XXX the char * cast is needed for mingcc */
    return setsockopt(sock, SOL_SOCKET, SO_REUSEADDR, (char *)&v, sizeof(v));
}

int socketpairx(int d, int type, int protocol, int flags, int sv[2])
{
    RETHROW(socketpair(d, type, protocol, sv));
    if (!(flags & O_NONBLOCK))
        return 0;
    if (fd_set_features(sv[0], flags) || fd_set_features(sv[1], flags)) {
        PROTECT_ERRNO(({ close(sv[0]); close(sv[1]); }));
        sv[0] = sv[1] = -1;
        return -1;
    }
    return 0;
}

int bindx(int sock, const sockunion_t *addrs, int cnt,
          int type, int proto, int flags)
{
    int to_close = -1;

#ifdef HAVE_NETINET_SCTP_H
    if (proto != IPPROTO_SCTP && cnt != 1) {
        errno = EINVAL;
        return -1;
    }
#endif

    if (sock < 0) {
        to_close = sock = RETHROW(socket(addrs->family, type, proto));
    }

    if (sock_reuseaddr(sock))
        goto error;

    if (fd_set_features(sock, flags))
        goto error;

#ifdef HAVE_NETINET_SCTP_H
    if (proto != IPPROTO_SCTP || cnt == 1) {
#endif
        if (addrs->family == AF_UNIX) {
            unlink(addrs->sunix.sun_path);
        }

        if (bind(sock, &addrs->sa, sockunion_len(addrs)) < 0)
            goto error;
#ifdef HAVE_NETINET_SCTP_H
    } else {
        socklen_t sz = 0;
        while (cnt-- > 0) {
            sockunion_t *su = (sockunion_t *)((char *)addrs + sz);
            socklen_t len = sockunion_len(su);
            if (len == (socklen_t)-1) {
                errno = EINVAL;
                goto error;
            }
            sz += len;
        }
        if (setsockopt(sock, SOL_SCTP, SCTP_SOCKOPT_BINDX_ADD, addrs, sz))
            goto error;
    }
#endif

    return sock;

  error:
    close(to_close);
    return -1;
}

int listenx(int sock, const sockunion_t *addrs, int cnt,
            int type, int proto, int flags)
{
    int to_close = -1;

    if (sock < 0) {
        to_close = sock = bindx(-1, addrs, cnt, type, proto, flags);
    }

    if (listen(sock, SOMAXCONN) < 0)
        goto error;
    return sock;

  error:
    if (to_close >= 0)
        PROTECT_ERRNO(close(to_close));
    return -1;
}

int connectx(int sock, const sockunion_t *addrs, int cnt, int type, int proto,
             int flags)
{
    int to_close = -1;

#ifdef HAVE_NETINET_SCTP_H
    if (proto != IPPROTO_SCTP && cnt != 1) {
        errno = EINVAL;
        return -1;
    }
#endif

    if (sock < 0) {
        to_close = sock = RETHROW(socket(addrs->family, type, proto));
    }

    if (fd_set_features(sock, flags))
        goto error;

#ifdef HAVE_NETINET_SCTP_H
    if (proto != IPPROTO_SCTP || cnt == 1) {
#endif
        if (connect(sock, &addrs->sa, sockunion_len(addrs)) < 0
        &&  !ERR_CONNECT_RETRIABLE(errno))
        {
            goto error;
        }
#ifdef HAVE_NETINET_SCTP_H
    } else {
        socklen_t sz = 0;
        while (cnt-- > 0) {
            sockunion_t *su = (sockunion_t *)((char *)addrs + sz);
            socklen_t len = sockunion_len(su);
            if (len == (socklen_t)-1) {
                errno = EINVAL;
                goto error;
            }
            sz += len;
        }
        if (setsockopt(sock, SOL_SCTP, SCTP_SOCKOPT_CONNECTX, addrs, sz))
            goto error;
    }
#endif

    return sock;

  error:
    PROTECT_ERRNO(close(to_close));
    return -1;
}

int acceptx(int server_fd, int flags)
{
    int sock = RETHROW(accept(server_fd, NULL, NULL));

    if (fd_set_features(sock, flags)) {
        PROTECT_ERRNO(close(sock));
        return -1;
    }
    return sock;
}

int getsockport(int sock, sa_family_t family)
{
    sockunion_t local = { .family = family };
    socklen_t   size  = sockunion_len(&local);

    if (getsockname(sock, &local.sa, &size)) {
        e_trace(1, "getsockname failed: %m");
        return 0;
    }
    return sockunion_getport(&local);
}

int getpeerport(int sock, sa_family_t family)
{
    sockunion_t local = { .family = family };
    socklen_t   size  = sockunion_len(&local);

    if (getpeername(sock, &local.sa, &size)) {
        e_trace(1, "getsockname failed: %m");
        return 0;
    }
    return sockunion_getport(&local);
}

int socket_connect_status(int sock)
{
    int err;
    socklen_t size = sizeof(err);

    RETHROW(getsockopt(sock, SOL_SOCKET, SO_ERROR, (void *)&err, &size));
    if (!err)
        return 1;
    errno = err;
    return ERR_CONNECT_RETRIABLE(err) ? 0 : -1;
}
