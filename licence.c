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

#include "core.h"
#ifndef OS_WINDOWS
#include <net/if.h>
#ifdef OS_LINUX
#include <net/if_arp.h>
#endif
#include <netinet/in.h>
#include <sys/ioctl.h>

#include "conf.h"
#include "licence.h"
#include "time.h"
#include "hash.h"
#include "property.h"

int trace_override;

#if defined(__CYGWIN__)
struct if_nameindex {
    int if_index;
    char if_name[32];
};
#define ARPHRD_ETHER  0
static struct if_nameindex *if_nameindex(void) {
    struct if_nameindex *p = calloc(sizeof(struct if_nameindex), 2);
    strcpy(p->if_name, "eth0");
    return p;
}

static void if_freenameindex(struct if_nameindex *p) {
    free(p);
}
#endif

bool is_my_mac_addr(const char *addr)
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
        struct ifreq if_hwaddr;

        pstrcpy(if_hwaddr.ifr_ifrn.ifrn_name, IFNAMSIZ, cur->if_name);
        if (ioctl(s, SIOCGIFHWADDR, &if_hwaddr) < 0) {
            continue;
        }

        if (if_hwaddr.ifr_hwaddr.sa_family != ARPHRD_ETHER) {
            continue;
        }

        if (!memcmp(if_hwaddr.ifr_hwaddr.sa_data, mac, sizeof(mac))) {
            found = true;
            break;
        }
    }
    close(s);
    if_freenameindex(iflist);
    return found;
#endif
}


int list_my_macs(char *dst, size_t size)
{
#ifdef __sun
    /* TODO PORT */
    pstrcpy(dst, size, "00:00:00:00:00:00");
    return true;
#else
    struct if_nameindex *iflist;
    struct if_nameindex *cur;
    char *mac;
    int s = -1;
    int written, ret = 0;
    const char *sep = "";

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
        struct ifreq if_hwaddr;

        pstrcpy(if_hwaddr.ifr_ifrn.ifrn_name, IFNAMSIZ, cur->if_name);
        if (ioctl(s, SIOCGIFHWADDR, &if_hwaddr) < 0) {
            continue;
        }

        if (if_hwaddr.ifr_hwaddr.sa_family != ARPHRD_ETHER) {
            continue;
        }

        mac = if_hwaddr.ifr_hwaddr.sa_data;
        written = snprintf(dst, size, "%s%02X:%02X:%02X:%02X:%02X:%02X",
                           sep, mac[0], mac[1], mac[2], mac[3], mac[4], mac[5]);
        sep = ",";
        if (written < 0 || (size_t)written >= size) {
            ret = 1;
            break;
        }
        dst += written;
        size -= written;
    }
    close(s);
    if_freenameindex(iflist);
    return ret;
#endif
}

