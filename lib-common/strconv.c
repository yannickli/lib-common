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

#include "strconv.h"

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

unsigned char const __str_escape_value[128 + 256] = {
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
