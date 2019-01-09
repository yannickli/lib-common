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

bool sockaddr_equal(const sockunion_t *a1, const sockunion_t *a2)
{
    if (a1->family != a2->family)
        return false;

    switch (a1->family) {
      case AF_INET:
        return a1->sin.sin_port == a2->sin.sin_port
            && a1->sin.sin_addr.s_addr == a2->sin.sin_addr.s_addr;

      case AF_INET6:
        if (a1->sin6.sin6_port != a2->sin6.sin6_port)
            return false;
        return !memcmp(a1->sin6.sin6_addr.s6_addr, a2->sin6.sin6_addr.s6_addr,
                       sizeof(a2->sin6.sin6_addr.s6_addr));

#ifndef OS_WINDOWS
      case AF_UNIX:
        return !strcmp(a1->sunix.sun_path, a2->sunix.sun_path);
#endif

      default:
        e_panic("unknown kind of sockaddr: %d", a1->family);
    }
}

/*----- sys/socket.h enshancements -----*/
static int sock_reuseaddr(int sock)
{
    int v = 1;
    /* XXX the char * cast is needed for mingcc */
    if (setsockopt(sock, SOL_SOCKET, SO_REUSEADDR, (char *)&v, sizeof(v)))
        return e_error("setsockopt SO_REUSEADDR failed.");
    return 0;
}

int socketpairx(int d, int type, int protocol, int flags, int sv[2])
{
    RETHROW(socketpair(d, type, protocol, sv));
    if (!(flags & O_NONBLOCK))
        return 0;
    if (fd_set_features(sv[0], flags) || fd_set_features(sv[1], flags)) {
        int save_err = errno;
        close(sv[0]);
        close(sv[1]);
        sv[0] = sv[1] = -1;
        errno = save_err;
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
        to_close = sock = socket(addrs->family, type, proto);
        if (sock < 0)
            return e_error("socket failed %m");
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

        if (bind(sock, &addrs->sa, sockunion_len(addrs)) < 0) {
            switch (addrs->family) {
              case AF_INET:
                e_error("bind failed (port %d).",
                        ntohs(addrs->sin.sin_port));
                break;

              case AF_INET6:
                e_error("bind failed (port %d).",
                        ntohs(addrs->sin6.sin6_port));
                break;

              default:
                e_error("bind failed.");
                break;
            }
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
                e_error("bind failed.");
                goto error;
            }
            sz += len;
        }
        if (setsockopt(sock, SOL_SCTP, SCTP_SOCKOPT_BINDX_ADD, addrs, sz)) {
            e_error("sctp_bindx failed.");
            goto error;
        }
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

    if (listen(sock, SOMAXCONN) < 0) {
        e_error("listen failed.");
        goto error;
    }

    return sock;

  error:
    if (to_close >= 0)
        close(to_close);
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
        to_close = sock = socket(addrs->family, type, proto);
        if (sock < 0)
            return e_error("socket failed: %m");
    }

    if (fd_set_features(sock, flags))
        goto error;

#ifdef HAVE_NETINET_SCTP_H
    if (proto != IPPROTO_SCTP || cnt == 1) {
#endif
        if (connect(sock, &addrs->sa, sockunion_len(addrs)) < 0
#ifdef OS_WINDOWS
        &&  WSAGetLastError() != WSAEWOULDBLOCK
#else
        &&  errno != EINPROGRESS
#endif
        ) {
            e_error("connect failed (%m).");
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
                e_error("bind failed.");
                goto error;
            }
            sz += len;
        }
        if (setsockopt(sock, SOL_SCTP, SCTP_SOCKOPT_CONNECTX, addrs, sz)) {
            e_error("sctp_connectx failed.");
            goto error;
        }
    }
#endif

    return sock;

  error:
    close(to_close);
    return -1;
}

int acceptx(int server_fd, int flags)
{
    int sock = accept(server_fd, NULL, NULL);

    if (sock < 0) {
        if (errno != EAGAIN)
            return e_error("accept failed");
        return -1;
    }

    if (fd_set_features(sock, flags)) {
        close(sock);
        return e_error("fd_set_features failed.");
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
