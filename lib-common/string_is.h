#ifndef IS_STRING_IS_H
#define IS_STRING_IS_H

#include <stdlib.h>
#include <string.h>
#include <unistd.h>

static inline ssize_t sstrlen(const char * str)
{
    return (ssize_t)strlen((const char *)str);
}

char *pstrcpy(char *dest, ssize_t size, const char *src);
char *pstrcpylen(char *dest, ssize_t size, const char *src, int len);
char *pstrcat(char *dest, ssize_t size, const char *src);
const char *skipspaces(const char *s);
char *rstrtrim(char *str);
int strstart(const char *str, const char *p, const char **pp);
int stristart(const char *str, const char *p, const char **pp);
char *stristr(const char *haystack, const char *needle);

/*[ CHECK ]::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::{{{*/
#ifdef CHECK
#include <check.h>

Suite *check_string_is_suite(void);

#endif
/*:::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::}}}*/
#endif
