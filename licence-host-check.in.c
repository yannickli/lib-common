/**************************************************************************/
/*                                                                        */
/*  Copyright (C) 2004-2014 INTERSEC SA                                   */
/*                                                                        */
/*  Should you receive a copy of this source code, you must check you     */
/*  have a proper, written authorization of INTERSEC to hold it. If you   */
/*  don't have such an authorization, you must DELETE all source code     */
/*  files in your possession, and inform INTERSEC of the fact you obtain  */
/*  these files. Should you not comply to these terms, you can be         */
/*  prosecuted in the extent permitted by applicable law.                 */
/*                                                                        */
/**************************************************************************/

#include "z.h"
#ifndef OS_WINDOWS
#include <net/if.h>
#ifdef OS_LINUX
#include <net/if_arp.h>
#endif
#ifdef OS_APPLE
#include <net/if.h>
#include <net/if_dl.h>
#include <net/if_types.h>
#endif
#include <netinet/in.h>
#include <sys/ioctl.h>

#include "conf.h"
#include "licence.h"
#include "datetime.h"
#include "hash.h"
#include "property.h"
#include "core.iop.h"

#if defined(__CYGWIN__)
struct if_nameindex {
    int if_index;
    char if_name[32];
};
#define ARPHRD_ETHER  0
ATTRS
static struct if_nameindex *if_nameindex(void) {
    struct if_nameindex *p = calloc(sizeof(struct if_nameindex), 2);
    strcpy(p->if_name, "eth0");
    return p;
}

ATTRS
static void if_freenameindex(struct if_nameindex *p) {
    free(p);
}
#endif

ATTRS
static int F(get_mac_addr)(int s, const struct if_nameindex *iface,
                        byte mac[static 6])
{
#if defined(OS_LINUX)
    struct ifreq if_hwaddr;

    pstrcpy(if_hwaddr.ifr_ifrn.ifrn_name, IFNAMSIZ, iface->if_name);
    RETHROW(ioctl(s, SIOCGIFHWADDR, &if_hwaddr));
    THROW_ERR_IF(if_hwaddr.ifr_hwaddr.sa_family != ARPHRD_ETHER);

    p_copy(mac, if_hwaddr.ifr_hwaddr.sa_data, 6);
    return 0;
#elif defined(OS_APPLE)
    struct {
        struct if_msghdr ifm;
        struct sockaddr_dl sdl;
        char   extra[16];
    } res;
    size_t len = sizeof(res);
    int mib[] = {
        CTL_NET,
        AF_ROUTE,
        0,
        AF_LINK,
        NET_RT_IFLIST,
        iface->if_index
    };

    RETHROW(sysctl(mib, countof(mib), &res, &len, NULL, 0));
    THROW_ERR_IF(len > sizeof(res));
    THROW_ERR_IF(res.sdl.sdl_type != IFT_ETHER);
    p_copy(mac, LLADDR(&res.sdl), 6);
    return 0;
#else
# error "unsupported operating system"
#endif
}

ATTRS
#ifdef ALL_STATIC
static
#endif
bool F(is_my_mac_addr)(const char *addr)
{
#ifdef __sun
    /* TODO PORT */
    return true;
#else
    bool found = false;
    struct if_nameindex *iflist;
    struct if_nameindex *cur;
    byte mac[6];
    int s = -1;

    /* addr points to a string of the form "XX:XX:XX:XX:XX:XX" */
    if (strlen(addr) != 17) {
        return false;
    }
    if (addr[2] != ':' || addr[5] != ':' || addr[8] != ':'
    ||  addr[11] != ':' || addr[14] != ':') {
        return false;
    }
#define PARSE_HEX(dst, s) do { \
        int x = hexdecode(s); \
        if (x < 0) { \
            return false; \
        } \
        dst = x & 0xFF; \
    } while (0)
    PARSE_HEX(mac[0], addr + 0 * 3);
    PARSE_HEX(mac[1], addr + 1 * 3);
    PARSE_HEX(mac[2], addr + 2 * 3);
    PARSE_HEX(mac[3], addr + 3 * 3);
    PARSE_HEX(mac[4], addr + 4 * 3);
    PARSE_HEX(mac[5], addr + 5 * 3);
