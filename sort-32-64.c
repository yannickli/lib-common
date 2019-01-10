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

#include "sort.h"

#define type_t   uint32_t
#define dsort    dsort32
#define uniq     uniq32
#define bisect   bisect32
#define contains contains32
#include "sort-32-64.in.c"

#define type_t   uint64_t
#define dsort    dsort64
#define uniq     uniq64
#define bisect   bisect64
#define contains contains64
#include "sort-32-64.in.c"
