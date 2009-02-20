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
        sb_addc(out, c);
    }
    return 0;

error:
    blob_setlen(out, init_len);
    return -1;
}
int blob_utf8_to_latin1_n(blob_t *out, const char *s, int len, int rep)
{
    int init_len = out->len;
    const char *end = s + len;

    while (s < end) {
        const char *p = s;
        int c;

        while (*p > 0 && *p < 128 && p < end)
            p++;
        blob_append_data(out, s, p - s);
        s = p;

        if (s == end)
            break;

        c = utf8_ngetc(s, end - s, &s);
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
        sb_addc(out, c);
    }
    return 0;

error:
    blob_setlen(out, init_len);
    return -1;
}
