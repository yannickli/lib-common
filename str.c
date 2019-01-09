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
 *
 * RFE/RFC: In a lot of cases, we do not care that much about the length of
 * the source string. What we want is "has the string been truncated
 * and we should stop here, or is everything fine ?". Therefore, may be
 * we need a function like :
 * int pstrcpy_ok(char *dest, ssize_t size, const char *src)
 * {
 *     if (pstrcpy(dest, size, src) < size) {
 *        return 0;
 *     } else {
 *        return 1;
 *     }
 * }
 *
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

/** Counts the number of occurrences of character <code>c</code>
 * in string <code>str</code>.
 *
 * @returns the number of occurrences, returns 0 if <code>size</code>
 * is <code>NULL</code>.
 */
int pstrchrcount(const char *str, int c)
{
    const char *p = str;
    int res = 0;

    if (p) {
        while ((p = strchr(p, c)) != NULL) {
            res++;
            p++;
        }
    }

    return res;
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
        if (tolower((unsigned char)*str++) != tolower((unsigned char)*p++))
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

    nc = toupper((unsigned char)*needle);
    for (;; str++) {
        /* find first char of pattern in str */
        c = toupper((unsigned char)*str);
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
                c = toupper((unsigned char)str[i]);
                if (c != toupper((unsigned char)needle[i]))
                    break;
            }
        }
    }
}

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

const void *pmemrchr(const void *s, int c, ssize_t n)
{
    for (const char *p = (const char *)s + n; p-- > (const char *)s;) {
        if (*p == c)
            return p;
    }
    return NULL;
}


/* find a word in a list of words separated by sep.
 */
bool strfind(const char *keytable, const char *str, int sep)
{
    int c, len;
    const char *p;

    c = *str;
    len = strlen(str);
    /* need to special case the empty string */
    if (len == 0) {
        char empty[3];
        empty[0] = empty[1] = sep;
        empty[2] = '\0';
        return strstr(keytable, empty) != NULL;
    }

    /* initial and trailing separators are optional */
    /* they do not cause the empty string to match */
    for (p = keytable;;) {
        if (!memcmp(p, str, len) && (p[len] == sep || p[len] == '\0'))
            return true;
        for (;;) {
            p = strchr(p + 1, c);
            if (!p)
                return false;
            if (p[-1] == sep)
                break;
        }
    }
}

/** Increment last counter in a buffer
 *
 * <code>buf</code> points to the start of the buffer.
 * <code>len</code> can be negative, in this case <code>len=strlen(buf)</code>
 *
 * Examples :
 *  "000" => "001"
 *  "999" => "000"
 *  "foobar-0-01" => "foobar-0-02"
 *  "foobar-0-99" => "foobar-0-00"
 *
 * @return 1 if an overflow occurred, 0 otherwise
 */
int buffer_increment(char *buf, int len)
{
    int pos;

    if (!buf) {
        return 1;
    }
    if (len < 0) {
        len = strlen(buf);
    }
    for (pos = len - 1; pos >= 0; pos--) {
        switch (buf[pos]) {
          case '0': case '1': case '2': case '3': case '4': case '5':
          case '6': case '7': case '8':
            buf[pos]++;
            return 0;
          case '9':
            buf[pos] = '0';
            break;
          default:
            return 1;
        }
    }
    return 1;
}

/** Increment last counter in an hexadecimal buffer
 *
 * <code>buf</code> points to the start of the buffer.
 * <code>len</code> can be negative, in this case <code>len=strlen(buf)</code>
 *
 * Examples :
 *  "000" => "001"
 *  "999" => "99A"
 *  "foobar-0-01" => "foobar-0-02"
 *  "foobar-0-FF" => "foobar-0-00"
 *
 * @return 1 if an overflow occurred, 0 otherwise
 */
int buffer_increment_hex(char *buf, int len)
{
    int pos;

    if (!buf) {
        return 1;
    }
    if (len < 0) {
        len = strlen(buf);
    }
    for (pos = len - 1; pos >= 0; pos--) {
        switch (buf[pos]) {
          case '0': case '1': case '2': case '3': case '4': case '5':
          case '6': case '7': case '8':
            buf[pos]++;
            return 0;
          case '9':
            buf[pos] = 'A';
            return 0;
          case 'a': case 'A': case 'b': case 'B': case 'c': case 'C':
          case 'd': case 'D': case 'e': case 'E':
            buf[pos]++;
            return 0;
          case 'F': case 'f':
            buf[pos] = '0';
            break;
          default:
            return 1;
        }
    }
    return 1;
}

/** Put random hexadecimal digits in destination buffer
 *
 * @return the number of digits set.
 */
