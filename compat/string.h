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

#ifndef IS_COMPAT_STRING_H
#define IS_COMPAT_STRING_H

#include_next <string.h>

#ifdef __APPLE__

/* Contrary to GNU's version that returns the haystack pointer in case the
 * needle is empty, OS X version (while explicitly stating this function is a
 * GNU extension...) returns NULL in that case. Ensure we rely on a
 * consistent behavior for the function.
 */
#define memmem(a, a_len, b, b_len)  ({                                       \
        const void *__a = (a);                                               \
        size_t __b_len = (b_len);                                            \
                                                                             \
        __b_len ? (memmem)(__a, (a_len), (b), __b_len) : (void *)__a;        \
    })

#endif

#endif
