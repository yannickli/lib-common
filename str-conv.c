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

#include <string.h>

#include "str.h"
#include "str-conv.h"

static uint8_t const __utf8_mark[7] = {
    0x00, 0x00, 0xc0, 0xe0, 0xf0, 0xf8, 0xfc
};

char const __utf8_trail[256] = {
#define X (-1)
    0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0, 0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
    0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0, 0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
    0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0, 0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
    0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0, 0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
    X,X,X,X,X,X,X,X,X,X,X,X,X,X,X,X, X,X,X,X,X,X,X,X,X,X,X,X,X,X,X,X,
    X,X,X,X,X,X,X,X,X,X,X,X,X,X,X,X, X,X,X,X,X,X,X,X,X,X,X,X,X,X,X,X,
    1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1, 1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,
    2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2, 3,3,3,3,3,3,3,3,X,X,X,X,X,X,X,X,
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

unsigned char const __str_url_invalid[128 + 256] = {
    REPEAT16(255), REPEAT16(255), REPEAT16(255), REPEAT16(255),
    REPEAT16(255), REPEAT16(255), REPEAT16(255), REPEAT16(255),
    REPEAT16(255), REPEAT16(255),
    255, 255, 255, 255, 255, 255, 255, 255,
    255, 255, '*', 255, 255, '-', '.', 255,
    '0', '1', '2', '3', '4', '5', '6', '7',
    '8', '9', 255, 255, 255, 255, 255, 255,
    '@', 'A', 'B', 'C', 'D', 'E', 'F', 'G',
    'H', 'I', 'J', 'K', 'L', 'M', 'N', 'O',
    'P', 'Q', 'R', 'S', 'T', 'U', 'V', 'W',
    'X', 'Y', 'Z', 255, 255, 255, 255, '_',
    255, 'a', 'b', 'c', 'd', 'e', 'f', 'g',
    'h', 'i', 'j', 'k', 'l', 'm', 'n', 'o',
    'p', 'q', 'r', 's', 't', 'u', 'v', 'w',
    'x', 'y', 'z', 255, 255, 255, 255, 255,
    REPEAT16(255), REPEAT16(255), REPEAT16(255), REPEAT16(255),
    REPEAT16(255), REPEAT16(255), REPEAT16(255), REPEAT16(255),
};

char const __str_digits_upper[36] =
    "0123456789ABCDEFGHIJKLMNOPQRSTUVWXYZ";
char const __str_digits_lower[36] =
    "0123456789abcdefghijklmnopqrstuvwxyz";

static unsigned char const __str_escape_value[128 + 256] = {
#define REPEAT16(x)  x,x,x,x,x,x,x,x,x,x,x,x,x,x,x,x
    REPEAT16(255), REPEAT16(255), REPEAT16(255), REPEAT16(255),
    REPEAT16(255), REPEAT16(255), REPEAT16(255), REPEAT16(255),
    REPEAT16(255), REPEAT16(255),
    255, 255, 255, 255, 255, 255, 255, 255,
    255, 255, '*', '+', 255, '-', '.', '/',
    '0', '1', '2', '3', '4', '5', '6', '7',
    '8', '9', 255, 255, 255, 255, 255, 255,
    '@', 'A', 'B', 'C', 'D', 'E', 'F', 'G',
    'H', 'I', 'J', 'K', 'L', 'M', 'N', 'O',
    'P', 'Q', 'R', 'S', 'T', 'U', 'V', 'W',
    'X', 'Y', 'Z', 255, 255, 255, 255, '_',
    255, 'a', 'b', 'c', 'd', 'e', 'f', 'g',
    'h', 'i', 'j', 'k', 'l', 'm', 'n', 'o',
    'p', 'q', 'r', 's', 't', 'u', 'v', 'w',
    'x', 'y', 'z', 255, 255, 255, 255, 255,
    REPEAT16(255), REPEAT16(255), REPEAT16(255), REPEAT16(255),
    REPEAT16(255), REPEAT16(255), REPEAT16(255), REPEAT16(255),
};

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

static inline int str_escape_value(int x) {
    return __str_escape_value[x + 128];
}

/* String URL-escape characters */
int strconv_escape(char *dest, int size, const char *src, int len)
{
    int i, j;

    if (len < 0) {
        len = strlen(src);
    }

    for (i = j = 0; i < len; i++) {
        int c = src[i], cc;

        if (c <= 0xFF) {
            if ((cc = str_escape_value(c)) < 255) {
                if (j < size)
                    dest[j] = cc;
                j++;
            } else {
                if (j <= size - 3) {
                    dest[j + 0] = '%';
                    dest[j + 1] = __str_digits_upper[(c >> 4) & 15];
                    dest[j + 2] = __str_digits_upper[(c >> 0) & 15];
                }
                j += 3;
            }
        } else {
            if (j <= size - 6) {
                dest[j + 0] = '%';
                dest[j + 1] = 'u';
                dest[j + 2] = __str_digits_upper[(c >> 12) & 15];
                dest[j + 3] = __str_digits_upper[(c >>  8) & 15];
                dest[j + 4] = __str_digits_upper[(c >>  4) & 15];
                dest[j + 5] = __str_digits_upper[(c >>  0) & 15];
            }
            j += 6;
        }
    }
    if (j < size) {
        dest[j] = '\0';
    }
    return j;
}

/* A helper function for unescape(). */
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

int strconv_unescape(char *dest, int size, const char *src, int len)
{
    int i, j;

    if (len < 0) {
        len = strlen(src);
    }

    for (i = j = 0; i < len;) {
        int c = src[i];

        if (c != '%') {
            i += 1;
        } else
        if (i + 6 <= len && src[i + 1] == 'u'
        &&  str_scanhexdigits(src + i + 2, 4, 4, &c)) {
            i += 6;
        } else
        if (i + 3 <= len
        &&  str_scanhexdigits(src + i + 1, 2, 2, &c)) {
            i += 3;
        } else {
            c = src[i];
            i += 1;
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

int strconv_quote(char *dest, int size, const char *src, int len, int delim)
{
    int i, j;

    if (len < 0) {
        len = strlen(src);
    }

    for (i = j = 0; i < len; i++) {
        int c = src[i];

        switch (c) {
        case '\f':
            c = 'f';
            break;
        case '\n':
            c = 'n';
            break;
        case '\r':
            c = 'r';
            break;
        case '\t':
            c = 't';
            break;
        case '\v':
            c = 'v';
            break;
        case '\b':
            c = 'b';
            break;
        case '\a':
            c = 'a';
            break;
        case '\\':
            break;
        case '\'':
        case '\"':
            if (!delim || c == delim)
                goto escape;
            else
                goto normal;
        case 0:
            if (i == len - 1 || str_digit_value(src[i + 1]) >= 8) {
                c = '0';
                break;
            }
            /* FALL THRU */
        default:
            if (c == delim)
                goto escape;

            if (c < ' ' || (c >= 127 && c <= 255)) {
                /* Do not use octal except for \0 */
                if (j <= size - 4) {
                    dest[j + 0] = '\\';
                    dest[j + 1] = 'x';
                    dest[j + 2] = __str_digits_upper[(c >> 4) & 15];
                    dest[j + 3] = __str_digits_upper[(c >> 0) & 15];
                }
                j += 4;
                continue;
            } else
            if (c > 255) {
                if (j <= size - 6) {
                    dest[j + 0] = '\\';
                    dest[j + 1] = 'u';
                    dest[j + 2] = __str_digits_upper[(c >> 12) & 15];
                    dest[j + 3] = __str_digits_upper[(c >>  8) & 15];
                    dest[j + 4] = __str_digits_upper[(c >>  4) & 15];
                    dest[j + 5] = __str_digits_upper[(c >>  0) & 15];
                }
                j += 6;
                continue;
            } else {
              normal:
                if (j < size) {
                    dest[j] = c;
                }
                j++;
                continue;
            }
        }
      escape:
        if (j <= size - 2) {
            dest[j] = '\\';
            dest[j + 1] = c;
        }
        j += 2;
    }
    if (j < size) {
        dest[j] = '\0';
    }
    return j;
}

int strconv_quote_char(char *dest, int size, int c, int delim)
{
    /* TODO: take flag for special case '\0' */
    int ok_to_encode_0_as_backslash_0 = 1;
    int j = 0;

    switch (c) {
    case '\f':
        c = 'f';
        goto escape;
    case '\n':
        c = 'n';
        goto escape;
    case '\r':
        c = 'r';
        goto escape;
    case '\t':
        c = 't';
        goto escape;
    case '\v':
        c = 'v';
        goto escape;
    case '\b':
        c = 'b';
        goto escape;
    case '\a':
        c = 'a';
        goto escape;
    case '\\':
        goto escape;
    case '\'':
    case '\"':
        if (!delim || c == delim)
            goto escape;
        else
            goto normal;
    case 0:
        if (ok_to_encode_0_as_backslash_0) {
            c = '0';
            goto escape;
        }
        /* FALL THRU */
    default:
        if (c == delim)
            goto escape;

        if (c < ' ' || (c >= 127 && c <= 255)) {
            /* Do not use octal except for \0 */
            if (j <= size - 4) {
                dest[j + 0] = '\\';
                dest[j + 1] = 'x';
                dest[j + 2] = __str_digits_upper[(c >> 4) & 15];
                dest[j + 3] = __str_digits_upper[(c >> 0) & 15];
            }
            j += 4;
            break;
        } else
        if (c > 255) {
            if (j <= size - 6) {
                dest[j + 0] = '\\';
                dest[j + 1] = 'u';
                dest[j + 2] = __str_digits_upper[(c >> 12) & 15];
                dest[j + 3] = __str_digits_upper[(c >>  8) & 15];
                dest[j + 4] = __str_digits_upper[(c >>  4) & 15];
                dest[j + 5] = __str_digits_upper[(c >>  0) & 15];
            }
            j += 6;
            break;
        } else {
            goto normal;
        }
    normal:
        if (j < size) {
            dest[j] = c;
        }
        j++;
        break;
    escape:
        if (j <= size - 2) {
            dest[j] = '\\';
            dest[j + 1] = c;
        }
        j += 2;
        break;
    }
    if (j < size) {
        dest[j] = '\0';
    }
    return j;
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

int strconv_unquote_char(int *cp, const char *src, int len)
{
    int i, c;

    if (len < 0) {
        len = strlen(src);
    }

    if (len == 0) {
        *cp = 0;
        return 0;
    } else {
        /* TODO: share code with strconv_strunquote() */
        i = 0;
        c = src[i];
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
        *cp = c;
        return i;
    }
}

int str_utf8_putc(char *dst, int c)
{
    int bytes = 1 + (c >= 0x80) + (c >= 0x800) + (c >= 0x10000);

    switch (bytes) {
      case 4: dst[3] = (c | 0x80) & 0xbf; c >>= 6;
      case 3: dst[2] = (c | 0x80) & 0xbf; c >>= 6;
      case 2: dst[1] = (c | 0x80) & 0xbf; c >>= 6;
      case 1: dst[0] = (c | __utf8_mark[bytes]);
    }

    return bytes;
}

/*  unescape XML entities, returns decoded length, or -1 on error
 * The resulting string is not NUL-terminated
 **/
int strconv_xmlunescape(char *str, int len)
{
    char *wpos = str, *start, *end;
    int res_len = 0;

    if (len < 0) {
        len = strlen(str);
    }

    while (len > 0) {
        char c;

#if 0
        if (is_utf8_data(str, len)) {
            int bytes = 1 + (c >= 0x80) + (c >= 0x800) + (c >= 0x10000);

            memcpy(wpos, str, bytes);
            wpos += bytes;
            str += bytes;
            len -= bytes;
            res_len += bytes;
            continue;
        }
#endif

        c = *str++;
        len--;

#if 0
        if (c >= 0xa0) {
            if (!__cp1252_or_latin9_to_utf8[c & 0x7f]) {
                int bytes = str_utf8_putc(wpos, c);

                wpos += bytes;
                res_len += bytes;
                continue;
            } else {
                int bytes = strlen(__cp1252_or_latin9_to_utf8[c & 0x7f]);

                memcpy(wpos, __cp1252_or_latin9_to_utf8[c & 0x7f], bytes);
                wpos += bytes;
                str += bytes;
                len -= bytes;
                res_len += bytes;
                continue;
            }
        }
#endif

        if (c == '&') {
            int val;

            start = str;
            end = memchr(str, ';', len);
            if (!end) {
                e_trace(1, "Could not find entity end (%.*s%s)",
                        5, str, len > 5 ? "..." : "");
                goto error;
            }
            *end = '\0';

            c = *str;
            if (c == '#') {
                str++;
                if (*str == 'x') {
                    /* hexadecimal */
                    str++;
                    val = strtol(str, (const char **)&str, 16);
                } else {
                    /* decimal */
                    val = strtol(str, (const char **)&str, 10);
                }
                if (str != end) {
                    e_trace(1, "Bad numeric entity, got trailing char: '%c'", *str);
                    goto error;
                }
            } else {
                /* TODO: Support externally defined entities */
                /* textual entity */
                switch (c) {
                  case 'a':
                    if (!strcmp(str, "amp")) {
                        val = '&';
                    } else
                    if (!strcmp(str, "apos")) {
                        val = '\'';
                    } else {
                        e_trace(1, "Unsupported entity: %s", str);
                        goto error;
                    }
                    break;

                  case 'l':
                    if (!strcmp(str, "lt")) {
                        val = '<';
                    } else {
                        e_trace(1, "Unsupported entity: %s", str);
                        goto error;
                    }
                    break;

                  case 'g':
                    if (!strcmp(str, "gt")) {
                        val = '>';
                    } else {
                        e_trace(1, "Unsupported entity: %s", str);
                        goto error;
                    }
                    break;

                  case 'q':
                    if (!strcmp(str, "quot")) {
                        val = '"';
                    } else {
                        e_trace(1, "Unsupported entity: %s", str);
                        goto error;
                    }
                    break;

                  default:
                    /* invalid entity */
                    e_trace(1, "Unsupported entity: %s", str);
                    goto error;
                }
            }

            /* write unicode char */
            {
                int bytes = str_utf8_putc(wpos, val);

                wpos += bytes;
                res_len += bytes;
            }

            str = end + 1;
            len -= str - start;
            continue;
        }

        *wpos++ = c;
        res_len++;
    }

    return res_len;

  error:
    /* Cut string */
    *wpos = '\0';
    return -1;
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

START_TEST(check_str_xmlunescape)
{
    int res;
    char dest[BUFSIZ];

    const char *encoded = "3&lt;=8: Toto -&gt; titi &amp; "
        "t&#xE9;t&#232; : &quot;you&apos;re&quot;";
    const char *decoded = "3<=8: Toto -> titi & tétè : \"you're\"";
    const char *badencoded = "3&lt;=8: Toto -&tutu; titi &amp; "
        "t&#xE9;t&#232; : &quot;you&apos;re&quot;";
    const char *baddecoded = "3<=8: Toto -";

    pstrcpy(dest, sizeof(dest), encoded);

    res = strconv_xmlunescape(dest, -1);
    if (res >= 0) {
        dest[res] = '\0';
    }
    fail_if(res != sstrlen(decoded), "strconv_xmlunescape returned bad"
            "length: expected %zd, got %d. (%s)", strlen(decoded), res, dest);
    fail_if(strcmp(dest, decoded), "strconv_xmlunescape failed decoding");

    pstrcpy(dest, sizeof(dest), badencoded);

    res = strconv_xmlunescape(dest, -1);
    fail_if(res != -1, "strconv_xmlunescape did not detect error");
    fail_if(strcmp(dest, baddecoded), "strconv_xmlunescape failed to clean "
            "badly encoded str");
}
END_TEST

Suite *check_make_strconv_suite(void)
{
    Suite *s  = suite_create("Strconv");
    TCase *tc = tcase_create("Core");

    suite_add_tcase(s, tc);
    tcase_add_test(tc, check_str_hexdecode);
    tcase_add_test(tc, check_str_xmlunescape);

    return s;
}

#endif
/*:::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::}}}*/
