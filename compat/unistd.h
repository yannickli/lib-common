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

#ifndef IS_COMPAT_UNISTD_H
#define IS_COMPAT_UNISTD_H

#include_next <unistd.h>

#if defined(__MINGW) || defined(__MINGW32__)
#  define mkdir(path, mode)  mkdir(path)
int usleep(unsigned long usec);
#elif defined(__APPLE__)

#define fdatasync(fd)  fsync(fd)

#endif

#endif /* !IS_COMPAT_UNISTD_H */
