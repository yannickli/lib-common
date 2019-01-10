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

#ifndef IS_COMPAT_ENDIAN_H
#define IS_COMPAT_ENDIAN_H

#include_next <sys/param.h>
#ifdef __GLIBC__
#  include_next <endian.h>
#elif defined(__MINGW) || defined(__MINGW32__)
#  define __LITTLE_ENDIAN  1234
#  define __BIG_ENDIAN     4321
#  define __BYTE_ORDER     __LITTLE_ENDIAN
#elif defined(__sun)
#    define __LITTLE_ENDIAN  1234
#    define __BIG_ENDIAN     4321
#    define __BYTE_ORDER     __LITTLE_ENDIAN
#else
#  error your platform is unsupported
#endif

#if (__BYTE_ORDER != __BIG_ENDIAN) && (__BYTE_ORDER != __LITTLE_ENDIAN)
#  error __BYTE_ORDER must be __BIG_ENDIAN or __LITTLE_ENDIAN
#endif

#endif /* !IS_COMPAT_ENDIAN_H */
