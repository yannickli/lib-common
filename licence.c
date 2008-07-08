/**************************************************************************/
/*                                                                        */
/*  Copyright (C) 2004-2008 INTERSEC SAS                                  */
/*                                                                        */
/*  Should you receive a copy of this source code, you must check you     */
/*  have a proper, written authorization of INTERSEC to hold it. If you   */
/*  don't have such an authorization, you must DELETE all source code     */
/*  files in your possession, and inform INTERSEC of the fact you obtain  */
/*  these files. Should you not comply to these terms, you can be         */
/*  prosecuted in the extent permitted by applicable law.                 */
/*                                                                        */
/**************************************************************************/

#ifndef MINGCC
#include <stdio.h>
#include <net/if.h>
#ifdef __linux__
#include <net/if_arp.h>
#endif
#include <netinet/in.h>
#include <sys/ioctl.h>

#include "mem.h"
#include "conf.h"
#include "str.h"
#include "licence.h"
#include "time.h"

#define xstr(x)  #x
#define str(x)   xstr(x)
#define str_INTERSECID  str(INTERSECID)
#define LF  "\n"


int show_licence(const char *arg)
{
#ifdef EXPIRATION_DATE
    time_t t = EXPIRATION_DATE;
#endif

    /* OG: Should use project based product string. */
    fprintf(stderr,
            "%s:"                                                           LF
            "Copyright (C) 2004-2008  INTERSEC SAS -- All Rights Reserved"  LF
            , arg);
#ifdef INTERSECID
    fprintf(stderr, "  Intersec ID: " str_INTERSECID LF);
#endif
#ifndef CHECK_TRACE
    fprintf(stderr, "  ptrace check DEACTIVATED" LF);
#endif
#ifdef EXPIRATION_DATE
    fprintf(stderr,
            "  Expiration: %s" LF, ctime(&t));
#endif
    fprintf(stderr,
            "     Contact: +33 (0)1 55 70 33 40 -- www.intersec.com"        LF
           );
    exit(1);
}

int set_licence(const char *arg, const char *licence_data)
{
    if (licence_data && !strcmp(licence_data, str(INTERSECID))) {
        fprintf(stderr, "%s: Licence installed\n", arg);
    } else {
        fprintf(stderr, "%s: Invalid licence key\n", arg);
    }
    exit(1);
}

static int parse_hex(int b)
{
    if (b >= '0' && b <= '9') {
        return b - '0';
    }
    if (b >= 'a' && b <= 'f') {
        return b - 'a' + 10;
    }
    if (b >= 'A' && b <= 'F') {
        return b - 'A' + 10;
    }
    return -1;
}

static int parse_hex2(int b1, int b0)
{
    int x0, x1;

    x0 = parse_hex(b0);
    if (x0 < 0 || x0 > 15) {
        return -1;
    }
    x1 = parse_hex(b1);
    if (x1 < 0 || x1 > 15) {
        return -1;
    }

    return (x1 << 4) | x0;
}

#if defined(__CYGWIN__)
struct if_nameindex {
    int if_index;
    char if_name[32];
};
#define ARPHRD_ETHER  0
static inline struct if_nameindex *if_nameindex(void) {
    struct if_nameindex *p = calloc(sizeof(struct if_nameindex), 2);
    strcpy(p->if_name, "eth0");
    return p;
}

static inline void if_freenameindex(struct if_nameindex *p) {
    free(p);
}
#endif

bool is_my_mac_addr(const char *addr)
{
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
#define PARSE_HEX(dst, b0, b1) do { \
        int x = parse_hex2((b0), (b1)); \
        if (x < 0 || x > 255) { \
            return false; \
        } \
        dst = x & 0xFF; \
    } while (0)
    PARSE_HEX(mac[0], addr[0], addr[1]);
    PARSE_HEX(mac[1], addr[3], addr[4]);
    PARSE_HEX(mac[2], addr[6], addr[7]);
    PARSE_HEX(mac[3], addr[9], addr[10]);
    PARSE_HEX(mac[4], addr[12], addr[13]);
    PARSE_HEX(mac[5], addr[15], addr[16]);
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
}


int list_my_macs(char *dst, size_t size)
{
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
}

