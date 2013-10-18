/**************************************************************************/
/*                                                                        */
/*  Copyright (C) 2004-2013 INTERSEC SA                                   */
/*                                                                        */
/*  Should you receive a copy of this source code, you must check you     */
/*  have a proper, written authorization of INTERSEC to hold it. If you   */
/*  don't have such an authorization, you must DELETE all source code     */
/*  files in your possession, and inform INTERSEC of the fact you obtain  */
/*  these files. Should you not comply to these terms, you can be         */
/*  prosecuted in the extent permitted by applicable law.                 */
/*                                                                        */
/**************************************************************************/

#include <math.h>

#include "http.h"
#include "z.h"

Z_GROUP_EXPORT(str)
{
    char    buf[BUFSIZ];
    ssize_t res;

    Z_TEST(ebcdic, "str: sb_conv_from_ebcdic297") {
        static char const ebcdic[] = {
#include "samples/ebcdic.sample.bin"
            0
        };
        static char const utf8[] = {
#include "samples/ebcdic.sample.utf-8.bin"
            0
        };
        static lstr_t const utf8s = LSTR_INIT(utf8, sizeof(utf8) - 1);
        SB_8k(sb);

        Z_ASSERT_N(sb_conv_from_ebcdic297(&sb, ebcdic, sizeof(ebcdic) - 1));
        Z_ASSERT_LSTREQUAL(LSTR_SB_V(&sb), utf8s);
    } Z_TEST_END;

    Z_TEST(strconv_hexdecode, "str: strconv_hexdecode") {
        const char *encoded = "30313233";
        const char *decoded = "0123";

        p_clear(buf, countof(buf));
        res = strconv_hexdecode(buf, sizeof(buf), encoded, -1);
        Z_ASSERT_EQ((size_t)res, strlen(encoded) / 2);
        Z_ASSERT_STREQUAL(buf, decoded);

        encoded = "1234567";
        p_clear(buf, countof(buf));
        Z_ASSERT_NEG(strconv_hexdecode(buf, sizeof(buf), encoded, -1),
                 "str_hexdecode should not accept odd-length strings");
        encoded = "1234567X";
        p_clear(buf, countof(buf));
        Z_ASSERT_NEG(strconv_hexdecode(buf, sizeof(buf), encoded, -1),
                 "str_hexdecode accepted non hexadecimal string");
    } Z_TEST_END;

    Z_TEST(utf8_stricmp, "str: utf8_stricmp test") {

#define RUN_UTF8_TEST_(Str1, Str2, Strip, Val) \
        ({  int len1 = strlen(Str1);                                         \
            int len2 = strlen(Str2);                                         \
            int cmp  = utf8_stricmp(Str1, len1, Str2, len2, Strip);          \
                                                                             \
            Z_ASSERT_EQ(cmp, Val, "utf8_stricmp(\"%.*s\", \"%.*s\", %d) "    \
                        "returned bad value: %d, expected %d",               \
                        len1, Str1, len2, Str2, Strip, cmp, Val);            \
        })

#define RUN_UTF8_TEST(Str1, Str2, Val) \
        ({  RUN_UTF8_TEST_(Str1, Str2, false, Val);                          \
            RUN_UTF8_TEST_(Str2, Str1, false, -(Val));                       \
            RUN_UTF8_TEST_(Str1, Str2, true, Val);                           \
            RUN_UTF8_TEST_(Str2, Str1, true, -(Val));                        \
            RUN_UTF8_TEST_(Str1"   ", Str2, true, Val);                      \
            RUN_UTF8_TEST_(Str1, Str2"    ", true, Val);                     \
            RUN_UTF8_TEST_(Str1"     ", Str2"  ", true, Val);                \
            if (Val == 0) {                                                  \
                RUN_UTF8_TEST_(Str1"   ", Str2, false, 1);                   \
                RUN_UTF8_TEST_(Str1, Str2"   ", false, -1);                  \
                RUN_UTF8_TEST_(Str1"  ", Str2"    ", false, -1);             \
            }                                                                \
        })

        /* Basic tests and case tests */
        RUN_UTF8_TEST("abcdef", "abcdef", 0);
        RUN_UTF8_TEST("AbCdEf", "abcdef", 0);
        RUN_UTF8_TEST("abcdef", "abbdef", 1);
        RUN_UTF8_TEST("aBCdef", "abbdef", 1);

        /* Accentuation tests */
        RUN_UTF8_TEST("abcdéf", "abcdef", 0);
        RUN_UTF8_TEST("abcdÉf", "abcdef", 0);
        RUN_UTF8_TEST("àbcdèf", "abcdef", 0);

        /* Collation tests */
        RUN_UTF8_TEST("æbcdef", "aebcdef", 0);
        RUN_UTF8_TEST("æbcdef", "aébcdef", 0);
        RUN_UTF8_TEST("abcdœf", "abcdoef", 0);
        RUN_UTF8_TEST("abcdŒf", "abcdoef", 0);

        RUN_UTF8_TEST("æ", "a", 1);
        RUN_UTF8_TEST("æ", "ae", 0);
        RUN_UTF8_TEST("ß", "ss", 0);
        RUN_UTF8_TEST("ßß", "ssss", 0);
        RUN_UTF8_TEST("ßß", "sßs", 0); /* Overlapping collations */

#undef RUN_UTF8_TEST_
#undef RUN_UTF8_TEST
    } Z_TEST_END;

    Z_TEST(utf8_strcmp, "str: utf8_strcmp test") {

#define RUN_UTF8_TEST_(Str1, Str2, Strip, Val) \
        ({  int len1 = strlen(Str1);                                         \
            int len2 = strlen(Str2);                                         \
            int cmp  = utf8_strcmp(Str1, len1, Str2, len2, Strip);           \
                                                                             \
            Z_ASSERT_EQ(cmp, Val, "utf8_strcmp(\"%.*s\", \"%.*s\", %d) "     \
                        "returned bad value: %d, expected %d",               \
                        len1, Str1, len2, Str2, Strip, cmp, Val);            \
        })

#define RUN_UTF8_TEST(Str1, Str2, Val) \
        ({  RUN_UTF8_TEST_(Str1, Str2, false, Val);                          \
            RUN_UTF8_TEST_(Str2, Str1, false, -(Val));                       \
            RUN_UTF8_TEST_(Str1, Str2, true, Val);                           \
            RUN_UTF8_TEST_(Str2, Str1, true, -(Val));                        \
            RUN_UTF8_TEST_(Str1"   ", Str2, true, Val);                      \
            RUN_UTF8_TEST_(Str1, Str2"    ", true, Val);                     \
            RUN_UTF8_TEST_(Str1"     ", Str2"  ", true, Val);                \
            if (Val == 0) {                                                  \
                RUN_UTF8_TEST_(Str1"   ", Str2, false, 1);                   \
                RUN_UTF8_TEST_(Str1, Str2"   ", false, -1);                  \
                RUN_UTF8_TEST_(Str1"  ", Str2"    ", false, -1);             \
            }                                                                \
        })

        /* Basic tests and case tests */
        RUN_UTF8_TEST("abcdef", "abcdef", 0);
        RUN_UTF8_TEST("AbCdEf", "abcdef", -1);
        RUN_UTF8_TEST("abcdef", "abbdef", 1);
        RUN_UTF8_TEST("aBCdef", "abbdef", -1);

        /* Accentuation tests */
        RUN_UTF8_TEST("abcdéf", "abcdef", 0);
        RUN_UTF8_TEST("abcdÉf", "abcdef", -1);
        RUN_UTF8_TEST("àbcdèf", "abcdef", 0);

        /* Collation tests */
        RUN_UTF8_TEST("æbcdef", "aebcdef", 0);
        RUN_UTF8_TEST("æbcdef", "aébcdef", 0);
        RUN_UTF8_TEST("abcdœf", "abcdoef", 0);
        RUN_UTF8_TEST("abcdŒf", "abcdoef", -1);

        RUN_UTF8_TEST("æ", "a", 1);
        RUN_UTF8_TEST("æ", "ae", 0);
        RUN_UTF8_TEST("ß", "ss", 0);
        RUN_UTF8_TEST("ßß", "ssss", 0);
        RUN_UTF8_TEST("ßß", "sßs", 0); /* Overlapping collations */

#undef RUN_UTF8_TEST_
#undef RUN_UTF8_TEST
    } Z_TEST_END;

    Z_TEST(path_simplify, "str-path: path_simplify") {
#define T(s0, s1)  \
        ({ pstrcpy(buf, sizeof(buf), s0);    \
            Z_ASSERT_N(path_simplify(buf)); \
            Z_ASSERT_STREQUAL(buf, s1); })

        buf[0] = '\0';
        Z_ASSERT_NEG(path_simplify(buf));
        T("/a/b/../../foo/./", "/foo");
        T("/test/..///foo/./", "/foo");
        T("/../test//foo///",  "/test/foo");
        T("./test/bar",        "test/bar");
        T("./test/../bar",     "bar");
        T("./../test",         "../test");
        T(".//test",           "test");
        T("a/..",              ".");
        T("a/../../..",        "../..");
        T("a/../../b/../c",    "../c");
#undef T
    } Z_TEST_END;

    Z_TEST(path_is_safe, "str-path: path_is_safe test") {
#define T(how, path)  Z_ASSERT(how path_is_safe(path), path)
        T(!, "/foo");
        T(!, "../foo");
        T( , "foo/bar");
        T(!, "foo/bar/foo/../../../../bar");
        T(!, "foo/bar///foo/../../../../bar");
#undef T
    } Z_TEST_END;

    Z_TEST(strstart, "str: strstart") {
        static const char *week =
            "Monday Tuesday Wednesday Thursday Friday Saturday Sunday";
        const char *p;

        Z_ASSERT(strstart(week, "Monday", &p));
        Z_ASSERT(week + strlen("Monday") == p,
                 "finding Monday at the proper position");
        Z_ASSERT(!strstart(week, "Tuesday", NULL),
                 "week doesn't start with Tuesday");
    } Z_TEST_END;

    Z_TEST(stristart, "str: stristart") {
        static const char *week =
            "Monday Tuesday Wednesday Thursday Friday Saturday Sunday";
        const char *p = NULL;

        Z_ASSERT(stristart(week, "mOnDaY", &p));
        Z_ASSERT(week + strlen("mOnDaY") == p,
                 "finding mOnDaY at the proper position");
        Z_ASSERT(!stristart(week, "tUESDAY", NULL),
                 "week doesn't start with tUESDAY");
    } Z_TEST_END;

    Z_TEST(stristrn, "str: stristrn") {
        static const char *alphabet = "abcdefghijklmnopqrstuvwxyz";

        Z_ASSERT(stristr(alphabet, "aBC") == alphabet,
                 "not found at start of string");
        Z_ASSERT(stristr(alphabet, "Z") == alphabet + 25,
                 "not found at end of string");
        Z_ASSERT(stristr(alphabet, "mn") == alphabet + 12,
                 "not found in the middle of the string");
        Z_ASSERT_NULL(stristr(alphabet, "123"), "inexistant string found");
    } Z_TEST_END;

    Z_TEST(strfind, "str: strfind") {
        Z_ASSERT( strfind("1,2,3,4", "1", ','));
        Z_ASSERT( strfind("1,2,3,4", "2", ','));
        Z_ASSERT( strfind("1,2,3,4", "4", ','));
        Z_ASSERT(!strfind("11,12,13,14", "1", ','));
        Z_ASSERT(!strfind("11,12,13,14", "2", ','));
        Z_ASSERT( strfind("11,12,13,14", "11", ','));
        Z_ASSERT(!strfind("11,12,13,14", "111", ','));
        Z_ASSERT(!strfind("toto,titi,tata,tutu", "to", ','));
        Z_ASSERT(!strfind("1|2|3|4|", "", '|'));
        Z_ASSERT( strfind("1||3|4|", "", '|'));
    } Z_TEST_END;

    Z_TEST(buffer_increment, "str: buffer_increment") {
#define T(initval, expectedval, expectedret)       \
        ({  pstrcpy(buf, sizeof(buf), initval);                      \
            Z_ASSERT_EQ(expectedret, buffer_increment(buf, -1));     \
            Z_ASSERT_STREQUAL(buf, expectedval); })

        T("0", "1", 0);
        T("1", "2", 0);
        T("00", "01", 0);
        T("42", "43", 0);
        T("09", "10", 0);
        T("99", "00", 1);
        T(" 99", " 00", 1);
        T("", "", 1);
        T("foobar-00", "foobar-01", 0);
        T("foobar-0-99", "foobar-0-00", 1);

#undef T
    } Z_TEST_END;

    Z_TEST(buffer_increment_hex, "str: buffer_increment_hex") {
#define T(initval, expectedval, expectedret)   \
        ({  pstrcpy(buf, sizeof(buf), initval);                          \
            Z_ASSERT_EQ(expectedret, buffer_increment_hex(buf, -1));     \
            Z_ASSERT_STREQUAL(buf, expectedval); })

        T("0", "1", 0);
        T("1", "2", 0);
        T("9", "A", 0);
        T("a", "b", 0);
        T("Ab", "Ac", 0);
        T("00", "01", 0);
        T("42", "43", 0);
        T("09", "0A", 0);
        T("0F", "10", 0);
        T("FFF", "000", 1);
        T(" FFF", " 000", 1);
        T("FFFFFFFFFFFFFFF", "000000000000000", 1);
        T("", "", 1);
        T("foobar", "foobar", 1);
        T("foobaff", "foobb00", 0);
        T("foobar-00", "foobar-01", 0);
        T("foobar-0-ff", "foobar-0-00", 1);

#undef T
    } Z_TEST_END;

    Z_TEST(strrand, "str: strrand") {
        char b[32];

        Z_ASSERT_EQ(0, pstrrand(b, sizeof(b), 0, 0));
        Z_ASSERT_EQ(strlen(b), 0U);

        Z_ASSERT_EQ(3, pstrrand(b, sizeof(b), 0, 3));
        Z_ASSERT_EQ(strlen(b), 3U);

        /* Ask for 32 bytes, where buffer can only contain 31. */
        Z_ASSERT_EQ(ssizeof(b) - 1, pstrrand(b, sizeof(b), 0, sizeof(b)));
        Z_ASSERT_EQ(sizeof(b) - 1, strlen(b));
    } Z_TEST_END;

    Z_TEST(strtoip, "str: strtoip") {
#define T(p, err_exp, val_exp, end_i) \
        ({  const char *endp;                                               \
            int end_exp = (end_i >= 0) ? end_i : (int)strlen(p);            \
                                                                            \
            errno = 0;                                                      \
            Z_ASSERT_EQ(val_exp, strtoip(p, &endp));                        \
            Z_ASSERT_EQ(err_exp, errno);                                    \
            Z_ASSERT_EQ(end_exp, endp - p);                                 \
        })

        T("123", 0, 123, -1);
        T(" 123", 0, 123, -1);
        T(" +123", 0, 123, -1);
        T("  -123", 0, -123, -1);
        T(" +-123", EINVAL, 0, 2);
        T("123 ", 0, 123, 3);
        T("123z", 0, 123, 3);
        T("123+", 0, 123, 3);
        T("2147483647", 0, 2147483647, -1);
        T("2147483648", ERANGE, 2147483647, -1);
        T("21474836483047203847094873", ERANGE, 2147483647, -1);
        T("000000000000000000000000000000000001", 0, 1, -1);
        T("-2147483647", 0, -2147483647, -1);
        T("-2147483648", 0, -2147483647 - 1, -1);
        T("-2147483649", ERANGE, -2147483647 - 1, -1);
        T("-21474836483047203847094873", ERANGE, -2147483647 - 1, -1);
        T("-000000000000000000000000000000000001", 0, -1, -1);
        T("", EINVAL, 0, -1);
        T("          ", EINVAL, 0, -1);
        T("0", 0, 0, -1);
        T("0x0", 0, 0, 1);
        T("010", 0, 10, -1);
#undef T
    } Z_TEST_END;

    Z_TEST(memtoip, "str: memtoip") {
#define T(p, err_exp, val_exp, end_i) \
        ({  const byte *endp;                                               \
            int end_exp = (end_i >= 0) ? end_i : (int)strlen(p);            \
                                                                            \
            errno = 0;                                                      \
            Z_ASSERT_EQ(val_exp, memtoip(p, strlen(p), &endp));             \
            Z_ASSERT_EQ(err_exp, errno);                                    \
            Z_ASSERT_EQ(end_exp, endp - (const byte *)p);                   \
        })

        T("123", 0, 123, -1);
        T(" 123", 0, 123, -1);
        T(" +123", 0, 123, -1);
        T("  -123", 0, -123, -1);
        T(" +-123", EINVAL, 0, 2);
        T("123 ", 0, 123, 3);
        T("123z", 0, 123, 3);
        T("123+", 0, 123, 3);
        T("2147483647", 0, 2147483647, -1);
        T("2147483648", ERANGE, 2147483647, -1);
        T("21474836483047203847094873", ERANGE, 2147483647, -1);
        T("000000000000000000000000000000000001", 0, 1, -1);
        T("-2147483647", 0, -2147483647, -1);
        T("-2147483648", 0, -2147483647 - 1, -1);
        T("-2147483649", ERANGE, -2147483647 - 1, -1);
        T("-21474836483047203847094873", ERANGE, -2147483647 - 1, -1);
        T("-000000000000000000000000000000000001", 0, -1, -1);
        T("", EINVAL, 0, -1);
        T("          ", EINVAL, 0, -1);
        T("0", 0, 0, -1);
        T("0x0", 0, 0, 1);
        T("010", 0, 10, -1);
#undef T
    } Z_TEST_END;

    Z_TEST(strtolp, "str: strtolp") {
#define T(p, flags, min, max, val_exp, ret_exp, end_i) \
    ({  const char *endp;                                                    \
        long val;                                                            \
        int end_exp = (end_i >= 0) ? end_i : (int)strlen(p);                 \
                                                                             \
        Z_ASSERT_EQ(ret_exp, strtolp(p, &endp, 0, &val, flags, min, max));   \
        if (ret_exp == 0) {                                                  \
            Z_ASSERT_EQ(val_exp, val);                                       \
            Z_ASSERT_EQ(end_exp, endp - p);                                  \
        }                                                                    \
    })

        T("123", 0, 0, 1000, 123, 0, -1);

        /* Check min/max */
        T("123", STRTOLP_CHECK_RANGE, 0, 100, 123, -ERANGE, 0);
        T("123", STRTOLP_CHECK_RANGE, 1000, 2000, 123, -ERANGE, 0);

        /* check min/max corner cases */
        T("123", STRTOLP_CHECK_RANGE, 0, 123, 123, 0, -1);
        T("123", STRTOLP_CHECK_RANGE, 0, 122, 123, -ERANGE, 0);
        T("123", STRTOLP_CHECK_RANGE, 123, 1000, 123, 0, -1);
        T("123", STRTOLP_CHECK_RANGE, 124, 1000, 123, -ERANGE, 0);

        /* Check skipspaces */
        T(" 123", 0, 0, 1000, 123, -EINVAL, 0);
        T("123 ", STRTOLP_CHECK_END, 0, 100, 123, -EINVAL, 0);
        T(" 123 ", STRTOLP_CHECK_END | STRTOLP_CHECK_RANGE, 0, 100, 123, -EINVAL, 0);
        T(" 123", STRTOLP_IGNORE_SPACES, 0, 100, 123, 0, -1);
        T(" 123 ", STRTOLP_IGNORE_SPACES, 0, 100, 123, 0, -1);
        T(" 123 ", STRTOLP_IGNORE_SPACES | STRTOLP_CHECK_RANGE, 0, 100, 123, -ERANGE, 0);
        T(" 123 ", STRTOLP_IGNORE_SPACES | STRTOLP_CLAMP_RANGE, 0, 100, 100, 0, -1);
        T("123456789012345678901234567890", 0, 0, 100, 123, -ERANGE, 0);
        T("123456789012345678901234567890 ", STRTOLP_CHECK_END, 0, 100, 123, -EINVAL, 0);
        T("123456789012345678901234567890",  STRTOLP_CLAMP_RANGE, 0, 100, 100, 0, -1);
        T("123456789012345678901234567890 ", STRTOLP_CLAMP_RANGE, 0, 100, 100, 0, 30);
#undef T
    } Z_TEST_END;

    Z_TEST(memtoxll_ext, "str: memtoxll_ext") {
#define T(str, sgn, p, len, _endp, base, val_exp, ret_exp, end_exp, err_exp) \
    do {                                                                     \
        uint64_t val;                                                        \
        int _len = (len == INT_MAX || str) ? (int)strlen(p) : len;           \
        int _ret_exp = (ret_exp == INT_MAX && _len >= 0) ? _len : ret_exp;   \
        int _end_exp = (_ret_exp >= 0 && !end_exp) ? _ret_exp : end_exp;     \
        int ret;                                                             \
                                                                             \
        if (str) {                                                           \
            ret = (sgn) ? strtoll_ext(p, (int64_t *)&val,                    \
                                      _endp ? (const char **)&endp : NULL,   \
                                      base)                                  \
                        : strtoull_ext(p, &val,                              \
                                       _endp ? (const char **)&endp : NULL,  \
                                       base);                                \
        } else {                                                             \
            ret = (sgn) ? memtoll_ext(p, _len, (int64_t *)&val,              \
                                      _endp ? &endp : NULL, base)            \
                        : memtoull_ext(p, _len, &val, _endp ? &endp : NULL,  \
                                       base);                                \
        }                                                                    \
                                                                             \
        Z_ASSERT_EQ(_ret_exp, ret);                                          \
        if (errno != EINVAL)                                                 \
            Z_ASSERT_EQ((uint64_t)(val_exp), val);                           \
        Z_ASSERT_EQ(err_exp, errno);                                         \
        if (_endp)                                                           \
            Z_ASSERT_EQ(_end_exp, (const char *)endp - (const char *)p);     \
    } while (0)

#define TT_MEM(p, len, _endp, base, val_exp, ret_exp, end_exp, err_exp)      \
    do {                                                                     \
        T(0, 0, p, len, _endp, base, val_exp, ret_exp, end_exp, err_exp);    \
        T(0, 1, p, len, _endp, base, val_exp, ret_exp, end_exp, err_exp);    \
    } while (0)

#define TT_USGN(p, len, _endp, base, val_exp, ret_exp, end_exp, err_exp)     \
    do {                                                                     \
        T(0, 0, p, len, _endp, base, val_exp, ret_exp, end_exp, err_exp);    \
        T(1, 0, p, len, _endp, base, val_exp, ret_exp, end_exp, err_exp);    \
    } while (0)

#define TT_SGN(p, len, _endp, base, val_exp, ret_exp, end_exp, err_exp)      \
    do {                                                                     \
        T(0, 1, p, len, _endp, base, val_exp, ret_exp, end_exp, err_exp);    \
        T(1, 1, p, len, _endp, base, val_exp, ret_exp, end_exp, err_exp);    \
    } while (0)

#define TT_ALL(p, len, _endp, base, val_exp, ret_exp, end_exp, err_exp)      \
    do {                                                                     \
        TT_USGN(p, len, _endp, base, val_exp, ret_exp, end_exp, err_exp);    \
        TT_SGN( p, len, _endp, base, val_exp, ret_exp, end_exp, err_exp);    \
    } while (0)

        const void *endp;

        TT_ALL("123",           3, true, 0, 123,  3, 0, 0);
        TT_ALL("123.456", INT_MAX, true, 0, 123,  3, 0, 0);

        /* different len */
        TT_MEM("123",  2, true, 0,  12,  2, 0, 0);
        TT_MEM("123;", 4, true, 0, 123,  3, 0, 0);
        TT_MEM("123k", 3, true, 0, 123,  3, 0, 0);
        TT_MEM("123",  0, true, 0,   0,  0, 0, 0);
        TT_MEM("123", -1, true, 0,   0, -1, 0, EINVAL);

        /* argument endp NULL */
        TT_ALL("123", INT_MAX, false, 0, 123, INT_MAX, 0, 0);

        /* spaces and sign char */
        TT_ALL("  123  ", INT_MAX, true, 0,  123, 5, 0, 0);
        TT_ALL("+123",    INT_MAX, true, 0,  123, INT_MAX, 0, 0);
        TT_SGN("-123",    INT_MAX, true, 0, -123, INT_MAX, 0, 0);
        TT_ALL("  +",     INT_MAX, true, 0,  0, 0, 0, 0);
        TT_ALL("  -",     INT_MAX, true, 0,  0, 0, 0, 0);

        /* other bases than 10 */
        TT_ALL("0x123", INT_MAX, true,  0, 0x123, INT_MAX, 0, 0);
        TT_ALL("0123",  INT_MAX, true,  0,  0123, INT_MAX, 0, 0);
        TT_ALL("123",   INT_MAX, true, 20,   443, INT_MAX, 0, 0);

        /* extensions */
        TT_ALL("100w",  INT_MAX, true, 0,   60480000, INT_MAX, 0, 0);
        TT_ALL("100d",  INT_MAX, true, 0,    8640000, INT_MAX, 0, 0);
        TT_ALL("100h",  INT_MAX, true, 0,     360000, INT_MAX, 0, 0);
        TT_ALL("100m",  INT_MAX, true, 0,       6000, INT_MAX, 0, 0);
        TT_ALL("100s",  INT_MAX, true, 0,        100, INT_MAX, 0, 0);
        TT_ALL("100T",  INT_MAX, true, 0, 100L << 40, INT_MAX, 0, 0);
        TT_ALL("100G",  INT_MAX, true, 0, 100L << 30, INT_MAX, 0, 0);
        TT_ALL("100M",  INT_MAX, true, 0, 100  << 20, INT_MAX, 0, 0);
        TT_ALL("100K",  INT_MAX, true, 0,     102400, INT_MAX, 0, 0);
        TT_ALL("100K;", INT_MAX, true, 0,     102400,       4, 0, 0);
        TT_MEM("100Ki",       4, true, 0,     102400,       4, 0, 0);

        /* extension with octal number */
        TT_ALL("012K",  INT_MAX, true, 0, 10240, INT_MAX, 0, 0);

        /* negative number with extension */
        TT_SGN("-100K", INT_MAX, true, 0, -102400, INT_MAX, 0, 0);

        /* invalid extensions */
        TT_ALL("100k",  INT_MAX, true, 0, 100, -1, 3, EDOM);
        TT_ALL("100Ki", INT_MAX, true, 0, 100, -1, 4, EDOM);

        /* values at limits for unsigned */
        TT_USGN("18446744073709551615s", INT_MAX, true, 0, UINT64_MAX,
                INT_MAX, 0, 0);
        TT_USGN("18446744073709551616s", INT_MAX, true, 0, UINT64_MAX,
                -1, 20, ERANGE);
        TT_USGN("16777215T", INT_MAX, true, 0, 16777215 * (1UL << 40),
                INT_MAX, 0, 0);
        TT_USGN("16777216T", INT_MAX, true, 0, UINT64_MAX, -1, 9, ERANGE);

        /* positives values at limits for signed */
        TT_SGN("9223372036854775807s", INT_MAX, true, 0, INT64_MAX,
               INT_MAX, 0, 0);
        TT_SGN("9223372036854775808s", INT_MAX, true, 0, INT64_MAX,
               -1, 19, ERANGE);
        TT_SGN("8388607T", INT_MAX, true, 0, 8388607 * (1L << 40),
               INT_MAX, 0, 0);
        TT_SGN("8388608T", INT_MAX, true, 0, INT64_MAX, -1, 8, ERANGE);

        /* negatives values at limits for signed */
        TT_SGN("-9223372036854775808s", INT_MAX, true, 0, INT64_MIN,
               INT_MAX, 0, 0);
        TT_SGN("-9223372036854775809s", INT_MAX, true, 0, INT64_MIN,
               -1, 20, ERANGE);
        TT_SGN("-8388608T", INT_MAX, true, 0, -8388608 * (1L << 40),
               INT_MAX, 0, 0);
        TT_SGN("-8388609T", INT_MAX, true, 0, INT64_MIN, -1, 9, ERANGE);

#undef T
#undef TT_MEM
#undef TT_USGN
#undef TT_SGN
#undef TT_ALL
    } Z_TEST_END;

    Z_TEST(memtod, "str: memtod") {

#define DOUBLE_ABS(_d)   (_d) > 0 ? (_d) : -(_d)

/* Absolute maximum error is bad, but in our case it is perfectly
 * acceptable
 */
#define DOUBLE_CMP(_d1, _d2)  (DOUBLE_ABS(_d1 - _d2) < 0.00001)

#define TD(p, err_exp, val_exp, end_i) \
        ({  const byte *endp;                                               \
            int end_exp = (end_i >= 0) ? end_i : (int)strlen(p);            \
                                                                            \
            errno = 0;                                                      \
            Z_ASSERT(DOUBLE_CMP(val_exp, memtod(p, strlen(p), &endp)));     \
            Z_ASSERT_EQ(err_exp, errno);                                    \
            Z_ASSERT_EQ(end_exp, endp - (const byte *)p);                   \
            Z_ASSERT(DOUBLE_CMP(val_exp, memtod(p, -1, &endp)));            \
            Z_ASSERT_EQ(err_exp, errno);                                    \
            Z_ASSERT_EQ(end_exp, endp - (const byte *)p);                   \
        })

        TD("123", 0, 123.0, -1);
        TD(" 123", 0, 123.0, -1);
        TD("123.18", 0, 123.18, -1);
        TD(" +123.90", 0, 123.90, -1);
        TD("  -123", 0, -123.0, -1);
        TD("123.50 ", 0, 123.50, 6);
        TD("123z.50", 0, 123, 3);
        TD("123+", 0, 123.0, 3);
        TD("000000000000000000000000000000000001", 0, 1, -1);
        TD("-000000000000000000000000000000000001", 0, -1, -1);
        TD("", 0, 0, -1);
        TD("          ", 0, 0, 0);
        TD("0", 0, 0, -1);
        TD("0x0", 0, 0, -1);
        TD("010", 0, 10, -1);
        TD("10e3", 0, 10000, -1);
        TD("0.1e-3", 0, 0.0001, -1);

#undef TD
#undef DOUBLE_CMP
#undef DOUBLE_ABS
    } Z_TEST_END;

    Z_TEST(str_tables, "str: test conversion tables") {
        for (int i = 0; i < countof(__str_unicode_lower); i++) {
            /* Check idempotence */
            if (__str_unicode_lower[i] < countof(__str_unicode_lower)) {
                Z_ASSERT_EQ(__str_unicode_lower[i],
                            __str_unicode_lower[__str_unicode_lower[i]],
                            "%x", i);
            }
            if (__str_unicode_upper[i] < countof(__str_unicode_upper)) {
                Z_ASSERT_EQ(__str_unicode_upper[i],
                            __str_unicode_upper[__str_unicode_upper[i]],
                            "%x", i);
            }
        }

        for (int i = 0; i < countof(__str_unicode_general_ci); i++) {
            uint32_t ci = __str_unicode_general_ci[i];
            uint32_t cs = __str_unicode_general_cs[i];

            cs = (__str_unicode_upper[cs >> 16] << 16)
               |  __str_unicode_upper[cs & 0xffff];

            Z_ASSERT_EQ(ci, cs);
        }
    } Z_TEST_END;

    Z_TEST(str_normalize, "str: utf8 normalizer") {
        SB_1k(sb);

#define T(from, ci, cs)  do {                                                \
        sb_reset(&sb);                                                       \
        Z_ASSERT_N(sb_normalize_utf8(&sb, from, sizeof(from) - 1, true));    \
        Z_ASSERT_EQUAL(sb.data, sb.len, ci, sizeof(ci) - 1);                 \
        sb_reset(&sb);                                                       \
        Z_ASSERT_N(sb_normalize_utf8(&sb, from, sizeof(from) - 1, false));   \
        Z_ASSERT_EQUAL(sb.data, sb.len, cs, sizeof(cs) - 1);                 \
    } while (0)

        T("toto", "TOTO", "toto");
        T("ToTo", "TOTO", "ToTo");
        T("électron", "ELECTRON", "electron");
        T("Électron", "ELECTRON", "Electron");

        T("Blisßs", "BLISSSS", "Blissss");
        T("Œœ", "OEOE", "OEoe");
#undef T
    } Z_TEST_END;

    Z_TEST(sb_add_double_fmt, "str: sb_add_double_fmt") {
#define T(val, nb_max_decimals, dec_sep, thousand_sep, res) \
    ({  SB_1k(sb);                                                           \
                                                                             \
        sb_add_double_fmt(&sb, val, nb_max_decimals, dec_sep, thousand_sep); \
        Z_ASSERT_LSTREQUAL(LSTR_SB_V(&sb), LSTR_IMMED_V(res));               \
    })

        T(    0,          5, '.', ',',  "0");
        T(   -0,          5, '.', ',',  "0");
        T(    1,          5, '.', ',',  "1");
        T(   12,          5, '.', ',',  "12");
        T(  123,          5, '.', ',',  "123");
        T( 1234,          5, '.', ',',  "1,234");
        T( 1234.123,      0, '.', ',',  "1,234");
        T( 1234.123,      1, '.', ',',  "1,234.1");
        T( 1234.123,      2, '.', ',',  "1,234.12");
        T( 1234.123,      3, '.', ',',  "1,234.123");
        T( 1234.123,      4, '.', ',',  "1,234.1230");
        T(-1234.123,      5, ',', ' ', "-1 234,12300");
        T(-1234.123,      5, '.',  -1, "-1234.12300");
        T( 1234.00000001, 2, '.', ',', "1,234");
        T(NAN,       5, '.',  -1, "NaN");
        T(INFINITY,  5, '.',  -1, "Inf");
#undef T
    } Z_TEST_END;

    Z_TEST(str_span, "str: filtering") {
        SB_1k(sb);

#define T(f, d, from, to) do {                                               \
        f(&sb, LSTR_IMMED_V(from), d);                                       \
        Z_ASSERT_LSTREQUAL(LSTR_SB_V(&sb), LSTR_IMMED_V(to));                \
        sb_reset(&sb);                                                       \
    } while (0)

        T(sb_add_filtered, &ctype_isdigit, "1a2b3C4D5e6f7", "1234567");
        T(sb_add_filtered, &ctype_islower, "1a2b3C4D5e6f7", "abef");
        T(sb_add_filtered, &ctype_isupper, "1a2b3C4D5e6f7", "CD");

        T(sb_add_filtered_out, &ctype_isdigit, "1a2b3C4D5e6f7", "abCDef");
        T(sb_add_filtered_out, &ctype_islower, "1a2b3C4D5e6f7", "123C4D567");
        T(sb_add_filtered_out, &ctype_isupper, "1a2b3C4D5e6f7", "1a2b345e6f7");

