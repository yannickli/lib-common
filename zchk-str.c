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

#include <math.h>

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

    Z_TEST(sb_add_int_fmt, "str: sb_add_int_fmt") {
#define T(val, thousand_sep, res) \
    ({  SB_1k(sb);                                                           \
                                                                             \
        sb_add_int_fmt(&sb, val, thousand_sep);                              \
        Z_ASSERT_LSTREQUAL(LSTR_SB_V(&sb), LSTR_IMMED_V(res));               \
    })

        T(        0, ',', "0");
        T(        1, ',', "1");
        T(       -1, ',', "-1");
        T(       12, ',', "12");
        T(      123, ',', "123");
        T(     1234, ',', "1,234");
        T(INT64_MIN, ',', "-9,223,372,036,854,775,808");
        T(INT64_MAX, ',', "9,223,372,036,854,775,807");
        T(     1234, ' ', "1 234");
        T(     1234,  -1, "1234");
#undef T
    } Z_TEST_END;

    Z_TEST(sb_add_uint_fmt, "str: sb_add_uint_fmt") {
#define T(val, thousand_sep, res) \
    ({  SB_1k(sb);                                                           \
                                                                             \
        sb_add_uint_fmt(&sb, val, thousand_sep);                             \
        Z_ASSERT_LSTREQUAL(LSTR_SB_V(&sb), LSTR_IMMED_V(res));               \
    })

        T(         0, ',', "0");
        T(         1, ',', "1");
        T(        12, ',', "12");
        T(       123, ',', "123");
        T(      1234, ',', "1,234");
        T(UINT64_MAX, ',', "18,446,744,073,709,551,615");
        T(      1234, ' ', "1 234");
        T(      1234,  -1, "1234");
#undef T
    } Z_TEST_END;

    Z_TEST(sb_add_csvescape, "") {
        SB_1k(sb);

#define CHECK(_str, _expected)  \
        do {                                                                 \
            sb_adds_csvescape(&sb, _str);                                    \
            Z_ASSERT_STREQUAL(_expected, sb.data);                           \
            sb_reset(&sb);                                                   \
        } while (0)

        CHECK("toto", "toto");
        CHECK("toto;tata", "\"toto;tata\"");
        CHECK("toto\"tata", "\"toto\"\"tata\"");
        CHECK("toto\n", "\"toto\n\"");
        CHECK("toto\"", "\"toto\"\"\"");
        CHECK("toto\ntata", "\"toto\ntata\"");
        CHECK("toto\n\"tata", "\"toto\n\"\"tata\"");
        CHECK("toto\"\ntata", "\"toto\"\"\ntata\"");
        CHECK("", "");
        CHECK("\"", "\"\"\"\"");
    } Z_TEST_END;
} Z_GROUP_END;
