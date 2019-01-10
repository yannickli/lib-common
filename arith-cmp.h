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

#if !defined(IS_LIB_COMMON_ARITH_H) || defined(IS_LIB_COMMON_ARITH_CMP_H)
#  error "you must include arith.h instead"
#else
#define IS_LIB_COMMON_ARITH_CMP_H

static inline int min_int(int a, int b)          { return MIN(a, b); }
static inline int max_int(int a, int b)          { return MAX(a, b); }
static inline int clamp_int(int a, int m, int M) { return CLIP(a, m, M); }

static inline void maximize(int * nonnull pi, int val) {
    if (*pi < val)
        *pi = val;
}
static inline void minimize(int * nonnull pi, int val) {
    if (*pi > val)
        *pi = val;
}

#endif