#undef T
    } Z_TEST_END;

    Z_TEST(lstr_ascii_reverse, "str: reverse a lstr") {
        t_scope;
#define T(f, t) do {                                                         \
        lstr_t a = t_lstr_dup(f);                                            \
        lstr_t b = t_lstr_dup_ascii_reversed(a);                             \
        lstr_ascii_reverse(&a);                                              \
        Z_ASSERT_LSTREQUAL(a, (t));                                          \
        Z_ASSERT_LSTREQUAL(b, (t));                                          \
    } while (0)
        T(LSTR_NULL_V, LSTR_NULL_V);
        T(LSTR_EMPTY_V, LSTR_EMPTY_V);
        T(LSTR_IMMED_V("a"), LSTR_IMMED_V("a"));
        T(LSTR_IMMED_V("ab"), LSTR_IMMED_V("ba"));
        T(LSTR_IMMED_V("abc"), LSTR_IMMED_V("cba"));
        T(LSTR_IMMED_V("abcd"), LSTR_IMMED_V("dcba"));
#undef T
    } Z_TEST_END;

    Z_TEST(lstr_utf8_reverse, "str: reverse a lstr") {
        t_scope;
#define T(f, t) do {                                                         \
        lstr_t a = t_lstr_dup_utf8_reversed(f);                              \
        Z_ASSERT_LSTREQUAL(a, (t));                                          \
    } while (0)
        T(LSTR_NULL_V, LSTR_NULL_V);
        T(LSTR_EMPTY_V, LSTR_EMPTY_V);
        T(LSTR_IMMED_V("a"), LSTR_IMMED_V("a"));
        T(LSTR_IMMED_V("ab"), LSTR_IMMED_V("ba"));
        T(LSTR_IMMED_V("abc"), LSTR_IMMED_V("cba"));
        T(LSTR_IMMED_V("abcd"), LSTR_IMMED_V("dcba"));
        T(LSTR_IMMED_V("aé"), LSTR_IMMED_V("éa"));
        T(LSTR_IMMED_V("é"), LSTR_IMMED_V("é"));
        T(LSTR_IMMED_V("éa"), LSTR_IMMED_V("aé"));
        T(LSTR_IMMED_V("béa"), LSTR_IMMED_V("aéb"));
