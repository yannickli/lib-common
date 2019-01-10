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

#ifndef _INT64_T
#define _INT64_T

#ifndef _INTMAX_T
# include <_types/_intmax_t.h>
#endif

typedef intmax_t int64_t;
_Static_assert(sizeof(int64_t) == 8, "invalid int64_t");

#endif
