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

int iprintf(const char *format, ...);
int ifprintf(FILE *stream, const char *format, ...);
int isnprintf(char *str, size_t size, const char *format, ...);
int ivprintf(const char *format, va_list arglist);
int ivfprintf(FILE *stream, const char *format, va_list arglist);
int ivsnprintf(char *str, size_t size, const char *format, va_list arglist);

#endif /* IS_LIB_COMMON_IPRINTF_H */
