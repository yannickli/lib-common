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

#ifndef _UINT64_T
#define _UINT64_T

#ifndef _UINTMAX_T
# include <_types/_uintmax_t.h>
#endif

typedef uintmax_t uint64_t;
_Static_assert(sizeof(uint64_t) == 8, "invalid uint64_t");

#endif
