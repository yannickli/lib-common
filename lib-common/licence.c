/**************************************************************************/
/*                                                                        */
/*  Copyright (C) 2004-2007 INTERSEC SAS                                  */
/*                                                                        */
/*  Should you receive a copy of this source code, you must check you     */
/*  have a proper, written authorization of INTERSEC to hold it. If you   */
/*  don't have such an authorization, you must DELETE all source code     */
/*  files in your possession, and inform INTERSEC of the fact you obtain  */
/*  these files. Should you not comply to these terms, you can be         */
/*  prosecuted in the extent permitted by applicable law.                 */
/*                                                                        */
/**************************************************************************/

#include <stdio.h>
#include <net/if.h>
#include <net/if_arp.h>
#include <netinet/in.h>
#include <sys/ioctl.h>

#include "macros.h"
#include "conf.h"
#include "string_is.h"
#include "licence.h"

#define xstr(x)  #x
#define str(x)   xstr(x)
#define str_INTERSECID  str(INTERSECID)
#define LF  "\n"

#ifdef EXPIRATION_DATE

int show_licence(const char *arg)
{
    time_t t = EXPIRATION_DATE;

    fprintf(stderr,
            "%s -- INTERSEC Multi Channel Marketing Suite version 2.6"      LF
            "Copyright (C) 2004-2007  INTERSEC SAS -- All Rights Reserved"  LF
            "Licence type: Temporary Test Licence"                          LF
            "    Licencee: OrangeFrance SA"                                 LF
            "  Intersecid: " str(INTERSECID) ""                                      LF
            , arg);
#ifdef EXPIRATION_DATE    
    fprintf(stderr,
            "  Expiration: %s", ctime(&t));
#endif
    fprintf(stderr,
            "     Contact: +33 1 7543 5100 -- www.intersec.eu"              LF
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
#endif

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

bool is_my_mac_addr(const char *addr)
{
    int x;
    bool found = false;
    struct if_nameindex *iflist;
    struct if_nameindex *cur;
    char mac[6];
    int s = -1;

    iflist = if_nameindex();
    if (!iflist) {
        return false;
    }

    /* addr points to a string of the form "XX:XX:XX:XX:XX:XX" */
    if (strlen(addr) != 17) {
        return false;
    }
    if (addr[2] != ':' || addr[5] != ':' || addr[8] != ':'
    ||  addr[11] != ':' || addr[14] != ':') {
        return false;
    }
#define PARSE_HEX(dst, b0, b1) do { \
        x = parse_hex2((b0), (b1)); \
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

    s = socket(PF_INET, SOCK_DGRAM, IPPROTO_IP);
    if (s < 0) {
        if_freenameindex(iflist);
        return false;
    }
    for (cur = iflist ; cur->if_index; cur++) {
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
    for (cur = iflist ; cur->if_index; cur++) {
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
        if ( written < 0 || written >= (int) size) {
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

int licence_compute_conf_signature(const conf_t *conf, char *dst, size_t size)
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
        "Registered-To",
        "max_sms",
        "max_mms",
        "max_wp",
        "max_ussd",
        "max_email",
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

int licence_check_signature_ok(const conf_t *conf)
{
    char lic_computed[128];
    const char *lic_inconf;

    lic_inconf = conf_get_raw(conf, "licence", "signature");

    if (!lic_inconf 
    || licence_compute_conf_signature(conf, lic_computed,
                                      sizeof(lic_computed))) {
        return 1;
    }

    return !strcmp(lic_inconf, lic_computed);
}

int licence_check_host_ok(const conf_t *conf)
{
    uint32_t cpusig;
    char buf[64];
    const char *p, *cpu_signature;

    if (!licence_check_signature_ok(conf)) {
        return 0;
    }
    if (conf_get_int(conf, "licence", "version", -1) != 1) {
        return 0;
    }

    /* cpu_signature is optional in licence section : If it does not
     * appear, we do not check it. */
    cpu_signature = conf_get_raw(conf, "licence", "cpu_signature");
    if (cpu_signature) {
        if (read_cpu_signature(&cpusig)) {
            return 0;
        }
        snprintf(buf, sizeof(buf), "0x%08X", cpusig);
        if (strcasecmp(buf, cpu_signature)) {
            return 0;
        }
    }

    p = conf_get_raw(conf, "licence", "mac_addresses");
    while (*p) {
        p += pstrcpylen(buf, sizeof(buf), p, 17);
        if (is_my_mac_addr(buf)) {
            goto mac_ok;
        }
        while (*p && (*p == ' ' || *p == ','))
            p++;
    }
    return 0;
mac_ok:
    return 1;
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
        "00:30:1B:B8:EB:3B", /* mad */
        "00:0F:1F:F9:29:96", /* mag */
        "00:30:1B:AC:39:C3", /* ogu21 */
        "00:14:22:0A:F0:FC", /* papyrus */
        "00:11:95:DC:D1:45", /* suze */
        "00:0F:B0:4A:98:11", /* tian */
        "00:13:20:C0:2A:25", /* vodka */
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
    int ret;

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

Suite *check_licence_suite(void)
{
    Suite *s  = suite_create("Licence");
    TCase *tc = tcase_create("Core");

    suite_add_tcase(s, tc);
    tcase_add_test(tc, check_parse_hex);
    tcase_add_test(tc, check_parse_hex2);
    tcase_add_test(tc, check_parse_hex2);
    tcase_add_test(tc, check_list_my_macs);
    tcase_add_test(tc, check_licence_check_signature_ok);
    return s;
}

/*.....................................................................}}}*/
#endif
/*:::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::}}}*/
