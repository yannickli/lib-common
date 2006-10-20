/**************************************************************************/
/*                                                                        */
/*  Copyright (C) 2004-2006 INTERSEC                                      */
/*                                                                        */
/*  Should you receive a copy of this source code, you must check you     */
/*  have a proper, written authorization of INTERSEC to hold it. If you   */
/*  don't have such an authorization, you must DELETE all source code     */
/*  files in your possession, and inform INTERSEC of the fact you obtain  */
/*  these files. Should you not comply to these terms, you can be         */
/*  prosecuted in the extent permitted by applicable law.                 */
/*                                                                        */
/**************************************************************************/

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <stdarg.h>
#include <limits.h>
#include <stdint.h>
#include <errno.h>

#include "iprintf.h"

/* This code only works on regular architectures: we assume
 *  - integer types are either 32 bit or 64 bit long.
 *  - long long is the same as int64_t
 *  - bytes (char type) have 8 bits
 *  - integer representation is 2's complement, no padding, no traps
 *
 * imported functions:
 *   stdio.h:    stdout, putc_unlocked, fwrite_unlocked;
 */

/*---------------- formatter ----------------*/

#define FLAG_UPPER      0x0001
#define FLAG_MINUS      0x0002
#define FLAG_PLUS       0x0004
#define FLAG_SPACE      0x0008
#define FLAG_ALT        0x0010
#define FLAG_QUOTE      0x0020
#define FLAG_ZERO       0x0040

#define FLAG_WIDTH      0x0080
#define FLAG_PREC       0x0100

#if !defined(INT32_MAX) || !defined(INT64_MAX)
#error "unsupported architecture"
#endif

#define SIZE_int          0
#define SIZE_char         1
#define SIZE_short        2
#define SIZE_long         3
#define SIZE_int32        4
#define SIZE_int64        5
#define SIZE_long_double  6

#if 0
/* No longer need this obsolete stuff */
#define SIZE_far          7
#define SIZE_near         8
#endif

#define SIZE_h          SIZE_short
#define SIZE_hh         SIZE_char
#define SIZE_l          SIZE_long
#define SIZE_ll         SIZE_int64
#define SIZE_L          SIZE_long_double

#define WANT_long         1
#define WANT_int32        1
#define WANT_int64        1

#if INT32_MAX == INT_MAX
#undef WANT_int32
#undef SIZE_int32
#define SIZE_int32      SIZE_int
#define convert_uint32  convert_uint
#elif INT32_MAX == LONG_MAX
#undef WANT_int32
#undef SIZE_int32
#define SIZE_int32      SIZE_long
#define convert_uint32  convert_ulong
#endif

#if INT64_MAX == INT_MAX
#undef WANT_int64
#undef SIZE_int64
#define SIZE_int64      SIZE_int
#define convert_uint64  convert_uint
#elif INT64_MAX == LONG_MAX
#undef WANT_int64
#undef SIZE_int64
#define SIZE_int64      SIZE_long
#define convert_uint64  convert_ulong
#endif

#if LONG_MAX == INT_MAX
#undef WANT_long
#undef SIZE_long
#define SIZE_long      SIZE_int
#define convert_ulong  convert_uint
#endif

#if SIZE_MAX == UINT32_MAX
#define SIZE_size_t     SIZE_int32
#elif SIZE_MAX == UINT64_MAX
#define SIZE_size_t     SIZE_int64
#else
#error "unsupported architecture"
#endif

#if INTMAX_MAX == INT32_MAX
#define SIZE_intmax_t   SIZE_int32
#elif INTMAX_MAX == INT64_MAX
#define SIZE_intmax_t   SIZE_int64
#else
#error "unsupported architecture"
#endif

#if PTRDIFF_MAX == INT32_MAX
#define SIZE_ptrdiff_t   SIZE_int32
#elif PTRDIFF_MAX == INT64_MAX
#define SIZE_ptrdiff_t   SIZE_int64
#else
#error "unsupported architecture"
#endif

/*---------------- helpers ----------------*/

static inline int strnlen(const char *str, int n)
{
    const char *p = memchr(str, '\0', n);
    return (p ? p - str : n);
}

static inline char *convert_int10(char *p, int value)
{
    /* compute absolute value without tests */
    unsigned int bits = value >> (8 * sizeof(int) - 1);
    unsigned int num = (value ^ bits) + (bits & 1);

    while (num >= 10) {
        *--p = '0' + (num % 10) ;
        num = num / 10;
    }
    *--p = '0' + num;
    if (value < 0) {
        *--p = '-';
    }
    return p;
}

