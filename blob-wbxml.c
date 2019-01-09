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

#include "blob.h"

/* For Wap Push ServiceIndication WBXML tokens:
 *
 * /home/data/doc/wpush/2.1/WAP-167-ServiceInd-20010731-a.pdf
 *
 * */
static void append_href_string(blob_t *out, const byte *data, int len)
{
    const byte *p;
    int written = 0, pos = 0;

    /* if input is empty, nothing is appended. */
    if (len < 1)
        return;

    blob_append_byte(out, 0x03);
    while (pos < len && (p = memchr(data + pos, '.', len - pos))) {
        if (len - (p - data) < 5) {
            /* Not enough data to contain extension */
            break;
        }

        switch (p[1]) {
#define CASE_VALUE_TOKEN(c1, c2, c3, tok)                                   \
          case c1:                                                          \
            if (p[2] == c2 && p[3] == c3 && p[4] == '/') {                  \
                blob_append_data(out, data + written, (p - data) - written);\
                blob_append_byte(out, 0x00);                                \
                blob_append_byte(out, tok);                                 \
                if (len - (p - data) > 5) { /* some characters remain */            \
                    blob_append_byte(out, 0x03);                            \
                }                                                           \
                pos = written = (p - data) + 5;                             \
                continue;                                                   \
            }                                                               \
            break;                                                          \

            CASE_VALUE_TOKEN('c', 'o', 'm', 0x85);
            CASE_VALUE_TOKEN('e', 'd', 'u', 0x86);
            CASE_VALUE_TOKEN('n', 'e', 't', 0x87);
            CASE_VALUE_TOKEN('o', 'r', 'g', 0x88);
            break;
        }
        pos = p - data + 1;
    }

    if (written < len) {
        blob_append_data(out, data + written, len - written);
        blob_append_byte(out, 0x00);
    }
}

void blob_append_wbxml_href(blob_t *dst, const byte *data, int len)
{
    if (len < sstrlen("http://")) {
        goto no_encoding;
    }

    if (memcmp(data, "http", 4)) {
        goto no_encoding;
    }

    if (data[4] == 's') {
        if (len < sstrlen("https://"))
            goto no_encoding;

        if (memcmp(data + 4, "s://", 4)) {
            goto no_encoding;
        }

        if (len >= sstrlen("https://www.")
        &&  !memcmp(data + 8, "www.", 4))
        {
            blob_append_byte(dst, 0x0F); /* href + "https://www." */
            append_href_string(dst, data + 12, len - 12);
            return;
        }

        blob_append_byte(dst, 0x0E); /* href + "https://" */
        append_href_string(dst, data + 8, len - 8);
        return;
    }

    if (data[4] != ':' || data[5] != '/' || data[6] != '/') {
        goto no_encoding;
    }

    if (len >= sstrlen("http://www.")
    &&  !memcmp(data + 7, "www.", 4))
    {
        blob_append_byte(dst, 0x0D); /* href + "http://www." */
        append_href_string(dst, data + 11, len - 11);
        return;
    }

    blob_append_byte(dst, 0x0C); /* href + "http://" */
    append_href_string(dst, data + 7, len - 7);
    return;

  no_encoding:
    blob_append_byte(dst, 0x0B);
    append_href_string(dst, data, len);
}

/*[ CHECK ]::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::{{{*/
#ifdef CHECK
/* public testing API                                                  {{{*/

#define SRC0           "abcdef"
#define EXPECTED0      "\x0B\x03""abcdef\x00"
#define SRC1           "http:/""/intersec.fr"
#define EXPECTED1      "\x0C\x03intersec.fr\x00"
#define SRC2           "http:/""/intersec.com/"
#define EXPECTED2      "\x0C\x03intersec\x00\x85"
#define SRC3           "https:/""/www.intersec.com/a"
#define EXPECTED3      "\x0F\x03intersec\x00\x85\x03""a\x00"
#define SRC4           "https:/""/intersec.eu"
#define EXPECTED4      "\x0E\x03intersec.eu\x00"
#define SRC5           "http:/""/www.intersec.eu"
#define EXPECTED5      "\x0D\x03intersec.eu\x00"
#define SRC6           "htt"
#define EXPECTED6      "\x0B\x03htt\x00"
#define SRC7           "http:/""/intersec.com"
#define EXPECTED7      "\x0C\x03intersec.com\x00"

START_TEST(check_append_href)
{
    blob_t dst;

    blob_init(&dst);

#define TEST_WBXML(src, expect) \
    blob_append_wbxml_href(&dst, (const byte *)src, strlen(src));   \
    fail_if(dst.len != sizeof(expect) - 1 || memcmp(dst.data, expect, sizeof(expect) - 1), \
            "Failed: result differs from expected (%d / %zd)", dst.len, sizeof(expect) - 1); \
    blob_reset(&dst);

    TEST_WBXML(SRC0, EXPECTED0);
    TEST_WBXML(SRC1, EXPECTED1);
    TEST_WBXML(SRC2, EXPECTED2);
    TEST_WBXML(SRC3, EXPECTED3);
    TEST_WBXML(SRC4, EXPECTED4);
    TEST_WBXML(SRC5, EXPECTED5);
    TEST_WBXML(SRC6, EXPECTED6);
    TEST_WBXML(SRC7, EXPECTED7);

    blob_wipe(&dst);
}
END_TEST

Suite *check_append_blob_wbxml_suite(Suite *blob_suite)
{
    Suite *s  = blob_suite;
    TCase *tc = tcase_create("WBXML");

    suite_add_tcase(s, tc);
    tcase_add_test(tc, check_append_href);

    return s;
}

/*.....................................................................}}}*/
#endif
/*:::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::}}}*/
