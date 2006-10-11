/**************************************************************************/
/*                                                                        */
/*  Copyright (C) 2004-2006 INTERSEC                                      */
/*                                                                        */
/*  Should you receive a copy of this source code, you must check you     */
/*  have a proper, written authorization of INTERSEC to hold it. If you   */
/*  don't have such an authorization, you must DELETE all source code     */
/*  files in your possession, and inform INTERSEC of the fact you obtain  */
/*  these files. Should you not comply to these terms, you can be         */
/*  prosecuted in the extent permitted by applicable law.                 */
/*                                                                        */
/**************************************************************************/

#ifndef IS_LIB_COMMON_TIMEVAL_H
#define IS_LIB_COMMON_TIMEVAL_H

#include <sys/time.h>
#include <time.h>

#include <lib-common/macros.h>

#ifndef NDEBUG
/* Return reference to static buf for immediate printing */
const char *timeval_format(struct timeval tv);
#endif

struct timeval timeval_add(struct timeval a, struct timeval b);
struct timeval timeval_sub(struct timeval a, struct timeval b);
struct timeval timeval_mul(struct timeval tv, int k);
struct timeval timeval_div(struct timeval tv, int k);
bool is_expired(const struct timeval *date, const struct timeval *now,
                struct timeval *left);

#endif /* IS_LIB_COMMON_TIMEVAL_H */
