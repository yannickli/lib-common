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

#if !defined(IS_LIB_COMMON_CORE_H) || defined(IS_LIB_COMMON_CORE_TEST_H)
#  error "you must include core.h instead"
#else
#define IS_LIB_COMMON_CORE_TEST_H

typedef int (tst_f)(void);
typedef struct tst_t {
    tst_f      *fun;
    const char *text;
    const char *file;
    int         lineno;
    int         flags;
} tst_t;

#ifndef NDEBUG
#  define __TEST_DECL2(name, what, fl) \
    static tst_f TST_##name##_fun;                     \
    static __attribute__((used,section(".intersec.tests."__FILE__))) \
    tst_t const TST_##name = {                                       \
        .fun    = &TST_##name##_fun,                                 \
        .text   = what,                                              \
        .file   = __FILE__,                                          \
        .lineno = __LINE__,                                          \
        .flags  = (fl),                                              \
    };                                                               \
    __attribute__((constructor)) static void TST_CTOR_##name(void) { \
        test_register(&TST_##name);                                  \
    }                                                                \
    static int TST_##name##_fun(void)
#else
#  define __TEST_DECL2(name, what, fl) \
    static __attribute__((unused)) int TST_##name##_fun(void)
#endif
#define __TEST_DECL1(l, what, fl)  __TEST_DECL2(l, what, fl)
#define TEST_DECL(what, fl)        __TEST_DECL1(__LINE__, what, fl)

#define TEST_FAIL_IF(expr, fmt, ...) \
    RETHROW(test_report((expr), fmt, __FILE__, __LINE__, ##__VA_ARGS__))
#define TEST_PASS_UNLESS(expr, fmt, ...)  TEST_FAIL_IF(expr, fmt, ##__VA_ARGS__)
#define TEST_PASS_IF(expr, fmt, ...)      TEST_FAIL_IF(!(expr), fmt, ##__VA_ARGS__)
#define TEST_FAIL_UNLESS(expr, fmt, ...)  TEST_FAIL_IF(!(expr), fmt, ##__VA_ARGS__)
#define TEST_SKIP(fmt, ...)               test_skip(fmt, __FILE__, __LINE__, ##__VA_ARGS__)
#define TEST_DONE()                       do { return 0; } while (0)

void test_register(const tst_t *tst);
int test_run(int argc, const char **argv);
__attribute__((format(printf, 2, 5)))
int test_report(bool fail, const char *fmt, const char *file, int lno, ...);
__attribute__((format(printf, 1, 4)))
int test_skip(const char *fmt, const char *file, int lno, ...);

#endif
