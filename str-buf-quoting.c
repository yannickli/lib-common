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

#include "core.h"

static const char __b64[] =
    "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";

static unsigned char const __str_url_invalid[256] = {
#define REPEAT16(x)  x,x,x,x,x,x,x,x,x,x,x,x,x,x,x,x
    REPEAT16(255), REPEAT16(255),
    255, 255, 255, 255, 255, 255, 255, 255,
    255, 255, '*', 255, 255, '-', '.', '/',
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
#define XP  2       /* Obviously, means "non XML printable char". */
    XP,    XP,    XP,    XP,    XP,    XP,    XP,    XP,
    XP,    0,     0,     XP,    XP,    0,     XP,    XP,     /* \n \t \r */
    REPEAT16(XP),
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

    if (!TST_BIT(buf, '\\')) {
        SET_BIT(buf, '\\');
        repl['\\'] = '\\';
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

void sb_add_unslashes(sb_t *sb, const void *_data, int len,
                      const char *tounesc, const char *unesc)
{
    uint32_t buf[BITS_TO_ARRAY_LEN(uint32_t, 256)] = { 0, };
    uint8_t  repl[256];
    const byte *p = _data, *end = p + len;

    while (*tounesc) {
        byte c = *tounesc++;
        SET_BIT(buf, c);
        repl[c] = *unesc++;
    }

    if (!TST_BIT(buf, '\\')) {
        SET_BIT(buf, '\\');
        repl['\\'] = '\\';
    }

    while (p < end) {
        const byte *q = p;

        /* -1 so that we always have a char after \ */
        p = memchr(p, '\\', end - p - 1);
        if (!p) {
            p = q;
            break;
        }
        sb_add(sb, q, p - q);

        if (TST_BIT(buf, p[1])) {
            sb_addc(sb, repl[*++p]);
        } else {
            sb_addc(sb, '\\');
        }
        p++;
    }
    sb_add(sb, p, end - p);
}

static char const __c_unescape[] = {
    ['a'] = '\a', ['b'] = '\b', ['e'] = '\e', ['t'] = '\t', ['n'] = '\n',
    ['v'] = '\v', ['f'] = '\f', ['r'] = '\r', ['\\'] = '\\',
    ['"'] = '"',  ['\''] = '\''
};

void sb_add_unquoted(sb_t *sb, const void *data, int len)
{
    const char *p = data, *end = p + len;

    while (p < end) {
        const char *q = p;
        int c;

        p = memchr(q, '\\', end - q);
        if (!p) {
            sb_add(sb, q, end - q);
            return;
        }
        sb_add(sb, q, p - q);
        p += 1;

        if (p == end) {
            sb_addc(sb, '\\');
            return;
        }

        switch (*p) {
          case 'a': case 'b': case 'e': case 't': case 'n': case 'v':
          case 'f': case 'r': case '\\': case '"': case '\'':
            sb_addc(sb, __c_unescape[(unsigned char)*p++]);
            continue;

          case '0'...'7':
            c = *p++ - '0';
            if (p < end && *p >= '0' && *p <= '7')
                c = (c << 3) + *p++ - '0';
            if (c < '\040' && p < end && *p >= '0' && *p <= '7')
                c = (c << 3) + *p++ - '0';
            sb_addc(sb, c);
            continue;

          case 'x':
            if (end - p < 3)
                break;
            c = hexdecode(p + 1);
            if (c < 0)
                break;
            p += 3;
            sb_addc(sb, c);
            continue;

          case 'u':
            if (end - p < 5)
                break;
            c = (hexdecode(p + 1) << 8) | hexdecode(p + 3);
            if (c < 0)
                break;
            p += 5;
            sb_adduc(sb, c);
            continue;
        }
        sb_addc(sb, '\\');
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

void sb_add_urldecode(sb_t *sb, const void *data, int len)
{
    const char *p = data, *end = p + len;

    for (;;) {
        const char *q = p;
        int c;

        p = memchr(q, '%', end - q);
        if (!p) {
            sb_add(sb, q, end - q);
            return;
        }
        sb_add(sb, q, p - q);

        if (end - p < 3) {
            sb_addc(sb, *p++);
            continue;
        }
        c = hexdecode(p + 1);
        if (c < 0) {
            sb_addc(sb, *p++);
            continue;
        }
        sb_addc(sb, c);
        p += 3;
    }
}

void sb_urldecode(sb_t *sb)
{
    const char *tmp, *r, *end = sb_end(sb);
    char *w;

    r = w = memchr(sb->data, '%', sb->len);
    if (!r)
        return;

    for (;;) {
        int c;

        if (end - r < 3) {
            *w++ = *r++;
            continue;
        }
        c = hexdecode(r + 1);
        if (c < 0) {
            *w++ = *r++;
            continue;
        }
        *w++ = c;
        r   += 3;

        r = memchr(tmp = r, '%', end - r);
        if (!r) {
            memmove(w, tmp, end - tmp);
            w += end - tmp;
            __sb_fixlen(sb, w - sb->data);
            return;
        }
        memmove(w, tmp, r - tmp);
        w += r - tmp;
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

        if (unlikely(c < 0))
            return __sb_rewind_adds(sb, &orig);
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
                 /* Invalid XML1.0 characters, we skip this one. */
                 break;
            }
            p++;
        }
    }
}

int sb_add_xmlunescape(sb_t *sb, const void *data, int len)
{
    sb_t orig = *sb;
    const char *p = data, *end = p + len;

    while (p < end) {
        const char *q = p;
        const char *semi;

        while (p < end && *p != '<' && *p != '&')
            p++;
        sb_add(sb, q, p - q);

        if (p == end)
            return 0;

        if (*p++ == '<') {
            /* strip out comments */
            if (p + 3 <= end && !memcmp(p, "!--", 3)) {
                p += 3;
                for (;;) {
                    p = memchr(p, '-', end - p);
                    if (!p || p + 3 > end)
                        goto error;
                    if (!memcmp(p, "-->", 3)) {
                        p += 3;
                        break;
                    }
                    p++;
                }
                continue;
            }

            /* extract CDATA stuff */
            if (p + 8 <= end && !memcmp(p, "![CDATA[", 8)) {
                p += 8;
                for (q = p;;) {
                    p = memchr(p, ']', end - p);
                    if (!p || p + 3 > end)
                        goto error;
                    if (!memcmp(p, "]]>", 3)) {
                        sb_add(sb, q, p - q);
                        p += 3;
                        break;
                    }
                    p++;
                }
                continue;
            }
            goto error;
        }

        /* entities: we have (p[-1] == '&') */
        semi = memchr(p, ';', end - p);
        if (!semi || p + 1 > semi)
            goto error;

        if (*p == '#') {
            int c = 0;

            if (semi - p > 7)
                goto error;

            if (p[1] == 'x') {
                for (p += 2; p < semi; p++) {
                    c = (c << 4) | hexdigit(*p);
                }
                if (c < 0)
                    goto error;
            } else {
                c = memtoip(p + 1, semi - p - 1, (const byte **)&p);
                if (p != semi)
                    goto error;
            }
            sb_adduc(sb, c);
        } else {
            /* &lt; &gt; &apos; &amp; &quot */
            switch (semi - p) {
              case 2:
                if (!memcmp(p, "lt", 2)) {
                    sb_addc(sb, '<');
                    break;
                }
                if (!memcmp(p, "gt", 2)) {
                    sb_addc(sb, '>');
                    break;
                }
                goto error;

              case 3:
                if (!memcmp(p, "amp", 3)) {
                    sb_addc(sb, '&');
                    break;
                }
                goto error;

              case 4:
                if (!memcmp(p, "apos", 4)) {
                    sb_addc(sb, '\'');
                    break;
                }
                if (!memcmp(p, "quot", 4)) {
                    sb_addc(sb, '"');
                    break;
                }
                goto error;

              default:
                goto error;
            }
        }

        p = semi + 1;
    }
    return 0;

  error:
    return __sb_rewind_adds(sb, &orig);
}

/* OG: should take width as a parameter */
void sb_add_qpe(sb_t *sb, const void *data, int len)
{
    int c, i = 0, j = 0, col = 0;
    const byte *src = data;

    sb_grow(sb, len);
    while (i < len) {
        if (col + i - j >= 75) {
            sb_add(sb, src + j, 75 - col);
            sb_adds(sb, "=\r\n");
            j  += 75 - col;
            col = 0;
        }
        if (test_quoted_printable(c = src[i++]))
            continue;
        /* only encode '.' if at the beginning of a line */
        if (c == '.' && col)
            continue;
        /* encode spaces only at end on line */
        if (isblank(c)) {
            if (!(i + 2 <= len && !memcmp(src + i, "\r\n", 2)))
                continue;
        }
        /* \r\n remain the same and reset col to 0 */
        if (c == '\r' && i + 1 <= len && src[i] == '\n') {
            i++;
            sb_add(sb, src + j, i - j);
            col = 0;
        } else {
            char *s;

            sb_add(sb, src + j, i - 1 - j);
            col += i - 1 - j ;
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
        j = i;
    }
    sb_add(sb, src + j, i - j);
    if (sb->data[sb->len - 1] != '\n')
        sb_adds(sb, "=\r\n");
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

/* computes a slightly overestimated size to write srclen bytes with `ppline`
 * packs per line, not knowing if we start at column 0 or not.
 *
 * The over-estimate is of at most 4 bytes, we can live with that.
 */
static int b64_rough_size(int srclen, int ppline)
{
    int nbpacks = ((srclen + 2) / 3);

    if (ppline < 0)
        return 4 * nbpacks;
    /*
     * Worst case is we're at `4 * ppline` column so we have to add a \r\n
     * straight away plus what is need for the rest.
     */
    return 4 * nbpacks + 2 + 2 * DIV_ROUND_UP(nbpacks, ppline);
}

void sb_add_b64_start(sb_t *dst, int len, int width, sb_b64_ctx_t *ctx)
{
    prefetch(__b64);
    p_clear(ctx, 1);
    if (width == 0) {
        width = 19; /* 76 characters + \r\n */
    } else {
        /* XXX: >>2 keeps the sign, unlike /4 */
        width >>= 2;
    }
    ctx->packs_per_line = width;
    sb_grow(dst, b64_rough_size(len, width));
}

void sb_add_b64_update(sb_t *dst, const void *src0, int len, sb_b64_ctx_t *ctx)
{
    short ppline    = ctx->packs_per_line;
    short pack_num  = ctx->pack_num;
    const byte *src = src0;
    const byte *end = src + len;
    unsigned pack;
    char *data;

    if (ctx->trail_len + len < 3) {
        memcpy(ctx->trail + ctx->trail_len, src, len);
        ctx->trail_len += len;
        return;
    }

    data = sb_grow(dst, b64_rough_size(ctx->trail_len + len, ppline));

    if (ctx->trail_len) {
        pack  = ctx->trail[0] << 16;
        pack |= (ctx->trail_len == 2 ? ctx->trail[1] : *src++) << 8;
        pack |= *src++;
        goto initialized;
    }

    do {
        pack  = *src++ << 16;
        pack |= *src++ <<  8;
        pack |= *src++ <<  0;

      initialized:
        *data++ = __b64[(pack >> (3 * 6)) & 0x3f];
        *data++ = __b64[(pack >> (2 * 6)) & 0x3f];
        *data++ = __b64[(pack >> (1 * 6)) & 0x3f];
        *data++ = __b64[(pack >> (0 * 6)) & 0x3f];

        if (ppline > 0 && ++pack_num >= ppline) {
            pack_num = 0;
            *data++ = '\r';
            *data++ = '\n';
        }
    } while (src + 3 <= end);

    memcpy(ctx->trail, src, end - src);
    ctx->trail_len = end - src;
    ctx->pack_num  = pack_num;
    __sb_fixlen(dst, data - dst->data);
}

void sb_add_b64_finish(sb_t *dst, sb_b64_ctx_t *ctx)
{
    if (ctx->trail_len) {
        unsigned c1 = ctx->trail[0];
        unsigned c2 = ctx->trail_len == 2 ? ctx->trail[1] : 0;
        char *data  = sb_growlen(dst, 4);

        *data++ = __b64[c1 >> 2];
        *data++ = __b64[((c1 << 4) | (c2 >> 4)) & 0x3f];
        *data++ = ctx->trail_len == 2 ? __b64[(c2 << 2) & 0x3f] : '=';
        *data++ = '=';
    }
    if (ctx->packs_per_line > 0 && ctx->pack_num != 0) {
        ctx->pack_num = 0;
        sb_adds(dst, "\r\n");
    }
    ctx->trail_len = 0;
}

/* width is the maximum length for output lines, not counting end of
 * line markers.  0 for standard 76 character lines, -1 for unlimited
 * line length.
 */
void sb_add_b64(sb_t *dst, const void *_src, int len, int width)
{
    sb_b64_ctx_t ctx;

    sb_add_b64_start(dst, len, width, &ctx);
    sb_add_b64_update(dst, _src, len, &ctx);
    sb_add_b64_finish(dst, &ctx);
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
    return __sb_rewind_adds(sb, &orig);
}

void sb_add_csvescape(sb_t *sb, const void *data, int len)
{
    static bool first_call = true;
    static ctype_desc_t ctype_needs_escape;
    pstream_t ps = ps_init(data, len);
    pstream_t cspan;

    if (unlikely(first_call)) {
        ctype_desc_build(&ctype_needs_escape, "\"\n\r;");
        first_call = false;
    }

    cspan = ps_get_cspan(&ps, &ctype_needs_escape);
    if (ps_done(&ps)) {
        /* No caracter needing escaping was found, just copy the input
         * string. */
        sb_add(sb, data, len);
        return;
    }

    /* TODO: use ps_get_ps_chr_and_skip and sb_add_ps in earlier versions of
     *       the lib-common.
     */

    /* There is at least one special character found in the input string, so
     * the whole string has to be double-quoted, and the double-quotes have
     * to be escaped by double-quotes.
     */
    sb_grow(sb, len + 2);
    sb_addc(sb, '"');
    sb_add(sb, cspan.s, ps_len(&cspan));

    while (!ps_done(&ps)) {
        if (ps_get_ps_chr(&ps, '"', &cspan) < 0) {
            sb_add(sb, ps.s, ps_len(&ps));
            break;
        }
        __ps_skip(&ps, 1);
        cspan.s_end++;
        sb_add(sb, cspan.s, ps_len(&cspan));
        sb_addc(sb, '"');
    }

    sb_addc(sb, '"');
}

/*{{{ Tests */

TEST_DECL("sb_add_csvescape", 0)
{
    SB_1k(sb);
    int test_id = 0;

#define CHECK(_str, _expected)  \
    do {                                                                     \
        sb_adds_csvescape(&sb, _str);                                        \
        test_id++;                                                           \
        TEST_FAIL_IF(strncmp(_expected, sb.data, sb.len),                    \
                     "test %d: expected `%s`, got `%s`",                     \
                     test_id, _expected, sb.data);                           \
        sb_reset(&sb);                                                       \
    } while (0)

    CHECK("toto", "toto");
    CHECK("toto;tata", "\"toto;tata\"");
    CHECK("toto\"tata", "\"toto\"\"tata\"");
    CHECK("toto\n", "\"toto\n\"");
    CHECK("toto\"", "\"toto\"\"\"");
    CHECK("toto\ntata", "\"toto\ntata\"");
    CHECK("toto\n\"tata", "\"toto\n\"\"tata\"");
    CHECK("toto\"\ntata", "\"toto\"\"\ntata\"");
    CHECK("", "");
    CHECK("\"", "\"\"\"\"");

    TEST_DONE();
#undef CHECK
}

/*}}}*/
