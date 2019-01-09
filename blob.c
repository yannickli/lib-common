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

/*---------------- blob conversion stuff ----------------*/

void blob_append_date_iso8601(blob_t *dst, time_t date)
{
    struct tm t;

    gmtime_r(&date, &t);
    blob_grow(dst, 20);
    blob_append_fmt(dst, "%04d-%02d-%02dT%02d:%02d:%02dZ",
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

int blob_pack(blob_t *blob, const char *fmt, ...)
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
            blob_append_data(blob, p, buf + sizeof(buf) - p);
            continue;
        case 'l':
            blob_append_fmt(blob, "%"PRIi64, va_arg(ap, int64_t));
            continue;
        case 'g':
            blob_append_fmt(blob, "%g", va_arg(ap, double));
            continue;
        case 'x':
            p = convert_hex(buf + sizeof(buf), va_arg(ap, unsigned int));
            blob_append_data(blob, p, buf + sizeof(buf) - p);
            continue;
        case 's':
            p = va_arg(ap, const char *);
            blob_append_cstr(blob, p);
            continue;
        case 'c':
            c = va_arg(ap, int);
            /* FALLTHRU */
        default:
            blob_append_byte(blob, c);
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


int blob_unpack(const blob_t *blob, int *pos, const char *fmt, ...)
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
int blob_serialize(blob_t *blob, const char *fmt, ...)
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
                blob_append_byte(blob, SERIALIZE_CHAR);
                blob_append_data(blob, &val_c, 1);
                continue;
              case 'l':
                if (fmt[0] == 'l' && fmt[1] == 'd') {
                    /* %lld: 64 bits int */
                    fmt += 2;
                    val_ll = va_arg(ap, long long);
                    val_ll = cpu_to_le64(val_ll);
                    blob_append_byte(blob, SERIALIZE_LONG_LONG);
                    blob_append_data(blob, &val_ll, sizeof(long long));
                } else if (fmt[0] == 'd') {
                    /* %l: long : 32 or 64 bytes depending on platform */
                    fmt++;
                    val_l = va_arg(ap, long);
                    switch (sizeof(long)) {
                      case 4:
                        val_l = cpu_to_le32(val_l);
                        blob_append_byte(blob, SERIALIZE_LONG_32);
                        break;
                      case 8:
                        val_l = cpu_to_le64(val_l);
                        blob_append_byte(blob, SERIALIZE_LONG_64);
                        break;
                      default:
                        goto error;
                    }
                    blob_append_data(blob, &val_l, sizeof(long));
                }
                continue;
              case 'd':
                /* %d: 32 bits int */
                val_d = va_arg(ap, int);
                val_d = cpu_to_le32(val_d);
                blob_append_byte(blob, SERIALIZE_INT);
                blob_append_data(blob, &val_d, sizeof(int));
                continue;
              case 's':
                /* "%s" : string, NIL-terminated */
                val_s = va_arg(ap, const char *);
                blob_append_byte(blob, SERIALIZE_STRING);
                blob_append_cstr(blob, val_s);
                blob_append_byte(blob, '\0');
                continue;
              case '*':
                /* "%*p" : binary object (can include a \0) */
                if (fmt[0] != 'p') {
                    goto error;
                }
                fmt++;
                blob_append_byte(blob, SERIALIZE_BINARY);
                val_len = va_arg(ap, int);
                val_b = va_arg(ap, const byte *);
                val_len_net = cpu_to_le32(val_len);
                blob_append_data(blob, &val_len_net, sizeof(int));
                blob_append_data(blob, val_b, val_len);
                continue;
              case '%':
                blob_append_byte(blob, '%');
                continue;
              default:
                /* Unsupported format... */
                goto error;
            }
            continue;
        default:
            blob_append_byte(blob, c);
            continue;
        }
        break;
    }
    va_end(ap);
    return 0;
error:
    va_end(ap);
    blob_setlen(blob, orig_len);
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

int blob_deserialize(const blob_t *blob, int *pos, const char *fmt, ...)
{
    va_list ap;
    int res;

    va_start(ap, fmt);
    res = buf_deserialize_vfmt((byte *)blob->data, blob->len, pos, fmt, ap);
    va_end(ap);

    return res;
}

