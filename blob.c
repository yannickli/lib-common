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

#include "blob.h"
#include "net.h"
#include "arith.h"

/**************************************************************************/
/* Blob string functions                                                  */
/**************************************************************************/

/*---------------- generic packing ----------------*/

static char *convert_int10(char *p, int value)
{
    /* compute absolute value without tests */
    unsigned int bits = value >> (bitsizeof(int) - 1);
    unsigned int num = (value ^ bits) + (bits & 1);

    while (num >= 10) {
        *--p = '0' + (num % 10);
        num = num / 10;
    }
    *--p = '0' + num;
    if (value < 0) {
        *--p = '-';
    }
    return p;
}

static char *convert_hex(char *p, unsigned int value)
{
    static const char hexdigits[16] = "0123456789abcdef";
    int i;

    for (i = 0; i < 8; i++) {
        *--p = hexdigits[value & 0xF];
        value >>= 4;
    }
    return p;
}

/* OG: essentially like strtoip */
static int parse_int10(const byte *data, const byte **datap)
{
    int neg;
    unsigned int digit, value = 0;

    neg = 0;
    while (isspace((unsigned char)*data))
        data++;
    if (*data == '-') {
        neg = 1;
        data++;
    } else
    if (*data == '+') {
        data++;
    }
    while ((digit = str_digit_value(*data)) < 10) {
        value = value * 10 + digit;
        data++;
    }
    *datap = data;
    return neg ? -value : value;
}

static int parse_hex(const byte *data, const byte **datap)
{
    unsigned int digit, value = 0;

    while (isspace((unsigned char)*data))
        data++;
    while ((digit = str_digit_value(*data)) < 16) {
        value = (value << 4) + digit;
        data++;
    }
    *datap = data;
    return value;
}

int blob_pack(sb_t *blob, const char *fmt, ...)
{
    char buf[(64 + 2) / 3 + 1 + 1];
    const char *p;
    va_list ap;
    int c, n = 0;

    va_start(ap, fmt);

    for (;;) {
        switch (c = *fmt++) {
        case '\0':
            break;
        case 'd':
            p = convert_int10(buf + sizeof(buf), va_arg(ap, int));
            sb_add(blob, p, buf + sizeof(buf) - p);
            continue;
        case 'l':
            sb_addf(blob, "%"PRIi64, va_arg(ap, int64_t));
            continue;
        case 'g':
            sb_addf(blob, "%g", va_arg(ap, double));
            continue;
        case 'x':
            p = convert_hex(buf + sizeof(buf), va_arg(ap, unsigned int));
            sb_add(blob, p, buf + sizeof(buf) - p);
            continue;
        case 's':
            p = va_arg(ap, const char *);
            sb_adds(blob, p);
            continue;
        case 'c':
            c = va_arg(ap, int);
            /* FALLTHRU */
        default:
            sb_addc(blob, c);
            continue;
        }
        break;
    }
    va_end(ap);

    return n;
}

/* OG: should change API:
 * - return 0 iff fmt is completely matched
 * - return -n or +n depending on mismatch type
 * - no update on pos if incomplete match
 * - change format syntax: use % to specify fields
 * - support more field specifiers (inline buffer, double, long...)
 * - in order to minimize porting effort, use a new name such as
 *   buf_parse, buf_match, buf_scan, buf_deserialize, ...
 */
static int buf_unpack_vfmt(const byte *buf, int buf_len,
                           int *pos, const char *fmt, va_list ap)
{
    const byte *data, *p, *q;
    int c, n = 0;

    if (buf_len < 0)
        buf_len = strlen((const char *)buf);

    if (*pos >= buf_len)
        return 0;

    /* OG: should limit scan to buf[0..buf_len] */
    data = buf + *pos;
    for (;;) {
        switch (c = *fmt++) {
            int64_t i64, *i64p;
            int ival, *ivalp;
            double dval, *dvalp;
            unsigned int uval, *uvalp;
            char **strvalp;

        case '\0':
            break;
        case 'd':
            ival = parse_int10(data, &data);
            ivalp = va_arg(ap, int *);
            if (ivalp) {
                *ivalp = ival;
            }
            n++;
            continue;
        case 'l':
            i64 = strtoll((const char *)data, (const char **)&data, 10);
            i64p = va_arg(ap, int64_t *);
            if (i64p) {
                *i64p = i64;
            }
            n++;
            continue;
        case 'g':
            dval = strtod((const char*)data, (char **)(void *)(&data));
            dvalp = va_arg(ap, double *);
            if (dvalp) {
                *dvalp = dval;
            }
            n++;
            continue;
        case 'x':
            uval = parse_hex(data, &data);
            uvalp = va_arg(ap, unsigned int *);
            if (uvalp) {
                *uvalp = uval;
            }
            n++;
            continue;
        case 'c':
            if (*data == '\0')
                break;

            ival = *data++;
            ivalp = va_arg(ap, int *);
            if (ivalp) {
                *ivalp = ival;
            }
            n++;
            continue;
        case 's':
            p = q = data;
            c = *fmt;
            for (;;) {
                if (*data == '\0' || *data == c || *data == '\n') {
                    q = data;
                    break;
                }
                if (*data == '\r' && data[1] == '\n') {
                    /* Match and skip \r before \n */
                    q = data;
                    data += 1;
                    break;
                }
                data++;
            }
            strvalp = va_arg(ap, char **);
            if (strvalp) {
                *strvalp = p_dupz(p, q - p);
            }
            n++;
            continue;
        case '\n':
            if (*data == '\r' && data[1] == '\n') {
                /* Match and skip \r before \n */
                data += 2;
                continue;
            }
            /* FALL THRU */
        default:
            if (c != *data)
                break;
            data++;
            continue;
        }
        break;
    }
    *pos = data - buf;

    return n;
}

int buf_unpack(const byte *buf, int buf_len,
               int *pos, const char *fmt, ...)
{
    va_list ap;
    int res;

    va_start(ap, fmt);
    res = buf_unpack_vfmt(buf, buf_len, pos, fmt, ap);
    va_end(ap);

    return res;
}

int blob_unpack(const sb_t *blob, int *pos, const char *fmt, ...)
{
    va_list ap;
    int res;

    va_start(ap, fmt);
    res = buf_unpack_vfmt((byte *)blob->data, blob->len, pos, fmt, ap);
    va_end(ap);

    return res;
}
