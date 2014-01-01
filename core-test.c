/**************************************************************************/
/*                                                                        */
/*  Copyright (C) 2004-2014 INTERSEC SA                                   */
/*                                                                        */
/*  Should you receive a copy of this source code, you must check you     */
/*  have a proper, written authorization of INTERSEC to hold it. If you   */
/*  don't have such an authorization, you must DELETE all source code     */
/*  files in your possession, and inform INTERSEC of the fact you obtain  */
/*  these files. Should you not comply to these terms, you can be         */
/*  prosecuted in the extent permitted by applicable law.                 */
/*                                                                        */
/**************************************************************************/

#include "core.h"

int test_report(bool fail, const char *fmt, const char *file, int lno, ...)
{
    va_list ap;

    fprintf(stderr, fail ? "tst fail %s:%d:" : "tst pass %s:%d:", file, lno);
    va_start(ap, lno);
    vfprintf(stderr, fmt, ap);
    va_end(ap);
    fputc('\n', stderr);
    fflush(stderr);
    return fail ? -1 : 0;
}

int test_skip(const char *fmt, const char *file, int lno, ...)
{
    va_list ap;

    fprintf(stderr, "tst skip %s:%d:", file, lno);
    va_start(ap, lno);
    vfprintf(stderr, fmt, ap);
    va_end(ap);
    fputc('\n', stderr);
    fflush(stderr);
    return 0;
}

#ifndef NDEBUG
extern tst_t const intersec_tests_start[];
extern tst_t const intersec_tests_end[];

int test_run(int argc, const char **argv)
{
    size_t test_count = intersec_tests_end - intersec_tests_start;
    int res = 0;

    for (size_t i = 0; i < test_count; i++) {
        const tst_t *t = intersec_tests_start + i;

        fprintf(stderr, "tst desc %s:%d:%s\n", t->file, t->lineno, t->text);
        res |= (*t->fun)();
    }

    return res;
}
#else
int test_run(int argc, const char **argv)
{
    return 0;
}
#endif
