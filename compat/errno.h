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

#ifndef IS_COMPAT_ERRNO_H
#define IS_COMPAT_ERRNO_H

#include_next <errno.h>

#ifndef EUCLEAN
#define EUCLEAN         117
#endif

#ifdef __APPLE__
# define program_invocation_short_name  getprogname()
#endif

#endif