/*[ CHECK ]::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::{{{*/
#ifdef CHECK
/* inlines (check invariants) + setup/teardowns                        {{{*/

static void check_blob_invariants(blob_t *blob)
{
    fail_if(blob->len >= blob->size,
            "a blob must have `len < size'. "
            "this one has `len = %d' and `size = %d'",
            blob->len, blob->size);
    fail_if(blob->data[blob->len] != '\0', \
            "a blob must have data[len] set to `\\0', `%c' found",
            blob->data[blob->len]);
}

static void check_setup(blob_t *blob, const char *data)
{
    blob_init(blob);
    blob_set_cstr(blob, data);
}

static void check_teardown(blob_t *blob, blob_t **blob2)
{
    blob_wipe(blob);
    if (blob2 && *blob2) {
        blob_delete(blob2);
    }
}

/*.....................................................................}}}*/
/* tests legacy functions                                              {{{*/

START_TEST(check_init_wipe)
{
    blob_t blob;
    blob_init(&blob);
    check_blob_invariants(&blob);

    fail_if(blob.len != 0,
            "initalized blob MUST have `len = 0', but has `len = %d'",
            blob.len);
    fail_if(blob.skip, "initalized blob MUST have `skip = 0'");

    blob_wipe(&blob);
}
END_TEST

START_TEST(check_blob_new)
{
    blob_t *blob = blob_new();

    check_blob_invariants(blob);
    fail_if(blob == NULL,
            "no blob was allocated");
    fail_if(blob->len != 0,
            "new blob MUST have `len = 0', but has `len = %d'", blob->len);
    fail_if(blob->skip, "new blob MUST have `skip = 0'");

    blob_delete(&blob);
    fail_if(blob != NULL,
            "pointer was not nullified by `blob_delete'");
}
END_TEST

/*.....................................................................}}}*/
/* test set functions                                                  {{{*/

START_TEST(check_set)
{
    blob_t blob, bloub;
    blob_init (&blob);
    blob_init (&bloub);

    /* blob set cstr */
    blob_set_cstr(&blob, "toto");
    check_blob_invariants(&blob);
    fail_if(blob.len != strlen("toto"),
            "blob.len should be %zd, but is %d", strlen("toto"), blob.len);
    fail_if(strcmp((const char *)blob.data, "toto") != 0,
            "blob is not set to `%s'", "toto");

    /* blob set data */
    blob_set_data(&blob, "tutu", strlen("tutu"));
    check_blob_invariants(&blob);
    fail_if(blob.len != strlen("tutu"),
            "blob.len should be %zd, but is %d", strlen("tutu"), blob.len);
    fail_if(strcmp((const char *)blob.data, "tutu") != 0,
            "blob is not set to `%s'", "tutu");

    /* blob set */
    blob_set(&bloub, &blob);
    check_blob_invariants(&bloub);
    fail_if(bloub.len != strlen("tutu"),
            "blob.len should be %zd, but is %d", strlen("tutu"), bloub.len);
    fail_if(strcmp((const char *)bloub.data, "tutu") != 0,
            "blob is not set to `%s'", "tutu");

    blob_wipe(&blob);
    blob_wipe(&bloub);
}
END_TEST

/*.....................................................................}}}*/
/* test blob_dup / blob_setlen                                         {{{*/

START_TEST(check_resize)
{
    blob_t b1;
    check_setup(&b1, "tototutu");

    blob_setlen(&b1, 4);
    check_blob_invariants(&b1);
    fail_if (b1.len != 4,
             "blob_setlen(&b1, 4) should have set len to 4, but has %d", b1.len);

    check_teardown(&b1, NULL);
}
END_TEST

/*.....................................................................}}}*/
/* test splice functions                                               {{{*/

START_TEST(check_splice)
{
    blob_t blob;
    blob_t *b2;

    check_setup(&blob, "ABCD");
    b2 = blob_new();
    blob_set_cstr(b2, "567");

    /* splice data */
    sb_splice(&blob, 0, 0, b2->data, b2->len);
    sb_skip(&blob, b2->len);
    sb_splice(&blob, blob.len - 1, 0, "89", 2);
    check_blob_invariants(&blob);
    fail_if(strcmp(blob.data, "ABC89D") != 0, "splice_data failed");
    fail_if(blob.len != strlen("ABC89D"), "splice_data failed");

    check_teardown(&blob, &b2);
}
END_TEST

