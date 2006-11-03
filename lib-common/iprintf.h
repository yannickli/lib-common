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

#ifndef IS_LIB_COMMON_IPRINTF_H
#define IS_LIB_COMMON_IPRINTF_H

#include <stdio.h>
#include <stdarg.h>

#include "macros.h"

int iprintf(const char *format, ...)
	__attr_format__(1, 2) __attr_nonnull__((1));
int ifprintf(FILE *stream, const char *format, ...)
	__attr_format__(2, 3) __attr_nonnull__((1, 2));
int isnprintf(char *str, size_t size, const char *format, ...)
	__attr_format__(3, 4) __attr_nonnull__((3));
int ivprintf(const char *format, va_list arglist)
	__attr_nonnull__((1));
int ivfprintf(FILE *stream, const char *format, va_list arglist)
	__attr_nonnull__((1, 2));
int ivsnprintf(char *str, size_t size, const char *format, va_list arglist)
	__attr_nonnull__((1, 3));
int ifputs_hex(FILE *stream, const byte *buf, int len);

#if defined(IPRINTF_HIDE_STDIO) && IPRINTF_HIDE_STDIO
#define printf(...)     iprintf(__VA_ARGS__)
#define fprintf(...)    ifprintf(__VA_ARGS__)
#define snprintf(...)   isnprintf(__VA_ARGS__)
#define vprintf(...)    ivprintf(__VA_ARGS__)
#define vfprintf(...)   ivfprintf(__VA_ARGS__)
#define vsnprintf(...)  ivsnprintf(__VA_ARGS__)
#endif

#endif /* IS_LIB_COMMON_IPRINTF_H */
