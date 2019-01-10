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

TEST_DECL("str: strstart", 0)
{
    static const char *week =
        "Monday Tuesday Wednesday Thursday Friday Saturday Sunday";
    const char *p = NULL;
    int res;

    res = strstart(week, "Monday", &p);
    TEST_FAIL_IF(!res, "finding Monday in week");
    TEST_FAIL_IF(p != week + strlen("Monday"), "finding Monday at the proper position");

    p = NULL;
    res = strstart(week, "Tuesday", &p);
    TEST_FAIL_IF(res, "week doesn't start with Tuesday");
    TEST_DONE();
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

TEST_DECL("str: stristart", 0)
{
    static const char *week =
        "Monday Tuesday Wednesday Thursday Friday Saturday Sunday";
    const char *p = NULL;
    int res;

    res = stristart(week, "monDay", &p);
    TEST_FAIL_IF(!res, "finding monDay in week");
    TEST_FAIL_IF(p != week + strlen("monDay"), "finding monDay at the proper position");

    p = NULL;
    res = stristart(week, "tUESDAY", &p);
    TEST_FAIL_IF(res, "string doesn't start with tUESDAY");
    TEST_DONE();
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

TEST_DECL("str: stristrn", 0)
{
    static const char *alphabet = "abcdefghijklmnopqrstuvwxyz";
    const char *p;

    p = stristr(alphabet, "aBC");
    TEST_FAIL_IF(p != alphabet, "not found at start of string");

    p = stristr(alphabet, "Z");
    TEST_FAIL_IF(p != alphabet + 25, "not found at end of string");

    p = stristr(alphabet, "mn");
    TEST_FAIL_IF(p != alphabet + 12, "not found in the middle of the string");

    p = stristr(alphabet, "123");
    TEST_FAIL_IF(p != NULL, "unexistant string found");

    TEST_DONE();
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

TEST_DECL("str: strfind", 0)
{
    TEST_FAIL_IF(strfind("1,2,3,4", "1", ',') != true, "");
    TEST_FAIL_IF(strfind("1,2,3,4", "2", ',') != true, "");
    TEST_FAIL_IF(strfind("1,2,3,4", "4", ',') != true, "");
    TEST_FAIL_IF(strfind("11,12,13,14", "1", ',') != false, "");
    TEST_FAIL_IF(strfind("11,12,13,14", "2", ',') != false, "");
    TEST_FAIL_IF(strfind("11,12,13,14", "11", ',') != true, "");
    TEST_FAIL_IF(strfind("11,12,13,14", "111", ',') != false, "");
    TEST_FAIL_IF(strfind("toto,titi,tata,tutu", "to", ',') != false, "");
    TEST_FAIL_IF(strfind("1|2|3|4|", "", '|') != false, "");
    TEST_FAIL_IF(strfind("1||3|4|", "", '|') != true, "");
    TEST_DONE();
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

#define check_buffer_increment_unit(initval, expectedval, expectedret)       \
    do {                                                                     \
        pstrcpy(buf, sizeof(buf), initval);                                  \
        ret = buffer_increment(buf, -1);                                     \
        TEST_FAIL_IF(strcmp(buf, expectedval),                               \
            "value is \"%s\", expecting \"%s\"", buf, expectedval);          \
        TEST_FAIL_IF(ret != expectedret, "bad return value for \"%s\"", initval); \
    } while (0)
TEST_DECL("str: buffer_increment", 0)
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
    TEST_DONE();
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

#define check_buffer_increment_hex_unit(initval, expectedval, expectedret)   \
    do {                                                                     \
        pstrcpy(buf, sizeof(buf), initval);                                  \
        ret = buffer_increment_hex(buf, -1);                                 \
        TEST_FAIL_IF(strcmp(buf, expectedval),                               \
                     "value is \"%s\", expecting \"%s\"", buf, expectedval); \
        TEST_FAIL_IF(ret != expectedret, "bad return value for \"%s\"", initval); \
    } while (0)
TEST_DECL("str: buffer_increment_hex", 0)
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
    TEST_DONE();
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

TEST_DECL("str: strrand", 0)
{
    char buf[32];
    int n, ret;

    for (n = 0; n < countof(buf); n++) {
        buf[n] = 'B' + n;
    }

    ret = pstrrand(buf, sizeof(buf), 0, 0);
    TEST_FAIL_IF(buf[0] != '\0', "Missing padding after len=0");
    TEST_FAIL_IF(ret != 0, "Bad return value for len=0");

    ret = pstrrand(buf, sizeof(buf), 0, 3);
    TEST_FAIL_IF(buf[3] != '\0', "Missing padding after len=3");
    TEST_FAIL_IF(ret != 3, "Bad return value for len=3");

    /* Ask for 32 bytes, where buffer can only contain 31. */
    ret = pstrrand(buf, sizeof(buf), 0, sizeof(buf));
    TEST_FAIL_IF(buf[31] != '\0', "Missing padding after len=sizeof(buf)");
    TEST_FAIL_IF(ret != sizeof(buf) - 1, "Bad return value for len=sizeof(buf)");
    //fprintf(stderr, "buf:%s\n", buf);

    buf[0] = buf[1] = buf[2] = 'Z';
    buf[3] = buf[4] = buf[5] = buf[6] = 0x42;
    ret = pstrrand(buf, sizeof(buf), 3, 2);
    TEST_FAIL_IF(buf[3] == 0x42 || buf[4] == 0x42, "len=2 did not set buffer");
    TEST_FAIL_IF(buf[5] != 0, "Missing 0 after len=2");
    TEST_FAIL_IF(buf[6] != 0x42, "len=2 set the buffer incorrectly");
    TEST_FAIL_IF(ret != 2, "Bad return value for len=2");
    //fprintf(stderr, "buf:%s\n", buf);
    TEST_DONE();
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