/*.....................................................................}}}*/
/* test append functions                                               {{{*/

START_TEST(check_append)
{
    blob_t blob;
    blob_t *b2;

    check_setup(&blob, "01");
    b2 = blob_new();
    blob_set_cstr(b2, "89");


    /* append cstr */
    blob_append_cstr(&blob, "2345");
    check_blob_invariants(&blob);
    fail_if(strcmp((const char *)blob.data, "012345") != 0,
            "append failed");
    fail_if(blob.len != strlen("012345"),
            "append failed");

    /* append data */
    blob_append_data(&blob, "67", 2);
    check_blob_invariants(&blob);
    fail_if(strcmp((const char *)blob.data, "01234567") != 0,
            "append_data failed");
    fail_if(blob.len != strlen("01234567"),
            "append_data failed");

    /* append */
    blob_append(&blob, b2);
    check_blob_invariants(&blob);
    fail_if(strcmp((const char *)blob.data, "0123456789") != 0,
            "append failed");
    fail_if(blob.len != strlen("0123456789"),
            "append failed");

    /* append escaped */
    blob_setlen(&blob, 0);
    blob_append_cstr_escaped2(&blob, "123\"45\\6", "\"\\", "\"\\");
    check_blob_invariants(&blob);
    fail_if(strcmp((const char *)blob.data, "123\\\"45\\\\6") != 0,
            "append escaped failed");
    fail_if(blob.len != strlen("123\\\"45\\\\6"),
            "append escaped failed");

    blob_setlen(&blob, 0);
    blob_append_cstr_escaped2(&blob, "123456", "\"\\", "\"\\");
    check_blob_invariants(&blob);
    fail_if(strcmp((const char *)blob.data, "123456") != 0,
            "append escaped failed on simple string");
    fail_if(blob.len != strlen("123456"),
            "append escaped failed on simple string");

    check_teardown(&blob, &b2);
}
END_TEST

START_TEST(check_append_file_data)
{
    const char file[] = "check.data";
    const char data[] = "my super data, just for the fun, and for the test \n"
                        "don't you like it ?\n"
                        "tests are soooo boring !!!";

    blob_t blob;
    int fd;

    blob_init(&blob);

    fd = open(file, O_WRONLY | O_CREAT | O_TRUNC, 0640);
    fail_if(fd < 0, "sample file not created");
    fail_if(write(fd, &data, strlen(data)) != sstrlen(data),
            "data not written");
    close(fd);

    fail_if(blob_append_file_data(&blob, file) != sstrlen(data),
            "file miscopied");
    check_blob_invariants(&blob);
    fail_if(blob.len != sstrlen(data),
            "file miscopied");
    fail_if(strcmp((const char *)blob.data, data) != 0,
            "garbage copied");

    unlink(file);

    blob_wipe(&blob);
}
END_TEST
/*.....................................................................}}}*/
/* test kill functions                                                 {{{*/

START_TEST(check_kill)
{
    blob_t blob;
    check_setup(&blob, "0123456789");

    /* kill first */
    blob_kill_first(&blob, 3);
    check_blob_invariants(&blob);
    fail_if(strcmp((const char *)blob.data, "3456789") != 0,
            "kill_first failed");
    fail_if(blob.len != strlen("3456789"),
            "kill_first failed");

    /* kill last */
    blob_kill_last(&blob, 3);
    check_blob_invariants(&blob);
    fail_if(strcmp((const char *)blob.data, "3456") != 0,
            "kill_last failed");
    fail_if(blob.len != strlen("3456"),
            "kill_last failed");

    /* kill */
    blob_kill_data(&blob, 1, 2);
    check_blob_invariants(&blob);
    fail_if(strcmp((const char *)blob.data, "36") != 0,
            "kill failed");
    fail_if(blob.len != strlen("36"),
            "kill failed");

    check_teardown(&blob, NULL);
}
END_TEST

/*.....................................................................}}}*/
/* test printf functions                                               {{{*/