#undef PARSE_HEX

    iflist = if_nameindex();
    if (!iflist) {
        return false;
    }

    s = socket(PF_INET, SOCK_DGRAM, IPPROTO_IP);
    if (s < 0) {
        if_freenameindex(iflist);
        return false;
    }
    for (cur = iflist; cur->if_index; cur++) {
        byte if_mac[6];

        if (F(get_mac_addr)(s, cur, if_mac) < 0) {
            continue;
        }
        if (memcmp(if_mac, mac, sizeof(mac)) == 0) {
            found = true;
            break;
        }
    }
    close(s);
    if_freenameindex(iflist);
    return found;
#endif
}

ATTRS
static void F(cpuid)(uint32_t request, uint32_t *eax, uint32_t *ebx,
                     uint32_t *ecx, uint32_t *edx)
{
#if defined(__i386__)
     /* The IA-32 ABI specifies that %ebx holds the address of the
         global offset table.  In PIC code, GCC seems to be unable to
         handle asms with result operands that live in %ebx; I get the
         message:

         error: can't find a register in class `BREG' while reloading
         `asm'

         So, to work around this, we save %ebx in %esi, restore %ebx
         after we've done the 'cpuid', and return %ebx's value in
         %esi.

         We include "memory" in the clobber list, because this is a
         synchronizing instruction; other processor's writes may
         become visible here.  */

      asm volatile ("mov %%ebx, %%esi\n\t" /* Save %ebx.  */
                    "cpuid\n\t"
                    "xchgl %%ebx, %%esi" /* Restore %ebx.  */
                    : "=a" (*eax), "=S" (*ebx), "=c" (*ecx), "=d" (*edx)
                    : "0" (request)
                    : "memory");
#elif defined(__x86_64__)
      asm volatile ("cpuid\n\t"
                    : "=a" (*eax), "=b" (*ebx), "=c" (*ecx), "=d" (*edx)
                    : "0" (request)
                    : "memory");
#else
#  error Your arch is unsupported
#endif
}

/* Get the CPU signature
 */
ATTRS
#ifdef ALL_STATIC
static
#endif
int F(read_cpu_signature)(uint32_t *dst)
{
    uint32_t eax, ebx, ecx, edx;

    if (dst) {
        F(cpuid)(1, &eax, &ebx, &ecx, &edx);
        /* Get 32 bits from EAX */
        *dst = eax;
        return 0;
    } else {
        return -1;
    }
}

/* IOP Licences {{{ */

ATTRS
static __must_check__
int F(licence_check_iop_host)(const core__licence__t *licence)
{
    /* check optional CPU Signatures */
    if (licence->cpu_signatures.len) {
        uint32_t cpusig;

        if (F(read_cpu_signature)(&cpusig)) {
#ifdef NDEBUG
            return -1;
#else
            return e_error("unable to get CPU signature");
#endif
        }
        tab_for_each_entry(id, &licence->cpu_signatures) {
            if (cpusig == id) {
                goto cpu_ok;
            }
        }
#ifdef NDEBUG
        return -1;
#else
        return e_error("invalid cpuSignature");
#endif
    }
  cpu_ok:

    /* check optional MAC addresses */
    if (licence->mac_addresses.len) {
        tab_for_each_entry(mac, &licence->mac_addresses) {
            if (F(is_my_mac_addr)(mac.s)) {
                goto mac_ok;
            }
        }
#ifdef NDEBUG
        return -1;
#else
        return e_error("invalid macAddresses");
#endif
    }
  mac_ok:

    return 0;
}

/* }}} */
#endif
