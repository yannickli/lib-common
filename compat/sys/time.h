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

#ifndef IS_COMPAT_SYS_TIME_H
#define IS_COMPAT_SYS_TIME_H

#include_next <sys/param.h>
#include_next <sys/time.h>

#if defined(__MINGW) || defined(__MINGW32__)
#  if __MINGW32_MAJOR_VERSION < 3 || \
     (__MINGW32_MAJOR_VERSION == 3 && __MINGW32_MINOR_VERSION < 12)
   /* 3.9 needs those. 3.12 does not. Test could be thinner */
int gettimeofday(struct timeval *p, void *tz);
#  endif
#endif

#endif /* !IS_COMPAT_SYS_TIME_H */
