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

static ALWAYS_INLINE int64_t
memtoip_impl(const byte *s, int _len, const byte **endp,
             const int64_t min, const int64_t max, bool ll, bool use_len)
{
    int64_t value = 0;

#define len ((use_len) ? _len : INT_MAX)
#define declen ((use_len) ? --_len : INT_MAX)

    if (!s || len < 0) {
        errno = EINVAL;
        goto done;
    }
    while (len && isspace((unsigned char)*s)) {
        s++;
        declen;
    }
    if (!len) {
        errno = EINVAL;
        goto done;
    }
    if (*s == '-') {
        s++;
        declen;
        if (!len || !isdigit((unsigned char)*s)) {
            errno = EINVAL;
            goto done;
        }
        value = '0' - *s++;
        while (declen && isdigit((unsigned char)*s)) {
            int digit = '0' - *s++;
            if ((value <= min / 10)
                &&  (value < min / 10 || digit < min % 10)) {
                errno = ERANGE;
                value = min;
                /* keep looping inefficiently in case of overflow */
            } else {
                value = value * 10 + digit;
            }
        }
    } else {
        if (*s == '+') {
            s++;
            declen;
        }
        if (!len || !isdigit((unsigned char)*s)) {
            errno = EINVAL;
            goto done;
        }
        value = *s++ - '0';
        while (declen && isdigit((unsigned char)*s)) {
            int digit = *s++ - '0';
            if ((value >= max / 10)
                &&  (value > max / 10 || digit > max % 10)) {
                errno = ERANGE;
                value = max;
                /* keep looping inefficiently in case of overflow */
            } else {
                value = value * 10 + digit;
            }
        }
    }
done:
    if (endp) {
        *endp = s;
    }
    return value;
#undef len
#undef declen
}

int strtoip(const char *s, const char **endp)
{
    return memtoip_impl((const byte *)s, 0, (const byte **)(void *)endp,
                        INT_MIN, INT_MAX, false, false);
}

int memtoip(const void *s, int len, const byte **endp)
{
    return memtoip_impl(s, len, endp, INT_MIN, INT_MAX, false, true);
}

int64_t memtollp(const void *s, int len, const byte **endp)
{
    return memtoip_impl(s, len, endp, INT64_MIN, INT64_MAX, true, true);
}

#define INVALID_NUMBER  INT64_MIN
int64_t parse_number(const char *str)
{
    int64_t value;
    int64_t mult = 1;
    int frac = 0;
    int denom = 1;
    int exponent;

    value = strtoll(str, &str, 0);
    if (*str == '.') {
        for (str++; isdigit((unsigned char)*str); str++) {
            if (denom <= (INT_MAX / 10)) {
                frac = frac * 10 + *str - '0';
                denom *= 10;
            }
        }
    }
    switch (*str) {
      case 'P':
        mult <<= 10;
        /* FALL THRU */
      case 'T':
        mult <<= 10;
        /* FALL THRU */
      case 'G':
        mult <<= 10;
        /* FALL THRU */
      case 'M':
        mult <<= 10;
        /* FALL THRU */
      case 'K':
        mult <<= 10;
        str++;
        break;
      case 'p':
        mult *= 1000;
        /* FALL THRU */
      case 't':
        mult *= 1000;
        /* FALL THRU */
      case 'g':
        mult *= 1000;
        /* FALL THRU */
      case 'm':
        mult *= 1000;
        /* FALL THRU */
      case 'k':
        mult *= 1000;
        str++;
        break;
      case 'E':
      case 'e':
        exponent = strtol(str + 1, &str, 10);
        for (; exponent > 0; exponent--) {
            if (mult > (INT64_MAX / 10))
                return INVALID_NUMBER;
            mult *= 10;
        }
        break;
    }
    if (*str != '\0') {
        return INVALID_NUMBER;
    }
    /* Catch most overflow cases */
    if ((value | mult) > INT32_MAX && INT64_MAX / mult > value) {
        return INVALID_NUMBER;
    }
    return value * mult + frac * mult / denom;
}

/** Parses a string to extract a long, checking some constraints.
 * <code>res</code> points to the destination of the long value
 * <code>p</code> points to the string to parse
 * <code>endp</code> points to the end of the parse (the next char to
 *   parse, after the value. spaces after the value are skipped if
 *   STRTOLP_IGNORE_SPACES is set)
 * <code>min</code> and <code>max</code> are extrema values (only checked
 *   if STRTOLP_CHECK_RANGE is set.
 * <code>dest</code> of <code>size</code> bytes.
 *
 * If STRTOLP_IGNORE_SPACES is set, leading and trailing spaces are
 * considered normal and skipped. If not set, then even leading spaces
 * lead to a failure.
 *
 * If STRTOLP_CHECK_END is set, the end of the value is supposed to
 * be the end of the string.
 *
 * @returns 0 if all constraints are met. Otherwise returns a negative
 * value corresponding to the error
 */
int strtolp(const char *p, const char **endp, int base, long *res,
            int flags, long min, long max)
{
    const char *end;
    long value;
    bool clamped;

    if (!endp) {
        endp = &end;
    }
    if (!res) {
        res = &value;
    }
    if (flags & STRTOLP_IGNORE_SPACES) {
        p = skipspaces(p);
    } else {
        if (isspace((unsigned char)*p))
            return -EINVAL;
    }
    errno = 0;
    *res = strtol(p, endp, base);
    if (flags & STRTOLP_IGNORE_SPACES) {
        *endp = skipspaces(*endp);
    }
    if ((flags & STRTOLP_CHECK_END) && **endp != '\0') {
        return -EINVAL;
    }
    if (*endp == p && !(flags & STRTOLP_EMPTY_OK)) {
        return -EINVAL;
    }
    clamped = false;
    if (flags & STRTOLP_CLAMP_RANGE) {
        if (*res < min) {
            *res = min;
            clamped = true;
        } else
        if (*res > max) {
            *res = max;
            clamped = true;
        }
        if (errno == ERANGE)
            errno = 0;
    }
    if (errno) {
        return -errno;  /* -ERANGE or maybe EINVAL as checked by strtol() */
    }
    if (flags & STRTOLP_CHECK_RANGE) {
        if (clamped || *res < min || *res > max) {
            return -ERANGE;
        }
    }
    return 0;
}
