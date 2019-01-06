/**************************************************************************/
/*                                                                        */
/*  Copyright (C) 2004-2019 INTERSEC SA                                   */
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

#if __has_feature(nullability)
#pragma GCC diagnostic push
#pragma GCC diagnostic error "-Wnullability-completeness"
#if __has_warning("-Wnullability-completeness-on-arrays")
#pragma GCC diagnostic ignored "-Wnullability-completeness-on-arrays"
#endif
#endif

#include "arith-endianess.h"
#include "arith-cmp.h"
#include "arith-float.h"
#include "arith-str.h"
#include "arith-scan.h"

unsigned gcd(unsigned a, unsigned b);
unsigned gcd_euclid(unsigned a, unsigned b);
unsigned gcd_stein(unsigned a, unsigned b);

/** Count the number of multiples of a number in a range.
 *
 * Count the number of multiples of a number 'n' in the range 'min' --> 'max'
 * (min and max included).
 *
 * \param[in]  n   The number 'n' whose we are counting the multiples.
 * \param[in]  min The lower inclusive boundary of the range.
 * \param[in]  max The upper inclusive boundary of the range.
 *
 * \return  The number of multiples of 'n' in the range min --> max.
 */
uint32_t get_multiples_nb_in_range(uint32_t n, uint32_t min, uint32_t max);

extern uint64_t const powerof10[16];

#if __has_feature(nullability)
#pragma GCC diagnostic pop
#endif

#endif