START_TEST(check_printf)
{
    char cmp[81];
    blob_t blob;
    check_setup(&blob, "01234");
    snprintf(cmp, 81, "%080i", 0);

    /* printf first */
    blob_append_fmt(&blob, "5");
    check_blob_invariants(&blob);
    fail_if(strcmp((const char *)blob.data, "012345") != 0,
            "blob_append_fmt failed");
    fail_if(blob.len != strlen("012345"),
            "blob_append_fmt failed");

    blob_append_fmt(&blob, "%s89", "67");
    check_blob_invariants(&blob);
    fail_if(strcmp((const char *)blob.data, "0123456789") != 0,
            "blob_append_fmt failed");
    fail_if(blob.len != strlen("0123456789"),
            "blob_append_fmt failed");

    blob_set_fmt(&blob, "%080i", 0);
    check_blob_invariants(&blob);
    fail_if(strcmp((const char *)blob.data, cmp) != 0,
            "blob_append_fmt failed");
    fail_if(blob.len != sstrlen(cmp),
            "blob_append_fmt failed");

    check_teardown(&blob, NULL);
}
END_TEST

/*.....................................................................}}}*/
/* test blob_urldecode                                                 {{{*/

START_TEST(check_url)
{
    blob_t blob;
    check_setup(&blob, "%20toto%79");

    blob_urldecode(&blob);
    check_blob_invariants(&blob);

    fail_if(strcmp((const char *)blob.data, " totoy") != 0,
            "urldecode failed");
    fail_if(blob.len != sstrlen(" totoy"),
            "urldecode failed");

    check_teardown(&blob, NULL);

    check_setup(&blob, "");
    sb_adds_urldecode(&blob, "toto%20tata");
    check_blob_invariants(&blob);
    fail_if(strcmp((const char *)blob.data, "toto tata") != 0,
            "sb_adds_urldecode failed");
    fail_if(blob.len != sstrlen("toto tata"),
            "sb_adds_urldecode failed");
    check_teardown(&blob, NULL);

    check_setup(&blob, "tutu");
    sb_adds_urldecode(&blob, "toto%20tata");
    check_blob_invariants(&blob);
    fail_if(strcmp((const char *)blob.data, "tututoto tata") != 0,
            "sb_adds_urldecode failed");
    fail_if(blob.len != sstrlen("tututoto tata"),
            "sb_adds_urldecode failed");
    check_teardown(&blob, NULL);
}
END_TEST

/*.....................................................................}}}*/
/* test blob_b64                                                       {{{*/

#define CHECK_BASE64_CORRECT(sb, enc, dec) \
    do {                                                                      \
        sb_reset((sb));                                                       \
        fail_if(sb_adds_unb64((sb), enc), "sb_adds_unb64 of %s failed", enc); \
        fail_if((sb)->len != strlen(dec) ||                                   \
                memcmp((sb)->data, dec, (sb)->len),                           \
                "sb_adds_unb64: bad decoding of `%s`:"                        \
                "expected: %s, got: %s", enc, dec, (sb)->data);               \
    } while (0)

#define CHECK_BASE64_INCORRECT(sb, enc) \
    do {                                                                      \
        sb_reset(sb);                                                         \
        fail_if(sb_adds_unb64(sb, enc) == 0,                                  \
                "sb_adds_unb64 failed to detect error in: %s", enc);          \
    } while (0)

START_TEST(check_b64)
{
    blob_t blob;
    SB_1k(sb);

    // Should check blob_append_base64_*
    check_setup(&blob, "abcdef");
    check_blob_invariants(&blob);

    CHECK_BASE64_CORRECT(&sb, "dHV0dTp0b3Rv", "tutu:toto");
    CHECK_BASE64_CORRECT(&sb, "dHV0    dTp0b3Rv", "tutu:toto");
    CHECK_BASE64_CORRECT(&sb, "dHV0d\nT p  0b \n3Rv    \n", "tutu:toto");
    CHECK_BASE64_CORRECT(&sb, "dHV0dTp0b3RvdA==", "tutu:totot");
    CHECK_BASE64_CORRECT(&sb, "dHV0dTp0b3RvdA=  =", "tutu:totot");
    CHECK_BASE64_CORRECT(&sb, "dHV0dTp0b3RvdHQ=", "tutu:totott");
    CHECK_BASE64_CORRECT(&sb, "dHV0dTp0b3RvdHQ =", "tutu:totott");

    CHECK_BASE64_INCORRECT(&sb, "dHV0dTp0b3R");
    CHECK_BASE64_INCORRECT(&sb, "dHV0dTp0b3RvdA==dHV0");
    CHECK_BASE64_INCORRECT(&sb, "dHV0dTp0b3RvdHQ=X");

    // TODO: check results
    sb_wipe(&sb);
    check_teardown(&blob, NULL);
}
END_TEST

