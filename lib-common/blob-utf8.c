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

#include "all.h"

/* This (unused) macro implements UNICODE to UTF-8 transcoding */
#define UNICODE_TO_UTF8(x)     \
                 ((x) < 0x007F ? (x) : \
                  (x) < 0x0FFF ? ((0xC0 | (((x) >> 6) & 0x3F)) << 0) | \
                                 ((0x80 | (((x) >> 0) & 0x3F)) << 8) : \
                  ((0xE0 | (((x) >> 12) & 0x1F)) <<  0) | \
                  ((0x80 | (((x) >>  6) & 0x3F)) <<  8) | \
                  ((0x80 | (((x) >>  0) & 0x3F)) << 16))

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

static uint32_t const __utf8_offs[6] = {
    0x00000000UL, 0x00003080UL, 0x000e2080UL,
    0x03c82080UL, 0xfa082080UL, 0x82082080UL
};

static uint8_t const __utf8_mark[7] = {
    0x00, 0x00, 0xc0, 0xe0, 0xf0, 0xf8, 0xfc
};

/* OG: this function could return the number of bytes instead of bool */
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

/* OG: this algorithm assumes s points to a well formed utf8 character.
 * It tests for end of string in the middle of the utf8 encoding, but
 * will not detect end of string at s (*s == '\0')
 * We should split this into an inline function for the ASCII subset
 * and a generic function for the complete treatment.
 */
int utf8_getc(const char *s, const char **outp)
{
    uint32_t ret = 0;
    int trail = __utf8_trail[(unsigned char)*s];

    switch (trail) {
      default: return -1;
      case 3: ret += (unsigned char)*s++; ret <<= 6; if (!*s) return -1;
      case 2: ret += (unsigned char)*s++; ret <<= 6; if (!*s) return -1;
      case 1: ret += (unsigned char)*s++; ret <<= 6; if (!*s) return -1;
      case 0: ret += (unsigned char)*s++;
    }

    if (*outp) {
        *outp = s;
    }

    return ret - __utf8_offs[trail];
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

static const char * const __cp1252_or_latin9_to_utf8[0x40] = {
#define XXX  NULL
    /* cp1252 to utf8 */
    "€", XXX, "‚", "ƒ", "„", "…", "†", "‡", "ˆ", "‰", "Š", "‹", "Œ", XXX, "Ž", XXX,
    XXX, "‘", "’", "“", "”", "•", "–", "—", "˜", "™", "š", "›", "œ", XXX, "ž", "Ÿ",
    /* latin9 to utf8 if != latin1 */
    XXX, XXX, XXX, XXX, "€", XXX, "Š", XXX, "š", XXX, XXX, XXX, XXX, XXX, XXX, XXX,
    XXX, XXX, XXX, XXX, "Ž", XXX, XXX, XXX, "ž", XXX, XXX, XXX, "Œ", "œ", "Ÿ", XXX,
#undef XXX
};

/* OG: this function should be made faster for the ASCII subset */
static int blob_latin1_to_utf8_aux(blob_t *out, const char *s, int len,
                                       int limit)
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
            if (*s >= limit || !__cp1252_or_latin9_to_utf8[*s & 0x7f]) {
                blob_utf8_putc(out, *s);
            } else {
                blob_append_cstr(out, __cp1252_or_latin9_to_utf8[*s & 0x7f]);
            }
            s++;
        }
        res++;
    }

    return res;
}

int blob_latin1_to_utf8(blob_t *out, const char *s, int len)
{
    return blob_latin1_to_utf8_aux(out, s, len, 0xa0);
}

int blob_latin9_to_utf8(blob_t *out, const char *s, int len)
{
    return blob_latin1_to_utf8_aux(out, s, len, 0xc0);
}
