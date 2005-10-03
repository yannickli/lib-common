#ifndef IS_STRING_IS_H
#define IS_STRING_IS_H

#include <stdlib.h>
#include <string.h>
#include <unistd.h>

static inline ssize_t sstrlen(const char *str) {
    return (ssize_t)strlen((const char *)str);
}

ssize_t pstrcpy(char *dest, ssize_t size, const char *src);
ssize_t pstrcpylen(char *dest, ssize_t size, const char *src, ssize_t n);
ssize_t pstrcat(char *dest, ssize_t size, const char *src);
const char *skipspaces(const char *s);
static inline char *vskipspaces(char *s) {
    return (char*)skipspaces((char *)s);
}
char *rstrtrim(char *str);
int strstart(const char *str, const char *p, const char **pp);
int stristart(const char *str, const char *p, const char **pp);
static inline int vstrstart(char *str, const char *p, char **pp) {
    return strstart(str, p, (const char **)pp);
}
static inline int vstristart(char *str, const char *p, char **pp) {
    return stristart(str, p, (const char **)pp);
}
char *stristr(const char *haystack, const char *needle);

const void *memsearch(const void *haystack, size_t hsize,
                      const void *needle, size_t nsize);

/*[ CHECK ]::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::{{{*/
#ifdef CHECK
#include <check.h>

Suite *check_string_is_suite(void);

#endif
/*:::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::}}}*/
#endif
