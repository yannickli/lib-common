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

#ifndef _OFF_T
#define _OFF_T

#ifndef _SSIZE_T
#include <sys/_types/_ssize_t.h>
#endif

typedef ssize_t off_t;
_Static_assert(sizeof(__darwin_off_t) == sizeof(off_t), "invalid off_t");

#endif
