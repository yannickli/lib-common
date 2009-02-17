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
