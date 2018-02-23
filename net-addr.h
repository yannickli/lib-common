/**************************************************************************/
/*                                                                        */
/*  Copyright (C) 2004-2018 INTERSEC SA                                   */
/*                                                                        */
/*  Should you receive a copy of this source code, you must check you     */
/*  have a proper, written authorization of INTERSEC to hold it. If you   */
/*  don't have such an authorization, you must DELETE all source code     */
/*  files in your possession, and inform INTERSEC of the fact you obtain  */
/*  these files. Should you not comply to these terms, you can be         */
/*  prosecuted in the extent permitted by applicable law.                 */
/*                                                                        */
/**************************************************************************/

#if !defined(IS_LIB_COMMON_NET_H) || defined(IS_LIB_COMMON_NET_ADDR_H)
#  error "you must include net.h instead"
#else
#define IS_LIB_COMMON_NET_ADDR_H

typedef struct addr_filter_t {
    union {
      struct {
        in_addr_t addr; /* 0 = 'any' */
        in_addr_t mask;
      } v4;
      struct {
        struct in6_addr addr;
        struct in6_addr mask;
      } v6;
    } u;
    uint16_t  port; /* 0 = 'any' */
    sa_family_t family;
} addr_filter_t;

typedef union sockunion_t {
    struct sockaddr_storage ss;
    struct sockaddr_in      sin;
    struct sockaddr_in6     sin6;
    struct sockaddr_un      sunix;
    struct sockaddr         sa;
#ifdef OS_APPLE
    struct {
        uint8_t len;
        sa_family_t family;
    };
#else
    sa_family_t             family;
#endif
} sockunion_t;

bool sockunion_equal(const sockunion_t *, const sockunion_t *);

static inline int sockunion_getport(const sockunion_t *su)
{
    switch (su->family) {
      case AF_INET:  return ntohs(su->sin.sin_port);
      case AF_INET6: return ntohs(su->sin6.sin6_port);
      default:       return 0;
    }
}
static inline void sockunion_setport(sockunion_t *su, int port)
{
    switch (su->family) {
      case AF_INET:  su->sin.sin_port   = ntohs(port); break;
      case AF_INET6: su->sin6.sin6_port = ntohs(port); break;
      default:       e_panic("should not happen");
    }
}

/** Convert IPv4 and IPv6 addresses into a string.
 *
 * This function is a helper that call inet_ntop() with appropriate arguments
 * according to "su->family" value.
 *
 * On error, "errno" is set appropriately. See inet_ntop(3) for more details.
 *
 * \return length of string written in "buf"
 * \retval -1 on error
 */
int sockunion_gethost(const sockunion_t *su, char *buf, int size);

/** Convert IPv4 and IPv6 addresses into a string.
 *
 * This helper is a wrapper around sockunion_gethost() that allocates memory
 * into the t_stack.
 *
 * \see sockunion_gethost()
 *
 * \return network address as a lstr_t
 * \return LSTR_NULL_V on error
 */
lstr_t t_sockunion_gethost_lstr(const sockunion_t *su);

static inline socklen_t sockunion_len(const sockunion_t *su)
{
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
__attribute__((pure))
uint32_t sockunion_hash(const sockunion_t *su);

/* This helper allows to iterate on an array of sockunion_t where each 'su'
 * can have different lengths (e.g. by mixing IPv4 and IPv6).
 */
#define sockunion_for_each(su, sus, len)                                     \
    for (typeof(sus) su = (sus), __su_start = su, __su_broken = su + (len);  \
         __su_broken-- != __su_start;                                        \
         su = (typeof(sus))((byte *)su + sockunion_len(su)))

/* -1 as defport means port is mandatory */
int addr_parse(pstream_t ps, pstream_t *host, in_port_t *port, int defport);
int addr_info(sockunion_t *, sa_family_t, pstream_t host, in_port_t);

/** Convert a TCP/IPv4, TCP/IPv6 or UNIX address into a string.
 *
 * Examples:
 *
 * IPv4:
 *    x.x.x.x:port
 *
 * IPv6:
 *    [x:x:x:x:x:x:x:x]:port
 *
 * UNIX:
 *    @/var/run/zpf-master.ctl
 *
 * \param[in]  su    sockunion filled with a network address
 * \param[out] slen  length of the formatted string
 *
 * \return string allocated in t_stack
 */
const char *t_addr_fmt(const sockunion_t *su, int *slen);
static inline lstr_t t_addr_fmt_lstr(const sockunion_t *su)
{
    int len;
    const char *s = t_addr_fmt(su, &len);
    return lstr_init_(s, len, MEM_STACK);
}

static inline int
addr_parse_str(const char *s, pstream_t *host, in_port_t *port, int defport)
{
    return addr_parse(ps_initstr(s), host, port, defport);
}

static inline int
addr_info_str(sockunion_t *su, const char *host, int port, int af)
{
    return addr_info(su, af, ps_initstr(host), port);
}

/** Build addr filter from a subnetwork.
 *
 * A subnetwork is identified by its CIDR notation, e.g. 192.168.0.1/24 or
 * fe80::202:b3ff:fe1e:8329/32, or by a single IP address, 192.168.0.12 or
 * ff09::1234:abcd.
 *
 * \param[in]  subnet the subnetwork.
 * \param[out] filter resulting filter.
 * \return -1 in case of error, 0 otherwise.
 */
int addr_filter_build(lstr_t subnet, addr_filter_t *filter);

int addr_filter_matches(const addr_filter_t *filter, const sockunion_t *peer);

static inline int
addr_resolve(const char *what, const lstr_t s, sockunion_t *out)
{
    pstream_t host;
    in_port_t port;

    if (addr_parse(ps_init(s.s, s.len), &host, &port, -1) < 0) {
        return e_error("unable to parse %s address: %*pM", what,
                       LSTR_FMT_ARG(s));
    }
    if (addr_info(out, AF_UNSPEC, host, port) < 0) {
        return e_error("unable to resolve %s address: %*pM", what,
                       LSTR_FMT_ARG(s));
    }
    return 0;
}

#endif
