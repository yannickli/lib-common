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

static inline ssize_t sstrlen(const char *str) {
    return (ssize_t)strlen((const char *)str);
}

ssize_t pstrcpy(char *dest, ssize_t size, const char *src);
ssize_t pstrcpylen(char *dest, ssize_t size, const char *src, ssize_t n);
ssize_t pstrcat(char *dest, ssize_t size, const char *src);
ssize_t pstrlen(const char *str, ssize_t size);

const char *skipspaces(const char *s);
static inline char *vskipspaces(char *s) {
    return (char*)skipspaces((char *)s);
}
static inline const byte *bskipspaces(const byte *s) {
    return (const byte*)skipspaces((const char *)s);
}

const char *strnextspace(const char *s);
static inline char *vstrnextspace(char *s){
    return (char*)strnextspace((char *)s);
}

/* Trim spaces at end of string, return pointer to '\0' */
char *strrtrim(char *str);

/* Wrappers to fix constness issue in strtol() */
static inline long cstrtol(const char *str, const char **endp, int base) {
    return (strtol)(str, (char **)endp, base);
}
static inline long vstrtol(char *str, char **endp, int base) {
    return (strtol)(str, endp, base);
}
#define strtol(str, endp, base)  cstrtol(str, endp, base)

static inline long long cstrtoll(const char *str, const char **endp, int base)
{
    return (strtoll)(str, (char **)endp, base);
}
static inline long long vstrtoll(char *str, char **endp, int base) {
    return (strtoll)(str, endp, base);
}
#define strtoll(str, endp, base)  cstrtoll(str, endp, base)

int strstart(const char *str, const char *p, const char **pp);
static inline int vstrstart(char *str, const char *p, char **pp) {
    return strstart(str, p, (const char **)pp);
}

int stristart(const char *str, const char *p, const char **pp);
static inline int vstristart(char *str, const char *p, char **pp) {
    return stristart(str, p, (const char **)pp);
}

const char *stristrn(const char *haystack, const char *needle, size_t nlen);
static inline char *
vstristrn(char *haystack, const char *needle, size_t nlen) {
    return (char *)stristrn(haystack, needle, nlen);
}
static inline const char *
stristr(const char *haystack, const char *needle) {
    return stristrn(haystack, needle, strlen(needle));
}
static inline char *vstristr(char *haystack, const char *needle) {
    return (char *)stristr(haystack, needle);
}

/* Implement inline using strcmp, unless strcmp is hopelessly fucked up */
static inline bool strequal(const char *str1, const char *str2) {
    return !strcmp(str1, str2);
}

const void *memsearch(const void *haystack, size_t hsize,
                      const void *needle, size_t nsize);

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
