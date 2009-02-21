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

#if !defined(IS_LIB_COMMON_STR_H) || defined(IS_LIB_COMMON_STR_CONV_H)
#  error "you must include <lib-common/str.h> instead"
#else
#define IS_LIB_COMMON_STR_CONV_H

extern unsigned char const __str_digit_value[128 + 256];
extern char const __str_digits_upper[36];
extern char const __str_digits_lower[36];

extern uint32_t const __utf8_offs[6];
extern const char * const __cp1252_or_latin9_to_utf8[0x40];

extern uint8_t const __utf8_clz_to_charlen[31];
extern uint8_t const __utf8_mark[7];
extern uint8_t const __utf8_char_len[32];


/****************************************************************************/
/* Base 36 stuff                                                            */
/****************************************************************************/

static inline int str_digit_value(int x)
{
    return __str_digit_value[x + 128];
}
static inline int hexdigit(int x)
{
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
int strconv_unquote(char *dest, int size, const char *src, int len);


/****************************************************************************/
/* utf-8 and charset conversions                                            */
/****************************************************************************/

static inline uint8_t __pstrputuc(char *dst, int c)
{
    /* XXX: 31 ^ clz(c) is actually bsr in x86 assembly */
    uint8_t len = __utf8_clz_to_charlen[31 ^ __builtin_clz(c | 1)];
    switch (len) {
      default: dst[3] = (c | 0x80) & 0xbf; c >>= 6;
      case 3:  dst[2] = (c | 0x80) & 0xbf; c >>= 6;
      case 2:  dst[1] = (c | 0x80) & 0xbf; c >>= 6;
      case 1:  dst[0] = (c | __utf8_mark[len]);
      case 0:  break;
    }
    return len;
}

/* XXX 0 means invalid utf8 char */
static inline uint8_t utf8_charlen(const char *s)
{
    uint8_t len = __utf8_char_len[(unsigned char)*s >> 3];
    switch (len) {
      default: if (unlikely((*++s & 0xc0) != 0x80)) return 0;
      case 3:  if (unlikely((*++s & 0xc0) != 0x80)) return 0;
      case 2:  if (unlikely((*++s & 0xc0) != 0x80)) return 0;
      case 1:  return len;
      case 0:  return len;
    }
}

static inline int utf8_getc(const char *s, const char **out)
{
    uint32_t ret = 0;
    uint8_t  len = utf8_charlen(s);

    switch (len - 1) {
      default: return -1;
      case 3:  ret += (unsigned char)*s++; ret <<= 6;
      case 2:  ret += (unsigned char)*s++; ret <<= 6;
      case 1:  ret += (unsigned char)*s++; ret <<= 6;
      case 0:  ret += (unsigned char)*s++;
    }

    if (out)
        *out = s;
    return ret - __utf8_offs[len];
}
static inline int utf8_ngetc(const char *s, int len, const char **out)
{
    uint8_t ulen = __utf8_char_len[(unsigned char)*s >> 3];
    if (unlikely(ulen > len))
        return -1;
    return utf8_getc(s, out);
}
static inline int utf8_vgetc(char *s, char **out)
{
    return utf8_getc(s, (const char **)out);
}


/*[ CHECK ]::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::{{{*/
#ifdef CHECK
#include <check.h>

Suite *check_make_strconv_suite(void);

#endif
/*:::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::}}}*/
#endif /* IS_LIB_COMMON_STR_CONV_H */