/*.....................................................................}}}*/
/* test check_zlib_compress_uncompress                                 {{{*/

START_TEST(check_zlib_compress_uncompress)
{
    blob_t b1;
    blob_t b2;
    blob_t b3;

    blob_init(&b1);
    blob_init(&b2);
    blob_init(&b3);
    blob_set_cstr(&b1, "toto string");

    blob_zlib_compress(&b2, &b1);
    blob_zlib_uncompress(&b3, &b2);

    fail_if(blob_cmp(&b1, &b3),
            "blob_uncompress dest does not correspond to blob_compress src");

    blob_wipe(&b1);
    blob_wipe(&b2);
    blob_wipe(&b3);
}
END_TEST

/*.....................................................................}}}*/
/* test check_zlib                                                     {{{*/

START_TEST(check_raw_compress_uncompress)
{
    blob_t b1;
    blob_t b2;
    blob_t b3;

    blob_init(&b1);
    blob_init(&b2);
    blob_init(&b3);
    blob_set_cstr(&b1, "toto string");

    blob_raw_compress(&b2, &b1);
    blob_raw_uncompress(&b3, &b2);

    fail_if(blob_cmp(&b1, &b3),
            "blob_uncompress dest does not correspond to blob_compress src");

    blob_wipe(&b1);
    blob_wipe(&b2);
    blob_wipe(&b3);
}
END_TEST


/*.....................................................................}}}*/
/* test blob_file_gunzip                                               {{{*/

static int check_gunzip_tpl(const char *file1, const char *file2)
{
    blob_t b1;
    blob_t b2;

    blob_init(&b1);
    blob_init(&b2);

    blob_file_gunzip(&b1, file1);
    blob_append_file_data(&b2, file2);

    fail_if (blob_cmp(&b1, &b2) != 0, "blob_gunzip failed on: %s, %s",
             file1, file2);

    blob_wipe(&b1);
    blob_wipe(&b2);

    return 0;
}

START_TEST(check_gunzip)
{
    check_gunzip_tpl("samples/example1_zlib.gz", "samples/example1_zlib");
}
END_TEST

/*.....................................................................}}}*/
/* test blob_file_gzip                                                 {{{*/

static int check_gzip_tpl(const char *file1, const char *file2)
{
    blob_t b1;
    blob_t b2;

    blob_init(&b1);
    blob_init(&b2);

    blob_file_gzip(&b1, file1);

    blob_append_file_data(&b2, file2);

    fail_if (blob_cmp(&b1, &b2) != 0, "blob_gzip failed on: %s, %s",
             file1, file2);

    blob_wipe(&b1);
    blob_wipe(&b2);

    return 0;
}

START_TEST(check_gzip)
{
    check_gzip_tpl("samples/example1_zlib", "samples/example1_zlib.gz");
}
END_TEST

/*.....................................................................}}}*/
/* test check_unpack                                                   {{{*/

START_TEST(check_unpack)
{
    int n1, n2;

    const char *buf = "12|x|skdjhsdkh|1|2|3|4|\n";
    int bufpos = 0;
    int res;

    res = buf_unpack((const byte*)buf, -1, &bufpos, "d|c|s|d|d|d|d|",
                     &n1, NULL, NULL, NULL, &n2, NULL, NULL);

    fail_if(res != 7,
            "buf_unpack() returned %d, expected %d\n", res, 7);
    fail_if(n1 != 12,
            "buf_unpack() failed: n1=%d, expected %d\n", n1, 12);
    fail_if(n2 != 2,
            "buf_unpack() failed: n2=%d, expected %d\n", n2, 2);
}
END_TEST

/*.....................................................................}}}*/
/* test blob_append_ira_hex, blob_append_ira_bin                       {{{*/