#undef T
    } Z_TEST_END;

    Z_TEST(ps_split, "str-array: str_explode") {
        qv_t(lstr) arr;

        qv_init(lstr, &arr);

#define T(str1, str2, str3, sep, seps) \
        TST_MAIN(NULL, str1, str2, str3, sep, seps, 0)

#define T_SKIP(str_main, str1, str2, str3, seps) \
        TST_MAIN(str_main, str1, str2, str3, "\0", seps, PS_SPLIT_SKIP_EMPTY)

#define TST_MAIN(str_main, str1, str2, str3, sep, seps, flags)               \
        ({  pstream_t ps;                                                    \
            ctype_desc_t desc;                                               \
                                                                             \
            if (flags & PS_SPLIT_SKIP_EMPTY) {                               \
                ps = ps_initstr(str_main);                                   \
            } else {                                                         \
                ps = ps_initstr(str1 sep str2 sep str3);                     \
            }                                                                \
            ctype_desc_build(&desc, seps);                                   \
            qv_deep_clear(lstr, &arr, lstr_wipe);                            \
            ps_split(ps, &desc, flags, &arr);                                \
            Z_ASSERT_EQ(arr.len, 3);                                         \
            Z_ASSERT_LSTREQUAL(arr.tab[0], LSTR_IMMED_V(str1));              \
            Z_ASSERT_LSTREQUAL(arr.tab[1], LSTR_IMMED_V(str2));              \
            Z_ASSERT_LSTREQUAL(arr.tab[2], LSTR_IMMED_V(str3)); })

        T("123", "abc", "!%*", "/", "/");
        T("123", "abc", "!%*", " ", " ");
        T("123", "abc", "!%*", "$", "$");
        T("   ", ":::", "!!!", ",", ",");

        T("secret1", "secret2", "secret3", " ", " ,;");
        T("secret1", "secret2", "secret3", ",", " ,;");
        T("secret1", "secret2", "secret3", ";", " ,;");

        qv_deep_wipe(lstr, &arr, lstr_wipe);

        T_SKIP("//123//abc/!%*", "123", "abc", "!%*", "/");
        T_SKIP("$123$$$abc$!%*", "123", "abc", "!%*", "$");
        T_SKIP(",   ,:::,!!!,,", "   ", ":::", "!!!", ",");

        T_SKIP(" secret1 secret2   secret3", "secret1",
               "secret2" , "secret3", " ,;");
        T_SKIP(",secret1;secret2,,secret3,;,,", "secret1", "secret2",
               "secret3", " ,;");
        T_SKIP("secret1;;,,secret2; ;secret3;;", "secret1", "secret2",
               "secret3", " ,;");

        qv_deep_wipe(lstr, &arr, lstr_wipe);
#undef T
#undef TST_MAIN
#undef T_SKIP
    } Z_TEST_END;

    Z_TEST(t_ps_get_http_var, "str: t_ps_get_http_var") {
        t_scope;
        pstream_t ps;
        lstr_t    key, value;

#define TST_INVALID(_text)  \
        do {                                                                 \
            ps = ps_initstr(_text);                                          \
            Z_ASSERT_NEG(t_ps_get_http_var(&ps, &key, &value));              \
        } while (0)

        TST_INVALID("");
        TST_INVALID("key");
        TST_INVALID("=value");
        TST_INVALID("=&");
#undef TST_INVALID

        ps = ps_initstr("cid1%3d1%26cid2=2&cid3=3&cid4=");
        Z_ASSERT_N(t_ps_get_http_var(&ps, &key, &value));
        Z_ASSERT_LSTREQUAL(key,   LSTR_IMMED_V("cid1=1&cid2"));
        Z_ASSERT_LSTREQUAL(value, LSTR_IMMED_V("2"));
        Z_ASSERT_N(t_ps_get_http_var(&ps, &key, &value));
        Z_ASSERT_LSTREQUAL(key,   LSTR_IMMED_V("cid3"));
        Z_ASSERT_LSTREQUAL(value, LSTR_IMMED_V("3"));
        Z_ASSERT_N(t_ps_get_http_var(&ps, &key, &value));
        Z_ASSERT_LSTREQUAL(key,   LSTR_IMMED_V("cid4"));
        Z_ASSERT_LSTREQUAL(value, LSTR_IMMED_V(""));
        Z_ASSERT(ps_done(&ps));
        Z_ASSERT_NEG(t_ps_get_http_var(&ps, &key, &value));
    } Z_TEST_END;
} Z_GROUP_END;


