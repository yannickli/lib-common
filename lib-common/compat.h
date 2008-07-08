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

#ifndef IS_LIB_COMMON_COMPAT_H
#define IS_LIB_COMMON_COMPAT_H

#ifdef MINGCC
#include <time.h>
#include <sys/time.h>
#include <ws2tcpip.h>   /* for socklen_t */

char *asctime_r(const struct tm *tm, char *buf);
char *ctime_r(const time_t *timep, char *buf);
struct tm *gmtime_r(const time_t *timep, struct tm *result);
struct tm *localtime_r(const time_t *timep, struct tm *result);

#define EUCLEAN         117
#define O_NONBLOCK 00004000
#endif

#ifndef O_BINARY
#define O_BINARY 0
#endif

#endif /* IS_LIB_COMMON_COMPAT_H */