START_TEST(check_ira)
{
    blob_t dst, bin, back;

    blob_init(&dst);
    blob_init(&bin);
    blob_init(&back);
#define TEST_STRING      "Injector X=1 Gagné! 1€"
#define TEST_STRING_ENC  "496E6A6563746F7220583D31204761676E052120311B65"

    blob_append_ira_hex(&dst, (const byte*)TEST_STRING, strlen(TEST_STRING));

    fail_if(strcmp(blob_get_cstr(&dst), TEST_STRING_ENC),
            "%s(\"%s\") -> \"%s\" : \"%s\"",
            "blob_append_ira_hex",
            TEST_STRING, blob_get_cstr(&dst), TEST_STRING_ENC);

    blob_decode_ira_hex_as_utf8(&back, blob_get_cstr(&dst), dst.len);
    fail_if(strcmp(blob_get_cstr(&back), TEST_STRING),
            "blob_decode_ira_hex_as_utf8 failure");

    blob_append_ira_bin(&bin, (const byte*)TEST_STRING, strlen(TEST_STRING));
    blob_reset(&dst);
    blob_append_hex(&dst, bin.data, bin.len);

    fail_if(strcmp(blob_get_cstr(&dst), TEST_STRING_ENC),
            "%s(\"%s\") -> \"%s\" : \"%s\"",
            "blob_append_ira_bin",
            TEST_STRING, blob_get_cstr(&dst), TEST_STRING_ENC);

#undef TEST_STRING
#undef TEST_STRING_ENC

    blob_wipe(&dst);
    blob_wipe(&bin);
    blob_wipe(&back);
}
END_TEST

START_TEST(check_quoted_printable)
{
    blob_t dst;
    blob_t back;

    blob_init(&dst);
    blob_init(&back);
#define TEST_STRING      "Injector X=1 Gagné! \nzertzertzertzertzertzertzertzertzertzertzertzertzertzertzertzertzert Last line "
#define TEST_STRING_ENC  "Injector X=3D1 Gagn=C3=A9!=20\r\nzertzertzertzertzertzertzertzertzertzertzertzertzertzertzertzertzert Last l=\r\nine=20"

    blob_append_quoted_printable(&dst, (const byte*)TEST_STRING,
                                 strlen(TEST_STRING));

    fail_if(strcmp(blob_get_cstr(&dst), TEST_STRING_ENC),
            "%s(\"%s\") -> \"%s\" : \"%s\"",
            "blob_append_quoted_printable",
            TEST_STRING, blob_get_cstr(&dst), TEST_STRING_ENC);

    blob_decode_quoted_printable(&back, blob_get_cstr(&dst), dst.len);
    fail_if(strcmp(blob_get_cstr(&back), TEST_STRING),
            "blob_decode_quoted_printable failure");

#undef TEST_STRING
#undef TEST_STRING_ENC

    blob_wipe(&dst);
    blob_wipe(&back);
}
END_TEST

START_TEST(check_xml_escape)
{
    blob_t dst;

    blob_init(&dst);
#define TEST_STRING      "<a href=\"Injector\" X='1' value=\"&Gagné!\" />"
#define TEST_STRING_ENC  "&lt;a href=&#34;Injector&#34; X=&#39;1&#39; value=&#34;&amp;Gagné!&#34; /&gt;"

    blob_append_xml_escape(&dst, TEST_STRING, strlen(TEST_STRING));
    fail_if(strcmp(blob_get_cstr(&dst), TEST_STRING_ENC),
            "%s(\"%s\") -> \"%s\" : \"%s\"",
            "blob_append_xml_escape",
            TEST_STRING, blob_get_cstr(&dst), TEST_STRING_ENC);
#if 0
    blob_decode_xml_escape(&back, dst);
    fail_if(strcmp(blob_get_cstr(&back), TEST_STRING),
            "blob_decode_xml_escape failure");
#endif

#undef TEST_STRING
#undef TEST_STRING_ENC

    blob_wipe(&dst);
}
END_TEST

