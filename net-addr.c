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
#include "net.h"

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
    struct addrinfo *ai = NULL;
    struct addrinfo hint = { .ai_family = af };
    int res;

    t_push();
    res = getaddrinfo(t_dupz(host.p, ps_len(&host)), NULL, &hint, &ai);
    t_pop();

    if (res < 0)
        return -1;

    if (ai->ai_addrlen > ssizeof(*su)) {
        freeaddrinfo(ai);
        return -1;
    }
    memcpy(su, ai->ai_addr, ai->ai_addrlen);
    freeaddrinfo(ai);
    sockunion_setport(su, port);
    return 0;
}
