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

#ifndef IS_COMPAT_SYS_MMAN_H
#define IS_COMPAT_SYS_MMAN_H

#  include_next <sys/mman.h>

#if defined(__APPLE__)
#define MAP_ANONYMOUS  MAP_ANON

#ifndef MADV_DONTFORK
# define MADV_DONTFORK 0
#endif
#endif

#endif