ssize_t pstrrand(char *dest, ssize_t size, int offset, ssize_t n)
{
    char *p;
    const char *last;
    static const char hex[16] = "0123456789ABCDEF";

    n = MIN(size - offset - 1, n);
    if (n < 0) {
        return -1;
    }

    /* RFE: This is very naive. Should at least call ha_rand() only every 4
     * bytes. */
    last = dest + offset + n;
    for (p = dest + offset; p < last; p++) {
        *p = hex[ha_rand_range(0, 15)];
    }
    *p = '\0';
    return n;
}

int str_replace(const char search, const char replace, char *subject)
{
    int nb_replace = 0;
    char *p = subject;

    p = strchr(p, search);
    while (p && *p != '\0') {
        *p = replace;
        /* Read replace char */
        p++;
        nb_replace++;
        p = strchr(p, search);
    }
    return nb_replace;
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
    const char *p = NULL;
    int res;
    res = strstart(week, "Monday", &p);
    fail_if(!res, "strstart did not find needle");
    fail_if(p != week + strlen("Monday"),
            "strstart did not set pp correctly"
            " (week=\"%s\", p=\"%s\")", week, p);

    p = NULL;
    res = strstart(week, "Tuesday", &p);
    fail_if(res, "strstart did not fail");
    fail_if(p != NULL, "strstart did set pp");
}
END_TEST

