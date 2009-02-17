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

static unsigned char const __str_url_invalid[256] = {
#define REPEAT16(x)  x,x,x,x,x,x,x,x,x,x,x,x,x,x,x,x
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
#undef REPEAT16
};

static unsigned char const __decode_base64[256] = {
#define INV       255
#define REPEAT8   INV, INV, INV, INV, INV, INV, INV, INV
#define REPEAT16  REPEAT8, REPEAT8
    REPEAT16, REPEAT16,
    REPEAT8, INV, INV, INV, 62 /* + */, INV, INV, INV, 63 /* / */,
    52, 53, 54, 55, 56, 57, 58, 59, 60, 61, INV, INV, INV, INV, INV, INV,
    INV, 0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14 /* O */,
    15, 16, 17, 18, 19, 20, 21, 22, 23, 24, 25 /* Z */, INV, INV, INV, INV, INV,
    INV, 26, 27, 28, 29, 30, 31, 32, 33, 34, 35, 36, 37, 38, 39, 40 /* o */,
    41, 42, 43, 44, 45, 46, 47, 48, 49, 50, 51 /* z */, INV, INV, INV, INV, INV,
    REPEAT16, REPEAT16, REPEAT16, REPEAT16,
    REPEAT16, REPEAT16, REPEAT16, REPEAT16,
#undef REPEAT8
#undef REPEAT16
#undef INV
};

static byte const __str_encode_flags[256] = {
#define REPEAT16(x)  x,x,x,x,x,x,x,x,x,x,x,x,x,x,x,x
#define QP  1
#define XP  2
    0,     0,     0,     0,     0,     0,     0,     0,
    0,     QP,    0,     0,     0,     0,     0,     0,      /* TAB */
    REPEAT16(0),
    0,     QP,    XP|QP, QP,    QP,    QP,    XP|QP, XP|QP,  /* "&' */
    QP,    QP,    QP,    QP,    QP,    QP,    0,     QP,     /* . */
    QP,    QP,    QP,    QP,    QP,    QP,    QP,    QP,
    QP,    QP,    QP,    QP,    XP|QP, 0,     XP|QP, QP,     /* <=> */
    QP,    QP,    QP,    QP,    QP,    QP,    QP,    QP,
    QP,    QP,    QP,    QP,    QP,    QP,    QP,    QP,
    QP,    QP,    QP,    QP,    QP,    QP,    QP,    QP,
    QP,    QP,    QP,    QP,    QP,    QP,    QP,    QP,
    QP,    QP,    QP,    QP,    QP,    QP,    QP,    QP,
    QP,    QP,    QP,    QP,    QP,    QP,    QP,    QP,
    QP,    QP,    QP,    QP,    QP,    QP,    QP,    QP,
    QP,    QP,    QP,    QP,    QP,    QP,    QP,    0,      /* DEL */
    REPEAT16(0), REPEAT16(0), REPEAT16(0), REPEAT16(0),
    REPEAT16(0), REPEAT16(0), REPEAT16(0), REPEAT16(0),
#undef REPEAT16
};

static int test_quoted_printable(int x) {
    return __str_encode_flags[x] & QP;
}

static int test_xml_printable(int x) {
    return !(__str_encode_flags[x] & XP);
}

#undef QP
#undef XP


void sb_add_slashes(sb_t *sb, const void *_data, int len,
                    const char *toesc, const char *esc)
{
    uint32_t buf[BITS_TO_ARRAY_LEN(uint32_t, 256)] = { 0, };
    uint8_t  repl[256];
    const byte *p = _data, *end = p + len;

    while (*toesc) {
        byte c = *toesc++;
        SET_BIT(buf, c);
        repl[c] = *esc++;
    }

    sb_grow(sb, len);
    while (p < end) {
        const byte *q = p;

        while (p < end && !TST_BIT(buf, *p))
            p++;
        sb_add(sb, q, p - q);

        while (p < end && TST_BIT(buf, *p)) {
            byte c = repl[*p++];

            if (c) {
                char *s = sb_growlen(sb, 2);
                s[0] = '\\';
                s[1] = c;
            }
        }
    }
}

void sb_add_urlencode(sb_t *sb, const void *_data, int len)
{
    const byte *p = _data, *end = p + len;

    sb_grow(sb, len);
    while (p < end) {
        const byte *q = p;

        while (p < end && __str_url_invalid[*p] != 255)
            p++;
        sb_add(sb, q, p - q);

        while (p < end && __str_url_invalid[*p] == 255) {
            char *s = sb_growlen(sb, 3);
            s[0] = '%';
            s[1] = __str_digits_upper[(*p >> 4) & 0xf];
            s[2] = __str_digits_upper[(*p >> 0) & 0xf];
            p++;
        }
    }
}

void sb_add_hex(sb_t *sb, const void *data, int len)
{
    char *s = sb_growlen(sb, len * 2);

    for (const byte *p = data, *end = p + len; p < end; p++) {
        *s++ = __str_digits_upper[(*p >> 4) & 0x0f];
        *s++ = __str_digits_upper[(*p >> 0) & 0x0f];
    }
}

