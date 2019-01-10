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
#include "str-mem.h"
#include "str-ctype.h"
#include "str-iprintf.h"
#include "str-num.h"
#include "str-path.h"
#include "str-conv.h"
#include "str-l.h"
#include "str-buf.h"
#include "str-stream.h"

const char *skipspaces(const char *s)  __attr_nonnull__((1));
__attr_nonnull__((1))
static inline char *vskipspaces(char *s) {
    return (char*)skipspaces(s);
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

/* find a word in a list of words separated by sep.
 */
bool strfind(const char *keytable, const char *str, int sep);

int buffer_increment(char *buf, int len);
int buffer_increment_hex(char *buf, int len);
ssize_t pstrrand(char *dest, ssize_t size, int offset, ssize_t len);

/* Return the number of occurences replaced */
/* OG: need more general API */
int str_replace(const char search, const char replace, char *subject);

#endif /* IS_LIB_COMMON_STR_IS_H */
