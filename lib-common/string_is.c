#include <ctype.h>
#include "string_is.h"
/** Copies the string pointed to by src to the buffer
 * <code>[dest, dest+size)</code>. In all cases, a terminating '\0' character is
 * put at the end of the copied string which means that any character from
 * <code>src+size-1</code> will be ignored.
 *
 * The source and destination strings may not overlap. No assumption should be
 * made on the values of the characters after the first '\0' character in the
 * destination buffer.
 *
 * @return a pointer to dest
 * @see pstrcpylen
 */
char *pstrcpy(char *dest, ssize_t size, const char *src)
{
    if (dest && size > 0) {
	ssize_t len = src ? strlen(src) : 0;
	if (len > size - 1)
	    len = size - 1;
	if (len > 0 && dest != src)
	    memcpy(dest, src, len); /* assuming no overlap */
	dest[len] = '\0';
    }
    return dest;
}

/** Copies the string pointed to by src (and of maximal length len) to the
 * buffer <code>[dest, dest+size)</code>. In all cases, a terminating '\0'
 * character is put at the end of the copied string which means that any
 * character from <code>src+size-1</code> will be ignored.
 *
 * The source and destination strings may not overlap. No assumption should be
 * made on the values of the characters after the first '\0' character in the
 * destination buffer.
 *
 * @return a pointer to dest
 */
char *pstrcpylen(char *dest, ssize_t size, const char *src, int len)
{
    if (dest && size > 0) {
        if (!src)
            len = 0;
        else if (len < 0)
	    len = strlen(src);
	if (len > size - 1)
	    len = size - 1;
	if (len > 0)
	    memcpy(dest, src, len); /* assuming no overlap */
	dest[len] = '\0';
    }
    return dest;
}

/** Appends the string pointed to by src after the string pointed to by dest
 * considering that no character shall be written from <code>dest+size</code>.
 *
 * The source and destination strings may not overlap. No assumption should be
 * made on the values of the characters after the first '\0' character in the
 * destination buffer.
 *
 * @return a pointer to dest
 */
char *pstrcat(char *dest, ssize_t size, const char *src)
{
    if (dest && size > 0) {
	ssize_t dlen = strlen(dest);
	if (dlen < size) {
	    ssize_t len = src ? strlen(src) : 0;
	    if (len > size - dlen - 1)
		len = size - dlen - 1;
	    if (len > 0)
		memcpy(dest + dlen, src, len); /* assuming no overlap */
	    dest[dlen + len] = '\0';
	}
    }
    return dest;
}

/** Skips initial blanks as per isspace(c).
 *
 * use vskipspaces for non const parameters
 * @see vskipspaces
 */
const char *skipspaces(const char *s)
{
    while (isspace((unsigned char)*s))
	s++;
    return s;
}

/** Replaces blank characters at end of string with '\0'.
 *
 * @returns str
 */
char *rstrtrim(char *str)
{
    char *p;

    if (str) {
        p = str + strlen(str);

        while (p > str && isspace((unsigned char)p[-1]))
            *--p = '\0';
    }
    return str;
}

/** Tells whether str begins with p.
 *
 * @param pp if not null and str begins with p, pp is given the address of the
 * first following character in str.
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

/** Find the first occurence of the substring needle in haystack, case
 *  insensitive.
 *
 * @returns a pointer to the beginning of the substring, or NULL if
 * it was not found.
 */
char *stristr(const char *haystack, const char *needle)
{
    char *nptr, *hptr, *start;
    int  hlen, nlen;


    start = (char *)haystack;
    nptr  = (char *)needle;
    nlen  = strlen(needle);

    for (hlen  = strlen(haystack); hlen >= nlen; start++, hlen--) {
	/* find start of pattern in haystack */
	while (toupper(*start) != toupper(*needle)) {
	    start++;
	    hlen--;

       	    /* needle longer than haystack */
 	    if (hlen < nlen) {
		return(NULL);
	    }
	}

	hptr = start;
    	nptr = (char *)needle;

	while (toupper(*hptr) == toupper(*nptr)) {
	    hptr++;
	    nptr++;

      	    /* if end of needle then needle was found */

	    if ('\0' == *nptr) {
		return (start);
	    }
	}
    }
    return(NULL);
}

/*}}}*/
/*[ CHECK ]::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::{{{*/
#ifdef CHECK
/* {{{*/
#include <check.h>

static const char *week = "Monday Tuesday Wednesday Thursday Friday Saturday Sunday";
START_TEST (check_strstart)
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

START_TEST (check_stristart)
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

Suite *check_string_is_suite(void)
{
    Suite *s  = suite_create("String");
    TCase *tc = tcase_create("Core");

    suite_add_tcase(s, tc);
    tcase_add_test(tc, check_strstart);
    tcase_add_test(tc, check_stristart);
    return s;
}

/*.........................................................................}}}*/
#endif
/*:::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::}}}*/

