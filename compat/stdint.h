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

#ifndef IS_COMPAT_STDINT_H
#define IS_COMPAT_STDINT_H

#include_next <stdint.h>

#ifdef __APPLE__

#undef UINT64_MAX
#define UINT64_MAX        18446744073709551615UL

#undef INT64_MAX
#define INT64_MAX        9223372036854775807L

#endif

#endif