static inline char *convert_uint(char *p, unsigned int value, int base)
{
    while (value > 0) {
        *--p = '0' + (value % base);
        value = value / base;
    }
    return p;
}

#ifndef convert_ulong
static char *convert_ulong(char *p, unsigned long value, int base)
{
    while (value > 0) {
        *--p = '0' + (value % base);
        value = value / base;
    }
    return p;
}
#endif

#ifndef convert_uint32
static char *convert_uint32(char *p, uint32_t value, int base)
{
    while (value > 0) {
        *--p = '0' + (value % base);
        value = value / base;
    }
    return p;
}
#endif

#ifndef convert_uint64
static char *convert_uint64(char *p, uint64_t value, int base)
{
    while (value > 0) {
        *--p = '0' + (value % base);
        value = value / base;
    }
    return p;
}
#endif

static inline int fmt_output_chars(FILE *stream, char *str, size_t size,
                                   int count, int c, int n)
{
    while (n-- > 0) {
        if (stream) {
            putc_unlocked(c, stream);
        } else {
            if ((size_t)count < size)
                str[count] = c;
        }
        count++;
    }
    return count;
}

static inline int fmt_output_chunk(FILE *stream, char *str, size_t size,
                                   int count, const char *lp, int len)
{
    int i;

    for (i = 0; i < len; i++) {
        if (stream) {
            putc_unlocked(lp[i], stream);
        } else {
            if ((size_t)count < size)
                str[count] = lp[i];
        }
        count++;
    }
    return count;
}

