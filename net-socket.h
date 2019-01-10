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

#ifndef OS_WINDOWS
int socketpairx(int d, int type, int protocol, int flags, int sv[2]);
#endif

int bindx(int sock, const sockunion_t *, int cnt,
          int type, int proto, int flags);
int listenx(int sock, const sockunion_t *, int cnt,
            int type, int proto, int flags);
int connectx(int sock, const sockunion_t *, int cnt,
             int type, int proto, int flags);
/** Connect using a specified src
 *
 * \param[in]   sock    a file descriptor for the socket, a negative value to
 *                      create a new socket
 * \param[in]   src     use specific network interface as source
 *
 * \Returns On success, the file descriptor for the socket
 *          On error, -1 and errno is set appropriately.
 *
 */
int connectx_as(int sock, const sockunion_t *, int cnt,
                const sockunion_t * nullable src, int type,
                int proto, int flags);
int acceptx(int server_fd, int flags);
int acceptx_get_addr(int server_fd, int flags, sockunion_t *su);

int getsockport(int sock, sa_family_t family);
int getpeerport(int sock, sa_family_t family);

/* returns -1 if broken, 0 if in progress, 1 if connected */
int socket_connect_status(int sock);

#endif
