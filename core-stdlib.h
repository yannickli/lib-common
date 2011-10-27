/**************************************************************************/
/*                                                                        */
/*  Copyright (C) 2004-2011 INTERSEC SAS                                  */
/*                                                                        */
/*  Should you receive a copy of this source code, you must check you     */
/*  have a proper, written authorization of INTERSEC to hold it. If you   */
/*  don't have such an authorization, you must DELETE all source code     */
/*  files in your possession, and inform INTERSEC of the fact you obtain  */
/*  these files. Should you not comply to these terms, you can be         */
/*  prosecuted in the extent permitted by applicable law.                 */
/*                                                                        */
/**************************************************************************/

#if !defined(IS_LIB_COMMON_CORE_H) || defined(IS_LIB_COMMON_CORE_STDLIB_H)
#  error "you must include core.h instead"
#else
#define IS_LIB_COMMON_CORE_STDLIB_H

/*----- core-havege.c -----*/

void ha_srand(void) __leaf;
uint32_t ha_rand(void) __leaf;
int ha_rand_range(int first, int last) __leaf;
double ha_rand_ranged(double first, double last) __leaf;


#endif
