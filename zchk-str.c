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

#include <math.h>

#include "http.h"
#include "z.h"

const void *to_free_g;

static void custom_free(mem_pool_t *m, void *p)
{
    free(p);
    if (p == to_free_g) {
        to_free_g = NULL;
    }
}

Z_GROUP_EXPORT(str)
{
    char    buf[BUFSIZ];
    ssize_t res;

    Z_TEST(lstr_copyc, "lstr_copyc") {
        lstr_t dst = lstr_dup(LSTR("a string"));
        lstr_t src = LSTR("an other string");
        void (*libc_free)(mem_pool_t *, void *) = mem_pool_libc.free;

        to_free_g = dst.s;

        mem_pool_libc.free = &custom_free,
        lstr_copyc(&dst, src);
        mem_pool_libc.free = libc_free;

        Z_ASSERT_NULL(to_free_g, "destination string has not been freed"
                      " before writing a new value to it");

        Z_ASSERT(dst.mem_pool == MEM_STATIC);
        Z_ASSERT(lstr_equal2(dst, src));
    } Z_TEST_END;

    Z_TEST(sb_detach, "sb_detach") {
        sb_t sb;
        char *p;
        int len;

        sb_init(&sb);
        p = sb_detach(&sb, NULL);
        Z_ASSERT(p != __sb_slop);
        Z_ASSERT_EQ(*p, '\0');
        p_delete(&p);

        sb_adds(&sb, "foo");
        p = sb_detach(&sb, &len);
        Z_ASSERT_EQ(len, 3);
        Z_ASSERT_STREQUAL(p, "foo");
        p_delete(&p);
    } Z_TEST_END;

    Z_TEST(sb_add_urlencode, "sb_add_urlencode") {
        SB_1k(sb);
        lstr_t raw = LSTR("test32@localhost-#!$;*");

        sb_add_urlencode(&sb, raw.s, raw.len);
        Z_ASSERT_LSTREQUAL(LSTR("test32%40localhost-%23%21%24%3B%2A"),
                           LSTR_SB_V(&sb));
    } Z_TEST_END;

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

    Z_TEST(lstr_hexencode, "str: t_lstr_hexencode/t_lstr_hexdecode") {
        t_scope;
        lstr_t src = LSTR_IMMED("intersec");
        lstr_t hex = LSTR_IMMED("696e746572736563");
        lstr_t out;

        out = t_lstr_hexencode(src);
        Z_ASSERT_LSTREQUAL(out, hex);
        out = t_lstr_hexdecode(hex);
        Z_ASSERT_LSTREQUAL(out, src);

        out = t_lstr_hexdecode(LSTR_IMMED_V("F"));
        Z_ASSERT_EQ(out.len, 0);
        Z_ASSERT_NULL(out.s);
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

    Z_TEST(utf8_str_istartswith, "str: utf8_str_istartswith test") {

#define RUN_UTF8_TEST(Str1, Str2, Val)  \
        ({  int len1 = strlen(Str1);                                         \
            int len2 = strlen(Str2);                                         \
            int cmp  = utf8_str_istartswith(Str1, len1, Str2, len2);         \
                                                                             \
            Z_ASSERT_EQ(cmp, Val,                                            \
                        "utf8_str_istartswith(\"%.*s\", \"%.*s\") "          \
                        "returned bad value: %d, expected %d",               \
                        len1, Str1, len2, Str2, cmp, Val);                   \
        })

        /* Basic tests and case tests */
        RUN_UTF8_TEST("abcdef", "abc", true);
        RUN_UTF8_TEST("abcdef", "abcdef", true);
        RUN_UTF8_TEST("abcdef", "abcdefg", false);
        RUN_UTF8_TEST("AbCdEf", "abc", true);
        RUN_UTF8_TEST("abcdef", "AbC", true);
        RUN_UTF8_TEST("aBCdef", "AbC", true);

        /* Accentuation tests */
        RUN_UTF8_TEST("abcdéf", "abcde", true);
        RUN_UTF8_TEST("abcdÉf", "abcdè", true);
        RUN_UTF8_TEST("àbcdèf", "abcdé", true);
        RUN_UTF8_TEST("àbcdè", "abcdé", true);
        RUN_UTF8_TEST("abcde", "àbCdé", true);
        RUN_UTF8_TEST("abcde", "àbcdéf", false);

#undef RUN_UTF8_TEST
    } Z_TEST_END;

    Z_TEST(lstr_utf8_iendswith, "str: lstr_utf8_iendswith test") {

#define RUN_UTF8_TEST(Str1, Str2, Val)  \
        ({  lstr_t lstr1 = LSTR(Str1);                                       \
            lstr_t lstr2 = LSTR(Str2);                                       \
            int cmp  = lstr_utf8_iendswith(&lstr1, &lstr2);                  \
                                                                             \
            Z_ASSERT_EQ(cmp, Val,                                            \
                        "lstr_utf8_iendswith(\"%s\", \"%s\") "               \
                        "returned bad value: %d, expected %d",               \
                        Str1, Str2, cmp, Val);                               \
        })

        /* Basic tests and case tests */
        RUN_UTF8_TEST("abcdef", "def", true);
        RUN_UTF8_TEST("abcdef", "abcdef", true);
        RUN_UTF8_TEST("abcdef", "0abcdef", false);
        RUN_UTF8_TEST("AbCdEf", "def", true);
        RUN_UTF8_TEST("AbCdEf", "abc", false);
        RUN_UTF8_TEST("abcdef", "DeF", true);
        RUN_UTF8_TEST("abcDEf", "deF", true);

        /* Accentuation tests */
        RUN_UTF8_TEST("abcdéf", "bcdef", true);
        RUN_UTF8_TEST("abcdÉf", "bcdèf", true);
        RUN_UTF8_TEST("àbcdèf", "abcdéF", true);
        RUN_UTF8_TEST("àbcdè", "abcdé", true);
        RUN_UTF8_TEST("abcde", "àbCdé", true);
        RUN_UTF8_TEST("abcde", "0àbcdé", false);

#undef RUN_UTF8_TEST
    } Z_TEST_END;

    Z_TEST(utf8_str_startswith, "str: utf8_str_startswith test") {

#define RUN_UTF8_TEST(Str1, Str2, Val)  \
        ({  int len1 = strlen(Str1);                                         \
            int len2 = strlen(Str2);                                         \
            int cmp  = utf8_str_startswith(Str1, len1, Str2, len2);          \
                                                                             \
            Z_ASSERT_EQ(cmp, Val,                                            \
                        "utf8_str_startswith(\"%.*s\", \"%.*s\") "           \
                        "returned bad value: %d, expected %d",               \
                        len1, Str1, len2, Str2, cmp, Val);                   \
        })

        /* Basic tests and case tests */
        RUN_UTF8_TEST("abcdef", "abc", true);
        RUN_UTF8_TEST("abcdef", "abcdef", true);
        RUN_UTF8_TEST("abcdef", "abcdefg", false);
        RUN_UTF8_TEST("AbCdEf", "abc", false);
        RUN_UTF8_TEST("abcdef", "AbC", false);
        RUN_UTF8_TEST("aBCdef", "AbC", false);
        RUN_UTF8_TEST("aBCdef", "aBC", true);

        /* Accentuation tests */
        RUN_UTF8_TEST("abcdéf", "abcde", true);
        RUN_UTF8_TEST("abcdÉf", "abcdè", false);
        RUN_UTF8_TEST("àbcdèf", "abcdé", true);
        RUN_UTF8_TEST("àbcdèf", "abcdé", true);
        RUN_UTF8_TEST("abcde", "àbcdé", true);
        RUN_UTF8_TEST("abcde", "àbcdéf", false);

#undef RUN_UTF8_TEST
    } Z_TEST_END;

    Z_TEST(lstr_utf8_endswith, "str: lstr_utf8_endswith test") {

#define RUN_UTF8_TEST(Str1, Str2, Val)  \
        ({  lstr_t lstr1 = LSTR(Str1);                                       \
            lstr_t lstr2 = LSTR(Str2);                                       \
            int cmp  = lstr_utf8_endswith(&lstr1, &lstr2);                   \
                                                                             \
            Z_ASSERT_EQ(cmp, Val,                                            \
                        "lstr_utf8_endswith(\"%s\", \"%s\") "                \
                        "returned bad value: %d, expected %d",               \
                        Str1, Str2, cmp, Val);                               \
        })

        /* Basic tests and case tests */
        RUN_UTF8_TEST("abcdef", "def", true);
        RUN_UTF8_TEST("abcdef", "abcdef", true);
        RUN_UTF8_TEST("abcdef", "0abcdef", false);
        RUN_UTF8_TEST("AbCdEf", "def", false);
        RUN_UTF8_TEST("abcdef", "DeF", false);
        RUN_UTF8_TEST("aBCdef", "deF", false);
        RUN_UTF8_TEST("aBCdEf", "dEf", true);

        /* Accentuation tests */
        RUN_UTF8_TEST("abcdéf", "bcdef", true);
        RUN_UTF8_TEST("abcdÉf", "bcdèf", false);
        RUN_UTF8_TEST("àbcdèf", "bcdéf", true);
        RUN_UTF8_TEST("àbcdèf", "abcdéf", true);
        RUN_UTF8_TEST("abcde", "0àbcdé", false);

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

    Z_TEST(path_extend, "str-path: path_extend") {
        const char *env_home = getenv("HOME") ?: "/";
        char path_test[PATH_MAX];
        char expected[PATH_MAX];
        char long_prefix[PATH_MAX];
        char very_long_prefix[2 * PATH_MAX];
        char very_long_suffix[2 * PATH_MAX];

#define T(_expected, _prefix, _suffix, ...)  \
        ({ Z_ASSERT_EQ(path_extend(path_test, _prefix, _suffix,              \
                                   ##__VA_ARGS__),                           \
                       (int)strlen(_expected));                              \
            Z_ASSERT_STREQUAL(_expected, path_test);                         \
        })

        T("/foo/bar/1", "/foo/bar/", "%d", 1);
        T("/foo/bar/", "/foo/bar/", "");
        T("/1", "/foo/bar/", "/%d", 1);
        T("/foo/bar/1", "/foo/bar", "%d", 1);
        T("/foo/bar/", "/foo/bar", "");
        T("1", "", "%d", 1);
        T("/1", "", "/%d", 1);

        memset(long_prefix, '1', sizeof(long_prefix));
        long_prefix[PATH_MAX - 3] = '\0';
        T("/foo/bar", long_prefix, "/foo/bar");

        memset(very_long_prefix, '1', sizeof(very_long_prefix));
        very_long_prefix[PATH_MAX + 5] = '\0';
        T("/foo/bar1", very_long_prefix, "/foo/bar%d", 1);

        memset(very_long_prefix, '1', sizeof(very_long_prefix));
        very_long_prefix[PATH_MAX + 5] = '\0';
        Z_ASSERT_EQ(path_extend(path_test, very_long_prefix, "foo/bar%d", 1),
                    -1);

        memset(long_prefix, '1', sizeof(long_prefix));
        long_prefix[PATH_MAX - 1] = '\0';
        Z_ASSERT_EQ(path_extend(path_test, long_prefix, ""), -1);

        memset(long_prefix, '1', sizeof(long_prefix));
        long_prefix[PATH_MAX - 2] = '/';
        long_prefix[PATH_MAX - 1] = '\0';
        Z_ASSERT_EQ(path_extend(path_test, long_prefix, ""), PATH_MAX - 1);

        memset(long_prefix, '1', sizeof(long_prefix));
        long_prefix[PATH_MAX - 2] = '\0';
        Z_ASSERT_EQ(path_extend(path_test, long_prefix, "a"), -1);

        memset(long_prefix, '1', sizeof(long_prefix));
        long_prefix[PATH_MAX - 3] = '/';
        long_prefix[PATH_MAX - 2] = '\0';
        Z_ASSERT_EQ(path_extend(path_test, long_prefix, "a"), PATH_MAX - 1);

        memset(very_long_prefix, '1', sizeof(very_long_prefix));
        very_long_prefix[PATH_MAX-1] = '\0';
        very_long_prefix[PATH_MAX-2] = '/';
        T("/foo/bar1", very_long_prefix, "/foo/bar%d", 1);

        memset(very_long_suffix, '1', sizeof(very_long_suffix));
        memset(long_prefix, '1', sizeof(long_prefix));
        very_long_suffix[0] = '/';
        very_long_suffix[PATH_MAX + 5] = '\0';
        long_prefix[PATH_MAX - 4] = '\0';
        Z_ASSERT_EQ(path_extend(path_test, long_prefix, "%s",
                                very_long_suffix), -1);

        memset(very_long_suffix, '1', sizeof(very_long_suffix));
        memset(very_long_prefix, '1', sizeof(very_long_prefix));
        very_long_suffix[0] = '/';
        very_long_suffix[PATH_MAX + 5] = '\0';
        very_long_prefix[PATH_MAX + 5] = '\0';
        Z_ASSERT_EQ(path_extend(path_test, very_long_prefix, "%s",
                                very_long_suffix), -1);

        memset(very_long_prefix, '1', sizeof(very_long_prefix));
        very_long_prefix[PATH_MAX-2] = '\0';
        very_long_prefix[PATH_MAX-3] = '/';
        T("/foo/bar1", very_long_prefix, "/foo/bar%d", 1);

        snprintf(expected, sizeof(expected), "%s/foo/bar/1", env_home);
        T(expected, "/prefix", "~/foo/bar/%d", 1);

        memset(very_long_prefix, '1', sizeof(very_long_prefix));
        very_long_prefix[PATH_MAX + 5] = '\0';
        T(expected, very_long_prefix, "~/foo/bar/%d", 1);
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
        if (!errno)                                                          \
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
        TT_ALL("  123  ", INT_MAX, true, 0,  123, 5,       0, 0);
        TT_ALL("+123",    INT_MAX, true, 0,  123, INT_MAX, 0, 0);
        TT_SGN("-123",    INT_MAX, true, 0, -123, INT_MAX, 0, 0);
        TT_ALL("  +",     INT_MAX, true, 0,  -1, -1, 0, EINVAL);
        TT_ALL("  -",     INT_MAX, true, 0,  -1, -1, 0, EINVAL);

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
        TT_USGN("-123",    INT_MAX, true, 0, 0, -1, 0, ERANGE);
        TT_USGN("   -123", INT_MAX, true, 0, 0, -1, 0, ERANGE);
        TT_USGN("    -0 ", INT_MAX, true, 0,  0, 6, 0, 0);
        TT_USGN("  -az ",  INT_MAX, true, 0,  -1, -1, 0, EINVAL);
        TT_USGN("  - az ", INT_MAX, true, 0,  -1, -1, 0, EINVAL);
        TT_USGN("  az ",   INT_MAX, true, 0,  -1, -1, 0, EINVAL);

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

    Z_TEST(str_lowup, "str: utf8 tolower/toupper") {
        SB_1k(sb);

#define T(from, low, up)  do {                                               \
        sb_reset(&sb);                                                       \
        Z_ASSERT_N(sb_add_utf8_tolower(&sb, from, sizeof(from) - 1));        \
        Z_ASSERT_EQUAL(sb.data, sb.len, low, sizeof(low) - 1);               \
        sb_reset(&sb);                                                       \
        Z_ASSERT_N(sb_add_utf8_toupper(&sb, from, sizeof(from) - 1));        \
        Z_ASSERT_EQUAL(sb.data, sb.len, up, sizeof(up) - 1);                 \
    } while (0)

        T("toto", "toto", "TOTO");
        T("ToTo", "toto", "TOTO");
        T("électron", "électron", "ÉLECTRON");
        T("Électron", "électron", "ÉLECTRON");

        T("Blisßs", "blisßs", "BLISßS");
        T("Œœ", "œœ", "ŒŒ");
#undef T
    } Z_TEST_END;


    Z_TEST(sb_add_double_fmt, "str: sb_add_double_fmt") {
#define T(val, nb_max_decimals, dec_sep, thousand_sep, res) \
    ({  SB_1k(sb);                                                           \
                                                                             \
        sb_add_double_fmt(&sb, val, nb_max_decimals, dec_sep, thousand_sep); \
        Z_ASSERT_LSTREQUAL(LSTR_SB_V(&sb), LSTR(res));                       \
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

    Z_TEST(sb_add_punycode, "str: sb_add_punycode") {
        SB_1k(sb);

#define T(_in, _out) \
        do {                                                                 \
            Z_ASSERT_N(sb_add_punycode_str(&sb, _in, strlen(_in)));          \
            Z_ASSERT_LSTREQUAL(LSTR_SB_V(&sb), LSTR(_out));                  \
            sb_reset(&sb);                                                   \
        } while (0)

        /* Basic test cases to validate sb_add_punycode_str */
        T("hello-world", "hello-world-");
        T("hellö-world", "hell-world-hcb");
        T("bücher", "bcher-kva");
        T("bücherü", "bcher-kvae");
#undef T

#define T(_name, _out, ...) \
        do {                                                                 \
            const uint32_t _in[] = { __VA_ARGS__ };                          \
                                                                             \
            Z_ASSERT_N(sb_add_punycode_vec(&sb, _in, countof(_in)),          \
                       "punycode encoding failed for " _name);               \
            Z_ASSERT_LSTREQUAL(LSTR_SB_V(&sb), LSTR(_out),                   \
                               "punycode comparison failed for " _name);     \
            sb_reset(&sb);                                                   \
        } while (0)

        /* More complex test cases taken in section 7.1 (Sample strings) of
         * RFC 3492. */
        T("(A) Arabic (Egyptian)", "egbpdaj6bu4bxfgehfvwxn",
          0x0644, 0x064A, 0x0647, 0x0645, 0x0627, 0x0628, 0x062A, 0x0643,
          0x0644, 0x0645, 0x0648, 0x0634, 0x0639, 0x0631, 0x0628, 0x064A,
          0x061F);
        T("(B) Chinese (simplified)", "ihqwcrb4cv8a8dqg056pqjye",
          0x4ED6, 0x4EEC, 0x4E3A, 0x4EC0, 0x4E48, 0x4E0D, 0x8BF4, 0x4E2D,
          0x6587);
        T("(C) Chinese (traditional)", "ihqwctvzc91f659drss3x8bo0yb",
          0x4ED6, 0x5011, 0x7232, 0x4EC0, 0x9EBD, 0x4E0D, 0x8AAA, 0x4E2D,
          0x6587);
        T("(D) Czech: Pro<ccaron>prost<ecaron>nemluv<iacute><ccaron>esky",
          "Proprostnemluvesky-uyb24dma41a",
          0x0050, 0x0072, 0x006F, 0x010D, 0x0070, 0x0072, 0x006F, 0x0073,
          0x0074, 0x011B, 0x006E, 0x0065, 0x006D, 0x006C, 0x0075, 0x0076,
          0x00ED, 0x010D, 0x0065, 0x0073, 0x006B, 0x0079,
        );
        T("(E) Hebrew:", "4dbcagdahymbxekheh6e0a7fei0b",
          0x05DC, 0x05DE, 0x05D4, 0x05D4, 0x05DD, 0x05E4, 0x05E9, 0x05D5,
          0x05D8, 0x05DC, 0x05D0, 0x05DE, 0x05D3, 0x05D1, 0x05E8, 0x05D9,
          0x05DD, 0x05E2, 0x05D1, 0x05E8, 0x05D9, 0x05EA);
        T("(F) Hindi (Devanagari):",
          "i1baa7eci9glrd9b2ae1bj0hfcgg6iyaf8o0a1dig0cd",
          0x092F, 0x0939, 0x0932, 0x094B, 0x0917, 0x0939, 0x093F, 0x0928,
          0x094D, 0x0926, 0x0940, 0x0915, 0x094D, 0x092F, 0x094B, 0x0902,
          0x0928, 0x0939, 0x0940, 0x0902, 0x092C, 0x094B, 0x0932, 0x0938,
          0x0915, 0x0924, 0x0947, 0x0939, 0x0948, 0x0902);
        T("(G) Japanese (kanji and hiragana):",
          "n8jok5ay5dzabd5bym9f0cm5685rrjetr6pdxa",
          0x306A, 0x305C, 0x307F, 0x3093, 0x306A, 0x65E5, 0x672C, 0x8A9E,
          0x3092, 0x8A71, 0x3057, 0x3066, 0x304F, 0x308C, 0x306A, 0x3044,
          0x306E, 0x304B);
        T("(H) Korean (Hangul syllables):",
          "989aomsvi5e83db1d2a355cv1e0vak1dwrv93d5xbh15a0dt30a5jpsd879ccm6fea98c",
          0xC138, 0xACC4, 0xC758, 0xBAA8, 0xB4E0, 0xC0AC, 0xB78C, 0xB4E4,
          0xC774, 0xD55C, 0xAD6D, 0xC5B4, 0xB97C, 0xC774, 0xD574, 0xD55C,
          0xB2E4, 0xBA74, 0xC5BC, 0xB9C8, 0xB098, 0xC88B, 0xC744, 0xAE4C);
        T("(I) Russian (Cyrillic):", "b1abfaaepdrnnbgefbadotcwatmq2g4l",
          0x043F, 0x043E, 0x0447, 0x0435, 0x043C, 0x0443, 0x0436, 0x0435,
          0x043E, 0x043D, 0x0438, 0x043D, 0x0435, 0x0433, 0x043E, 0x0432,
          0x043E, 0x0440, 0x044F, 0x0442, 0x043F, 0x043E, 0x0440, 0x0443,
          0x0441, 0x0441, 0x043A, 0x0438);
        T("(J) Spanish: Porqu<eacute>nopuedensimplementehablarenEspa<ntilde>ol",
          "PorqunopuedensimplementehablarenEspaol-fmd56a",
          0x0050, 0x006F, 0x0072, 0x0071, 0x0075, 0x00E9, 0x006E, 0x006F,
          0x0070, 0x0075, 0x0065, 0x0064, 0x0065, 0x006E, 0x0073, 0x0069,
          0x006D, 0x0070, 0x006C, 0x0065, 0x006D, 0x0065, 0x006E, 0x0074,
          0x0065, 0x0068, 0x0061, 0x0062, 0x006C, 0x0061, 0x0072, 0x0065,
          0x006E, 0x0045, 0x0073, 0x0070, 0x0061, 0x00F1, 0x006F, 0x006C);
        T("(K) Vietnamese:", "TisaohkhngthchnitingVit-kjcr8268qyxafd2f1b9g",
          0x0054, 0x1EA1, 0x0069, 0x0073, 0x0061, 0x006F, 0x0068, 0x1ECD,
          0x006B, 0x0068, 0x00F4, 0x006E, 0x0067, 0x0074, 0x0068, 0x1EC3,
          0x0063, 0x0068, 0x1EC9, 0x006E, 0x00F3, 0x0069, 0x0074, 0x0069,
          0x1EBF, 0x006E, 0x0067, 0x0056, 0x0069, 0x1EC7, 0x0074);
        T("(L) 3<nen>B<gumi><kinpachi><sensei>", "3B-ww4c5e180e575a65lsy2b",
          0x0033, 0x5E74, 0x0042, 0x7D44, 0x91D1, 0x516B, 0x5148, 0x751F);
        T("(M) <amuro><namie>-with-SUPER-MONKEYS",
          "-with-SUPER-MONKEYS-pc58ag80a8qai00g7n9n",
          0x5B89, 0x5BA4, 0x5948, 0x7F8E, 0x6075, 0x002D, 0x0077, 0x0069,
          0x0074, 0x0068, 0x002D, 0x0053, 0x0055, 0x0050, 0x0045, 0x0052,
          0x002D, 0x004D, 0x004F, 0x004E, 0x004B, 0x0045, 0x0059, 0x0053);
        T("(N) Hello-Another-Way-<sorezore><no><basho>",
          "Hello-Another-Way--fc4qua05auwb3674vfr0b",
          0x0048, 0x0065, 0x006C, 0x006C, 0x006F, 0x002D, 0x0041, 0x006E,
          0x006F, 0x0074, 0x0068, 0x0065, 0x0072, 0x002D, 0x0057, 0x0061,
          0x0079, 0x002D, 0x305D, 0x308C, 0x305E, 0x308C, 0x306E, 0x5834,
          0x6240);
        T("(O) <hitotsu><yane><no><shita>2", "2-u9tlzr9756bt3uc0v",
          0x3072, 0x3068, 0x3064, 0x5C4B, 0x6839, 0x306E, 0x4E0B, 0x0032);
        T("(P) Maji<de>Koi<suru>5<byou><mae>", "MajiKoi5-783gue6qz075azm5e",
          0x004D, 0x0061, 0x006A, 0x0069, 0x3067, 0x004B, 0x006F, 0x0069,
          0x3059, 0x308B, 0x0035, 0x79D2, 0x524D);
        T("(Q) <pafii>de<runba>", "de-jg4avhby1noc0d",
          0x30D1, 0x30D5, 0x30A3, 0x30FC, 0x0064, 0x0065, 0x30EB, 0x30F3,
          0x30D0);
        T("(R) <sono><supiido><de>", "d9juau41awczczp",
          0x305D, 0x306E, 0x30B9, 0x30D4, 0x30FC, 0x30C9, 0x3067);
        T("(S) -> $1.00 <-", "-> $1.00 <--",
          0x002D, 0x003E, 0x0020, 0x0024, 0x0031, 0x002E, 0x0030, 0x0030,
          0x0020, 0x003C, 0x002D);
#undef T
    } Z_TEST_END;

    Z_TEST(sb_add_idna_domain_name, "str: sb_add_idna_domain_name") {
        SB_1k(sb);
        SB_1k(domain);

#define T_OK(_in, _out, _flags, _nb_labels) \
        do {                                                                 \
            int nb_labels = sb_add_idna_domain_name(&sb, _in, strlen(_in),   \
                                                    _flags);                 \
            Z_ASSERT_N(nb_labels);                                           \
            Z_ASSERT_EQ(nb_labels, _nb_labels);                              \
            Z_ASSERT_LSTREQUAL(LSTR_SB_V(&sb), LSTR(_out));                  \
            sb_reset(&sb);                                                   \
        } while (0)

#define T_KO(_in, _flags) \
        do {                                                                 \
            Z_ASSERT_NEG(sb_add_idna_domain_name(&sb, _in, strlen(_in),      \
                                                 _flags));                   \
            sb_reset(&sb);                                                   \
        } while (0)

        /* Basic failure cases */
        T_KO("intersec", 0);
        T_KO("intersec..com", 0);
        T_KO("intersec.com.", 0);
        T_KO("intersec-.com", IDNA_USE_STD3_ASCII_RULES);
        T_KO("intersec.-com", IDNA_USE_STD3_ASCII_RULES);
        T_KO("xN--bücher.com", 0);
        T_KO("123456789012345678901234567890123456789012345678901234567890"
             "1234.com", 0);
        T_KO("InSighted!.intersec.com", IDNA_USE_STD3_ASCII_RULES);

        /* Basic success cases */
        T_OK("jObs.InTerseC.coM", "jObs.InTerseC.coM",
             IDNA_USE_STD3_ASCII_RULES, 3);
        T_OK("jObs.InTerseC.coM", "jobs.intersec.com",
             IDNA_USE_STD3_ASCII_RULES | IDNA_ASCII_TOLOWER, 3);
        T_OK("jobs.intersec.com", "jobs.intersec.com",
             IDNA_USE_STD3_ASCII_RULES, 3);
        T_OK("bücher.com", "xn--bcher-kva.com", IDNA_USE_STD3_ASCII_RULES, 2);
        T_OK("xn--bcher-kva.com", "xn--bcher-kva.com",
             IDNA_USE_STD3_ASCII_RULES, 2);
        T_OK("label1.label2。label3．label4｡com",
             "label1.label2.label3.label4.com",
             IDNA_USE_STD3_ASCII_RULES, 5);
        T_OK("intersec-.com", "intersec-.com", 0, 2);
        T_OK("intersec.-com", "intersec.-com", 0, 2);
        T_OK("xn-bücher.com", "xn--xn-bcher-95a.com", 0, 2);
        T_OK("InSighted!.intersec.com", "InSighted!.intersec.com", 0, 3);
#undef T_OK
#undef T_KO

        /* Commonly mapped to nothing */
        sb_reset(&domain);
        sb_adds(&domain, "int");
        sb_adduc(&domain, 0x00ad);
        sb_adds(&domain, "er");
        sb_adduc(&domain, 0xfe01);
        sb_adds(&domain, "sec.com");
        Z_ASSERT_N(sb_add_idna_domain_name(&sb, domain.data, domain.len, 0));
        Z_ASSERT_LSTREQUAL(LSTR_SB_V(&sb), LSTR("intersec.com"));
        sb_reset(&domain);
        sb_reset(&sb);
        sb_adds(&domain, "büc");
        sb_adduc(&domain, 0x00ad);
        sb_adds(&domain, "he");
        sb_adduc(&domain, 0xfe01);
        sb_adds(&domain, "r.com");
        Z_ASSERT_N(sb_add_idna_domain_name(&sb, domain.data, domain.len, 0));
        Z_ASSERT_LSTREQUAL(LSTR_SB_V(&sb), LSTR("xn--bcher-kva.com"));

        /* Prohibited output */
        sb_reset(&domain);
        sb_adds(&domain, "inter");
        sb_adduc(&domain, 0x00a0);
        sb_adds(&domain, "sec.com");
        Z_ASSERT_NEG(sb_add_idna_domain_name(&sb, domain.data, domain.len,
                                             0));

        /* Unassigned Code Points */
        sb_reset(&domain);
        sb_adds(&domain, "inter");
        sb_adduc(&domain, 0x0221);
        sb_adds(&domain, "sec.com");
        Z_ASSERT_NEG(sb_add_idna_domain_name(&sb, domain.data, domain.len,
                                             0));
        Z_ASSERT(sb_add_idna_domain_name(&sb, domain.data, domain.len,
                                         IDNA_ALLOW_UNASSIGNED) == 2);
    } Z_TEST_END;

    Z_TEST(str_span, "str: filtering") {
        SB_1k(sb);

#define T(f, d, from, to) do {                                               \
        f(&sb, LSTR(from), d);                                               \
        Z_ASSERT_LSTREQUAL(LSTR_SB_V(&sb), LSTR(to));                        \
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

    Z_TEST(lstr_startswithc, "str: starts with character") {
        Z_ASSERT(lstr_startswithc(LSTR("1234"), '1'));
        Z_ASSERT(!lstr_startswithc(LSTR("1234"), '2'));
        Z_ASSERT(lstr_startswithc(LSTR("a"), 'a'));
        Z_ASSERT(!lstr_startswithc(LSTR_NULL_V, '2'));
        Z_ASSERT(!lstr_startswithc(LSTR_EMPTY_V, '2'));
    } Z_TEST_END;

    Z_TEST(lstr_endswithc, "str: ends with character") {
        Z_ASSERT(!lstr_endswithc(LSTR("1234"), '1'));
        Z_ASSERT(lstr_endswithc(LSTR("a"), 'a'));
        Z_ASSERT(lstr_endswithc(LSTR("1234"), '4'));
        Z_ASSERT(!lstr_endswithc(LSTR_NULL_V, '2'));
        Z_ASSERT(!lstr_endswithc(LSTR_EMPTY_V, '2'));
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
        T(LSTR("a"), LSTR("a"));
        T(LSTR("ab"), LSTR("ba"));
        T(LSTR("abc"), LSTR("cba"));
        T(LSTR("abcd"), LSTR("dcba"));
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
        T(LSTR("a"), LSTR("a"));
        T(LSTR("ab"), LSTR("ba"));
        T(LSTR("abc"), LSTR("cba"));
        T(LSTR("abcd"), LSTR("dcba"));
        T(LSTR("aé"), LSTR("éa"));
        T(LSTR("é"), LSTR("é"));
        T(LSTR("éa"), LSTR("aé"));
        T(LSTR("béa"), LSTR("aéb"));
#undef T
    } Z_TEST_END;

    Z_TEST(lstr_dl_distance, "str: Damerau–Levenshtein distance") {
#define T(s1, s2, exp) do {                                                  \
        Z_ASSERT_EQ(lstr_dlevenshtein(LSTR(s1), LSTR(s2), exp), exp);        \
        Z_ASSERT_EQ(lstr_dlevenshtein(LSTR(s2), LSTR(s1), exp), exp);        \
        Z_ASSERT_EQ(lstr_dlevenshtein(LSTR(s1), LSTR(s2), -1), exp);         \
        if (exp > 0) {                                                       \
            Z_ASSERT_NEG(lstr_dlevenshtein(LSTR(s1), LSTR(s2), exp - 1));    \
        }                                                                    \
    } while (0)

        T("",         "",         0);
        T("abcd",     "abcd",     0);
        T("",         "abcd",     4);
        T("toto",     "totototo", 4);
        T("ba",       "abc",      2);
        T("fee",      "deed",     2);
        T("hurqbohp", "qkhoz",    6);
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
            Z_ASSERT_LSTREQUAL(arr.tab[0], LSTR(str1));                      \
            Z_ASSERT_LSTREQUAL(arr.tab[1], LSTR(str2));                      \
            Z_ASSERT_LSTREQUAL(arr.tab[2], LSTR(str3)); })

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
        Z_ASSERT_LSTREQUAL(key,   LSTR("cid1=1&cid2"));
        Z_ASSERT_LSTREQUAL(value, LSTR("2"));
        Z_ASSERT_N(t_ps_get_http_var(&ps, &key, &value));
        Z_ASSERT_LSTREQUAL(key,   LSTR("cid3"));
        Z_ASSERT_LSTREQUAL(value, LSTR("3"));
        Z_ASSERT_N(t_ps_get_http_var(&ps, &key, &value));
        Z_ASSERT_LSTREQUAL(key,   LSTR("cid4"));
        Z_ASSERT_LSTREQUAL(value, LSTR(""));
        Z_ASSERT(ps_done(&ps));
        Z_ASSERT_NEG(t_ps_get_http_var(&ps, &key, &value));
    } Z_TEST_END;

    Z_TEST(sb_add_int_fmt, "str: sb_add_int_fmt") {
#define T(val, thousand_sep, res) \
    ({  SB_1k(sb);                                                           \
                                                                             \
        sb_add_int_fmt(&sb, val, thousand_sep);                              \
        Z_ASSERT_LSTREQUAL(LSTR_SB_V(&sb), LSTR(res));                       \
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
        Z_ASSERT_LSTREQUAL(LSTR_SB_V(&sb), LSTR(res));                       \
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

    Z_TEST(ps_skip_afterlastchr, "") {
        pstream_t ps = ps_initstr("test_1_2");
        pstream_t ps2 = ps_initstr("test1.02");
        pstream_t ps3 = ps_initstr("test_2");

        Z_ASSERT_N(ps_skip_afterlastchr(&ps, '_'));
        Z_ASSERT(ps_len(&ps) == 1);
        Z_ASSERT(ps_strequal(&ps, "2"));

        Z_ASSERT_NEG(ps_skip_afterlastchr(&ps2, '_'));
        Z_ASSERT(ps_len(&ps2) == strlen("test1.02"));
        Z_ASSERT(ps_strequal(&ps2, "test1.02"));
        Z_ASSERT_N(ps_skip_afterlastchr(&ps2, '.'));
        Z_ASSERT(ps_len(&ps2) == 2);
        Z_ASSERT(ps_strequal(&ps2, "02"));

        Z_ASSERT_N(ps_skip_afterlastchr(&ps3, '_'));
        Z_ASSERT(ps_len(&ps3) == 1);
        Z_ASSERT(ps_strequal(&ps3, "2"));
    } Z_TEST_END;

    Z_TEST(ps_clip_atlastchr, "") {
        pstream_t ps = ps_initstr("test_1_2");
        pstream_t ps2 = ps_initstr("test1.02");
        pstream_t ps3 = ps_initstr("test_2");

        Z_ASSERT_N(ps_clip_atlastchr(&ps, '_'));
        Z_ASSERT(ps_len(&ps) == 6);
        Z_ASSERT(ps_strequal(&ps, "test_1"));

        Z_ASSERT_NEG(ps_clip_atlastchr(&ps2, '_'));
        Z_ASSERT(ps_len(&ps2) == strlen("test1.02"));
        Z_ASSERT(ps_strequal(&ps2, "test1.02"));
        Z_ASSERT_N(ps_clip_atlastchr(&ps2, '.'));
        Z_ASSERT(ps_len(&ps2) == 5);
        Z_ASSERT(ps_strequal(&ps2, "test1"));

        Z_ASSERT_N(ps_clip_atlastchr(&ps3, '_'));
        Z_ASSERT(ps_len(&ps3) == 4);
        Z_ASSERT(ps_strequal(&ps3, "test"));
    } Z_TEST_END;

    Z_TEST(ps_clip_afterlastchr, "") {
        pstream_t ps = ps_initstr("test_1_2");
        pstream_t ps2 = ps_initstr("test1.02");
        pstream_t ps3 = ps_initstr("test_2");

        Z_ASSERT_N(ps_clip_afterlastchr(&ps, '_'));
        Z_ASSERT(ps_len(&ps) == 7);
        Z_ASSERT(ps_strequal(&ps, "test_1_"));

        Z_ASSERT_NEG(ps_clip_afterlastchr(&ps2, '_'));
        Z_ASSERT(ps_len(&ps2) == strlen("test1.02"));
        Z_ASSERT(ps_strequal(&ps2, "test1.02"));
        Z_ASSERT_N(ps_clip_afterlastchr(&ps2, '.'));
        Z_ASSERT(ps_len(&ps2) == 6);
        Z_ASSERT(ps_strequal(&ps2, "test1."));

        Z_ASSERT_N(ps_clip_afterlastchr(&ps3, '_'));
        Z_ASSERT(ps_len(&ps3) == 5);
        Z_ASSERT(ps_strequal(&ps3, "test_"));
    } Z_TEST_END;

    Z_TEST(ps_skip_upto_str, "") {
        const char *str = "foo bar baz";
        pstream_t ps = ps_initstr(str);

        Z_ASSERT_NEG(ps_skip_upto_str(&ps, "toto"));
        Z_ASSERT(ps_len(&ps) ==  strlen(str));
        Z_ASSERT(ps_strequal(&ps, str));

        Z_ASSERT_N(ps_skip_upto_str(&ps, ""));
        Z_ASSERT(ps_len(&ps) ==  strlen(str));
        Z_ASSERT(ps_strequal(&ps, str));

        Z_ASSERT_N(ps_skip_upto_str(&ps, "bar"));
        Z_ASSERT(ps_len(&ps) == 7);
        Z_ASSERT(ps_strequal(&ps, "bar baz"));
    } Z_TEST_END;

    Z_TEST(ps_skip_after_str, "") {
        const char *str = "foo bar baz";
        pstream_t ps = ps_initstr(str);

        Z_ASSERT_NEG(ps_skip_after_str(&ps, "toto"));
        Z_ASSERT(ps_len(&ps) ==  strlen(str));
        Z_ASSERT(ps_strequal(&ps, str));

        Z_ASSERT_N(ps_skip_after_str(&ps, ""));
        Z_ASSERT(ps_len(&ps) ==  strlen(str));
        Z_ASSERT(ps_strequal(&ps, str));

        Z_ASSERT_N(ps_skip_after_str(&ps, "bar"));
        Z_ASSERT(ps_len(&ps) == 4);
        Z_ASSERT(ps_strequal(&ps, " baz"));
    } Z_TEST_END;

    Z_TEST(ps_get_ps_upto_str, "") {
        const char *str = "foo bar baz";
        pstream_t ps = ps_initstr(str);
        pstream_t extract;

        p_clear(&extract, 1);
        Z_ASSERT_NEG(ps_get_ps_upto_str(&ps, "toto", &extract));
        Z_ASSERT(ps_len(&ps) ==  strlen(str));
        Z_ASSERT(ps_strequal(&ps, str));
        Z_ASSERT(ps_len(&extract) == 0);

        Z_ASSERT_N(ps_get_ps_upto_str(&ps, "", &extract));
        Z_ASSERT(ps_len(&ps) ==  strlen(str));
        Z_ASSERT(ps_strequal(&ps, str));
        Z_ASSERT(ps_len(&extract) == 0);

        Z_ASSERT_N(ps_get_ps_upto_str(&ps, "bar", &extract));
        Z_ASSERT(ps_len(&ps) ==  7);
        Z_ASSERT(ps_strequal(&ps, "bar baz"));
        Z_ASSERT(ps_len(&extract) == 4);
        Z_ASSERT(ps_strequal(&extract, "foo "));
    } Z_TEST_END;

    Z_TEST(ps_get_ps_upto_str_and_skip, "") {
        const char *str = "foo bar baz";
        pstream_t ps = ps_initstr(str);
        pstream_t extract;

        p_clear(&extract, 1);
        Z_ASSERT_NEG(ps_get_ps_upto_str_and_skip(&ps, "toto", &extract));
        Z_ASSERT(ps_len(&ps) ==  strlen(str));
        Z_ASSERT(ps_strequal(&ps, str));
        Z_ASSERT(ps_len(&extract) == 0);

        Z_ASSERT_N(ps_get_ps_upto_str_and_skip(&ps, "", &extract));
        Z_ASSERT(ps_len(&ps) ==  strlen(str));
        Z_ASSERT(ps_strequal(&ps, str));
        Z_ASSERT(ps_len(&extract) == 0);

        Z_ASSERT_N(ps_get_ps_upto_str_and_skip(&ps, "bar", &extract));
        Z_ASSERT(ps_len(&ps) ==  4);
        Z_ASSERT(ps_strequal(&ps, " baz"));
        Z_ASSERT(ps_len(&extract) == 4);
        Z_ASSERT(ps_strequal(&extract, "foo "));
    } Z_TEST_END;

    Z_TEST(lstr_ascii_icmp, "str: lstr_ascii_icmp") {
#define T(_str1, _str2, _expected)                                       \
        Z_ASSERT(lstr_ascii_icmp(&LSTR_IMMED_V(_str1),                   \
                 &LSTR_IMMED_V(_str2)) _expected)

        T("a",    "b",     <  0);
        T("b",    "a",     >  0);
        T("a",    "a",     == 0);
        T("A",    "a",     == 0);
        T("aaa",  "b",     <  0);
        T("bbb",  "a",     >  0);
        T("aaa",  "aa",    >  0);
        T("aaa",  "AA",    >  0);
        T("AbCd", "aBcD",  == 0);
        T("AbCd", "aBcDe", <  0);
        T("faaa", "FAAB",  <  0);
        T("FAAA", "faab",  <  0);
        T("faaa", "FAAA",  == 0);
        T("faab", "faaba", <  0);
        T("faab", "faaab", >  0);
#undef T
    } Z_TEST_END;

    Z_TEST(lstr_to_int, "str: lstr_to_int and friends") {
        t_scope;
        int      i;
        uint32_t u32;
        int64_t  i64;
        uint64_t u64;

#define T_OK(_str, _exp)  \
        do {                                                                 \
            Z_ASSERT_N(lstr_to_int(LSTR(_str), &i));                         \
            Z_ASSERT_EQ(i, _exp);                                            \
            Z_ASSERT_N(lstr_to_uint(LSTR(_str), &u32));                      \
            Z_ASSERT_EQ(u32, (uint32_t)_exp);                                \
            Z_ASSERT_N(lstr_to_int64(LSTR(_str), &i64));                     \
            Z_ASSERT_EQ(i64, _exp);                                          \
            Z_ASSERT_N(lstr_to_uint64(LSTR(_str), &u64));                    \
            Z_ASSERT_EQ(u64, (uint64_t)_exp);                                \
        } while (0)

        T_OK("0",        0);
        T_OK("1234",     1234);
        T_OK("  1234  ", 1234);
#undef T_OK

        Z_ASSERT_N(lstr_to_uint(t_lstr_fmt("%u", UINT32_MAX), &u32));
        Z_ASSERT_EQ(u32, UINT32_MAX);

#define T_KO(_str)  \
        do {                                                                 \
            Z_ASSERT_NEG(lstr_to_int(LSTR(_str), &i));                       \
            Z_ASSERT_NEG(lstr_to_uint(LSTR(_str), &u32));                    \
            Z_ASSERT_NEG(lstr_to_int64(LSTR(_str), &i64));                   \
            Z_ASSERT_NEG(lstr_to_uint64(LSTR(_str), &u64));                  \
        } while (0)

        T_KO("");
        T_KO("   ");
        T_KO("abcd");
        T_KO("  12 12 ");
        T_KO("  12abcd");
        T_KO("12.12");
#undef T_KO

        errno = 0;
        Z_ASSERT_NEG(lstr_to_uint(LSTR(" -123"), &u32));
        Z_ASSERT_EQ(errno, ERANGE);
        Z_ASSERT_NEG(lstr_to_uint(t_lstr_fmt("%jd", (uint64_t)UINT32_MAX + 1),
                                  &u32));
        Z_ASSERT_EQ(errno, ERANGE);

        errno = 0;
        Z_ASSERT_NEG(lstr_to_uint64(LSTR(" -123"), &u64));
        Z_ASSERT_EQ(errno, ERANGE);
    } Z_TEST_END;

    Z_TEST(lstr_to_double, "str: lstr_to_double") {
        double d;

#define T_OK(_str, _exp)  \
        do {                                                                 \
            Z_ASSERT_N(lstr_to_double(LSTR(_str), &d));                      \
            Z_ASSERT_EQ(d, _exp);                                            \
        } while (0)

        T_OK("0",        0);
        T_OK("1234",     1234);
        T_OK("  1234  ", 1234);
        T_OK("-1.33e12", -1.33e12);
        T_OK("INF", INFINITY);
        T_OK("INFINITY", INFINITY);
#undef T_OK

#define T_KO(_str)  \
        do {                                                                 \
            Z_ASSERT_NEG(lstr_to_double(LSTR(_str), &d));                    \
        } while (0)

        T_KO("abcd");
        T_KO("  12 12 ");
        T_KO("  12abcd");
#undef T_KO
    } Z_TEST_END;

    Z_TEST(str_match_ctype, "str: strings match the ctype description") {
        struct {
            lstr_t              s;
            const ctype_desc_t *d;
            bool                expected;
        } t[] = {
#define T(_str, _ctype, _expected)                                           \
            {.s = LSTR_IMMED(_str), .d = _ctype, .expected = _expected}

            T("0123456789",       &ctype_isdigit,    true),
            T("abcde",            &ctype_islower,    true),
            T("ABCDE",            &ctype_isupper,    true),
            T(" \n",              &ctype_isspace,    true),
            T("0123456789ABCDEF", &ctype_ishexdigit, true),
            T("0123456789abcdef", &ctype_ishexdigit, true),

            T("abcdEF",           &ctype_isdigit,    false),
            T("ABC",              &ctype_islower,    false),
            T("abcABC",           &ctype_islower,    false),
            T("abc132",           &ctype_islower,    false),
            T("abc",              &ctype_isupper,    false),
            T("aBCDE",            &ctype_isupper,    false),

#undef T
        };

        for (int i = 0; i < countof(t); i++) {
            Z_ASSERT_EQ(lstr_match_ctype(t[i].s, t[i].d), t[i].expected);
        }
    } Z_TEST_END;

    Z_TEST(lstr_macros, "lstr: macros") {
        uint16_t data[] = { 11, 22, 33 };
        lstr_t data_ref, data_s, data_c;

        data_ref = LSTR_INIT_V((const char *)data, sizeof(data));
        data_s = LSTR_DATA_V(data, sizeof(data));
        data_c = LSTR_CARRAY_V(data);

        Z_ASSERT_LSTREQUAL(data_s, data_ref);
        Z_ASSERT_LSTREQUAL(data_c, data_ref);
    } Z_TEST_END;

    Z_TEST(ps_has_char, "ps: ps_has_char_in_ctype") {
        pstream_t p;

        p = ps_initstr("aBcdEfGhij");
        Z_ASSERT(!ps_has_char_in_ctype(&p, &ctype_isdigit));
        Z_ASSERT( ps_has_char_in_ctype(&p, &ctype_isalpha));

        p = ps_initstr("abcdef1hij");
        Z_ASSERT(ps_has_char_in_ctype(&p, &ctype_isdigit));
        Z_ASSERT(ps_has_char_in_ctype(&p, &ctype_isalpha));

        p = ps_initstr("ABCDEFJHIJ8");
        Z_ASSERT(ps_has_char_in_ctype(&p, &ctype_isdigit));
        Z_ASSERT(ps_has_char_in_ctype(&p, &ctype_isalpha));

        p = ps_initstr("9191959485889");
        Z_ASSERT( ps_has_char_in_ctype(&p, &ctype_isdigit));
        Z_ASSERT(!ps_has_char_in_ctype(&p, &ctype_isalpha));
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

#define CSV_TEST_GET_ROW(out_line)                                           \
    qv_deep_clear(lstr, &fields, lstr_wipe);                                 \
    Z_ASSERT_N(ps_get_csv_line(NULL, &str, separator,                        \
                               quoting_character, &fields, out_line))

#define CSV_TEST_FAIL_ROW() \
    qv_deep_clear(lstr, &fields, lstr_wipe);                                 \
    Z_ASSERT_NEG(ps_get_csv_line(NULL, &str, separator, quoting_character,   \
                                 &fields, NULL))

#define CSV_TEST_CHECK_EOF()  Z_ASSERT(ps_done(&str))

#define CSV_TEST_CHECK_NB_FIELDS(_n) \
    Z_ASSERT_EQ(fields.len, _n, "field count mismatch");

#define CSV_TEST_CHECK_FIELD(_n, _str)                                       \
    if (_str == NULL) {                                                      \
        Z_ASSERT_NULL(fields.tab[_n].s);                                     \
    } else {                                                                 \
        Z_ASSERT_P(fields.tab[_n].s);                                        \
        Z_ASSERT_LSTREQUAL(fields.tab[_n], LSTR(_str), "field value");       \
    }

Z_GROUP_EXPORT(csv) {
    Z_TEST(row1, "no row") {
        /* No row */
        CSV_TEST_START("", ',', '"');
        CSV_TEST_CHECK_EOF();
        CSV_TEST_END();
    } Z_TEST_END;

    Z_TEST(row2, "Single row") {
        pstream_t row;

        CSV_TEST_START("foo,bar,baz\r\n", ',', '"');
        CSV_TEST_GET_ROW(&row);
        Z_ASSERT_LSTREQUAL(LSTR("foo,bar,baz"), LSTR_PS_V(&row));
        CSV_TEST_END();
    } Z_TEST_END;

    Z_TEST(row3, "Several rows") {
        pstream_t row;

        CSV_TEST_START("foo,bar,baz\r\n"
                       "truc,machin,bidule\r\n",
                       ',', '"');
        CSV_TEST_GET_ROW(NULL);
        CSV_TEST_GET_ROW(&row);
        Z_ASSERT_LSTREQUAL(LSTR("truc,machin,bidule"), LSTR_PS_V(&row));
        CSV_TEST_END();
    } Z_TEST_END;

    Z_TEST(row4, "Mixed line terminators") {
        pstream_t row;

        CSV_TEST_START("foo,bar,baz\n"
                       "truc,machin,bidule\r\n",
                       ',', '"');
        CSV_TEST_GET_ROW(&row);
        Z_ASSERT_LSTREQUAL(LSTR("foo,bar,baz"), LSTR_PS_V(&row));
        CSV_TEST_GET_ROW(NULL);
        CSV_TEST_END();
    } Z_TEST_END;

    Z_TEST(row5, "No line terminator") {
        pstream_t row;

        CSV_TEST_START("foo,bar,baz", ',', '"');
        CSV_TEST_GET_ROW(&row);
        Z_ASSERT_LSTREQUAL(LSTR("foo,bar,baz"), LSTR_PS_V(&row));
        CSV_TEST_END();
    } Z_TEST_END;

    Z_TEST(base1, "Base") {
        CSV_TEST_START("foo", ',', '"');
        CSV_TEST_GET_ROW(NULL);
        CSV_TEST_CHECK_NB_FIELDS(1);
        CSV_TEST_CHECK_FIELD(0, "foo");
        CSV_TEST_END();
    } Z_TEST_END;

    Z_TEST(base2, "Base 2") {
        CSV_TEST_START("foo,bar", ',', '"');
        CSV_TEST_GET_ROW(NULL);
        CSV_TEST_CHECK_NB_FIELDS(2);
        CSV_TEST_CHECK_FIELD(0, "foo");
        CSV_TEST_CHECK_FIELD(1, "bar");
        CSV_TEST_END();
    } Z_TEST_END;

    Z_TEST(base3, "Base 3") {
        CSV_TEST_START("foo,bar,baz", ',', '"');
        CSV_TEST_GET_ROW(NULL);
        CSV_TEST_CHECK_NB_FIELDS(3);
        CSV_TEST_CHECK_FIELD(0, "foo");
        CSV_TEST_CHECK_FIELD(1, "bar");
        CSV_TEST_CHECK_FIELD(2, "baz");
        CSV_TEST_END();
    } Z_TEST_END;

    Z_TEST(allowed1, "Invalid but allowed fields 1") {
        CSV_TEST_START("foo,bar\"baz", ',', '"');
        CSV_TEST_GET_ROW(NULL);
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
        CSV_TEST_GET_ROW(NULL);
        CSV_TEST_CHECK_NB_FIELDS(3);
        CSV_TEST_CHECK_FIELD(0, "foo");
        CSV_TEST_CHECK_FIELD(1, NULL);
        CSV_TEST_CHECK_FIELD(2, "baz");
        CSV_TEST_END();
    } Z_TEST_END;

    Z_TEST(empty2, "Empty fields 2") {
        CSV_TEST_START("foo,bar,", ',', '"');
        CSV_TEST_GET_ROW(NULL);
        CSV_TEST_CHECK_NB_FIELDS(3);
        CSV_TEST_CHECK_FIELD(0, "foo");
        CSV_TEST_CHECK_FIELD(1, "bar");
        CSV_TEST_CHECK_FIELD(2, NULL);
        CSV_TEST_END();
    } Z_TEST_END;

    Z_TEST(empty3, "Empty fields 3") {
        CSV_TEST_START(",bar,baz", ',', '"');
        CSV_TEST_GET_ROW(NULL);
        CSV_TEST_CHECK_NB_FIELDS(3);
        CSV_TEST_CHECK_FIELD(0, NULL);
        CSV_TEST_CHECK_FIELD(1, "bar");
        CSV_TEST_CHECK_FIELD(2, "baz");
        CSV_TEST_END();
    } Z_TEST_END;

    Z_TEST(empty4, "Empty fields 4") {
        CSV_TEST_START(",,", ',', '"');
        CSV_TEST_GET_ROW(NULL);
        CSV_TEST_CHECK_NB_FIELDS(3);
        CSV_TEST_CHECK_FIELD(0, NULL);
        CSV_TEST_CHECK_FIELD(1, NULL);
        CSV_TEST_CHECK_FIELD(2, NULL);
        CSV_TEST_END();
    } Z_TEST_END;

    Z_TEST(empty4, "Empty fields 4") {
        CSV_TEST_START(",,", ',', '"');
        CSV_TEST_GET_ROW(NULL);
        CSV_TEST_CHECK_NB_FIELDS(3);
        CSV_TEST_CHECK_FIELD(0, NULL);
        CSV_TEST_CHECK_FIELD(1, NULL);
        CSV_TEST_CHECK_FIELD(2, NULL);
        CSV_TEST_END();
    } Z_TEST_END;

    Z_TEST(quoted1, "Quoted fields 1") {
        CSV_TEST_START("foo,\"bar\",baz", ',', '"');
        CSV_TEST_GET_ROW(NULL);
        CSV_TEST_CHECK_NB_FIELDS(3);
        CSV_TEST_END();
    } Z_TEST_END;

    Z_TEST(quoted2, "Quoted fields 2") {
        CSV_TEST_START("foo,bar,\"baz\"", ',', '"');
        CSV_TEST_GET_ROW(NULL);
        CSV_TEST_CHECK_NB_FIELDS(3);
        CSV_TEST_CHECK_FIELD(0, "foo");
        CSV_TEST_CHECK_FIELD(1, "bar");
        CSV_TEST_CHECK_FIELD(2, "baz");
        CSV_TEST_END();
    } Z_TEST_END;

    Z_TEST(quoted3, "Quoted fields 3") {
        CSV_TEST_START("\"foo\",bar,baz", ',', '"');
        CSV_TEST_GET_ROW(NULL);
        CSV_TEST_CHECK_NB_FIELDS(3);
        CSV_TEST_CHECK_FIELD(0, "foo");
        CSV_TEST_CHECK_FIELD(1, "bar");
        CSV_TEST_CHECK_FIELD(2, "baz");
        CSV_TEST_END();
    } Z_TEST_END;

    Z_TEST(quoted4, "Quoted fields 4") {
        CSV_TEST_START("\"foo,bar\",baz", ',', '"');
        CSV_TEST_GET_ROW(NULL);
        CSV_TEST_CHECK_NB_FIELDS(2);
        CSV_TEST_CHECK_FIELD(0, "foo,bar");
        CSV_TEST_CHECK_FIELD(1, "baz");
        CSV_TEST_END();
    } Z_TEST_END;

    Z_TEST(quoted5, "Quoted fields 5") {
        CSV_TEST_START("\"foo,\"\"\"", ',', '"');
        CSV_TEST_GET_ROW(NULL);
        CSV_TEST_CHECK_NB_FIELDS(1);
        CSV_TEST_CHECK_FIELD(0, "foo,\"");
        CSV_TEST_END();
    } Z_TEST_END;

    Z_TEST(quoted6, "Quoted fields 6") {
        CSV_TEST_START("\"foo\n"
                       "bar\",baz", ',', '"');
        CSV_TEST_GET_ROW(NULL);
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
        CSV_TEST_GET_ROW(NULL);
        CSV_TEST_CHECK_NB_FIELDS(2);
        CSV_TEST_CHECK_FIELD(0, "foo");
        CSV_TEST_CHECK_FIELD(1, "bar");
        CSV_TEST_END();
    } Z_TEST_END;

    Z_TEST(noquoting2, "No quoting character 2") {
        CSV_TEST_START("foo,\"bar\"", ',', -1);
        CSV_TEST_GET_ROW(NULL);
        CSV_TEST_CHECK_NB_FIELDS(2);
        CSV_TEST_CHECK_FIELD(0, "foo");
        CSV_TEST_CHECK_FIELD(1, "\"bar\"");
        CSV_TEST_END();
    } Z_TEST_END;

    Z_TEST(noquoting3, "No quoting character 3") {
        CSV_TEST_START("fo\"o", ',', -1);
        CSV_TEST_GET_ROW(NULL);
        CSV_TEST_CHECK_NB_FIELDS(1);
        CSV_TEST_CHECK_FIELD(0, "fo\"o");
        CSV_TEST_END();
    } Z_TEST_END;
} Z_GROUP_END;