START_TEST(check_stristart)
{
    const char *p = NULL;
    int res;
    res = stristart(week, "monDAY", &p);
    fail_if(!res, "stristart did not find needle");
    fail_if(p != week + strlen("MonDAY"),
            "stristart did not set pp correctly"
            " (week=\"%s\", p=\"%s\")", week, p);

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

START_TEST(check_pstrcpylen)
{
    char p[128];
    fail_if (pstrcpylen(p, sizeof(p), "123", 4) != 3,
             "pstrcpylen \"123\", 4 failed");
    fail_if (pstrcpylen(p, sizeof(p), "123", -1) != 3,
             "pstrcpylen \"123\", -1 failed");
}
END_TEST

START_TEST(check_pstrchrcount)
{
    fail_if(pstrchrcount("1111", '1') != 4, "bad count 1");
    fail_if(pstrchrcount("1221", '1') != 2, "bad count 2");
    fail_if(pstrchrcount("2222", '1') != 0, "bad count 3");
    fail_if(pstrchrcount(NULL,   '1') != 0, "bad count 4");
    fail_if(pstrchrcount("a|b|c|d|", '|') != 4, "bad count 5");
}
END_TEST

#define check_strtoip_unit(p, err_exp, val_exp, end_i)                  \
    do {                                                                \
        const char *endp;                                               \
        int val;                                                        \
        int end_exp = (end_i >= 0) ? end_i : (int)strlen(p);            \
                                                                        \
        errno = 0;                                                      \
        val = strtoip(p, &endp);                                        \
                                                                        \
        fail_if (err_exp != errno || val != val_exp || endp != p + end_exp, \
                 "('%s', &endp)"                                        \
                 "val=%d (expected %d), endp='%s' expected '%s'\n",     \
                 p, val, val_exp, endp, p + end_exp);                   \
    } while (0)

START_TEST(check_strtoip)
{
    check_strtoip_unit("123", 0, 123, -1);
    check_strtoip_unit(" 123", 0, 123, -1);
    check_strtoip_unit(" +123", 0, 123, -1);
    check_strtoip_unit("  -123", 0, -123, -1);
    check_strtoip_unit(" +-123", EINVAL, 0, 2);
    check_strtoip_unit("123 ", 0, 123, 3);
    check_strtoip_unit("123z", 0, 123, 3);
    check_strtoip_unit("123+", 0, 123, 3);
    check_strtoip_unit("2147483647", 0, 2147483647, -1);
    check_strtoip_unit("2147483648", ERANGE, 2147483647, -1);
    check_strtoip_unit("21474836483047203847094873", ERANGE, 2147483647, -1);
    check_strtoip_unit("000000000000000000000000000000000001", 0, 1, -1);
    check_strtoip_unit("-2147483647", 0, -2147483647, -1);
    check_strtoip_unit("-2147483648", 0, -2147483647 - 1, -1);
    check_strtoip_unit("-2147483649", ERANGE, -2147483647 - 1, -1);
    check_strtoip_unit("-21474836483047203847094873", ERANGE, -2147483647 - 1, -1);
    check_strtoip_unit("-000000000000000000000000000000000001", 0, -1, -1);
    check_strtoip_unit("", EINVAL, 0, -1);
    check_strtoip_unit("          ", EINVAL, 0, -1);
    check_strtoip_unit("0", 0, 0, -1);
    check_strtoip_unit("0x0", 0, 0, 1);
    check_strtoip_unit("010", 0, 10, -1);
}
END_TEST

#define check_memtoip_unit(p, err_exp, val_exp, end_i)                  \
    do {                                                                \
        const byte *endp;                                               \
        int val, len = strlen(p);                                       \
        int end_exp = (end_i >= 0) ? end_i : len;                       \
                                                                        \
        errno = 0;                                                      \
        val = memtoip((const byte *)p, len, &endp);                     \
                                                                        \
        fail_if (err_exp != errno || val != val_exp || endp != (const byte *)p + end_exp, \
                 "(\"%s\", %d, &endp)\n -> "                            \
                 "val=%d (expected %d), endp='%s' (expected '%s'), "    \
                 "errno=%d (expected %d)\n",                            \
                 p, len, val, val_exp, endp, p + end_exp, errno, err_exp); \
    } while (0)

START_TEST(check_memtoip)
{
    check_memtoip_unit("123", 0, 123, -1);
    check_memtoip_unit(" 123", 0, 123, -1);
    check_memtoip_unit(" +123", 0, 123, -1);
    check_memtoip_unit("  -123", 0, -123, -1);
    check_memtoip_unit(" +-123", EINVAL, 0, 2);
    check_memtoip_unit("123 ", 0, 123, 3);
    check_memtoip_unit("123z", 0, 123, 3);
    check_memtoip_unit("123+", 0, 123, 3);
    check_memtoip_unit("2147483647", 0, 2147483647, -1);
    check_memtoip_unit("2147483648", ERANGE, 2147483647, -1);
    check_memtoip_unit("21474836483047203847094873", ERANGE, 2147483647, -1);
    check_memtoip_unit("000000000000000000000000000000000001", 0, 1, -1);
    check_memtoip_unit("-2147483647", 0, -2147483647, -1);
    check_memtoip_unit("-2147483648", 0, -2147483647 - 1, -1);
    check_memtoip_unit("-2147483649", ERANGE, -2147483647 - 1, -1);
    check_memtoip_unit("-21474836483047203847094873", ERANGE, -2147483647 - 1, -1);
    check_memtoip_unit("-000000000000000000000000000000000001", 0, -1, -1);
    check_memtoip_unit("", EINVAL, 0, -1);
    check_memtoip_unit("          ", EINVAL, 0, -1);
    check_memtoip_unit("0", 0, 0, -1);
    check_memtoip_unit("0x0", 0, 0, 1);
    check_memtoip_unit("010", 0, 10, -1);
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
        fail_if(ret != ret_exp,                                         \
                "(\"%s\", flags=%d, min=%ld, max=%ld, val_exp=%ld, ret_exp=%d, end_i=%d)" \
                " -> ret=%d (expected %d)\n",                           \
                p, flags, (long)(min), (long)(max), (long)(val_exp),    \
                ret_exp, end_i, ret, ret_exp);                          \
                                                                        \
        if (ret == 0) {                                                 \
            fail_if(val != val_exp,                                     \
                    "(\"%s\", flags=%d, min=%ld, max=%ld, val_exp=%ld, ret_exp=%d, end_i=%d)" \
                    " -> val=%ld (expected %ld)\n",                     \
                    p, flags, (long)(min), (long)(max), (long)(val_exp),\
                    ret_exp, end_i, (long)(val), (long)(val_exp));      \
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

START_TEST(check_strfind)
{
    fail_if(strfind("1,2,3,4", "1", ',') != true, "");
    fail_if(strfind("1,2,3,4", "2", ',') != true, "");
    fail_if(strfind("1,2,3,4", "4", ',') != true, "");
    fail_if(strfind("11,12,13,14", "1", ',') != false, "");
    fail_if(strfind("11,12,13,14", "2", ',') != false, "");
    fail_if(strfind("11,12,13,14", "11", ',') != true, "");
    fail_if(strfind("11,12,13,14", "111", ',') != false, "");
    fail_if(strfind("toto,titi,tata,tutu", "to", ',') != false, "");
    fail_if(strfind("1|2|3|4|", "", '|') != false, "");
    fail_if(strfind("1||3|4|", "", '|') != true, "");
}
END_TEST

#define check_buffer_increment_unit(initval, expectedval, expectedret)       \
    do {                                                                     \
        pstrcpy(buf, sizeof(buf), initval);                                  \
        ret = buffer_increment(buf, -1);                                     \
        fail_if(strcmp(buf, expectedval),                                    \
            "value is \"%s\", expecting \"%s\"", buf, expectedval);          \
        fail_if(ret != expectedret, "bad return value for \"%s\"", initval); \
    } while (0)
START_TEST(check_buffer_increment)
{
    char buf[32];
    int ret;
    check_buffer_increment_unit("0", "1", 0);
    check_buffer_increment_unit("1", "2", 0);
    check_buffer_increment_unit("00", "01", 0);
    check_buffer_increment_unit("42", "43", 0);
    check_buffer_increment_unit("09", "10", 0);
    check_buffer_increment_unit("99", "00", 1);
    check_buffer_increment_unit(" 99", " 00", 1);
    check_buffer_increment_unit("", "", 1);
    check_buffer_increment_unit("foobar-00", "foobar-01", 0);
    check_buffer_increment_unit("foobar-0-99", "foobar-0-00", 1);
}
END_TEST

#define check_buffer_increment_hex_unit(initval, expectedval, expectedret)   \
    do {                                                                     \
        pstrcpy(buf, sizeof(buf), initval);                                  \
        ret = buffer_increment_hex(buf, -1);                                 \
        fail_if(strcmp(buf, expectedval),                                    \
            "value is \"%s\", expecting \"%s\"", buf, expectedval);          \
        fail_if(ret != expectedret, "bad return value for \"%s\"", initval); \
    } while (0)
START_TEST(check_buffer_increment_hex)
{
    char buf[32];
    int ret;
    check_buffer_increment_hex_unit("0", "1", 0);
    check_buffer_increment_hex_unit("1", "2", 0);
    check_buffer_increment_hex_unit("9", "A", 0);
    check_buffer_increment_hex_unit("a", "b", 0);
    check_buffer_increment_hex_unit("Ab", "Ac", 0);
    check_buffer_increment_hex_unit("00", "01", 0);
    check_buffer_increment_hex_unit("42", "43", 0);
    check_buffer_increment_hex_unit("09", "0A", 0);
    check_buffer_increment_hex_unit("0F", "10", 0);
    check_buffer_increment_hex_unit("FFF", "000", 1);
    check_buffer_increment_hex_unit(" FFF", " 000", 1);
    check_buffer_increment_hex_unit("FFFFFFFFFFFFFFF", "000000000000000", 1);
    check_buffer_increment_hex_unit("", "", 1);
    check_buffer_increment_hex_unit("foobar", "foobar", 1);
    check_buffer_increment_hex_unit("foobaff", "foobb00", 0);
    check_buffer_increment_hex_unit("foobar-00", "foobar-01", 0);
    check_buffer_increment_hex_unit("foobar-0-ff", "foobar-0-00", 1);
}
END_TEST

START_TEST(check_pstrrand)
{
    char buf[32];
    int n, ret;

    for (n = 0; n < countof(buf); n++) {
        buf[n] = 'B' + n;
    }

    ret = pstrrand(buf, sizeof(buf), 0, 0);
    fail_if(buf[0] != '\0', "Missing padding after len=0");
    fail_if(ret != 0, "Bad return value for len=0");

    ret = pstrrand(buf, sizeof(buf), 0, 3);
    fail_if(buf[3] != '\0', "Missing padding after len=3");
    fail_if(ret != 3, "Bad return value for len=3");

    /* Ask for 32 bytes, where buffer can only contain 31. */
    ret = pstrrand(buf, sizeof(buf), 0, sizeof(buf));
    fail_if(buf[31] != '\0', "Missing padding after len=sizeof(buf)");
    fail_if(ret != sizeof(buf) - 1, "Bad return value for len=sizeof(buf)");
    //fprintf(stderr, "buf:%s\n", buf);

    buf[0] = buf[1] = buf[2] = 'Z';
    buf[3] = buf[4] = buf[5] = buf[6] = 0x42;
    ret = pstrrand(buf, sizeof(buf), 3, 2);
    fail_if(buf[3] == 0x42 || buf[4] == 0x42, "len=2 did not set buffer");
    fail_if(buf[5] != 0, "Missing 0 after len=2");
    fail_if(buf[6] != 0x42, "len=2 set the buffer incorrectly");
    fail_if(ret != 2, "Bad return value for len=2");
    //fprintf(stderr, "buf:%s\n", buf);
}
END_TEST

Suite *check_string_suite(void)
{
    Suite *s  = suite_create("String");
    TCase *tc = tcase_create("Core");

    suite_add_tcase(s, tc);
    tcase_add_test(tc, check_strstart);
    tcase_add_test(tc, check_stristart);
    tcase_add_test(tc, check_stristr);
    tcase_add_test(tc, check_memsearch);
    tcase_add_test(tc, check_strfind);
    tcase_add_test(tc, check_pstrlen);
    tcase_add_test(tc, check_pstrcpylen);
    tcase_add_test(tc, check_pstrchrcount);
    tcase_add_test(tc, check_strtoip);
    tcase_add_test(tc, check_memtoip);
    tcase_add_test(tc, check_strtolp);
    tcase_add_test(tc, check_buffer_increment);
    tcase_add_test(tc, check_buffer_increment_hex);
    tcase_add_test(tc, check_pstrrand);
    return s;
}

/*.....................................................................}}}*/
#endif
/*:::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::}}}*/
