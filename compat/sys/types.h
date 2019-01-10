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

#ifndef IS_COMPAT_SYS_TYPES_H
#define IS_COMPAT_SYS_TYPES_H

#include_next <sys/types.h>

#if defined(__MINGW) || defined(__MINGW32__)

typedef _off_t __off_t;
typedef _pid_t __pid_t;

#endif

#endif /* !IS_COMPAT_SYS_TYPES_H */
