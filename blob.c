/**************************************************************************/
/*                                                                        */
/*  Copyright (C) 2004-2010 INTERSEC SAS                                  */
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

/*---------------- blob conversion stuff ----------------*/

void blob_append_date_iso8601(sb_t *dst, time_t date)
{
    struct tm t;

    gmtime_r(&date, &t);
    sb_grow(dst, 20);
    sb_addf(dst, "%04d-%02d-%02dT%02d:%02d:%02dZ",
            t.tm_year + 1900, t.tm_mon + 1, t.tm_mday,
            t.tm_hour, t.tm_min, t.tm_sec);
}

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

/* Serialize common objects to a blob with a printf-like syntax.
 * XXX: Serialize in little-endian order. As we usually run on
 * little-endian-ordered machines, serialization is a nop. Deserialization
 * could be a nop if we allow unaligned accesses.
 */
#define SERIALIZE_CHAR        1
#define SERIALIZE_INT         4
#define SERIALIZE_LONG_LONG   8
#define SERIALIZE_LONG_32   104
#define SERIALIZE_LONG_64   108
#define SERIALIZE_BINARY    150
#define SERIALIZE_STRING    151
int blob_serialize(sb_t *blob, const char *fmt, ...)
{
    va_list ap;
    int c, orig_len, val_len, val_len_net;
    char val_c;
    int val_d;
    long val_l;
    long long val_ll;
    const char *val_s;
    const byte *val_b;

    orig_len = blob->len;
    va_start(ap, fmt);

    for (;;) {
        switch (c = *fmt++) {
        case '\0':
            break;
        case '%':
            switch (c = *fmt++) {
              case '\0':
                goto error;
              case 'c':
                /* %c : single char.
                 * XXX: "char is promoted to int when passed through ..."
                 */
                val_d = va_arg(ap, int);
                val_c = val_d;
                sb_addc(blob, SERIALIZE_CHAR);
                sb_add(blob, &val_c, 1);
                continue;
              case 'l':
                if (fmt[0] == 'l' && fmt[1] == 'd') {
                    /* %lld: 64 bits int */
                    fmt += 2;
                    val_ll = va_arg(ap, long long);
                    val_ll = cpu_to_le64(val_ll);
                    sb_addc(blob, SERIALIZE_LONG_LONG);
                    sb_add(blob, &val_ll, sizeof(long long));
                } else if (fmt[0] == 'd') {
                    /* %l: long : 32 or 64 bytes depending on platform */
                    fmt++;
                    val_l = va_arg(ap, long);
                    switch (sizeof(long)) {
                      case 4:
                        val_l = cpu_to_le32(val_l);
                        sb_addc(blob, SERIALIZE_LONG_32);
                        break;
                      case 8:
                        val_l = cpu_to_le64(val_l);
                        sb_addc(blob, SERIALIZE_LONG_64);
                        break;
                      default:
                        goto error;
                    }
                    sb_add(blob, &val_l, sizeof(long));
                }
                continue;
              case 'd':
                /* %d: 32 bits int */
                val_d = va_arg(ap, int);
                val_d = cpu_to_le32(val_d);
                sb_addc(blob, SERIALIZE_INT);
                sb_add(blob, &val_d, sizeof(int));
                continue;
              case 's':
                /* "%s" : string, NIL-terminated */
                val_s = va_arg(ap, const char *);
                sb_addc(blob, SERIALIZE_STRING);
                sb_adds(blob, val_s);
                sb_addc(blob, '\0');
                continue;
              case '*':
                /* "%*p" : binary object (can include a \0) */
                if (fmt[0] != 'p') {
                    goto error;
                }
                fmt++;
                sb_addc(blob, SERIALIZE_BINARY);
                val_len = va_arg(ap, int);
                val_b = va_arg(ap, const byte *);
                val_len_net = cpu_to_le32(val_len);
                sb_add(blob, &val_len_net, sizeof(int));
                sb_add(blob, val_b, val_len);
                continue;
              case '%':
                sb_addc(blob, '%');
                continue;
              default:
                /* Unsupported format... */
                goto error;
            }
            continue;
        default:
            sb_addc(blob, c);
            continue;
        }
        break;
    }
    va_end(ap);
    return 0;
error:
    va_end(ap);
    __sb_fixlen(blob, orig_len);
    return -1;
}

/* Deserialize a buffer that was created with blob_serialize().
 * Returns 0 and update pos if OK fmt was completly matched,
 * Returns -(n+1) if parse failed before or while parsing arg n.
 * Format string is (almost) the same as for blob_serialize:
 * %c, %d, %ld, %lld, %p are symetric, and "compatible" with scanf()
 * Unfortunately, we have to extend %*p for binary buffers, making this
 * function incompatible with attr_scanf (it's a shame!).
 */
