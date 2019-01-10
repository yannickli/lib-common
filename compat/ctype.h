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

#ifndef IS_COMPAT_CTYPE_H
#define IS_COMPAT_CTYPE_H

#include_next <sys/param.h>
#include_next <ctype.h>

/* Glibc's ctype system issues a function call to handle locale issues.
 * Glibc only defines isblank() if USE_ISOC99, for some obsure reason,
 * isblank() cannot be defined as an inline either (intrinsic
 * function?)
 * Should rewrite these functions and use our own simpler version
 */
#ifndef isblank
#  if defined(__GLIBC__) && defined(__isctype) && defined(_ISbit)
#    define isblank(c)      __isctype((c), _ISblank)
#  elif defined(__MINGW) || defined(__MINGW32__)
#    if  (__MINGW32_MAJOR_VERSION < 3 || \
         (__MINGW32_MAJOR_VERSION == 3 && __MINGW32_MINOR_VERSION < 12))
/* OG: we should really have our own char type macros */
static inline int isblank(int c) { return (c == ' ' || c == '\t'); }
#    endif
#  endif
#endif

#endif /* !IS_COMPAT_CTYPE_H */