START_TEST(check_serialize_c)
{
    blob_t dst;
    char val1 = 0x10, val2 = 0x11;
    int pos, res, j;

    blob_init(&dst);

    blob_serialize(&dst, "%c%c", 'A', 'a');
    pos = 0;
    res = blob_deserialize(&dst, &pos, "%c%c", &val1, &val2);
    fail_if(res != 0, "res:%d", res);
    fail_if(val1 != 'A' || val2 != 'a', "val1:%c val2:%c", val1, val2);

    /* Check all possible values for %c */
    blob_reset(&dst);
    for (j = 0; j <= 0xFF; j++) {
        blob_serialize(&dst, "%c", j);
    }
    pos = 0;
    for (j = 0; j <= 0xFF; j++) {
        res = blob_deserialize(&dst, &pos, "%c", &val1);
        fail_if(res != 0, "res:%d j:%d", res, j);
        fail_if(val1 != j, "val1:%d j:%d", val1, j);
    }
    fail_if (pos != dst.len, "pos:%d dst.len:%d", pos, (int)dst.len);

    blob_wipe(&dst);
}
END_TEST

START_TEST(check_serialize_fmt)
{
    blob_t dst;
    char val1 = 0x10, val2 = 0x11;
    int pos, res;

    blob_init(&dst);

    blob_serialize(&dst, "AA%cBBB%cCCCC", 'G', 'H');
    pos = 0;
    res = blob_deserialize(&dst, &pos, "AA%cBBB%cCCCC", &val1, &val2);
    fail_if(res != 0, "res:%d", res);
    fail_if(val1 != 'G' || val2 != 'H', "val1:%c val2:%c", val1, val2);

    blob_wipe(&dst);
}
END_TEST

START_TEST(check_serialize_d)
{
    blob_t dst;
    int val1 = 71, val2 = 42, pos, res;

    blob_init(&dst);

    blob_serialize(&dst, "%d%d", 3, 4);
    pos = 0;
    res = blob_deserialize(&dst, &pos, "%d%d", &val1, &val2);
    fail_if(val1 != 3 || val2 != 4, "val1:%d val2:%d", val1, val2);
    fail_if(res != 0, "res:%d", res);

    blob_reset(&dst);
    blob_serialize(&dst, "%d%d", 42, -42);
    pos = 0;
    res = blob_deserialize(&dst, &pos, "%d%d", &val1, &val2);
    fail_if(val1 != 42 || val2 != -42, "val1:%d val2:%d", val1, val2);
    fail_if(res != 0, "res:%d", res);

    blob_reset(&dst);
    blob_serialize(&dst, "%d", 0x12345678);
    pos = 0;
    res = blob_deserialize(&dst, &pos, "%d", &val1);
    fail_if(val1 != 0x12345678, "val1:%d", val1);
    fail_if(res != 0, "res:%d", res);

    blob_reset(&dst);
    blob_serialize(&dst, "AA%dAA", 0x12345678);
    pos = 0;
    res = blob_deserialize(&dst, &pos, "AA%d", &val1);
    fail_if(val1 != 0x12345678, "val1:%d", val1);
    fail_if(res != 0, "res:%d", res);
    fail_if(pos != dst.len - 2, "pos:%d", pos);
    res = blob_deserialize(&dst, &pos, "AA");
    fail_if(res != 0, "res:%d", res);
    fail_if(pos != dst.len, "pos:%d", pos);

    blob_wipe(&dst);
}
END_TEST

START_TEST(check_serialize_ld)
{
    blob_t dst;
    int pos, res;
    long val1 = 4242;

    blob_init(&dst);

#define CHECKVAL(val) do {\
    blob_reset(&dst); \
    blob_serialize(&dst, "%ld", (long)val); \
    pos = 0; \
    val1 = 42; \
    res = blob_deserialize(&dst, &pos, "%ld", &val1); \
    fail_if(val1 != val, "val1:%ld", val1); \
    fail_if(res != 0, "val1:%ld, res:%d", val1, res); \
    fail_if(pos != dst.len, "pos:%d", pos); \
    } while (0)

    CHECKVAL(0);
    CHECKVAL(1);
    CHECKVAL(-1);
    CHECKVAL(31415);
#undef CHECKVAL

    blob_wipe(&dst);
}
END_TEST


