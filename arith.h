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

#ifndef IS_LIB_COMMON_ARITH_H
#define IS_LIB_COMMON_ARITH_H

#include "core.h"

#include "arith-endianess.h"
#include "arith-cmp.h"
#include "arith-float.h"
#include "arith-str-stream.h"
#include "arith-scan.h"

unsigned gcd(unsigned a, unsigned b);
unsigned gcd_euclid(unsigned a, unsigned b);
unsigned gcd_stein(unsigned a, unsigned b);

extern uint64_t const powerof10[16];

#endif
