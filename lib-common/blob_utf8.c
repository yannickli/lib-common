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

#include "macros.h"
#include "strconv.h"
#include "blob.h"

static char const __utf8_trail[256] = {
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

static const uint8_t __utf8_mark[7] = {
    0x00, 0x00, 0xc0, 0xe0, 0xf0, 0xf8, 0xfc
};

static bool is_utf8_char(const char *s)
{
    int trail = __utf8_trail[(unsigned char)*s++];

    switch (trail) {
      case 3: if ((*s++ & 0xc0) != 0x80) return false;
      case 2: if ((*s++ & 0xc0) != 0x80) return false;
      case 1: if ((*s++ & 0xc0) != 0x80) return false;
      case 0: return true;

      default: return false;
    }
}

int blob_utf8_putc(blob_t *out, int c)
{
    int bytes = 1 + (c >= 0x80) + (c >= 0x800) + (c >= 0x10000);
    char *dst;

    blob_extend(out, bytes);
    dst = (char *)out->data + out->len - bytes;
    switch (bytes) {
        case 4: dst[3] = (c | 0x80) & 0xbf; c >>= 6;
        case 3: dst[2] = (c | 0x80) & 0xbf; c >>= 6;
        case 2: dst[1] = (c | 0x80) & 0xbf; c >>= 6;
        case 1: dst[0] = (c | __utf8_mark[bytes]);
    }

    return bytes;
}

static const char *__cp1252_to_utf8[0x20] = {
    "€", NULL, "‚", "ƒ", "„", "…", "†", "‡", "ˆ", "‰", "Š", "‹", "Œ", NULL, "Ž", NULL,
    NULL, "‘", "’", "“", "”", "•", "–", "—", "˜", "™", "š", "›", "œ", NULL, "ž", "Ÿ",
};

ssize_t blob_latin1_to_utf8(blob_t *out, const char *s, int len)
{
    int res = 0;
    const char *end = s + len;

    while (*s && (len < 0 || s < end)) {
        if (is_utf8_char(s)) {
            int trail = __utf8_trail[(unsigned char)*s];
            blob_append_data(out, s, trail + 1);
            s += trail + 1;
        } else {
            /* assume its cp1252 or latin1 */
            if (*s >= 0xa0 || !__cp1252_to_utf8[*s & 0x7f]) {
                blob_utf8_putc(out, *s);
            } else {
                blob_append_cstr(out, __cp1252_to_utf8[*s & 0x7f]);
            }
            s++;
        }
        res++;
    }

    return res;
}
