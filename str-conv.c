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

#include "core.h"

uint8_t const __utf8_mark[7] = {
    0x00, 0x00, 0xc0, 0xe0, 0xf0, 0xf8, 0xfc
};

uint8_t const __utf8_clz_to_charlen[31] = {
#define X  0
    1, 1, 1, 1, 1, 1, 1, /* <=  7 bits */
    2, 2, 2, 2,          /* <= 11 bits */
    3, 3, 3, 3, 3,       /* <= 16 bits */
    4, 4, 4, 4, 4,       /* <= 21 bits */
    X, X, X, X, X,       /* <= 26 bits */
    X, X, X, X, X,       /* <= 31 bits */
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

const char * const __cp1252_or_latin9_to_utf8[0x40] = {
#define XXX  NULL
    /* cp1252 to utf8 */
    "€", XXX, "‚", "ƒ", "„", "…", "†", "‡", "ˆ", "‰", "Š", "‹", "Œ", XXX, "Ž", XXX,
    XXX, "‘", "’", "“", "”", "•", "–", "—", "˜", "™", "š", "›", "œ", XXX, "ž", "Ÿ",
    /* latin9 to utf8 if != latin1 */
    XXX, XXX, XXX, XXX, "€", XXX, "Š", XXX, "š", XXX, XXX, XXX, XXX, XXX, XXX, XXX,
    XXX, XXX, XXX, XXX, "Ž", XXX, XXX, XXX, "ž", XXX, XXX, XXX, "Œ", "œ", "Ÿ", XXX,
#undef XXX
};


unsigned char const __str_digit_value[128 + 256] = {
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

char const __str_digits_upper[36] =
    "0123456789ABCDEFGHIJKLMNOPQRSTUVWXYZ";
char const __str_digits_lower[36] =
    "0123456789abcdefghijklmnopqrstuvwxyz";

int strconv_hexdecode(byte *dest, int size, const char *src, int len)
{
    int prev_len;

    if (len < 0) {
        len = strlen(src);
    }
    prev_len = len;

    if (len & 1) {
        /* Should accept and ignore white space in source string? */
        /* Alternatively, we could stop on first non hex character */
        return -1;
    }

    while (len && size > 0) {
        int val = hexdecode(src);

        if (val < 0) {
            return -1;
        }
        *dest++ = val;
        size--;
        src += 2;
        len -= 2;
    }

    return prev_len / 2;
}

int strconv_hexencode(char *dest, int size, const byte *src, int len)
{
    int prev_len;

    if (len < 0) {
        return -1;
    }
    prev_len = len;

    while (len && size > 1) {
        *dest++ = __str_digits_lower[(*src >> 4) & 15];
        *dest++ = __str_digits_lower[(*src++) & 15];
        size -= 2;
        len--;
    }

    if (size)
        *dest = '\0';
    else
        *--dest = '\0';

    return prev_len * 2;
}

static int str_scanhexdigits(const char *dp, int min, int max, int *cp)
{
    int val = 0;
    int i, digit;

    for (i = 0; i < max; i++) {
        if ((digit = str_digit_value(dp[i])) < 16) {
            val = (val << 4) + digit;
        } else {
            break;
        }
    }
    if (i < min) {
        return 0;
    } else {
        *cp = val;
        return i;
    }
}

int strconv_unquote(char *dest, int size, const char *src, int len)
{
    int i, j;

    if (len < 0) {
        len = strlen(src);
    }

    for (i = j = 0; i < len;) {
        int c = src[i];

        i += 1;
        if (c == '\\' && i < len) {
            c = src[i];
            i += 1;
            switch (c) {
            case 'f':
                c = '\f';
                break;
            case 'n':
                c = '\n';
                break;
            case 'r':
                c = '\r';
                break;
            case 't':
                c = '\t';
                break;
            case 'v':
                c = '\v';
                break;
            case 'b':
                c = '\b';
                break;
            case 'a':
                c = '\a';
                break;
            case '0'...'7':
                c = c - '0';
                if (i < len && src[i] >= '0' && src[i] <= '7') {
                    c = (c << 3) + src[i] - '0';
                    i++;
                    if (c < '\040' && i < len
                    &&  src[i] >= '0' && src[i] <= '7') {
                        c = (c << 3) + src[i] - '0';
                        i++;
                    }
                }
                break;
            case 'u':
                if (i + 4 <= len && str_scanhexdigits(src + i, 4, 4, &c))
                    i += 4;
                break;
            case 'x':
                if (i + 2 <= len && str_scanhexdigits(src + i, 2, 2, &c))
                    i += 2;
                break;
            default:
                break;
            }
        }
        if (j < size) {
            dest[j] = c;
        }
        j++;
    }
    if (j < size) {
        dest[j] = '\0';
    }
    return j;
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
