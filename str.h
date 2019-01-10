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

#ifndef IS_LIB_COMMON_STR_H
#define IS_LIB_COMMON_STR_H

#define IPRINTF_HIDE_STDIO 1
#include "core.h"
#include "str-ctype.h"
#include "str-iprintf.h"

__attr_nonnull__((1))
static inline ssize_t sstrlen(const char *str) {
    return (ssize_t)strlen(str);
}

static inline ssize_t sstrnlen(const char *str, ssize_t n)
{
    const char *p = memchr(str, '\0', n);
    return p ? p - str : n;
}

ssize_t pstrcpy(char *dest, ssize_t size, const char *src);
ssize_t pstrcpylen(char *dest, ssize_t size, const char *src, ssize_t n);
ssize_t pstrcat(char *dest, ssize_t size, const char *src);
ssize_t pstrlen(const char *str, ssize_t size)  __attr_nonnull__((1));

/* OG: RFE: should have a generic naming convention for functions that
 * return the end of string upon failure to find or completion.
 * use that for strcpy, strcat, memcpy, memmove, memchr, strstr,
 * memsearch, stristr stristrn, pstrcpy, pstrcat, pstrcpylen... and
 * their v* equivalent
 */
static inline const char *pstrchrnul(const char *s, int c) {
    while (*s && *s != c)
        s++;
    return s;
}

/* count number of occurences of c in str */
int pstrchrcount(const char *str, int c);

const char *skipspaces(const char *s)  __attr_nonnull__((1));
__attr_nonnull__((1))
static inline char *vskipspaces(char *s) {
    return (char*)skipspaces(s);
}

__attr_nonnull__((1))
static inline const byte *bskipspaces(const byte *s) {
    return (const byte*)skipspaces((const char *)s);
}

const char *strnextspace(const char *s)  __attr_nonnull__((1));
__attr_nonnull__((1))
static inline char *vstrnextspace(char *s) {
    return (char*)strnextspace(s);
}

const char *skipblanks(const char *s)  __attr_nonnull__((1));
__attr_nonnull__((1))
static inline char *vskipblanks(char *s) {
    return (char*)skipblanks(s);
}

/* Trim spaces at end of string, return pointer to '\0' */
char *strrtrim(char *str);

/* Wrappers to fix constness issue in strtol() */
__attr_nonnull__((1))
static inline unsigned long cstrtoul(const char *str, const char **endp, int base) {
    return (strtoul)(str, (char **)endp, base);
}

__attr_nonnull__((1))
static inline unsigned long vstrtoul(char *str, char **endp, int base) {
    return (strtoul)(str, endp, base);
}
#define strtoul(str, endp, base)  cstrtoul(str, endp, base)

__attr_nonnull__((1))
static inline long cstrtol(const char *str, const char **endp, int base) {
    return (strtol)(str, (char **)endp, base);
}

__attr_nonnull__((1))
static inline long vstrtol(char *str, char **endp, int base) {
    return (strtol)(str, endp, base);
}
#define strtol(str, endp, base)  cstrtol(str, endp, base)

__attr_nonnull__((1))
static inline long long cstrtoll(const char *str, const char **endp, int base) {
    return (strtoll)(str, (char **)endp, base);
}
__attr_nonnull__((1))
static inline long long vstrtoll(char *str, char **endp, int base) {
    return (strtoll)(str, endp, base);
}
#define strtoll(str, endp, base)  cstrtoll(str, endp, base)

__attr_nonnull__((1))
static inline unsigned long long
cstrtoull(const char *str, const char **endp, int base) {
    return (strtoull)(str, (char **)endp, base);
}
__attr_nonnull__((1))
static inline unsigned long long
vstrtoull(char *str, char **endp, int base) {
    return (strtoull)(str, endp, base);
}
#define strtoull(str, endp, base)  cstrtoull(str, endp, base)

int strtoip(const char *p, const char **endp)  __attr_nonnull__((1));
static inline int vstrtoip(char *p, char **endp) {
    return strtoip(p, (const char **)endp);
}
int memtoip(const void *p, int len, const byte **endp)  __attr_nonnull__((1));
int64_t memtollp(const void *s, int len, const byte **endp)
    __attr_nonnull__((1));
int64_t parse_number(const char *str);

#define STRTOLP_IGNORE_SPACES  (1 << 0)
#define STRTOLP_CHECK_END      (1 << 1)
#define STRTOLP_EMPTY_OK       (1 << 2)
#define STRTOLP_CHECK_RANGE    (1 << 3)
#define STRTOLP_CLAMP_RANGE    (1 << 4)
/* returns 0 if success, negative errno if error */
int strtolp(const char *p, const char **endp, int base, long *res,
            int flags, long min, long max);

int strstart(const char *str, const char *p, const char **pp);
static inline int vstrstart(char *str, const char *p, char **pp) {
    return strstart(str, p, (const char **)pp);
}

int stristart(const char *str, const char *p, const char **pp);
static inline int vstristart(char *str, const char *p, char **pp) {
    return stristart(str, p, (const char **)pp);
}

const char *stristrn(const char *haystack, const char *needle, size_t nlen)
          __attr_nonnull__((1, 2));

__attr_nonnull__((1, 2))
static inline char *
vstristrn(char *haystack, const char *needle, size_t nlen) {
    return (char *)stristrn(haystack, needle, nlen);
}

__attr_nonnull__((1, 2))
static inline const char *
stristr(const char *haystack, const char *needle) {
    return stristrn(haystack, needle, strlen(needle));
}

__attr_nonnull__((1, 2))
static inline char *vstristr(char *haystack, const char *needle) {
    return (char *)stristr(haystack, needle);
}

/* Implement inline using strcmp, unless strcmp is hopelessly fucked up */
__attr_nonnull__((1, 2))
static inline bool strequal(const char *str1, const char *str2) {
    return !strcmp(str1, str2);
}

const void *memsearch(const void *haystack, size_t hsize,
                      const void *needle, size_t nsize)
        __attr_nonnull__((1, 3));

const void *pmemrchr(const void *s, int c, ssize_t n) __attr_nonnull__((1));

__attr_nonnull__((1, 3))
static inline void *vmemsearch(void *haystack, size_t hsize,
                               const void *needle, size_t nsize) {
    return (void *)memsearch(haystack, hsize, needle, nsize);
}

/* find a word in a list of words separated by sep.
 */
bool strfind(const char *keytable, const char *str, int sep);

int buffer_increment(char *buf, int len);
int buffer_increment_hex(char *buf, int len);
ssize_t pstrrand(char *dest, ssize_t size, int offset, ssize_t len);

/* Return the number of occurences replaced */
/* OG: need more general API */
int str_replace(const char search, const char replace, char *subject);

#include "str-path.h"
#include "str-conv.h"
#include "str-l.h"
#include "str-buf.h"
#include "str-stream.h"

/*[ CHECK ]::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::{{{*/
#ifdef CHECK
#include <check.h>

Suite *check_string_suite(void);

#endif
/*:::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::}}}*/
#endif /* IS_LIB_COMMON_STR_IS_H */
