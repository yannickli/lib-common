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

#ifndef IS_LIB_COMMON_STR_CONV_H
#define IS_LIB_COMMON_STR_CONV_H

#include "core.h"

/* string parsing and conversions */
extern unsigned char const __str_digit_value[128 + 256];
extern char const __str_digits_upper[36];
extern char const __str_digits_lower[36];

extern char const __utf8_trail[256];
extern uint32_t const __utf8_offs[6];
extern const char * const __cp1252_or_latin9_to_utf8[0x40];

/* OG: this function could return the number of bytes instead of bool */
static inline bool is_utf8_char(const char *s)
{
    int trail = __utf8_trail[(unsigned char)*s++];

    switch (trail) {
      case 3: if ((*s++ & 0xc0) != 0x80) return false;
      case 2: if ((*s++ & 0xc0) != 0x80) return false;
      case 1: if ((*s++ & 0xc0) != 0x80) return false;
      case 0: return true;

      default: return false;
    }
}
static inline bool is_utf8_data(const char *s, int len)
{
    if (len < __utf8_trail[(unsigned char)*s])
        return false;

    return is_utf8_char(s);
}

/* This (unused) macro implements UNICODE to UTF-8 transcoding */
#define UNICODE_TO_UTF8(x)     \
                 ((x) < 0x007F ? (x) : \
                  (x) < 0x0FFF ? ((0xC0 | (((x) >> 6) & 0x3F)) << 0) | \
                                 ((0x80 | (((x) >> 0) & 0x3F)) << 8) : \
                  ((0xE0 | (((x) >> 12) & 0x1F)) <<  0) | \
                  ((0x80 | (((x) >>  6) & 0x3F)) <<  8) | \
                  ((0x80 | (((x) >>  0) & 0x3F)) << 16))



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

/* XXX: dest will not be NUL terminated in strconv_hexdecode*/
int strconv_hexdecode(byte *dest, int size, const char *src, int len);
int strconv_hexencode(char *dest, int size, const byte *src, int len);
int strconv_escape(char *dest, int size, const char *src, int len);
int strconv_unescape(char *dest, int size, const char *src, int len);
int strconv_quote(char *dest, int size, const char *src, int len, int delim);
int strconv_unquote(char *dest, int size, const char *src, int len);
int strconv_quote_char(char *dest, int size, int c, int delim);
int strconv_unquote_char(int *cp, const char *src, int len);
int str_utf8_putc(char *dst, int c);
/* Decode XML entities in place */
int strconv_xmlunescape(char *str, int len);

/*[ CHECK ]::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::{{{*/
#ifdef CHECK
#include <check.h>

Suite *check_make_strconv_suite(void);

#endif
/*:::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::}}}*/

#endif /* IS_LIB_COMMON_STR_CONV_H */