int  sb_add_unhex(sb_t *sb, const void *data, int len)
{
    sb_t orig = *sb;
    char *s;

    if (unlikely(len & 1))
        return -1;

    s = sb_growlen(sb, len / 2);
    for (const char *p = data, *end = p + len; p < end; p += 2) {
        int c = hexdecode(p);

        if (unlikely(c < 0)) {
            __sb_rewind_adds(sb, &orig);
            return -1;
        }
        *s++ = c;
    }
    return 0;
}

void sb_add_xmlescape(sb_t *sb, const void *data, int len)
{
    const byte *p = data, *end = p + len;

    sb_grow(sb, len);
    while (p < end) {
        const byte *q = p;

        while (p < end && test_xml_printable(*p))
            p++;
        sb_add(sb, q, p - q);

        while (p < end && !test_xml_printable(*p)) {
            switch (*p) {
              case '&':  sb_adds(sb, "&amp;"); break;
              case '<':  sb_adds(sb, "&lt;");  break;
              case '>':  sb_adds(sb, "&gt;");  break;
              case '\'': sb_adds(sb, "&#39;"); break;
              case '"':  sb_adds(sb, "&#34;"); break;
              default:
                 /* should not be triggered */
                 sb_addf(sb, "&#%d;", *p);
                 break;
            }
            p++;
        }
    }
}

/* OG: should take width as a parameter */
void sb_add_qpe(sb_t *sb, const void *data, int len)
{
    int i, j, c, col;
    const byte *src = data;

    sb_grow(sb, len);
    for (col = i = j = 0; i < len; i++) {
        if (col + i - j >= 75) {
            sb_add(sb, src + j, 75 - col);
            sb_adds(sb, "=\r\n");
            j  += 75 - col;
            col = 0;
        }
        if (!test_quoted_printable(c = src[i])) {
            /* only encode '.' if at the beginning of a line */
            if (c == '.' && i == j && col)
                continue;
            /* encode spaces only at end on line */
            if (isblank(c) && (i < len - 1 && src[i + 1] != '\n')) {
                continue;
            }
            sb_add(sb, src + j, i - j);
            col += i - j;
            if (c == '\n') {
                sb_adds(sb, "\r\n");
                col = 0;
            } else {
                char *s;
                if (col > 75 - 3) {
                    sb_adds(sb, "=\r\n");
                    col = 0;
                }
                s = sb_growlen(sb, 3);
                s[0] = '=';
                s[1] = __str_digits_upper[(c >> 4) & 0xf];
                s[2] = __str_digits_upper[(c >> 0) & 0xf];
                col += 3;
            }
            j = i + 1;
        }
    }
    sb_add(sb, src + j, i - j);
}

void sb_add_unqpe(sb_t *sb, const void *data, int len)
{
    const byte *p = data, *end = p + len;
    sb_grow(sb, len);

    while (p < end) {
        const byte *q = p;

        while (p < end && *p != '=' && *p != '\r' && *p)
            p++;
        sb_add(sb, q, p - q);

        if (p >= end)
            return;
        switch (*p++) {
          case '\0':
            return;

          case '=':
            if (end - p < 2) {
                sb_addc(sb, '=');
            } else
            if (p[0] == '\r' && p[1] == '\n') {
                p += 2;
            } else {
                int c = hexdecode((const char *)p);
                if (c < 0) {
                    sb_addc(sb, '=');
                } else {
                    sb_addc(sb, c);
                    p += 2;
                }
            }
            break;

          case '\r':
            if (p < end && *p == '\n') {
                sb_addc(sb, *p++);
            } else {
                sb_addc(sb, '\r');
            }
            break;
        }
    }
}

int sb_add_unb64(sb_t *sb, const void *data, int len)
{
    const byte *src = data, *end = src + len;
    sb_t orig = *sb;

    while (src < end) {
        byte in[4];
        int ilen = 0;
        char *s;

        while (ilen < 4 && src < end) {
            int c = *src++;

            if (isspace(c))
                continue;

            /*
             * '=' must be at the end, they can only be 1 or 2 of them
             * IOW we must have 2 or 3 fill `in` bytes already.
             *
             * And after them we can only have spaces. No data.
             *
             */
            if (c == '=') {
                if (ilen < 2)
                    goto error;
                if (ilen == 2) {
                    while (src < end && isspace(*src))
                        src++;
                    if (src >= end || *src++ != '=')
                        goto error;
                }
                while (src < end) {
                    if (!isspace(*src++))
                        goto error;
                }

                s = sb_growlen(sb, ilen - 1);
                if (ilen == 3) {
                    s[1] = (in[1] << 4) | (in[2] >> 2);
                }
                s[0] = (in[0] << 2) | (in[1] >> 4);
                return 0;
            }

            in[ilen++] = c = __decode_base64[c];
            if (unlikely(c == 255))
                goto error;
        }

        if (ilen == 0)
            return 0;

        if (ilen != 4)
            goto error;

        s    = sb_growlen(sb, 3);
        s[0] = (in[0] << 2) | (in[1] >> 4);
        s[1] = (in[1] << 4) | (in[2] >> 2);
        s[2] = (in[2] << 6) | (in[3] >> 0);
    }
    return 0;

error:
    __sb_rewind_adds(sb, &orig);
    return -1;
}
