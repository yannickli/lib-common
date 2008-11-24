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

    blob_extend(out, bytes);
    return str_utf8_putc((char *)out->data + out->len - bytes, c);
}

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

int blob_utf8_to_latin1(blob_t *out, const char *s, int rep)
{
    int init_len = out->len;

    for (;;) {
        const char *p = s;
        int c;

        while (*p > 0 && *p < 128)
            p++;
        blob_append_data(out, s, p - s);
        s = p;

        c = utf8_getc(s, &s);
        if (c < 0)
            goto error;

        if (c == 0)
            break;

        if (c >= 256) {
            switch (rep) {
              case -1:
                goto error;
              case 0:
                continue;
              default:
                c = rep;
                break;
            }
        }
        blob_ensure(out, out->len + 1);
        out->data[out->len++] = c;
    }
    /* set len force invariant, blob is terminated with '\0' */
    blob_setlen(out, out->len);
    return 0;

error:
    blob_setlen(out, init_len);
    return -1;
}
