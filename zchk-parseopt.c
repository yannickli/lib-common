/**************************************************************************/
/*                                                                        */
/*  Copyright (C) 2004-2019 INTERSEC SA                                   */
/*                                                                        */
/*  Should you receive a copy of this source code, you must check you     */
/*  have a proper, written authorization of INTERSEC to hold it. If you   */
/*  don't have such an authorization, you must DELETE all source code     */
/*  files in your possession, and inform INTERSEC of the fact you obtain  */
/*  these files. Should you not comply to these terms, you can be         */
/*  prosecuted in the extent permitted by applicable law.                 */
/*                                                                        */
/**************************************************************************/

#include "z.h"
#include "parseopt.h"

static struct {
    bool a;
    const char *b;
    int c;
    unsigned d;
    char e;
} zchk_parseopt_g;
#define _G  zchk_parseopt_g

static popt_t popts_g[] = {
    OPT_GROUP("Options:"),
    OPT_FLAG('a', "opta", &_G.a, "Opt a"),
    OPT_STR( 'b', "optb", &_G.b, "Opt b"),
    OPT_INT( 'c', "optc", &_G.c, "Opt c"),
    OPT_UINT('d', "optd", &_G.d, "Opt d"),
    OPT_CHAR('e', "opte", &_G.e, "Opt e"),
    OPT_END(),
};

Z_GROUP_EXPORT(parseopt)
{
    Z_TEST(basic, "basic valid test") {
        const char *argv[] = {
            "-a",
            "--optb", "plop",
            "-c", "-12",
            "--optd=8777",
            "-e", "c",
            "plic",
            "ploc",
        };
        int argc = countof(argv);

        p_clear(&_G, 1);
        argc = parseopt(argc, (char **)argv, popts_g, 0);
        Z_ASSERT_EQ(argc, 2);

        Z_ASSERT_STREQUAL(argv[0], "plic");
        Z_ASSERT_STREQUAL(argv[1], "ploc");

        Z_ASSERT(_G.a);
        Z_ASSERT_STREQUAL(_G.b, "plop");
        Z_ASSERT_EQ(_G.c, -12);
        Z_ASSERT_EQ(_G.d, 8777u);
        Z_ASSERT_EQ(_G.e, 'c');
    } Z_TEST_END;

    Z_TEST(optional, "opts are optionals") {
        const char *argv[] = {
            "pouet",
        };
        int argc = countof(argv);

        p_clear(&_G, 1);
        argc = parseopt(argc, (char **)argv, popts_g, 0);
        Z_ASSERT_EQ(argc, 1);

        Z_ASSERT_STREQUAL(argv[0], "pouet");

        Z_ASSERT(!_G.a);
        Z_ASSERT_NULL(_G.b);
        Z_ASSERT_EQ(_G.c, 0);
        Z_ASSERT_EQ(_G.d, 0u);
        Z_ASSERT_EQ(_G.e, 0);
    } Z_TEST_END;

    Z_TEST(invalid_flag, "error is returned for invalid flag opt") {
        const char *argv[] = {
            "--opta=uh",
        };
        int argc = countof(argv);

        p_clear(&_G, 1);
        Z_ASSERT_NEG(parseopt(argc, (char **)argv, popts_g, 0));
    } Z_TEST_END;

    Z_TEST(invalid_str, "error is returned for invalid str opt") {
        const char *argv[] = {
            "--optb",
        };
        int argc = countof(argv);

        p_clear(&_G, 1);
        Z_ASSERT_NEG(parseopt(argc, (char **)argv, popts_g, 0));
    } Z_TEST_END;

    Z_TEST(invalid_int, "error is returned for invalid int opt") {
        const char *argv[] = {
            "--optc=ghtir",
        };
        int argc = countof(argv);

        p_clear(&_G, 1);
        Z_ASSERT_NEG(parseopt(argc, (char **)argv, popts_g, 0));
    } Z_TEST_END;

    Z_TEST(invalid_uint, "error is returned for invalid uint opt") {
        const char *argv[] = {
            "--optd=fjcd",
        };
        int argc = countof(argv);

        p_clear(&_G, 1);
        Z_ASSERT_NEG(parseopt(argc, (char **)argv, popts_g, 0));
    } Z_TEST_END;

    Z_TEST(invalid_char, "error is returned for invalid char opt") {
        const char *argv[] = {
            "--opte=dheuhez",
        };
        int argc = countof(argv);

        p_clear(&_G, 1);
        Z_ASSERT_NEG(parseopt(argc, (char **)argv, popts_g, 0));
    } Z_TEST_END;

    Z_TEST(unknown, "error is returned for unknown opt") {
        const char *argv[] = {
            "--optplop",
        };
        int argc = countof(argv);

        p_clear(&_G, 1);
        Z_ASSERT_NEG(parseopt(argc, (char **)argv, popts_g, 0));
    } Z_TEST_END;

    Z_TEST(stop_at_nonarg, "POPT_STOP_AT_NONARG flag") {
        const char *argv[] = {
            "-a",
            "--optb", "plop",
            "stop",
            "-c", "-12",
            "--optd=8777",
            "-e", "c",
            "plic",
            "ploc",
        };
        int argc = countof(argv);

        p_clear(&_G, 1);
        argc = parseopt(argc, (char **)argv, popts_g, POPT_STOP_AT_NONARG);
        Z_ASSERT_EQ(argc, 8);

        Z_ASSERT_STREQUAL(argv[0], "stop");
        Z_ASSERT_STREQUAL(argv[1], "-c");
        Z_ASSERT_STREQUAL(argv[2], "-12");
        Z_ASSERT_STREQUAL(argv[3], "--optd=8777");
        Z_ASSERT_STREQUAL(argv[4], "-e");
        Z_ASSERT_STREQUAL(argv[5], "c");
        Z_ASSERT_STREQUAL(argv[6], "plic");
        Z_ASSERT_STREQUAL(argv[7], "ploc");

        Z_ASSERT(_G.a);
        Z_ASSERT_STREQUAL(_G.b, "plop");
        Z_ASSERT_EQ(_G.c, 0);
        Z_ASSERT_EQ(_G.d, 0u);
        Z_ASSERT_EQ(_G.e, 0);
    } Z_TEST_END;

    Z_TEST(ignore_unknown_opts, "POPT_IGNORE_UNKNOWN_OPTS flag") {
        const char *argv[] = {
            "-a", "--myarg", "-tata",
            "--optb", "plop",
            "-c", "-12",
            "--optd=8777", "toto",
            "-e", "c",
            "plic",
            "ploc",
        };
        int argc = countof(argv);

        p_clear(&_G, 1);
        argc = parseopt(argc, (char **)argv, popts_g,
                        POPT_IGNORE_UNKNOWN_OPTS);
        Z_ASSERT_EQ(argc, 5);

        Z_ASSERT_STREQUAL(argv[0], "--myarg");
        Z_ASSERT_STREQUAL(argv[1], "-tata");
        Z_ASSERT_STREQUAL(argv[2], "toto");
        Z_ASSERT_STREQUAL(argv[3], "plic");
        Z_ASSERT_STREQUAL(argv[4], "ploc");

        Z_ASSERT(_G.a);
        Z_ASSERT_STREQUAL(_G.b, "plop");
        Z_ASSERT_EQ(_G.c, -12);
        Z_ASSERT_EQ(_G.d, 8777u);
        Z_ASSERT_EQ(_G.e, 'c');
    } Z_TEST_END;
} Z_GROUP_END