static void cpuid(uint32_t request, uint32_t *eax, uint32_t *ebx,
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
int read_cpu_signature(uint32_t *dst)
{
    uint32_t eax, ebx, ecx, edx;

    if (dst) {
        cpuid(1, &eax, &ebx, &ecx, &edx);
        /* Get 32 bits from EAX */
        *dst = eax;
        return 0;
    } else {
        return -1;
    }
}

int licence_do_signature(const conf_t *conf, char dst[65])
{
    int version;
    char buf[32];
    const char *p;
    char c;
    const char *blacklisted[] = {
        "signature",
        NULL
    };
    props_array tosign;
    const conf_section_t *s;
    int k0, k1, k2, k3;
    int len;
    sha2_ctx  ctx;

    version = conf_get_int(conf, "licence", "version", -1);
    if (version != 1) {
        return -1;
    }

    s = conf_get_section_by_name(conf, "licence");
    if (!s) {
        return -1;
    }

    props_array_init(&tosign);
    props_array_dup(&tosign, &s->vals);
    props_array_filterout(&tosign, blacklisted);
    props_array_qsort(&tosign);

    /* XXX: version 1 is weak.
     */
    k0 = 0;
    k1 = 28;
    k2 = 17;
    k3 = 11;
    sha2_starts(&ctx, 0);
    for (int i = 0; i < tosign.len; i++) {
#define DO_SIGNATURE(content)                                       \
        len = strlen(content);                                      \
        k0 += len;                                                  \
        for (p = content; *p; p++) {                                \
            c = *p;                                                 \
            /* Signature is case-insensitive and does not sign non  \
             * 'usual' chars. We don't want to get into trouble     \
             * because the format of a field slightly changes.      \
             * */                                                   \
            if ('A' <= c && c <= 'Z') {                             \
                c = c - 'A' + 'a';                                  \
            } else                                                  \
            if (('a' <= c && c <= 'z')                              \
            ||  ('0' <= c && c <= '9')                              \
            || c == ':' || c == '-' || c == '+' || c == '*') {      \
                /* use it for signature */                          \
                /* XXX: '-' is very important : sms_max_rate = -1   \
                 * means unlimited, whereas '1' means 1/s ! */      \
                                                                    \
            } else {                                                \
                /* skip */                                          \
                continue;                                           \
            }                                                       \
            k1 = (k1 * c + 371) & 0xFFFF;                           \
            k2 = (k3 * 21407 + c) & 0xFFFF;                         \
            k3 = (k2 * 17669 + c) & 0xFFFF;                         \
        }                                                           \
        sha2_update(&ctx, content, len);

        DO_SIGNATURE(tosign.tab[i]->name);
        sha2_update(&ctx, ":", 1);
        DO_SIGNATURE(tosign.tab[i]->value);
    }
    k0 = (k0 * 3643) & 0xFFFF;
    len = snprintf(buf, sizeof(buf), "%04X%04X%04X%04X", k0, k1, k2, k3);
    sha2_update(&ctx, buf, len);
    sha2_finish_hex(&ctx, dst);
    props_array_wipe(&tosign);
//    printf("signature = %s\n", dst);
    return 0;
}

bool licence_check_expiration_ok(const conf_t *conf)
{
    const char *expires = conf_get_raw(conf, "licence", "expires");
    struct tm t;

    if (!expires)
        return false;

    p_clear(&t, 1);
    t.tm_isdst = -1;

    if (strtotm(expires, &t))
        return false;

    return mktime(&t) > time(NULL);
}

bool licence_check_signature_ok(const conf_t *conf)
{
    char lic_computed[65];
    const char *lic_inconf;

    lic_inconf = conf_get_raw(conf, "licence", "signature");
    if (!lic_inconf)
        return false;

    if (licence_do_signature(conf, lic_computed)) {
        return false;
    }

    return strequal(lic_inconf, lic_computed);
}

bool licence_check_general_ok(const conf_t *conf)
{
    if (!licence_check_signature_ok(conf)) {
        return false;
    }
    if (!licence_check_expiration_ok(conf)) {
        return false;
    }
    if (conf_get_int(conf, "licence", "version", -1) != 1) {
        return false;
    }
    return true;
}

bool licence_check_specific_host_ok(const conf_t *conf)
{
    uint32_t cpusig;
    char buf[64];
    const char *p;

    /* cpu_signature is optional in licence section : If it does not
     * appear, we do not check it. */
    p = conf_get_raw(conf, "licence", "cpu_signature");
    if (p) {
        uint32_t sig;

        if (read_cpu_signature(&cpusig)) {
            return false;
        }

        while (*p) {
            sig = strtol(p, &p, 0);
            if (sig == cpusig)
                goto cpu_ok;

            while (*p && (*p == ' ' || *p == ','))
                p++;
        }
        return false;
    }

  cpu_ok:
    p = conf_get_raw(conf, "licence", "mac_addresses");
    if (!p)
        return false;
    while (*p) {
        p += pstrcpylen(buf, sizeof(buf), p, 17);
        if (is_my_mac_addr(buf)) {
            goto mac_ok;
        }
        while (*p && (*p == ' ' || *p == ','))
            p++;
    }
    return false;

  mac_ok:
    return true;
}

bool licence_check_host_ok(const conf_t *conf)
{
    if (licence_check_general_ok(conf) == false) {
        return false;
    }
    return licence_check_specific_host_ok(conf);
}


#include <sched.h>

int list_my_cpus(char *dst, size_t size)
{
#ifdef __linux__
    /* OG: Should use cpu_set_t type and macros ? */
    /* OG: Should check return value of these system calls */
    int i = 0, pos = 0, res = -1;
    cpu_set_t oldmask, newmask;

    if (sched_getaffinity(0, sizeof(oldmask), &oldmask))
        goto exit;

    for (i = 0; i < ssizeof(oldmask) * 8; i++) {
        uint32_t cpusig;

        /* Only enumerate cpus enabled by default */
        if (!CPU_ISSET(i, &oldmask))
            continue;
        CPU_ZERO(&newmask);
        (void)CPU_SET(i, &newmask);

        /* Tell linux we prefer to run on CPU number i. */
        if (sched_setaffinity(0, sizeof(newmask), &newmask))
            goto exit;

        /* OG: this might not be necessary. */
        usleep(100);

        if (read_cpu_signature(&cpusig))
            goto exit;

        pos += snprintf(dst + pos, size - pos, "%s0x%08X",
                        pos ? "," : "", cpusig);
    }
    res = 0;

  exit:
    sched_setaffinity(0, sizeof(oldmask), (void*)&oldmask);
    return res;
#else
    uint32_t cpusig;

    if (!read_cpu_signature(&cpusig)) {
        snprintf(dst, size, "0x%08X", cpusig);
        return 0;
    }
    return -1;
#endif
}

/*[ CHECK ]::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::{{{*/
#ifdef CHECK
/* {{{*/
#include <check.h>

START_TEST(check_list_my_macs)
{
    char buf[128];

    fail_if(list_my_macs(buf, sizeof(buf)), "Non 0 return value");
//    printf("Found addresses : %s\n", buf);
}
END_TEST

START_TEST(check_is_my_mac_addr)
{
    int j;
    bool found = false;
    const char *maclist[] = {
        /* List your Macaddr here if you want the check to pass */
        "00:0F:3D:F1:A2:F0", /* exile */
        "00:11:09:E8:C6:72", /* fle1 */
        "00:1A:A0:1C:54:B3", /* new fle1 */
        "00:30:1B:B8:EB:3B", /* mad */
        "00:0F:1F:F9:29:96", /* mag */
        "00:30:1B:AC:39:C3", /* ogu21 */
        "00:11:95:25:A5:66", /* ogu69 */
        "00:13:72:13:18:c7", /* ogu69 */
        "00:14:22:0A:F0:FC", /* papyrus */
        "00:11:95:DC:D1:45", /* suze */
        "00:0F:B0:4A:98:11", /* tian */
        "00:13:20:C0:2A:25", /* vodka */
        "00:18:8B:5B:CB:BF", /* soho */
        "00:13:77:48:4A:06", /* artemis */
        "00:18:8B:5B:71:3D", /* gin */
        "00:1A:A0:1C:53:33", /* tequila */
        "00:18:8B:D5:89:88", /* curacao */
    };

    for (j = 0; j < countof(maclist); j++) {
        if (is_my_mac_addr(maclist[j])) {
            found = true;
            break;
        }
    }

    fail_if(!found, "Did not find any listed mac addr on this machine.");
}
END_TEST

START_TEST(check_licence_check_signature_ok)
{
    conf_t * conf;

    conf = conf_load("samples/licence-v1-ok.conf");
    fail_if(!conf, "Unable to open test file");
    fail_if(!licence_check_signature_ok(conf), "licence-test-ok failed to pass");
    conf_delete(&conf);

    conf = conf_load("samples/licence-v1-ko.conf");
    fail_if(!conf, "Unable to open test file");
    fail_if(licence_check_signature_ok(conf), "licence-test-ko passed");
    conf_delete(&conf);
}
END_TEST

START_TEST(check_licence_check_expiration_ok)
{
    conf_t * conf;

    conf = conf_load("samples/licence-v1-ok.conf");
    fail_if(!conf, "Unable to open test file");
    fail_if(!licence_check_expiration_ok(conf), "licence-test-ok failed to pass");
    conf_delete(&conf);

    conf = conf_load("samples/licence-v1-ko.conf");
    fail_if(!conf, "Unable to open test file");
    fail_if(licence_check_expiration_ok(conf), "licence-test-ko passed");
    conf_delete(&conf);
}
END_TEST

Suite *check_licence_suite(void)
{
    Suite *s  = suite_create("Licence");
    TCase *tc = tcase_create("Core");

    suite_add_tcase(s, tc);
#ifndef __CYGWIN__
    tcase_add_test(tc, check_is_my_mac_addr);
#endif
    tcase_add_test(tc, check_list_my_macs);
    tcase_add_test(tc, check_licence_check_signature_ok);
    tcase_add_test(tc, check_licence_check_expiration_ok);
    return s;
}

/*.....................................................................}}}*/
#endif
/*:::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::}}}*/
#endif
