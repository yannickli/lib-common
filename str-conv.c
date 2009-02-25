/**************************************************************************/
/*                                                                        */
/*  Copyright (C) 2004-2009 INTERSEC SAS                                  */
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

uint8_t const __utf8_mark[7] = { 0x00, 0x00, 0xc0, 0xe0, 0xf0, 0xf8, 0xfc };

uint8_t const __utf8_clz_to_charlen[32] = {
#define X  0
    1, 1, 1, 1, 1, 1, 1, /* <=  7 bits */
    2, 2, 2, 2,          /* <= 11 bits */
    3, 3, 3, 3, 3,       /* <= 16 bits */
    4, 4, 4, 4, 4,       /* <= 21 bits */
    X, X, X, X, X,       /* <= 26 bits */
    X, X, X, X, X,       /* <= 31 bits */
    X,                   /* 0x80000000 and beyond */
#undef X
};

uint8_t const __utf8_char_len[32] = {
#define X  0
    1, 1, 1, 1, 1, 1, 1, 1,  /* 00... */
    1, 1, 1, 1, 1, 1, 1, 1,  /* 01... */
    X, X, X, X, X, X, X, X,  /* 100.. */
    2, 2, 2, 2,              /* 1100. */
    3, 3,                    /* 1110. */
    4,                       /* 11110 */
    X,                       /* 11111 */
#undef X
};

uint32_t const __utf8_offs[6] = {
    0x00000000UL, 0x00003080UL, 0x000e2080UL,
    0x03c82080UL, 0xfa082080UL, 0x82082080UL
};

uint8_t const __str_digit_value[128 + 256] = {
#define REPEAT16(x)  x,x,x,x,x,x,x,x,x,x,x,x,x,x,x,x
    REPEAT16(255), REPEAT16(255), REPEAT16(255), REPEAT16(255),
    REPEAT16(255), REPEAT16(255), REPEAT16(255), REPEAT16(255),
    REPEAT16(255), REPEAT16(255), REPEAT16(255),
    0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 255, 255, 255, 255, 255, 255,
    255, 10, 11, 12, 13, 14, 15, 16, 17, 18, 19, 20, 21, 22, 23, 24,
    25, 26, 27, 28, 29, 30, 31, 32, 33, 34, 35, 255, 255, 255, 255, 255,
    255, 10, 11, 12, 13, 14, 15, 16, 17, 18, 19, 20, 21, 22, 23, 24,
    25, 26, 27, 28, 29, 30, 31, 32, 33, 34, 35, 255, 255, 255, 255, 255,
    REPEAT16(255), REPEAT16(255), REPEAT16(255), REPEAT16(255),
    REPEAT16(255), REPEAT16(255), REPEAT16(255), REPEAT16(255),
};

char const __str_digits_upper[36] = "0123456789ABCDEFGHIJKLMNOPQRSTUVWXYZ";
char const __str_digits_lower[36] = "0123456789abcdefghijklmnopqrstuvwxyz";

/* XXX: strconv_hexdecode does not check source correcness beyond
 * dest size, not a big issue for current uses.
 */
int strconv_hexdecode(void *dest, int size, const char *src, int len)
{
    const char *end;
    byte *w = dest;

    if (len < 0)
        len = strlen(src);
    end = src + MIN(len, 2 * size);

    if (len & 1)
        return -1;

    for (; src < end; src += 2) {
        int c = hexdecode(src);

        if (c < 0)
            return -1;
        *w++ = c;
    }

    return len / 2;
}

int strconv_hexencode(char *dest, int size, const void *src, int len)
{
    const byte *s = src, *end = s + MIN(len, (size - 1) / 2);

    if (size <= 0)
        return 2 * len;

    while (s < end) {
        *dest++ = __str_digits_lower[(*s >> 4) & 0xf];
        *dest++ = __str_digits_lower[(*s++)    & 0xf];
    }

    *dest = '\0';
    return len * 2;
}

/****************************************************************************/
/* Charset conversions                                                      */
/****************************************************************************/

