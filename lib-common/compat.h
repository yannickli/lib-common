/**************************************************************************/
/*                                                                        */
/*  Copyright (C) 2004-2007 INTERSEC SAS                                  */
/*                                                                        */
/*  Should you receive a copy of this source code, you must check you     */
/*  have a proper, written authorization of INTERSEC to hold it. If you   */
/*  don't have such an authorization, you must DELETE all source code     */
/*  files in your possession, and inform INTERSEC of the fact you obtain  */
/*  these files. Should you not comply to these terms, you can be         */
/*  prosecuted in the extent permitted by applicable law.                 */
/*                                                                        */
/**************************************************************************/

#ifndef IS_LIB_COMMON_COMPAT_H
#define IS_LIB_COMMON_COMPAT_H

/* Global initializer to enable maximum compatibility */
void intersec_initialize(void);

#ifdef MINGCC
#include <time.h>
#include <sys/time.h>
#include <ws2tcpip.h>   /* for socklen_t */

char *asctime_r(const struct tm *tm, char *buf);
char *ctime_r(const time_t *timep, char *buf);
struct tm *gmtime_r(const time_t *timep, struct tm *result);
struct tm *localtime_r(const time_t *timep, struct tm *result);
long int lrand48(void);

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

#define EUCLEAN         117
#define O_NONBLOCK 00004000
#endif

#if !defined(MINGCC) && defined(LINUX)

#define O_BINARY 0

#endif

#endif /* IS_LIB_COMMON_COMPAT_H */