static int buf_deserialize_vfmt(const byte *buf, int buf_len,
                                int *pos, const char *fmt, va_list ap)
{
    const byte *data, *dataend, *endp;
    int c, n = 0;
    char *cvalp;
    int *ivalp;
    long *lvalp;
    long long *llvalp;
    const char **cpvalp;
    const byte **bpvalp;

    if (buf_len < 0)
        buf_len = strlen((const char *)buf);

    if (*pos >= buf_len)
        return -1;

    data = buf + *pos;
    dataend = buf + buf_len;
    for (;;) {
        switch (c = *fmt++) {
          case '\0':
            goto end;
          case '%':
            switch (c = *fmt++) {
              case '\0':
                goto error;
                break;
              case 'c':
                cvalp = va_arg(ap, char *);
                if (*data++ != SERIALIZE_CHAR) {
                    goto error;
                }
                if (data + 1 > dataend) {
                    goto error;
                }
                *cvalp = *(char*)data;
                data += 1;
                n++;
                break;
              case 'd':
                ivalp = va_arg(ap, int *);
                if (*data++ != SERIALIZE_INT) {
                    goto error;
                }
                if (data + sizeof(int) > dataend) {
                    goto error;
                }
                *ivalp = le_to_cpu32pu(data);
                data += sizeof(int);
                n++;
                break;
              case 'l':
                if (fmt[0] == 'd') {
                    fmt++;
                    lvalp = va_arg(ap, long *);
                    if (data + 1 + sizeof(long) > dataend) {
                        goto error;
                    }
                    switch (sizeof(long)) {
                      case 4:
                        /* %ld => 32 bits "long" */
                        if (*data++ != SERIALIZE_LONG_32) {
                            goto error;
                        }
                        *lvalp = le_to_cpu32pu(data);
                        break;
                      case 8:
                        /* %ld => 64 bits "long" */
                        if (*data++ != SERIALIZE_LONG_64) {
                            goto error;
                        }
                        *lvalp = le_to_cpu64pu(data);
                        break;
                      default:
                        goto error;
                    }
                    data += sizeof(long);
                    n++;
                } else if (fmt[0] == 'l' && fmt[1] == 'd') {
                    fmt += 2;
                    /* %lld : long long => 64 bits */
                    if (data + 1 + sizeof(long long) > dataend) {
                        goto error;
                    }
                    llvalp = va_arg(ap, long long *);
                    if (*data++ != SERIALIZE_LONG_LONG) {
                        goto error;
                    }
                    *llvalp = le_to_cpu64pu(data);
                    data += sizeof(long long);
                    n++;
                } else {
                    goto error;
                }
                break;
              case 'p':
                /* %p => NIL-terminated string */
                if (*data++ != SERIALIZE_STRING) {
                    goto error;
                }
                endp = data;
                while (*endp++) {
                    if (endp >= dataend) {
                        goto error;
                    }
                }
                cpvalp= va_arg(ap, const char **);
                *cpvalp = (const char *)data;
                data = endp;
                n++;
                break;
              case '*':
                /* %*p => pointer to int (size) and pointer
                 * to pointer to binary object */
                if (fmt[0] != 'p') {
                    goto error;
                }
                fmt++;
                if (*data++ != SERIALIZE_BINARY) {
                    goto error;
                }
                ivalp = va_arg(ap, int*);
                bpvalp = va_arg(ap, const byte **);
                *ivalp = le_to_cpu32pu(data);
                data += sizeof(int);
                *bpvalp = (const byte *)data;
                data += *ivalp;
                n++;
                break;
            }
            break;
          default:
            if (data[0] != c) {
                goto error;
            }
            data++;
            break;
        }
    }
end:
    va_end(ap);
    *pos = data - buf;
    return 0;
error:
    va_end(ap);
    return -(n + 1);
}

int buf_deserialize(const byte *buf, int buf_len,
                    int *pos, const char *fmt, ...)
{
    va_list ap;
    int res;

    va_start(ap, fmt);
    res = buf_deserialize_vfmt(buf, buf_len, pos, fmt, ap);
    va_end(ap);

    return res;
}

int blob_deserialize(const sb_t *blob, int *pos, const char *fmt, ...)
{
    va_list ap;
    int res;

    va_start(ap, fmt);
    res = buf_deserialize_vfmt((byte *)blob->data, blob->len, pos, fmt, ap);
    va_end(ap);

    return res;
}
