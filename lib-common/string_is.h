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

#ifndef IS_LIB_COMMON_STRING_IS_H
#define IS_LIB_COMMON_STRING_IS_H

#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "macros.h"

__attr_nonnull__((1))
static inline ssize_t sstrlen(const char *str) {
    return (ssize_t)strlen((const char *)str);
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

const char *skipspaces(const char *s)  __attr_nonnull__((1));
__attr_nonnull__((1))
static inline char *vskipspaces(char *s) {
    return (char*)skipspaces((char *)s);
}

__attr_nonnull__((1)) 
static inline const byte *bskipspaces(const byte *s) {
    return (const byte*)skipspaces((const char *)s);
}

const char *strnextspace(const char *s)  __attr_nonnull__((1));
__attr_nonnull__((1)) 
static inline char *vstrnextspace(char *s) {
    return (char*)strnextspace((char *)s);
}

/* Trim spaces at end of string, return pointer to '\0' */
char *strrtrim(char *str);

/* Wrappers to fix constness issue in strtol() */
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

int strtoip(const char *p, const char **endp)  __attr_nonnull__((1));

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

__attr_nonnull__((1, 3))
static inline void *vmemsearch(void *haystack, size_t hsize,
                               const void *needle, size_t nsize) {
    return (void *)memsearch(haystack, hsize, needle, nsize);
}

/*[ CHECK ]::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::{{{*/
#ifdef CHECK
#include <check.h>

Suite *check_string_is_suite(void);

#endif
/*:::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::}}}*/
#endif /* IS_LIB_COMMON_STRING_IS_H */
