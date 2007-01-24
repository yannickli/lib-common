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

Suite *check_licence_suite(void)
{
    Suite *s  = suite_create("Licence");
    TCase *tc = tcase_create("Core");

    suite_add_tcase(s, tc);
    tcase_add_test(tc, check_parse_hex);
    tcase_add_test(tc, check_parse_hex2);
    tcase_add_test(tc, check_parse_hex2);
    tcase_add_test(tc, check_list_my_macs);
    return s;
}

/*.....................................................................}}}*/
#endif
/*:::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::}}}*/
