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

#ifndef IS_COMPAT_FNMATCH_H
#define IS_COMPAT_FNMATCH_H

#if defined(__MINGW) || defined(__MINGW32__)
/* Bits set in the FLAGS argument to `fnmatch'.  */
#define FNM_PATHNAME    (1 << 0) /* No wildcard can ever match `/'.  */
#define FNM_NOESCAPE    (1 << 1) /* Backslashes don't quote special chars.  */
#define FNM_PERIOD      (1 << 2) /* Leading `.' is matched only explicitly.  */
#define FNM_FILE_NAME   FNM_PATHNAME   /* Preferred GNU name.  */
#define FNM_LEADING_DIR (1 << 3) /* Ignore `/...' after a match.  */
#define FNM_CASEFOLD    (1 << 4) /* Compare without regard to case.  */
#define FNM_EXTMATCH    (1 << 5) /* Use ksh-like extended matching. */
#define FNM_NOMATCH     1
int fnmatch(const char *pattern, const char *string, int flags);
#else
#include_next <fnmatch.h>
#endif


#endif
