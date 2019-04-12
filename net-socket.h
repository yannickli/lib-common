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
#  error "you must include net.h instead"
#else
#define IS_LIB_COMMON_NET_SOCKET_H

/* Create a pair of connected sockets with some socket options.
 *
 * The socket pair is obtain with `socketpair(d, type, protocol)`. See
 * socketpair(2) for more information about socketpair. The flags are ignored
 * if O_NONBLOCK is missing.
 */
int socketpairx(int d, int type, int protocol, int flags, int sv[2]);

int bindx(int sock, const sockunion_t * nonnull, int cnt,
          int type, int proto, int flags);
int listenx(int sock, const sockunion_t * nonnull, int cnt,
            int type, int proto, int flags);
int isconnectx(int sock, const sockunion_t * nonnull, int cnt,
               int type, int proto, int flags);
#define connectx(sock, su, cnt, type, proto, flags)  \
    isconnectx((sock), (su), (cnt), (type), (proto), (flags))

/** Connect using a specified src
 *
 * \param[in]   sock    a file descriptor for the socket, a negative value to
 *                      create a new socket
 * \param[in]   addrs   addresses to connect to
 * \param[in]   cnt     number of addresses to connect to (=1 except for SCTP)
 * \param[in]   src     use specific network interface as source
 * \param[in]   type    see socket(2)
 * \param[in]   proto   see socket(2)
 * \param[in]   flags   see fd_features_flags_t
 * \param[in]   timeout  the maximum time, in seconds, spent to establish the
 *                       connection; only for blocking connects; bound to the
 *                       socket and kept after connection such that further
 *                       blocking read attempts would also have this timeout;
 *                       ignored if equals to 0.
 *
 * \Returns On success, the file descriptor for the socket
 *          On error, -1 and errno is set appropriately.
 *
 */
int connectx_as(int sock, const sockunion_t * nonnull addrs, int cnt,
                const sockunion_t * nullable src, int type, int proto,
                int flags, int timeout);
int acceptx(int server_fd, int flags);
int acceptx_get_addr(int server_fd, int flags, sockunion_t * nullable su);

int getsockport(int sock, sa_family_t family);
int getpeerport(int sock, sa_family_t family);

/* returns -1 if broken, 0 if in progress, 1 if connected */
int socket_connect_status(int sock);

#endif
