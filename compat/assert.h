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

#ifndef IS_COMPAT_ASSERT_H
#define IS_COMPAT_ASSERT_H

#include_next <assert.h>
#include <stdlib.h>
#include <stdio.h>

#ifdef __APPLE__
static inline void __assert_fail(const char *expr, const char *file,
                                 int line, const char *func)
{
    fprintf(stderr, "assertion failure at %s (%s:%d): %s", func, file, line,
            expr);
    abort();
}
#endif

#endif

