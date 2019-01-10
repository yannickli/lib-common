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

#include <netdb.h>
#include "z.h"
#include "net.h"
#include "container.h"

bool sockunion_equal(const sockunion_t *a1, const sockunion_t *a2)
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

uint32_t sockunion_hash(const sockunion_t *su)
{
    uint64_t u64;
    uint32_t u32;

    switch (su->family) {
      case AF_INET:
        u64 = su->sin.sin_family | (su->sin.sin_port << 16)
            | ((uint64_t)su->sin.sin_addr.s_addr << 32);
        return qhash_hash_u64(NULL, u64);

      case AF_INET6:
        u32 = su->sin6.sin6_family | (su->sin6.sin6_port << 16);
        return u32 ^ mem_hash32(su->sin6.sin6_addr.s6_addr,
                                sizeof(su->sin6.sin6_addr.s6_addr));

#ifndef OS_WINDOWS
      case AF_UNIX:
        return mem_hash32(&su->sunix, sockunion_len(su));
#endif

      default:
        e_panic("unknown kind of sockaddr: %d", su->family);
    }
}

int sockunion_gethost(const sockunion_t *su, char *buf, int size)
{
    switch (su->family) {
      case AF_INET:
        RETHROW_PN(inet_ntop(AF_INET, &su->sin.sin_addr.s_addr, buf, size));
        break;

      case AF_INET6:
        RETHROW_PN(inet_ntop(AF_INET6, &su->sin6.sin6_addr.s6_addr, buf,
                             size));
        break;

      default:
        return -1;
    }
    return strlen(buf);
}

lstr_t t_sockunion_gethost_lstr(const sockunion_t *su)
{
    int size = MAX(INET_ADDRSTRLEN, 2 + INET6_ADDRSTRLEN);
    char *buf;
    int len;

    buf = t_new(char, size);
    len = sockunion_gethost(su, buf, size);
    if (len < 0) {
        return LSTR_NULL_V;
    }

    return lstr_init_(buf, len, MEM_STACK);
}

int addr_parse(pstream_t ps, pstream_t *host, in_port_t *port, int defport)
{
    int i;

    PS_WANT(ps_has(&ps, 1));
    if (ps.b[0] == '[') {
        __ps_skip(&ps, 1);
        PS_CHECK(ps_get_ps_chr(&ps, ']', host));
        __ps_skip(&ps, 1);
    } else {
        if (ps_get_ps_chr(&ps, ':', host) < 0) {
            *host = ps;
            ps.b  = ps.b_end;
            goto no_port;
        }
    }
    if (!ps_has(&ps, 1)) {
      no_port:
        *port = defport;
        return defport < 0 ? -1 : 0;
    }
    PS_WANT(__ps_getc(&ps) == ':');
    PS_WANT(ps_has(&ps, 1));
    i = ps_geti(&ps);
    if (i < 1 || i > UINT16_MAX)
        return -1;
    *port = i;
    return ps_done(&ps) ? 0 : -1;
}

char *t_addr_fmt(const sockunion_t *su, int *slen)
{
    int   len = MAX(INET_ADDRSTRLEN, 2 + INET6_ADDRSTRLEN) + 1 + 5;
    char *buf = t_new(char, len);
    int   pos;

    if (su->family == AF_INET) {
        inet_ntop(AF_INET, &su->sin.sin_addr.s_addr, buf, len);
        pos = strlen(buf);
    } else {
        buf[0] = '[';
        inet_ntop(AF_INET6, &su->sin6.sin6_addr.s6_addr, buf + 1, len - 1);
        pos = strlen(buf);
        buf[pos++] = ']';
    }
    assert (pos < len);
    pos += snprintf(buf + pos, len - pos, ":%d", sockunion_getport(su));
    assert (pos <= len);
    if (slen)
        *slen = pos;
    return buf;
}

