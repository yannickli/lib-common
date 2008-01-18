/**************************************************************************/
/*                                                                        */
/*  Copyright (C) 2004-2008 INTERSEC SAS                                  */
/*                                                                        */
/*  Should you receive a copy of this source code, you must check you     */
/*  have a proper, written authorization of INTERSEC to hold it. If you   */
/*  don't have such an authorization, you must DELETE all source code     */
/*  files in your possession, and inform INTERSEC of the fact you obtain  */
/*  these files. Should you not comply to these terms, you can be         */
/*  prosecuted in the extent permitted by applicable law.                 */
/*                                                                        */
/**************************************************************************/

#ifndef IS_LIB_COMMON_STRCONV_H
#define IS_LIB_COMMON_STRCONV_H

#include "macros.h"

/* string parsing and conversions */
extern unsigned char const __str_digit_value[128 + 256];
extern char const __str_digits_upper[36];
extern char const __str_digits_lower[36];

static inline int str_digit_value(int x) {
    return __str_digit_value[x + 128];
}
static inline int hexdigit(int x) {
    int i = __str_digit_value[x + 128];
    return i < 16 ? i : -1;
}

static inline int hexdecode(const char *str)
{
    int h = hexdigit(str[0]);

    return (h < 0) ? h : ((h << 4) | hexdigit(str[1]));
}

/* XXX: dest will not be NUL terminated */
int strconv_hexdecode(byte *dest, int size, const char *src, int len);
int strconv_escape(char *dest, int size, const char *src, int len);
int strconv_unescape(char *dest, int size, const char *src, int len);
int strconv_quote(char *dest, int size, const char *src, int len, int delim);
int strconv_unquote(char *dest, int size, const char *src, int len);
int strconv_quote_char(char *dest, int size, int c, int delim);
int strconv_unquote_char(int *cp, const char *src, int len);

/*[ CHECK ]::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::{{{*/
#ifdef CHECK
#include <check.h>

Suite *check_make_strconv_suite(void);

#endif
/*:::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::}}}*/

#endif /* IS_LIB_COMMON_STRCONV_H */
