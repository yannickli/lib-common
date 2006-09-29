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

#ifndef IS_STRCONV_H
#define IS_STRCONV_H

/* string parsing and conversions */
extern unsigned char const __str_digit_value[128 + 256];
static inline int str_digit_value(int x) {
    return __str_digit_value[x + 128];
}

int strconv_escape(char *dest, int size, const char *src, int len);
int strconv_unescape(char *dest, int size, const char *src, int len);
int strconv_quote(char *dest, int size, const char *src, int len, int delim);
int strconv_unquote(char *dest, int size, const char *src, int len);
int strconv_quote_char(char *dest, int size, int c, int delim);
int strconv_unquote_char(int *cp, const char *src, int len);

#endif
