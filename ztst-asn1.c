/**************************************************************************/
/*                                                                        */
/*  Copyright (C) 2004-2010 INTERSEC SAS                                  */
/*                                                                        */
/*  Should you receive a copy of this source code, you must check you     */
/*  have a proper, written authorization of INTERSEC to hold it. If you   */
/*  don't have such an authorization, you must DELETE all source code     */
/*  files in your possession, and inform INTERSEC of the fact you obtain  */
/*  these files. Should you not comply to these terms, you can be         */
/*  prosecuted in the extent permitted by applicable law.                 */
/*                                                                        */
/**************************************************************************/

#include <lib-common/all.h>
#include "asn1.h"

/********************************************************************/
/*  Tests.                                                          */
/********************************************************************/
int main(int argc, char **argv)
{
    int test_count = 0;
    int fail_count = 0;

    puts ("\nber_decode_len32 - TESTS :");

    {
        const byte buf[4] = { 0x80 | 0x3, 0xfa, 0x56, 0x09 };
        pstream_t ps = ps_init(buf, 4);
        uint32_t len32;
        puts("Test asn1 function : "
             "ber_decode_len32 - Normal case 1...");
        ber_decode_len32(&ps, &len32);       /* TEST */
        test_count++;
        if(len32 != 0xfa5609) {
            fail_count++;
            e_trace(0, "FAIL : got %x, expected %x", len32, 0xfa5609);
        } else {
            puts("...OK");
        }
    }

    {
        const byte buf[1] = { 0x3 };
        pstream_t ps = ps_init(buf, 1);
        uint32_t len32;
        puts("Test asn1 function : "
             "ber_decode_len32 - Normal case 2...");
        ber_decode_len32(&ps, &len32);       /* TEST */
        test_count++;
        if(len32 != 3) {
            fail_count++;
            e_trace(0, "FAIL");
        } else {
            puts("...OK");
        }
    }

    {
        int return_value;
        const byte buf[3] = { 0x80, 0xb5, 0x45 };
        pstream_t ps = ps_init(buf, 3);
        uint32_t len32;
        puts("Test asn1 function : "
             "ber_decode_len32 - indefinite length...");
        return_value = ber_decode_len32(&ps, &len32);       /* TEST */
        test_count++;
        if(return_value != 1) {
            fail_count++;
            e_trace(0, "FAIL");
        } else {
            puts("...OK");
        }
    }

    {
        int return_value;
        const byte buf[7] = { 0x85, 0x1, 0x2, 0x3, 0x4, 0x5, 0x6 };
        uint32_t len32;
        pstream_t ps = ps_init(buf, 7);
        puts("Test asn1 function : "
             "ber_decode_len32 - error length too long...");
        return_value = ber_decode_len32(&ps, &len32);       /* TEST */
        test_count++;
        if(return_value >= 0) {
            fail_count++;
            e_trace(0, "FAIL");
        } else {
            puts("...OK");
        }
    }

    {
        int return_value;
        const byte buf[4] = { 0x84, 0x1, 0x2, 0x3};
        pstream_t ps = ps_init(buf, 4);
        uint32_t len32;
        puts("Test asn1 function : "
             "ber_decode_len32 - error not enough data...");
        return_value = ber_decode_len32(&ps, &len32);       /* TEST */
        test_count++;
        if(return_value >= 0) {
            fail_count++;
            e_trace(0, "FAIL");
        } else {
            puts("...OK");
        }
    }

    puts ("\nber_decode_int32 - TESTS :");
    {
        const byte buf[4] = { 0x3, 0xfa, 0x56, 0x09 };
        pstream_t ps = ps_init(buf, 4);
        int32_t int32;
        puts("Test asn1 function : "
             "ber_decode_int32 - normal conditions...");
        ber_decode_int32(&ps, &int32);       /* TEST */
        test_count++;
        if(int32 != 0x3fa5609) {
            fail_count++;
            e_trace(0, "FAIL : got %x, expected %x", int32, 0x3fa5609);
        } else {
            puts("...OK");
        }
    }

    {
        const byte buf[3] = { 0x83, 0xfa, 0x56 };
        pstream_t ps = ps_init(buf, 3);
        int32_t int32;
        puts("Test asn1 function : "
             "ber_decode_int32 - normal conditions (negative integer)...");
        ber_decode_int32(&ps, &int32);       /* TEST */
        test_count++;
        if (int32 != (int32_t)0xff83fa56) {
            fail_count++;
            e_trace(0, "FAIL - obtained %d, waited %d",
                    int32, (int32_t)0xff83fa56);
        } else {
            puts("...OK");
        }
    }

    {
        int return_value;
        const byte buf[5] = { 0xff, 0xfa, 0x56, 0x45, 0xf5 };
        pstream_t ps = ps_init(buf, 5);
        int32_t int32;
        puts("Test asn1 function : "
             "ber_decode_int32 - integer too long...");
        return_value = ber_decode_int32(&ps, &int32);  /* TEST */
        test_count++;
        if(return_value >= 0) {
            fail_count++;
            e_trace(0, "FAIL - expected error did not occur");
            iprintf("-int32 = %x", -int32);
        } else {
            puts("...OK");
        }
    }

    if (fail_count) {
        puts("");
        e_trace(0, "warning : FAILS occured while testing.");
    }

    iprintf("\nTests finished. (success/passed) (%d/%d)\n",
            test_count - fail_count, test_count);

    return 0;
}