int addr_info(sockunion_t *su, sa_family_t af, pstream_t host, in_port_t port)
{
    t_scope;
    struct addrinfo *ai = NULL;
    struct addrinfo *cur;
    struct addrinfo hint = { .ai_family = af };

    RETHROW(getaddrinfo(t_dupz(host.p, ps_len(&host)), NULL, &hint, &ai));
    for (cur = ai; cur != NULL; cur = cur->ai_next) {
        switch (cur->ai_family) {
          case AF_INET:
          case AF_INET6:
          case AF_UNIX:
            if (cur->ai_addrlen > ssizeof(*su)) {
                continue;
            }
            break;

          default:

            continue;
        }
        break;
    }

    if (cur) {
        memcpy(su, cur->ai_addr, cur->ai_addrlen);
        sockunion_setport(su, port);
    }
    freeaddrinfo(ai);
    return cur ? 0 : -1;
}

int addr_filter_matches(const addr_filter_t *filter, const sockunion_t *peer)
{
    if (peer->family != filter->family)
        return -1;

    if (filter->port && filter->port != sockunion_getport(peer))
        return -1;

    if (filter->family == AF_INET) {
        if (filter->u.v4.addr
        && ((filter->u.v4.addr & filter->u.v4.mask)
            != (peer->sin.sin_addr.s_addr & filter->u.v4.mask)))
        {
            return -1;
        }
    } else {
#ifndef s6_addr32
#define s6_addr32  __u6_addr.__u6_addr32
#endif
        /* filter->family == AF_INET6 */
        for (int i = 3; i >= 0; i--) {
            if ((filter->u.v6.addr.s6_addr32[i]
                 & filter->u.v6.mask.s6_addr32[i])
            != (peer->sin6.sin6_addr.s6_addr32[i]
                  & filter->u.v6.mask.s6_addr32[i])) {
                return -1;
            }
        }
    }

    return 0;
}

Z_GROUP_EXPORT(net_addr)
{
    t_scope;
#define NET_ADDR_IPV4  "1.1.1.1"
#define NET_ADDR_IPV6  "1:1:1:1:1:1:1:1"
#define NET_ADDR_PORT  4242

    lstr_t ipv4 = LSTR_IMMED(NET_ADDR_IPV4);
    lstr_t ipv6 = LSTR_IMMED(NET_ADDR_IPV6);
    lstr_t tcp_ipv4 = LSTR_IMMED(NET_ADDR_IPV4 ":" TOSTR(NET_ADDR_PORT));
    lstr_t tcp_ipv6 = LSTR_IMMED("[" NET_ADDR_IPV6 "]:" TOSTR(NET_ADDR_PORT));
    sockunion_t su;

    Z_TEST(ipv4, "IPv4") {
        Z_ASSERT_N(addr_info(&su, AF_INET, ps_initlstr(&ipv4),
                             NET_ADDR_PORT));
        Z_ASSERT_LSTREQUAL(ipv4, t_sockunion_gethost_lstr(&su));
        Z_ASSERT_EQ(NET_ADDR_PORT, sockunion_getport(&su));
        Z_ASSERT_LSTREQUAL(t_addr_fmt_lstr(&su), tcp_ipv4);
    } Z_TEST_END;

    Z_TEST(ipv6, "IPv6") {
        Z_ASSERT_N(addr_info(&su, AF_INET6, ps_initlstr(&ipv6),
                             NET_ADDR_PORT));
        Z_ASSERT_LSTREQUAL(ipv6, t_sockunion_gethost_lstr(&su));
        Z_ASSERT_EQ(NET_ADDR_PORT, sockunion_getport(&su));
        Z_ASSERT_LSTREQUAL(t_addr_fmt_lstr(&su), tcp_ipv6);
    } Z_TEST_END;

#undef NET_ADDR_PORT
#undef NET_ADDR_IPV6
#undef NET_ADDR_IPV4
} Z_GROUP_END;
