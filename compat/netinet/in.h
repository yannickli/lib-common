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

#ifndef IS_COMPAT_NETINET_IN_H
#define IS_COMPAT_NETINET_IN_H

#if defined(__MINGW) || defined(__MINGW32__)
#include <winsock2.h>
#else
#include_next <netinet/in.h>
#endif

#endif /* !IS_COMPAT_NETINET_IN_H */
