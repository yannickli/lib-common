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

#ifndef IS_COMPAT_STDIO_H
#define IS_COMPAT_STDIO_H

#include_next <stdio.h>

#if defined(__GLIBC__)
/* Wrap glibc specific unlocked API with obnoxious macros */
#  define ISPUTC(c, f)          putc_unlocked(c, f)
#  define ISFWRITE(b, s, n, f)  fwrite_unlocked(b, s, n, f)
#  define ISGETC(f)             getc_unlocked(f)
#  define ISFREAD(b, s, n, f)   fread_unlocked(b, s, n, f)
#else
#  define ISPUTC(c, f)          putc(c, f)
#  define ISFWRITE(b, s, n, f)  fwrite(b, s, n, f)
#  define ISGETC(f)             getc(f)
#  define ISFREAD(b, s, n, f)   fread(b, s, n, f)
#endif

#endif /* !IS_COMPAT_STDIO_H */