START_TEST(check_serialize_lld)
{
    blob_t dst;
    int pos, res;
    unsigned long long int val1 = 4242;

    blob_init(&dst);

#define CHECKVAL(val) do {\
    blob_reset(&dst); \
    blob_serialize(&dst, "%lld", (unsigned long long) val); \
    pos = 0; \
    val1 = 42; \
    res = blob_deserialize(&dst, &pos, "%lld", &val1); \
    fail_if(val1 != val, "val1:%lld", val1); \
    fail_if(res != 0, "val1:%lld, res:%d", val1, res); \
    fail_if(pos != dst.len, "pos:%d", pos); \
    } while (0)

    CHECKVAL(0);
    CHECKVAL(1);
    CHECKVAL(3141592653589793238ULL);
    CHECKVAL(0x0001000200030004ULL);
    CHECKVAL(0x0102030405060708ULL);
    CHECKVAL(0xFF00FF00FF00FF00ULL);
    CHECKVAL(0x00FF00FF00FF00FFULL);
    CHECKVAL(0xAAAAAAAAAAAAAAAAULL);
#undef CHECKVAL

    blob_wipe(&dst);
}
END_TEST

START_TEST(check_serialize_s)
{
    blob_t dst;
    int pos, res;
    const char *val1 = NULL;

    blob_init(&dst);

#define CHECKVAL(val) do {\
    blob_reset(&dst); \
    blob_serialize(&dst, "%s", val); \
    pos = 0; \
    val1 = NULL; \
    res = blob_deserialize(&dst, &pos, "%p", &val1); \
    fail_if(strcmp(val, val1), "val1:%s", val1); \
    fail_if(res != 0, "res:%d", res); \
    fail_if(pos != dst.len, "pos:%d, dst.len:%d", pos, dst.len); \
    } while (0)

    CHECKVAL("ABC");
    CHECKVAL("Clavier AZERTY");
    CHECKVAL("QWERTZ Tastatur");
    CHECKVAL("QWERTY keyboard");
    CHECKVAL("");
#undef CHECKVAL

    blob_wipe(&dst);
}
END_TEST

START_TEST(check_serialize_p)
{
    blob_t dst;
    int pos, res;
    const char *val1 = NULL;
    int len1;

    blob_init(&dst);

#define CHECKVAL(val, val_len) do {\
    blob_reset(&dst); \
    blob_serialize(&dst, "%*p", val_len, val); \
    pos = 0; \
    val1 = NULL; \
    res = blob_deserialize(&dst, &pos, "%*p", &len1, &val1); \
    fail_if(len1 != val_len, "len1:%d val_len:%d", len1, val_len); \
    fail_if(memcmp(val, val1, val_len), "memcmp failed"); \
    fail_if(res != 0, "res:%d", res); \
    fail_if(pos != dst.len, "pos:%d, dst.len:%d", pos, dst.len); \
    } while (0)

    CHECKVAL("ABC", 3);
    CHECKVAL("\000\000\000", 3);
#undef CHECKVAL

    blob_wipe(&dst);
}
END_TEST

/*.....................................................................}}}*/
/* public testing API                                                  {{{*/

Suite *check_make_blob_suite(void)
{
    Suite *s  = suite_create("Blob");
    TCase *tc = tcase_create("Core");

    suite_add_tcase(s, tc);
    tcase_add_test(tc, check_init_wipe);
    tcase_add_test(tc, check_blob_new);
    tcase_add_test(tc, check_set);
    tcase_add_test(tc, check_splice);
    tcase_add_test(tc, check_append);
    tcase_add_test(tc, check_append_file_data);
    tcase_add_test(tc, check_kill);
    tcase_add_test(tc, check_resize);
    tcase_add_test(tc, check_printf);
    tcase_add_test(tc, check_url);
    tcase_add_test(tc, check_b64);
    tcase_add_test(tc, check_ira);
    tcase_add_test(tc, check_quoted_printable);
    tcase_add_test(tc, check_xml_escape);
    tcase_add_test(tc, check_raw_compress_uncompress);
    tcase_add_test(tc, check_zlib_compress_uncompress);
    tcase_add_test(tc, check_gunzip);
    tcase_add_test(tc, check_gzip);
    tcase_add_test(tc, check_unpack);
    tcase_add_test(tc, check_serialize_c);
    tcase_add_test(tc, check_serialize_fmt);
    tcase_add_test(tc, check_serialize_d);
    tcase_add_test(tc, check_serialize_ld);
    tcase_add_test(tc, check_serialize_lld);
    tcase_add_test(tc, check_serialize_s);
    tcase_add_test(tc, check_serialize_p);

    return s;
}

/*.....................................................................}}}*/
#endif
/*:::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::}}}*/
