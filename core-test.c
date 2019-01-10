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

#include "container.h"

#ifndef NDEBUG
static struct {
    const tst_t *start;
    const tst_t *end;
} test_g;
#define _G  test_g

void test_register(const tst_t *tst)
{
    if (!_G.start || tst < _G.start)
        _G.start = tst;
    if (!_G.end || tst > _G.end)
        _G.end = tst + 1;
}

int test_run(int argc, const char **argv)
{
    size_t test_count = _G.end - _G.start;
    int res = 0;

    for (size_t i = 0; i < test_count; i++) {
        const tst_t *t = _G.start + i;

        fprintf(stderr, "tst desc %s:%d:%s\n", t->file, t->lineno, t->text);
        res |= (*t->fun)();
    }

    return res;
}

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
#else
int test_run(int argc, const char **argv)
{
    return 0;
}

void test_register(const tst_t *tst)
{
}

int test_report(bool fail, const char *fmt, const char *file, int lno, ...)
{
    return 0;
}

int test_skip(const char *fmt, const char *file, int lno, ...)
{
    return 0;
}
#endif