static uint16_t const __latinX_to_utf8[0x40] = {
    /* cp1252 to utf8 */
    /*  "€"            "‚"     "ƒ"     "„"     "…"      "†"    "‡"    */
    0x20ac,   0x81, 0x201a, 0x0192, 0x201e, 0x2026, 0x2020, 0x2021,
    /* "ˆ"     "‰"     "Š"     "‹"     "Œ"             "Ž"            */
    0x02c6, 0x2030, 0x0160, 0x2039, 0x0152,   0x8d, 0x017d,   0x8f,
    /*         "‘"     "’"     "“"     "”"     "•"     "–"     "—"    */
      0x90, 0x2018, 0x2019, 0x201c, 0x201d, 0x2022, 0x2013, 0x2014,
    /* "˜"     "™"     "š"     "›"     "œ"             "ž"     "Ÿ"    */
    0x02dc, 0x2122, 0x0161, 0x203a, 0x0153,   0x9d, 0x017e, 0x0178,

    /* latin9 to utf8 */
    /*                                 "€"             "Š"            */
      0xa0,   0xa1,   0xa2,   0xa3, 0x20ac,   0xa5, 0x0160,   0xa7,
    /* "š"                                                            */
    0x0161,   0xa9,   0xaa,   0xab,   0xac,   0xad,   0xae,   0xaf,
    /*                                 "Ž"                            */
      0xb0,   0xb1,   0xb2,   0xb3, 0x017d,   0xb5,   0xb6,   0xb7,
    /* "ž"                             "Œ"     "œ"     "Ÿ"            */
    0x017e,   0xb9,   0xba,   0xbb, 0x0152, 0x0153, 0x0178,   0xbf,
};

static void __from_latinX_aux(sb_t *sb, const void *data, int len, int limit)
{
    const char *s = data, *end = s + len;

    while (s < end) {
        const char *p = s;

        s = utf8_skip(s, end);
        sb_add(sb, p, s - p);

        if (s < end) {
            int c = (unsigned char)*s++;
            if (c < limit)
                c = __latinX_to_utf8[c & 0x7f];
            sb_adduc(sb, c);
        }
    }
}

void sb_conv_from_latin1(sb_t *sb, const void *s, int len)
{
    __from_latinX_aux(sb, s, len, 0xa0);
}

void sb_conv_from_latin9(sb_t *sb, const void *s, int len)
{
    __from_latinX_aux(sb, s, len, 0xc0);
}

int sb_conv_to_latin1(sb_t *sb, const void *data, int len, int rep)
{
    sb_t orig = *sb;
    const char *s = data, *end = s + len;

    while (s < end) {
        const char *p = s;

        while (s < end && !(*s & 0x80))
            s++;
        sb_add(sb, p, s - p);

        while (s < end && (*s & 0x80)) {
            int c = utf8_ngetc(s, end - s, &s);

            if (c < 0)
                return __sb_rewind_adds(sb, &orig);

            if (c >= 256) {
                if (rep < 0)
                    return __sb_rewind_adds(sb, &orig);
                if (!rep)
                    continue;
                c = rep;
            }
            sb_addc(sb, c);
        }
    }
    return 0;
}

/*[ CHECK ]::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::{{{*/
#ifdef CHECK

START_TEST(check_str_hexdecode)
{
    int res;
    byte dest[BUFSIZ];

    const char *encoded = "30313233";
    const char *decoded = "0123";

    res = strconv_hexdecode(dest, sizeof(dest), encoded, -1);

    fail_if((size_t)res != strlen(encoded) / 2, "str_hexdecode returned bad"
            "length: expected %zd, got %d.", strlen(encoded) / 2, res);
    fail_if(memcmp(dest, decoded, res), "str_hexdecode failed decoding");

    encoded = "1234567";
    fail_if(strconv_hexdecode(dest, sizeof(dest), encoded, -1) >= 0,
            "str_hexdecode should not accept odd-length strings");
    encoded = "1234567X";
    fail_if(strconv_hexdecode(dest, sizeof(dest), encoded, -1) >= 0,
            "str_hexdecode accepted non hexadecimal string");
}
END_TEST

Suite *check_make_strconv_suite(void)
{
    Suite *s  = suite_create("Strconv");
    TCase *tc = tcase_create("Core");

    suite_add_tcase(s, tc);
    tcase_add_test(tc, check_str_hexdecode);

    return s;
}

#endif
/*:::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::}}}*/