#define CSV_TEST_START(_str, _separator, _qchar)                             \
        __unused__ int quoting_character = (_qchar);                         \
        __unused__ int separator = (_separator);                             \
        qv_t(lstr) fields;                                                   \
        pstream_t str = ps_initstr(_str);                                    \
                                                                             \
        qv_init(lstr, &fields)


#define CSV_TEST_END()                                                       \
        qv_deep_wipe(lstr, &fields, lstr_wipe)

#define CSV_TEST_GET_ROW()                                                   \
    qv_deep_clear(lstr, &fields, lstr_wipe);                                 \
    Z_ASSERT_N(ps_get_csv_line(NULL, &str, separator,                        \
                               quoting_character, &fields))

#define CSV_TEST_FAIL_ROW() \
    qv_deep_clear(lstr, &fields, lstr_wipe);                                 \
    Z_ASSERT_NEG(ps_get_csv_line(NULL, &str, separator, quoting_character,   \
                                 &fields))

#define CSV_TEST_CHECK_EOF()  Z_ASSERT(ps_done(&str))

#define CSV_TEST_CHECK_NB_FIELDS(_n) \
    Z_ASSERT_EQ(fields.len, _n, "field count mismatch");

#define CSV_TEST_CHECK_FIELD(_n, _str)                                       \
    if (_str == NULL) {                                                      \
        Z_ASSERT_NULL(fields.tab[_n].s);                                     \
    } else {                                                                 \
        Z_ASSERT_P(fields.tab[_n].s);                                        \
        Z_ASSERT_LSTREQUAL(fields.tab[_n], LSTR_STR_V(_str), "field value"); \
    }

