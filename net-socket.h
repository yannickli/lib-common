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

#if !defined(IS_LIB_COMMON_NET_H) || defined(IS_LIB_COMMON_NET_SOCKET_H)
#  error "you must include <lib-common/net.h> instead"
#else
#define IS_LIB_COMMON_NET_SOCKET_H

typedef union sockunion_t {
    struct sockaddr_storage ss;
    struct sockaddr_in      sin;
    struct sockaddr_in6     sin6;
#ifndef OS_WINDOWS
    struct sockaddr_un      sunix;
#endif
    struct sockaddr         sa;
    sa_family_t             family;
} sockunion_t;

bool sockaddr_equal(const sockunion_t *, const sockunion_t *);

static inline be32_t sockaddr_getip4(const struct sockaddr_in *addr) {
    return force_cast(be32_t, addr->sin_addr.s_addr);
}
static inline int sockunion_getport(const sockunion_t *su) {
    switch (su->family) {
      case AF_INET:  return ntohs(su->sin.sin_port);
      case AF_INET6: return ntohs(su->sin6.sin6_port);
      default:       return 0;
    }
}
static inline void sockunion_setport(sockunion_t *su, int port) {
    switch (su->family) {
      case AF_INET:  su->sin.sin_port   = ntohs(port); break;
      case AF_INET6: su->sin6.sin6_port = ntohs(port); break;
      default:       e_panic("should not happen");
    }
}
static inline socklen_t sockunion_len(const sockunion_t *su) {
    switch (su->family) {
      case AF_INET:
        return sizeof(struct sockaddr_in);
      case AF_INET6:
        return sizeof(struct sockaddr_in6);
      case AF_UNIX:
        /* XXX: the +1 isn't a bug, it's really what we mean
         *      it's to support linux abstract sockets
         */
        return offsetof(struct sockaddr_un, sun_path)
            + 1 + strlen(su->sunix.sun_path + 1);
      default:
        return (socklen_t)-1;
    }
}

/*----- sys/socket.h enshancements -----*/
#ifndef OS_WINDOWS
int socketpairx(int d, int type, int protocol, int flags, int sv[2]);
#endif

int bindx(int sock, const sockunion_t *, int cnt,
          int type, int proto, int flags);
int listenx(int sock, const sockunion_t *, int cnt,
            int type, int proto, int flags);
int connectx(int sock, const sockunion_t *, int cnt,
             int type, int proto, int flags);
int acceptx(int server_fd, int flags);

int getsockport(int sock, sa_family_t family);
int getpeerport(int sock, sa_family_t family);

#endif
