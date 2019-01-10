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

    Z_TEST(sb_add_punycode, "str: sb_add_punycode") {
        SB_1k(sb);

#define T(_in, _out) \
        do {                                                                 \
            Z_ASSERT_N(sb_add_punycode_str(&sb, _in, strlen(_in)));          \
            Z_ASSERT_LSTREQUAL(LSTR_SB_V(&sb), LSTR_IMMED_V(_out));          \
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
            Z_ASSERT_LSTREQUAL(LSTR_SB_V(&sb), LSTR_IMMED_V(_out),           \
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
            Z_ASSERT_LSTREQUAL(LSTR_SB_V(&sb), LSTR_IMMED_V(_out));          \
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
        Z_ASSERT_LSTREQUAL(LSTR_SB_V(&sb), LSTR_IMMED_V("intersec.com"));
        sb_reset(&domain);
        sb_reset(&sb);
        sb_adds(&domain, "büc");
        sb_adduc(&domain, 0x00ad);
        sb_adds(&domain, "he");
        sb_adduc(&domain, 0xfe01);
        sb_adds(&domain, "r.com");
        Z_ASSERT_N(sb_add_idna_domain_name(&sb, domain.data, domain.len, 0));
        Z_ASSERT_LSTREQUAL(LSTR_SB_V(&sb), LSTR_IMMED_V("xn--bcher-kva.com"));

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

    Z_TEST(ps_split, "str-stream: ps_split") {
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