#define CSV_TEST_QUOTE(_str, _expected)                                      \
    do {                                                                     \
        SB_1k(buf);                                                          \
                                                                             \
        sb_adds_csvescape(&buf, _str);                                       \
        Z_ASSERT_STREQUAL(buf.data, _expected, "invalid quoting");           \
    } while (0)

Z_GROUP_EXPORT(csv) {
    Z_TEST(row1, "no row") {
        /* No row */
        CSV_TEST_START("", ',', '"');
        CSV_TEST_CHECK_EOF();
        CSV_TEST_END();
    } Z_TEST_END;

    Z_TEST(row2, "Single row") {
        CSV_TEST_START("foo,bar,baz\r\n", ',', '"');
        CSV_TEST_GET_ROW();
        CSV_TEST_END();
    } Z_TEST_END;

    Z_TEST(row3, "Several rows") {
        CSV_TEST_START("foo,bar,baz\r\n"
                       "truc,machin,bidule\r\n",
                       ',', '"');
        CSV_TEST_GET_ROW();
        CSV_TEST_GET_ROW();
        CSV_TEST_END();
    } Z_TEST_END;

    Z_TEST(row4, "Mixed line terminators") {
        CSV_TEST_START("foo,bar,baz\n"
                       "truc,machin,bidule\r\n",
                       ',', '"');
        CSV_TEST_GET_ROW();
        CSV_TEST_GET_ROW();
        CSV_TEST_END();
    } Z_TEST_END;

    Z_TEST(row5, "No line terminator") {
        CSV_TEST_START("foo,bar,baz", ',', '"');
        CSV_TEST_GET_ROW();
        CSV_TEST_END();
    } Z_TEST_END;

    Z_TEST(base1, "Base") {
        CSV_TEST_START("foo", ',', '"');
        CSV_TEST_GET_ROW();
        CSV_TEST_CHECK_NB_FIELDS(1);
        CSV_TEST_CHECK_FIELD(0, "foo");
        CSV_TEST_END();
    } Z_TEST_END;

    Z_TEST(base2, "Base 2") {
        CSV_TEST_START("foo,bar", ',', '"');
        CSV_TEST_GET_ROW();
        CSV_TEST_CHECK_NB_FIELDS(2);
        CSV_TEST_CHECK_FIELD(0, "foo");
        CSV_TEST_CHECK_FIELD(1, "bar");
        CSV_TEST_END();
    } Z_TEST_END;

    Z_TEST(base3, "Base 3") {
        CSV_TEST_START("foo,bar,baz", ',', '"');
        CSV_TEST_GET_ROW();
        CSV_TEST_CHECK_NB_FIELDS(3);
        CSV_TEST_CHECK_FIELD(0, "foo");
        CSV_TEST_CHECK_FIELD(1, "bar");
        CSV_TEST_CHECK_FIELD(2, "baz");
        CSV_TEST_END();
    } Z_TEST_END;

    Z_TEST(allowed1, "Invalid but allowed fields 1") {
        CSV_TEST_START("foo,bar\"baz", ',', '"');
        CSV_TEST_GET_ROW();
        CSV_TEST_CHECK_NB_FIELDS(2);
        CSV_TEST_CHECK_FIELD(0, "foo");
        CSV_TEST_CHECK_FIELD(1, "bar\"baz");
        CSV_TEST_END();
    } Z_TEST_END;

    Z_TEST(invalid1, "Invalid fields 2") {
        CSV_TEST_START("foo,\"ba\"z", ',', '"');
        CSV_TEST_FAIL_ROW();
        CSV_TEST_END();
    } Z_TEST_END;

    Z_TEST(empty1, "Empty fields 1") {
        CSV_TEST_START("foo,,baz", ',', '"');
        CSV_TEST_GET_ROW();
        CSV_TEST_CHECK_NB_FIELDS(3);
        CSV_TEST_CHECK_FIELD(0, "foo");
        CSV_TEST_CHECK_FIELD(1, NULL);
        CSV_TEST_CHECK_FIELD(2, "baz");
        CSV_TEST_END();
    } Z_TEST_END;

    Z_TEST(empty2, "Empty fields 2") {
        CSV_TEST_START("foo,bar,", ',', '"');
        CSV_TEST_GET_ROW();
        CSV_TEST_CHECK_NB_FIELDS(3);
        CSV_TEST_CHECK_FIELD(0, "foo");
        CSV_TEST_CHECK_FIELD(1, "bar");
        CSV_TEST_CHECK_FIELD(2, NULL);
        CSV_TEST_END();
    } Z_TEST_END;

    Z_TEST(empty3, "Empty fields 3") {
        CSV_TEST_START(",bar,baz", ',', '"');
        CSV_TEST_GET_ROW();
        CSV_TEST_CHECK_NB_FIELDS(3);
        CSV_TEST_CHECK_FIELD(0, NULL);
        CSV_TEST_CHECK_FIELD(1, "bar");
        CSV_TEST_CHECK_FIELD(2, "baz");
        CSV_TEST_END();
    } Z_TEST_END;

    Z_TEST(empty4, "Empty fields 4") {
        CSV_TEST_START(",,", ',', '"');
        CSV_TEST_GET_ROW();
        CSV_TEST_CHECK_NB_FIELDS(3);
        CSV_TEST_CHECK_FIELD(0, NULL);
        CSV_TEST_CHECK_FIELD(1, NULL);
        CSV_TEST_CHECK_FIELD(2, NULL);
        CSV_TEST_END();
    } Z_TEST_END;
    Z_TEST(empty4, "Empty fields 4") {
        CSV_TEST_START(",,", ',', '"');
        CSV_TEST_GET_ROW();
        CSV_TEST_CHECK_NB_FIELDS(3);
        CSV_TEST_CHECK_FIELD(0, NULL);
        CSV_TEST_CHECK_FIELD(1, NULL);
        CSV_TEST_CHECK_FIELD(2, NULL);
        CSV_TEST_END();
    } Z_TEST_END;


    Z_TEST(quoted1, "Quoted fields 1") {
        CSV_TEST_START("foo,\"bar\",baz", ',', '"');
        CSV_TEST_GET_ROW();
        CSV_TEST_CHECK_NB_FIELDS(3);
        CSV_TEST_END();
    } Z_TEST_END;

    Z_TEST(quoted2, "Quoted fields 2") {
        CSV_TEST_START("foo,bar,\"baz\"", ',', '"');
        CSV_TEST_GET_ROW();
        CSV_TEST_CHECK_NB_FIELDS(3);
        CSV_TEST_CHECK_FIELD(0, "foo");
        CSV_TEST_CHECK_FIELD(1, "bar");
        CSV_TEST_CHECK_FIELD(2, "baz");
        CSV_TEST_END();
    } Z_TEST_END;

    Z_TEST(quoted3, "Quoted fields 3") {
        CSV_TEST_START("\"foo\",bar,baz", ',', '"');
        CSV_TEST_GET_ROW();
        CSV_TEST_CHECK_NB_FIELDS(3);
        CSV_TEST_CHECK_FIELD(0, "foo");
        CSV_TEST_CHECK_FIELD(1, "bar");
        CSV_TEST_CHECK_FIELD(2, "baz");
        CSV_TEST_END();
    } Z_TEST_END;

    Z_TEST(quoted4, "Quoted fields 4") {
        CSV_TEST_START("\"foo,bar\",baz", ',', '"');
        CSV_TEST_GET_ROW();
        CSV_TEST_CHECK_NB_FIELDS(2);
        CSV_TEST_CHECK_FIELD(0, "foo,bar");
        CSV_TEST_CHECK_FIELD(1, "baz");
        CSV_TEST_END();
    } Z_TEST_END;

    Z_TEST(quoted5, "Quoted fields 5") {
        CSV_TEST_START("\"foo,\"\"\"", ',', '"');
        CSV_TEST_GET_ROW();
        CSV_TEST_CHECK_NB_FIELDS(1);
        CSV_TEST_CHECK_FIELD(0, "foo,\"");
        CSV_TEST_END();
    } Z_TEST_END;

    Z_TEST(quoted6, "Quoted fields 6") {
        CSV_TEST_START("\"foo\n"
                       "bar\",baz", ',', '"');
        CSV_TEST_GET_ROW();
        CSV_TEST_CHECK_NB_FIELDS(2);
        CSV_TEST_CHECK_FIELD(0, "foo\nbar");
        CSV_TEST_CHECK_FIELD(1, "baz");
        CSV_TEST_END();
    } Z_TEST_END;

    Z_TEST(quoted7, "Quoted fields 7") {
        CSV_TEST_START("\"foo,\"\"", ',', '"');
        CSV_TEST_FAIL_ROW();
        CSV_TEST_END();
    } Z_TEST_END;

    Z_TEST(quoted8, "Quoted fields 8") {
        CSV_TEST_START("\"foo,\"bar\"", ',', '"');
        CSV_TEST_FAIL_ROW();
        CSV_TEST_END();
    } Z_TEST_END;

    Z_TEST(noquoting1, "No quoting character 1") {
        CSV_TEST_START("foo,bar", ',', -1);
        CSV_TEST_GET_ROW();
        CSV_TEST_CHECK_NB_FIELDS(2);
        CSV_TEST_CHECK_FIELD(0, "foo");
        CSV_TEST_CHECK_FIELD(1, "bar");
        CSV_TEST_END();
    } Z_TEST_END;

    Z_TEST(noquoting2, "No quoting character 2") {
        CSV_TEST_START("foo,\"bar\"", ',', -1);
        CSV_TEST_GET_ROW();
        CSV_TEST_CHECK_NB_FIELDS(2);
        CSV_TEST_CHECK_FIELD(0, "foo");
        CSV_TEST_CHECK_FIELD(1, "\"bar\"");
        CSV_TEST_END();
    } Z_TEST_END;

    Z_TEST(noquoting3, "No quoting character 3") {
        CSV_TEST_START("fo\"o", ',', -1);
        CSV_TEST_GET_ROW();
        CSV_TEST_CHECK_NB_FIELDS(1);
        CSV_TEST_CHECK_FIELD(0, "fo\"o");
        CSV_TEST_END();
    } Z_TEST_END;

    Z_TEST(quoting1, "Field quoting 1") {
        CSV_TEST_QUOTE("", "");
    } Z_TEST_END;

    Z_TEST(quoting2, "Field quoting 2") {
        CSV_TEST_QUOTE("\"", "\"\"");
    } Z_TEST_END;

    Z_TEST(quoting3, "Field quoting 3") {
        CSV_TEST_QUOTE("\"\"", "\"\"\"\"");
    } Z_TEST_END;

    Z_TEST(quoting5, "Field quoting 5") {
        CSV_TEST_QUOTE("foo", "foo");
    } Z_TEST_END;

    Z_TEST(quoting5, "Field quoting 5") {
        CSV_TEST_QUOTE("foo\"", "foo\"\"");
    } Z_TEST_END;

    Z_TEST(quoting6, "Field quoting 6") {
        CSV_TEST_QUOTE("\"foo", "\"\"foo");
    } Z_TEST_END;

    Z_TEST(quoting7, "Field quoting 7") {
        CSV_TEST_QUOTE("\"foo\"", "\"\"foo\"\"");
    } Z_TEST_END;

    Z_TEST(quoting8, "Field quoting 8") {
        CSV_TEST_QUOTE("foo \" bar", "foo \"\" bar");
    } Z_TEST_END;

    Z_TEST(quoting9, "Field quoting 9") {
        CSV_TEST_QUOTE("\"foo \" bar", "\"\"foo \"\" bar");
    } Z_TEST_END;

    Z_TEST(quoting10, "Field quoting 10") {
        CSV_TEST_QUOTE("foo \" bar\"", "foo \"\" bar\"\"");
    } Z_TEST_END;

    Z_TEST(quoting11, "Field quoting 11") {
        CSV_TEST_QUOTE("\"foo \" bar\"", "\"\"foo \"\" bar\"\"");
    } Z_TEST_END;
} Z_GROUP_END;