static inline void cpuid(uint32_t request,
                         uint32_t *eax, uint32_t *ebx,
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
#elif
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

int licence_do_signature(const conf_t *conf, char *dst, size_t size)
{
    int version;
    const char **var, *content, *p;
    char c;
    const char *signed_vars[] = {
        "vendor",
        "cpu_signature",
        "cpu_id",
        "mac_addresses",
        "Expires",
        "Registered-To", "periodicity",
        "maxrate_sms",   "max_sms",
        "maxrate_mms",   "max_mms",
        "maxrate_wp",    "max_wp",
        "maxrate_ussd",  "max_ussd",
        "maxrate_email", "max_email",
        "production_use",
        NULL,
    };
    int k0, k1, k2, k3;

    version = conf_get_int(conf, "licence", "version", -1);
    if (version != 1) {
        return -1;
    }

    /* XXX: version 1 is weak.
     */
    k0 = 0;
    k1 = 28;
    k2 = 17;
    k3 = 11;
    for (var = signed_vars; *var; var++) {
        content = conf_get_raw(conf, "licence", *var);
        if (!content) {
            /* If a variable is missing, assume we do not want it. */
            continue;
        }
        k0 += strlen(content);
        for (p = content; *p; p++) {
            c = *p;
            /* Signature is case-insensitive and does not sign non
             * 'usual' chars. We don't want to get into trouble
             * because the format of a field slightly changes.
             * */
            if ('A' <= c && c <= 'Z') {
                c = c - 'A' + 'a';
            } else
            if (('a' <= c && c <= 'z')
            ||  ('0' <= c && c <= '9')
            || c == ':' || c == '-' || c == '+' || c == '*') {
                /* use it for signature */
                /* XXX: '-' is very important : sms_max_rate = -1
                 * means unlimited, whereas '1' means 1/s ! */

            } else {
                /* skip */
                continue;
            }
            k1 = (k1 * c + 371) & 0xFFFF;
            k2 = (k3 * 21407 + c) & 0xFFFF;
            k3 = (k2 * 17669 + c) & 0xFFFF;
        }
    }
    k0 = (k0 * 3643) & 0xFFFF;
    snprintf(dst, size, "%04X%04X%04X%04X", k0, k1, k2, k3);
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
    char lic_computed[128];
    const char *lic_inconf;

    lic_inconf = conf_get_raw(conf, "licence", "signature");
    if (!lic_inconf)
        return false;

    if (licence_do_signature(conf, lic_computed, sizeof(lic_computed))) {
        return false;
    }

    return strequal(lic_inconf, lic_computed);
}

bool licence_check_host_ok(const conf_t *conf)
{
    uint32_t cpusig;
    char buf[64];
    const char *p;

    if (!licence_check_signature_ok(conf)) {
        return false;
    }
    if (!licence_check_expiration_ok(conf)) {
        return false;
    }
    if (conf_get_int(conf, "licence", "version", -1) != 1) {
        return false;
    }

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

#define __USE_GNU
#include <sched.h>

int list_my_cpus(char *dst, size_t size)
{
#ifdef __linux__
    /* OG: Should use cpu_set_t type and macros ? */
    /* OG: Should check return value of these system calls */
    int i = 0, pos = 0, res = -1;
    unsigned long oldmask, newmask;

    if (sched_getaffinity(0, sizeof(oldmask), (void*)&oldmask))
        goto exit;

    for (i = 0; i < ssizeof(oldmask) * 8; i++) {
        uint32_t cpusig;

        /* Only enumerate cpus enabled by default */
        newmask = 1L << i;
        if (!(newmask & oldmask))
            continue;

        /* Tell linux we prefer to run on CPU number i. */
        if (sched_setaffinity(0, sizeof(newmask), (void*)&newmask))
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

START_TEST(check_parse_hex)
{
    fail_if(parse_hex('0') != 0x0, "failed\n");
    fail_if(parse_hex('9') != 0x9, "failed\n");
    fail_if(parse_hex('a') != 0xa, "failed\n");
    fail_if(parse_hex('f') != 0xf, "failed\n");
    fail_if(parse_hex('g') != -1, "failed\n");
}
END_TEST

START_TEST(check_parse_hex2)
{
    fail_if(parse_hex2('0', '0') != 0x00, "failed\n");
    fail_if(parse_hex2('0', '1') != 0x01, "failed\n");
    fail_if(parse_hex2('1', '0') != 0x10, "failed\n");
    fail_if(parse_hex2('0', 'a') != 0x0a, "failed\n");
    fail_if(parse_hex2('a', 'a') != 0xaa, "failed\n");
    fail_if(parse_hex2('f', 'f') != 0xff, "failed\n");
    fail_if(parse_hex2('g', 'z') != -1, "failed\n");
}
END_TEST

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
    tcase_add_test(tc, check_parse_hex);
    tcase_add_test(tc, check_parse_hex2);
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