static int fmt_output(FILE *stream, char *str, size_t size,
                      const char *format, va_list ap)
{
    char buf[(64 + 2) / 3 + 1 + 1];
    int c, count, len, len1, width, prec, base, flags, size_flags;
    int left_pad, prefix_len, zero_pad, right_pad;
    const char *format0, *lp;
    int sign;

    if (!format) {
        errno = EINVAL;
        return -1;
    }
    if (size > INT_MAX) {
        size = 0;
    }

    count = 0;
    right_pad = 0;

#if 0
    /* Lock stream.  */
    _IO_cleanup_region_start ((void (*) (void *)) &_IO_funlockfile, s);
    _IO_flockfile (s);
#endif

    for (;;) {
        for (lp = format; *format && *format != '%'; format++)
            continue;
        len = format - lp;
      haslp:
        if (stream) {
            switch (len) {
            default:
                count += fwrite_unlocked(lp, 1, len, stream);
                break;
            case 8: putc_unlocked(*lp++, stream);
            case 7: putc_unlocked(*lp++, stream);
            case 6: putc_unlocked(*lp++, stream);
            case 5: putc_unlocked(*lp++, stream);
            case 4: putc_unlocked(*lp++, stream);
            case 3: putc_unlocked(*lp++, stream);
            case 2: putc_unlocked(*lp++, stream);
            case 1: putc_unlocked(*lp++, stream);
            case 0: count += len;
                break;
            }
        } else {
            len1 = len;
            if ((size_t)(count + len1) > size) {
                len1 = ((size_t)count > size) ? 0 : size - count;
            }
            switch (len1) {
            default:
                memcpy(str + count, lp, len1);
                count += len;
                break;
            case 8: str[count + 7] = lp[7];
            case 7: str[count + 6] = lp[6];
            case 6: str[count + 5] = lp[5];
            case 5: str[count + 4] = lp[4];
            case 4: str[count + 3] = lp[3];
            case 3: str[count + 2] = lp[2];
            case 2: str[count + 1] = lp[1];
            case 1: str[count + 0] = lp[0];
            case 0: count += len;
                break;
            }
        }

        if (right_pad) {
            count = fmt_output_chars(stream, str, size, count, ' ', right_pad);
        }

        right_pad = 0;

        if (*format == '\0')
            goto done;

        if (*format != '%')
            continue;

        format++;

        /* special case naked %d and %s formats */
        if (*format == 'd') {
            format++;
            lp = convert_int10(buf + sizeof(buf), va_arg(ap, int));
            len = buf + sizeof(buf) - lp;
            goto haslp;
        }
        if (*format == 's') {
            format++;
            lp = va_arg(ap, const char *);
            len = strlen(lp);
            goto haslp;
        }

        /* general case: parse complete format syntax */
        format0 = format;
        flags = 0;

        /* parse optional flags */
        for (;; format++) {
            switch (*format) {
            case '\0': goto error;
            case '-':  flags |= FLAG_MINUS;  continue;
            case '+':  flags |= FLAG_PLUS;   continue;
            case '#':  flags |= FLAG_ALT;    continue;
            case '\'': flags |= FLAG_QUOTE;  continue;
            case ' ':  flags |= FLAG_SPACE;  continue;
            case '0':  flags |= FLAG_ZERO;   continue;
                       /* locale's alternative output digits */
            case 'I':  /* ignore this shit */;   continue;
            }
            break;
        }

        /* parse optional width */
        width = 0;
        if (*format == '*') {
            format++;
            flags |= FLAG_WIDTH;
            width = va_arg(ap, int);
            if (width < 0) {
                flags |= FLAG_MINUS;
                width = -width;
            }
        } else
        if (*format >= '1' && *format <= '9') {
            flags |= FLAG_WIDTH;
            width = *format++ - '0';
            while (*format >= '0' && *format <= '9') {
                width = width * 10 + *format++ - '0';
            }
        }

        /* parse optional precision */
        prec = 1;
        if (*format == '.') {
            format++;
            prec = 0;
            flags &= ~FLAG_ZERO;
            flags |= FLAG_PREC;
            if (*format == '*') {
                prec = va_arg(ap, int);
                if (prec < 0) {
                    prec = 0;
                }
            } else
            if (*format >= '0' && *format <= '9') {
                prec = *format++ - '0';
                while (*format >= '0' && *format <= '9') {
                    prec = prec * 10 + *format++ - '0';
                }
            }
        }

        /* parse optional format modifiers */
        size_flags = 0;
        switch (*format) {
        case '\0': goto error;
#ifdef SIZE_far
        case 'F':
            size_flags |= SIZE_far;
            format++;
            break;
        case 'N':
            size_flags |= SIZE_near;
            format++;
            break;
#endif
        case 'l':
            if (format[1] == 'l') {
                format++;
                size_flags |= SIZE_ll;
            } else {
                size_flags |= SIZE_l;
            }
            format++;
            break;
        case 'h':
            if (format[1] == 'h') {
                format++;
                size_flags |= SIZE_hh;
            } else {
                size_flags |= SIZE_h;
            }
            format++;
            break;
        case 'j':
            size_flags |= SIZE_intmax_t;
            format++;
            break;
        case 'z':
            size_flags |= SIZE_size_t;
            format++;
            break;
        case 't':
            size_flags |= SIZE_ptrdiff_t;
            format++;
            break;
        case 'L':
            size_flags |= SIZE_long_double;
            format++;
            break;
        }

        /* dispatch on actual format character */
        switch (c = *format++) {
        case '\0':
            format--;
            goto error;

        case 'n':
#if 1
            /* Consume pointer to int from argument list, but ignore it */
            va_arg(ap, int *);
#else
            /* The type of pointer defaults to int* but can be
             * specified with the SIZE_xxx prefixes */
            switch (size_flags) {
              case SIZE_char:
                *(char *)va_arg(ap, void *) = count;
                break;

              case SIZE_short:
                *(short *)va_arg(ap, void *) = count;
                break;

              case SIZE_int:
              default:
                *(int *)va_arg(ap, void *) = count;
                break;
#ifdef WANT_long
              case SIZE_long:
                *(long *)va_arg(ap, void *) = count;
                break;
#endif
#ifdef WANT_int32
              case SIZE_int32:
                *(int32_t *)va_arg(ap, void *) = count;
                break;
#endif
#ifdef WANT_int64
              case SIZE_int64:
                *(int64_t *)va_arg(ap, void *) = count;
                break;
#endif
            }
#endif
            break;

        case 'c':
            /* ignore 'l' prefix for wide char converted with wcrtomb() */
            c = (unsigned char)va_arg(ap, int);
            goto has_char;

        case '%':
        default:
        has_char:
            lp = buf + sizeof(buf) - 1;
            buf[sizeof(buf) - 1] = c;
            len = 1;
            goto has_string_len;

        case 'm':
            lp = strerror(errno);
            goto has_string;

        case 's':
            /* ignore 'l' prefix for wide char string */
            lp = va_arg(ap, char *);

            /* OG: should test NULL and print "(null)", prec >= 6 */

        has_string:
            if (flags & FLAG_PREC) {
                len = strnlen(lp, prec);
            } else {
                len = strlen(lp);
            }

        has_string_len:
            prefix_len = zero_pad = 0;
            flags &= ~FLAG_ZERO;

            goto apply_final_padding;

        case 'd':
        case 'i':
            sign = 0;
            switch (size_flags) {
                int int_value;

              case SIZE_char:
                int_value = (char)va_arg(ap, int);
                goto convert_int;

              case SIZE_short:
                int_value = (short)va_arg(ap, int);
                goto convert_int;

              case SIZE_int:
                int_value = va_arg(ap, int);
              convert_int:
                {
                    unsigned int bits = int_value >> (8 * sizeof(int_value) - 1);
                    unsigned int num = (int_value ^ bits) + (bits & 1);
                    sign = '-' & bits;
                    lp = convert_uint(buf + sizeof(buf), num, 10);
                    break;
                }
#ifdef WANT_long
              case SIZE_long:
                {
                    long value = va_arg(ap, long);
                    unsigned long bits = value >> (8 * sizeof(value) - 1);
                    unsigned long num = (value ^ bits) + (bits & 1);
                    sign = '-' & bits;
                    lp = convert_ulong(buf + sizeof(buf), num, 10);
                    break;
                }
#endif
#ifdef WANT_int32
              case SIZE_int32:
                {
                    int32_t value = va_arg(ap, int32_t);
                    uint32_t bits = value >> (8 * sizeof(value) - 1);
                    uint32_t num = (value ^ bits) + (bits & 1);
                    sign = '-' & bits;
                    lp = convert_uint32(buf + sizeof(buf), num, 10);
                    break;
                }
#endif
#ifdef WANT_int64
              case SIZE_int64:
                {
                    int64_t value = va_arg(ap, int64_t);
                    uint64_t bits = value >> (8 * sizeof(value) - 1);
                    uint64_t num = (value ^ bits) + (bits & 1);
                    sign = '-' & bits;
                    lp = convert_uint64(buf + sizeof(buf), num, 10);
                    break;
                }
#endif
              default:
                {
                    /* do not know what to fetch, must ignore remaining
                     * formats specifiers.  This is really an error.
                     */
                    goto error;
                }
            }
            /* We may have the following parts to output:
             * - left padding with spaces    (left_pad)
             * - the optional sign char      (buf, prefix_len)
             * - 0 padding                   (zero_pad)
             * - the converted number        (lp, len)
             * - right padding with spaces   (right_pad)
             */
            
            prefix_len = zero_pad = 0;

            len = buf + sizeof(buf) - lp;

            if (len < prec) {
                if (prec == 1) {
                    /* special case number 0 */
                    *(char*)--lp = '0';
                    len++;
                } else {
                    zero_pad = prec - len;
                }
            }
            if (!sign) {
                if (flags & FLAG_PLUS) {
                    sign = '+';
                } else
                if (flags & FLAG_SPACE) {
                    sign = ' ';
                }
            }
            if (sign) {
                if (zero_pad == 0 && !(flags & FLAG_ZERO)) {
                    *(char*)--lp = sign;
                    len++;
                } else {
                    buf[0] = (char)sign;
                    prefix_len = 1;
                }
            }
            goto apply_final_padding;

        case 'P':
            flags |= FLAG_UPPER;
            /* fall thru */
        case 'p':
            flags |= FLAG_ALT;
            base = 16;
            /* Should share with has_unsigned switch code */
#if UINTPTR_MAX == UINT32_MAX
            /* OG: should test NULL and print (nil) prec at least 5 */
            lp = convert_uint32(buf + sizeof(buf),
                                (uint32_t)va_arg(ap, void *), base);
#elif UINTPTR_MAX == UINT64_MAX
            lp = convert_uint64(buf + sizeof(buf),
                                (uint64_t)va_arg(ap, void *), base);
#else
#error "cannot determine pointer size"
#endif
            goto patch_unsigned_conversion;

        case 'X':
            flags |= FLAG_UPPER;
            /* fall thru */
        case 'x':
            base = 16;
            goto has_unsigned;

        case 'o':
            base = 8;
            goto has_unsigned;

        case 'u':
            base = 10;

        has_unsigned:
            switch (size_flags) {
                int uint_value;

              case SIZE_char:
                uint_value = (unsigned char)va_arg(ap, unsigned int);
                goto convert_uint;

              case SIZE_short:
                uint_value = (unsigned short)va_arg(ap, unsigned int);
                goto convert_uint;

              case SIZE_int:
                uint_value = va_arg(ap, unsigned int);
              convert_uint:
                lp = convert_uint(buf + sizeof(buf), uint_value, base);
                break;
#ifdef WANT_long
              case SIZE_long:
                lp = convert_ulong(buf + sizeof(buf),
                                   va_arg(ap, unsigned long), base);
                break;
#endif
#ifdef WANT_int32
              case SIZE_int32:
                lp = convert_uint32(buf + sizeof(buf),
                                    va_arg(ap, uint32_t), base);
                break;
#endif
#ifdef WANT_int64
              case SIZE_int64:
                lp = convert_uint64(buf + sizeof(buf),
                                    va_arg(ap, uint64_t), base);
                break;
#endif
              default:
                {
                    /* do not know what to fetch, must ignore remaining
                     * formats specifiers.  This is really an error.
                     */
                    goto error;
                }
            }

        patch_unsigned_conversion:
            /* We may have the following parts to output:
             * - left padding with spaces    (left_pad)
             * - the 0x or 0X prefix if any  (buf, prefix_len)
             * - 0 padding                   (zero_pad)
             * - the converted number        (lp, len)
             * - right padding with spaces   (right_pad)
             */
            
            prefix_len = zero_pad = 0;

            len = buf + sizeof(buf) - lp;

            if (base == 16) {
                char *p;
                int alpha_shift;

                if ((flags & FLAG_ALT) && len > 0) {
                    buf[0] = '0';
                    buf[1] = (flags & FLAG_UPPER) ? 'X' : 'x';
                    prefix_len = 2;
                }
                alpha_shift = (flags & FLAG_UPPER) ?
                              'A' - '9' - 1: 'a' - '9' - 1;

                for (p = (char*)lp; p < buf + sizeof(buf); p++) {
                    if (*p > '9') {
                        *p += alpha_shift;
                    }
                }
            }

            if (len < prec) {
                if (prec == 1) {
                    /* special case number 0 */
                    *(char*)--lp = '0';
                    len++;
                } else {
                    zero_pad = prec - len;
                }
            }

            if (base == 8) {
                if ((flags & FLAG_ALT) && zero_pad == 0 &&
                      (len == 0 || *lp != '0')) {
                    *(char*)--lp = '0';
                    len++;
                }
            }

        apply_final_padding:

            left_pad = 0;

            if (width > prefix_len + zero_pad + len) {
                if (flags & FLAG_MINUS) {
                    right_pad = width - prefix_len - zero_pad - len;
                } else
                if (flags & FLAG_ZERO) {
                    zero_pad = width - prefix_len - len;
                } else {
                    left_pad = width - prefix_len - zero_pad - len;
                }
            }

            if (left_pad) {
                count = fmt_output_chars(stream, str, size, count,
                                         ' ', left_pad);
            }
            if (prefix_len) {
                /* prefix_len is 0, 1 or 2 */
                count = fmt_output_chunk(stream, str, size, count,
                                         buf, prefix_len);
            }
            if (zero_pad) {
                count = fmt_output_chars(stream, str, size, count,
                                         '0', zero_pad);
            }
            goto haslp;

        case 'e':
        case 'E':
        case 'f':
        case 'F':
        case 'g':
        case 'G':
        case 'a':
        case 'A':
            /* fetch double value */
            if (size_flags == SIZE_long_double) {
                va_arg(ap, long double);
            } else {
                va_arg(ap, double);
            }
            /* ignore floating point conversions */
            break;
        }
        continue;

    error:
        lp = format;
        len = strlen(format);
        format += len;
        goto haslp;
    }
done:
    if (!stream) {
        if (count < (int)size) {
            str[count] = '\0';
        } else
        if (size > 0) {
            str[size - 1] = '\0';
        }
    }
#if 0
    /* Unlock the stream.  */
    _IO_funlockfile (s);
    _IO_cleanup_region_end (0);
#endif

    return count;
}

/*---------------- printf functions ----------------*/

int iprintf(const char *format, ...)
{
    va_list ap;
    int n;

    va_start(ap, format);
    n = fmt_output(stdout, NULL, 0, format, ap);
    va_end(ap);
    
    return n;
}

int ifprintf(FILE *stream, const char *format, ...)
{
    va_list ap;
    int n;

    if (!stream) {
        errno = EBADF;
        return -1;
    }
    va_start(ap, format);
    n = fmt_output(stream, NULL, 0, format, ap);
    va_end(ap);
    
    return n;
}

int isnprintf(char *str, size_t size, const char *format, ...)
{
    va_list ap;
    int n;

    va_start(ap, format);
    n = fmt_output(NULL, str, size, format, ap);
    va_end(ap);
    
    return n;
}

int ivprintf(const char *format, va_list arglist)
{
    return fmt_output(stdout, NULL, 0, format, arglist);
}

int ivfprintf(FILE *stream, const char *format, va_list arglist)
{
    if (!stream) {
        errno = EBADF;
        return -1;
    }
    return fmt_output(stream, NULL, 0, format, arglist);
}

int ivsnprintf(char *str, size_t size, const char *format, va_list arglist)
{
    return fmt_output(NULL, str, size, format, arglist);
}
