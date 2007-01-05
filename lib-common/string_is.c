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

#include <ctype.h>
#include <errno.h>
#include <string.h>

#include "mem.h"
#include "string_is.h"

int strtoip(const char *p, const char **endp)
{
    int res = 0;
    bool neg = false;

    while (isspace(*p)) {
        p++;
    }

    if (p[0] == '+' || p[0] == '-') {
        neg = (p[0] == '-');
        p++;
    }

    while (isdigit(*p)) {
        res = 10 * res + (*p++ - '0');
    }

    if (endp) {
        *endp = p;
    }
    return neg ? -res : res;
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
        if (isspace(*p))
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
        if (value > max) {
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

/** Copies the string pointed to by <code>src</code> to the buffer
 * <code>dest</code> of <code>size</code> bytes.
 * If <code>dest</code> is not NULL and <code>size</code> is greater
 * than 0, a terminating '\0' character will be put at the end of
 * the copied string.
 *
 * The source and destination strings should not overlap.
 * No assumption should be made on the values of the characters
 * after the first '\0' character in the destination buffer.
 *
 * @return the length of the source string.
 * @see pstrcpylen
 */
ssize_t pstrcpy(char *dest, ssize_t size, const char *src)
{
#ifdef FAST_LIBC
    size_t len, clen;

    len = src ? strlen(src) : 0;
    if (dest && size > 0) {
        clen = len;
        if (clen > (size_t)size - 1)
            clen = (size_t)size - 1;
        memcpy(dest, src, clen); /* assuming no overlap */
        dest[clen] = '\0';
    }
    return (ssize_t)len;
#else
    const char *start = src;

    if (src) {
        if (dest && size > 0) {
            char *stop = dest + size - 1;
            while (dest < stop) {
                if ((*dest++ = *src++) == '\0')
                    return src - start - 1;
            }
            *dest = '\0';
        }
        while (*src++)
            continue;
        return src - start - 1;
    }
    if (dest && size > 0) {
        *dest = '\0';
    }
    return 0;
#endif
}

/** Copies at most <code>n</code> characters from the string pointed
 * to by <code>src</code> to the buffer <code>dest</code> of
 * <code>size</code> bytes.
 * If <code>dest</code> is not NULL and <code>size</code> is greater
 * than 0, a terminating '\0' character will be put at the end of
 * the copied string.
 *
 * The source and destination strings should not overlap.
 * No assumption should be made on the values of the characters
 * after the first '\0' character in the destination buffer.
 *
 * If <code>n</code> is negative, the whole string is copied.
 * @return the length of the source string or <code>n</code> if smaller
 * and positive.
 */
ssize_t pstrcpylen(char *dest, ssize_t size, const char *src, ssize_t n)
{
    size_t len, clen;

    len = 0;

    if (src) {
        if (n < 0) {
            len = strlen(src);
        } else {
            /* OG: RFE: Should use strnlen */
            const char *p = (const char *)memchr(src, '\0', n);
            len = p ? p - src : n;
        }
    }

    if (dest && size > 0) {
        clen = len;
        if (clen > (size_t)size - 1)
            clen = (size_t)size - 1;
        memcpy(dest, src, clen); /* assuming no overlap */
        dest[clen] = '\0';
    }
    return (ssize_t)len;
}

/** Appends the string pointed to by <code>src</code> at the end of
 * the string pointed to by <code>dest</code> not overflowing
 * <code>size</code> bytes.
 *
 * The source and destination strings should not overlap.
 * No assumption should be made on the values of the characters
 * after the first '\0' character in the destination buffer.
 *
 * If the destination buffer doesn't contain a properly '\0' terminated
 * string, dest is unchanged and the value returned is size+strlen(src).
 *
 * @return the length of the source string plus the length of the
 * destination string.
 */
ssize_t pstrcat(char *dest, ssize_t size, const char *src)
{
#ifdef FAST_LIBC
    size_t len, clen, dlen;

    dlen = 0;
    len = src ? strlen(src) : 0;

    if (dest && size > 0) {
        /* There is a problem here if the dest buffer is not properly
         * '\0' terminated.  Unlike BSD's strlcpy, we do not want do
         * read from dest beyond size, therefore we assume use size for
         * the value of dlen.  Calling pstrcat with various values of
         * size for the same dest and src may return different results.
         */
        /* OG: RFE: should use strnlen() */
        const char *p = memchr(dest, '\0', size);

        if (p == NULL) {
            dlen = size;
        } else {
            dlen = p - dest;
            clen = len;
            if (clen > (size_t)size - dlen - 1)
                clen = (size_t)size - dlen - 1;

            memcpy(dest + dlen, src, clen); /* assuming no overlap */
            dest[dlen + clen] = '\0';
        }
    }
    return (ssize_t)(dlen + len);
#else
    /* Assumes size > strlen(dest), ie well formed strings */
    size_t dlen = 0;

    if (dest) {
        char *start = dest;
        while (size > 0 && *dest != '\0') {
            dest++;
            size--;
        }
        dlen = dest - start;
    }
    return dlen + pstrcpy(dest, size, src);
#endif
}

/** Returns the length of <code>str</code> not overflowing
 * <code>size</code> bytes.
 *
 * @returns the length of the string, or -1 if the string does
 * not end in the first <code>size</code> bytes.
 */
ssize_t pstrlen(const char *str, ssize_t size)
{
    /* TODO: use memchr() */
    /* TODO: inconsistent semantics: should return size instead of -1 */
    ssize_t len;

    for (len = 0; len < size; len++) {
        if (!*str) {
            return len;
        }
        str++;
    }
    return -1;
}

/** Skips initial blanks as per isspace(c).
 *
 * use vskipspaces for non const parameters
 * @see vskipspaces
 *
 * @return a pointer to the first non white space character in s.
 */
const char *skipspaces(const char *s)
{
    while (isspace((unsigned char)*s)) {
        s++;
    }
    return s;
}

/** Go to the next blank as per isspace(c).
 *
 *
 * @return a pointer to the first white space character in s
 * or points to the end of the string.
 * 
 */
const char *strnextspace(const char *s)
{
    while (*s && !isspace((unsigned char)*s)) {
        s++;
    }
    return s;
}

/** Skips initial blanks as per isblank(c).
 *
 * use vskipblanks for non const parameters
 * @see vskipblanks
 *
 * @return a pointer to the first non white space character in s.
 */
#ifndef isblank
/* Glibc's ctype system issues a function call to handle locale issues.
 * Glibc only defines isblank() if USE_ISOC99, for some obsure reason,
 * isblank() cannot be defined as an inline either (intrinsic
 * function?)
 * Should rewrite these functions and use our own simpler version
 */
#if defined(__isctype) && defined(_ISbit)    /* Glibc */
#define isblank(c)      __isctype((c), _ISblank)
#else
static inline int isblank(c) { return (c == ' ' || c == '\t'); }
#endif
#endif

const char *skipblanks(const char *s)
{
    while (isblank((unsigned char)*s)) {
        s++;
    }
    return s;
}

/** Replaces blank characters at end of string with '\0'.
 *
 * @return a pointer to the \0 at the end of str
 */
char *strrtrim(char *str)
{
    if (str) {
        char *p = str + strlen(str);

        while (p > str && isspace((unsigned char)p[-1])) {
            *--p = '\0';
        }
        return p;
    }

    return NULL;
}

/** Tells whether str begins with p.
 *
 * @param pp if not null and str begins with p, pp is given the address of the
 * first following character in str.
 *
 * @return 1 if a match is found, 0 otherwise.
 */
int strstart(const char *str, const char *p, const char **pp)
{
    if (!str)
        return 0;

    while (*p) {
        if (*str++ != *p++)
            return 0;
    }
    if (pp)
        *pp = str;
    return 1;
}

/** Tells whether str begins with p, case insensitive.
 *
 * @param pp if not null and str begins with p, pp is given the address of the
 * first following character in str.
 *
 * @return 1 if a match is found, 0 otherwise.
 */
int stristart(const char *str, const char *p, const char **pp)
{
    if (!str)
        return 0;

    while (*p) {
        if (tolower(*str++) != tolower(*p++))
            return 0;
    }
    if (pp)
        *pp = str;
    return 1;
}

/** Find the first occurrence of the substring needle in str, case
 *  insensitively.
 *
 * @return a pointer to the beginning of the substring, or NULL if
 * non was found.
 * a final embedded '\0' char in needle can match the final '\0'.
 */
const char *stristrn(const char *str, const char *needle, size_t nlen)
{
    size_t i;
    int c, nc;

    if (!nlen)
        return str;

    nc = toupper(*needle);
    for (;; str++) {
        /* find first char of pattern in str */
        c = toupper(*str);
        if (c != nc) {
            if (c == '\0')
                return NULL;
        } else {
            /* compare the rest of needle */
            for (i = 1;; i++) {
                if (i == nlen)
                    return str;
                if (c == '\0')
                    return NULL;
                c = toupper(str[i]);
                if (c != toupper(needle[i]))
                    break;
            }
        }
    }
}

#if 0
bool strequal(const char *str1, const char *str2)
{
    while (*str1 == *str2) {
        if (!*str1)
            return true;
        str1++;
        str2++;
    }
    return false;
}
#endif

/** Find the first occurrence of the needle in haystack.
 *
 * @return a pointer to the beginning of needle, or NULL if
 * it was not found.
 */
const void *memsearch(const void *_haystack, size_t hsize,
                      const void *_needle, size_t nsize)
{
    const unsigned char *haystack = (const unsigned char *)_haystack;
    const unsigned char *needle = (const unsigned char *)_needle;
    const unsigned char *last;
    unsigned char first;
    size_t pos;

    if (nsize == 0) {
        return haystack;
    }

    if (nsize == 1) {
        return memchr(haystack, *needle, hsize);
    }

    first = *needle;

    for (last = haystack + (hsize - nsize + 1); haystack < last; haystack++) {
        if (*haystack == first) {
            for (pos = 0;;) {
                if (++pos >= nsize)
                    return haystack;
                if (haystack[pos] != needle[pos])
                    break;
            }
        }
    }
    return NULL;
}

/*}}}*/
/*[ CHECK ]::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::{{{*/
#ifdef CHECK
/* {{{*/
#include <check.h>

static const char *week = "Monday Tuesday Wednesday Thursday Friday "
                          "Saturday Sunday";
START_TEST(check_strstart)
{
    const char *p;
    int res;
    res = strstart(week, "Monday", &p);
    fail_if(!res, "strstart did not find needle");
    fail_if(p != week + strlen("Monday"),
            "strstart did not set pp correctly", week, p);

    p = NULL;
    res = strstart(week, "Tuesday", &p);
    fail_if(res, "strstart did not fail");
    fail_if(p != NULL, "strstart did set pp");
}
END_TEST

START_TEST(check_stristart)
{
    const char *p;
    int res;
    res = stristart(week, "monDAY", &p);
    fail_if(!res, "stristart did not find needle");
    fail_if(p != week + strlen("MonDAY"),
            "stristart did not set pp correctly", week, p);

    p = NULL;
    res = stristart(week, "tUESDAY", &p);
    fail_if(res, "stristart did not fail");
    fail_if(p != NULL, "stristart did set pp");
}
END_TEST

static const char *alphabet = "abcdefghijklmnopqrstuvwxyz";
START_TEST(check_stristr)
{
    const char *p;

    p = stristr(alphabet, "aBC");
    fail_if(p != alphabet, "not found at start of string");

    p = stristr(alphabet, "Z");
    fail_if(p != alphabet + 25, "not found at end of string");

    p = stristr(alphabet, "mn");
    fail_if(p != alphabet + 12, "not found in the middle of the string");

    p = stristr(alphabet, "123");
    fail_if(p != NULL, "unexistant string found");
}
END_TEST

START_TEST(check_memsearch)
{
    const void *p;

    p = memsearch(alphabet, 5, "abcdef", 5);
    fail_if(p != alphabet, "exact match not found");

    p = memsearch(alphabet, 26, alphabet, 26);
    fail_if(p != alphabet, "exact match not found");

    p = memsearch(alphabet, 5, "ab", 2);
    fail_if(p != alphabet, "not found at start of zone");

    p = memsearch(alphabet, 26, "yz", 2);
    fail_if(p != alphabet + 24, "2 byte string not found at end of zone");

    p = memsearch(alphabet, 26, "uvwxyz", 6);
    fail_if(p != alphabet + 20, "6 byte string not found at end of zone");

    p = memsearch(alphabet, 26, "mn", 2);
    fail_if(p != alphabet + 12, "not found in the middle of the zone");

    p = memsearch(alphabet, 26, "123", 3);
    fail_if(p != NULL, "found bogus occurrence");

    p = memsearch(alphabet, 0, "ab", 2);
    fail_if(p != NULL, "match found in empty zone");

    p = memsearch(alphabet, 0, "ab", 0);
    fail_if(p != alphabet, "empty needle not found in empty zone");

    p = memsearch(alphabet, 1, "ab", 2);
    fail_if(p != NULL, "found partial match");

    p = memsearch(alphabet, 5, "ab", 0);
    fail_if(p != alphabet, "empty needle not found in non-empty zone");
}
END_TEST

START_TEST(check_pstrlen)
{
    const char *p = NULL;
    fail_if (pstrlen("123", 4) != 3, "pstrlen \"123\", 4 failed");
    fail_if (pstrlen("", 4) != 0, "pstrlen \"\", 4 failed");
    fail_if (pstrlen(p, 0) != -1, "pstrlen NULL, 0 failed");
    fail_if (pstrlen("abc\0def", 2) != -1, "pstrlen \"abc<NIL>def\", 2 failed");
    fail_if (pstrlen("abc\0def", 6) != 3, "pstrlen \"abc<NIL>def\", 6 failed");
}
END_TEST

#define check_strtolp_unit(p, flags, min, max, val_exp, ret_exp, end_i) \
    do {                                                                \
        const char *endp;                                               \
        int ret;                                                        \
        long val;                                                       \
                                                                        \
        ret = strtolp(p, &endp, 0, &val, flags, min, max);              \
                                                                        \
        fail_if (ret != ret_exp,                                        \
                 "('%s', %d, %ld<= XXX <= %ld. %ld, %d, %d)"            \
                 "ret=%d (expected %d)\n",                              \
                 p, flags, min, max, val_exp, ret_exp, end_i,           \
                 ret, ret_exp);                                         \
                                                                        \
        if (ret == 0) {                                                 \
            fail_if (val != val_exp,                                    \
                     "('%s', %d, %ld<= XXX <= %ld. %ld, %d, %d)"        \
                     "val=%ld (expected %ld)\n",                        \
                     p, flags, min, max, val_exp, ret_exp, end_i,       \
                     val, val_exp);                                     \
        }                                                               \
    } while (0)

START_TEST(check_strtolp)
{
    check_strtolp_unit("123", 0,
                       0, 1000,
                       123, 0, 3);

    /* Check min/max */
    check_strtolp_unit("123", STRTOLP_CHECK_RANGE,
                       0, 100,
                       123, -ERANGE, 3);
    check_strtolp_unit("123", STRTOLP_CHECK_RANGE,
                       1000, 2000,
                       123, -ERANGE, 3);

    /* check min/max corner cases */
    check_strtolp_unit("123", STRTOLP_CHECK_RANGE,
                       0, 123,
                       123, 0, 3);
    check_strtolp_unit("123", STRTOLP_CHECK_RANGE,
                       0, 122,
                       123, -ERANGE, 3);
    check_strtolp_unit("123", STRTOLP_CHECK_RANGE,
                       123, 1000,
                       123, 0, 3);
    check_strtolp_unit("123", STRTOLP_CHECK_RANGE,
                       124, 1000,
                       123, -ERANGE, 3);

    /* Check skipspaces */
    check_strtolp_unit(" 123", 0,
                       0, 1000,
                       123, -EINVAL, 3);

    check_strtolp_unit("123 ", STRTOLP_CHECK_END,
                       0, 100,
                       123, -EINVAL, 3);

    check_strtolp_unit(" 123 ", STRTOLP_CHECK_END | STRTOLP_CHECK_RANGE,
                       0, 100,
                       123, -EINVAL, 3);

    check_strtolp_unit(" 123", STRTOLP_IGNORE_SPACES,
                       0, 100,
                       123, 0, 3);

    check_strtolp_unit(" 123 ", STRTOLP_IGNORE_SPACES,
                       0, 100,
                       123, 0, 4);

    check_strtolp_unit(" 123 ", STRTOLP_IGNORE_SPACES | STRTOLP_CHECK_RANGE,
                       0, 100,
                       123, -ERANGE, 3);

    check_strtolp_unit(" 123 ", STRTOLP_IGNORE_SPACES | STRTOLP_CLAMP_RANGE,
                       0, 100,
                       100, 0, 3);

    check_strtolp_unit("123456789012345678901234567890", 0,
                       0, 100,
                       123, -ERANGE, 3);

    check_strtolp_unit("123456789012345678901234567890 ", STRTOLP_CHECK_END,
                       0, 100,
                       123, -EINVAL, 3);

    check_strtolp_unit("123456789012345678901234567890", STRTOLP_CLAMP_RANGE,
                       0, 100,
                       100, 0, 3);
}
END_TEST

Suite *check_string_is_suite(void)
{
    Suite *s  = suite_create("String");
    TCase *tc = tcase_create("Core");

    suite_add_tcase(s, tc);
    tcase_add_test(tc, check_strstart);
    tcase_add_test(tc, check_stristart);
    tcase_add_test(tc, check_stristr);
    tcase_add_test(tc, check_memsearch);
    tcase_add_test(tc, check_pstrlen);
    tcase_add_test(tc, check_strtolp);
    return s;
}

/*.....................................................................}}}*/
#endif
/*:::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::}}}*/
