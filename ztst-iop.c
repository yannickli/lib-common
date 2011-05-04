/**************************************************************************/
/*                                                                        */
/*  Copyright (C) 2004-2011 INTERSEC SAS                                  */
/*                                                                        */
/*  Should you receive a copy of this source code, you must check you     */
/*  have a proper, written authorization of INTERSEC to hold it. If you   */
/*  don't have such an authorization, you must DELETE all source code     */
/*  files in your possession, and inform INTERSEC of the fact you obtain  */
/*  these files. Should you not comply to these terms, you can be         */
/*  prosecuted in the extent permitted by applicable law.                 */
/*                                                                        */
/**************************************************************************/

#include <lib-common/iop.h>
#include <lib-common/parseopt.h>
#include <lib-common/xmlr.h>
#include "iop/tstiop.iop.h"

/* ---- opts -{{{- */
static struct {
    int help;
    int soap;
    int unions;
    int json;
    int cfold;
    int iopstd;
} opts;

static struct popt_t popt[] = {
    OPT_GROUP("Generic options:"),
    OPT_FLAG('h', "help",   &opts.help,
             "show this help"),
    OPT_FLAG('s', "soap",   &opts.soap,
             "launch the soap test"),
    OPT_FLAG('u', "union",  &opts.unions,
             "launch the union test"),
    OPT_FLAG('j', "json",   &opts.json,
             "launch the json test"),
    OPT_FLAG('c', "cfold",  &opts.cfold,
             "launch the constant folding test"),
    OPT_FLAG('i', "iop",  &opts.iopstd,
             "launch the iop test"),
    OPT_END(),
};

static char const * const usage[] = {
    "",
    "IOP testing application.",
    NULL
};
/* -}}}- */

/* ---- test the union usage -{{{- */
static void test_union(void)
{
    tstiop__my_union_a__t ua = IOP_UNION(tstiop__my_union_a, ua, 42);

    IOP_UNION_SWITCH(&ua) {
        IOP_UNION_CASE(tstiop__my_union_a, &ua, ua, v)
            e_info("Get ua->ua (%d)", v);
        IOP_UNION_CASE_P(tstiop__my_union_a, &ua, ub, v)
            e_info("Get ua->ub (%d)", *v);
        IOP_UNION_CASE(tstiop__my_union_a, &ua, us, v)
            e_info("Get ua->ys (%s)", v.s);
        /* Here the default case is useless since we already check for each
         * value */
        IOP_UNION_DEFAULT()
            e_info("ua failed");
    }

#if 0
    int *foo_id = pkg__u1__get(&foo, id);
    if (foo_id) {
        e_info("Foo->id is set (%d)", *foo_id);
    }

    int foo_idc;
    if (!pkg__u1__copy(foo_idc, &foo, id)) {
        e_info("Foo->id is set (%d) bis", foo_idc);
    }

    double bar_id;
    if (!pkg__u2__copy(bar_id, &bar, id)) {
        e_info("Bar->id is set (%f)", bar_id);
    }
#endif
}
/* -}}}- */

/* ---- test soap/xml -{{{- */
static void test_soap(void)
{
#define xp  xmlr_g
    bool verbose = false;
    sb_t sb;
    int ret;

    int32_t val[] = {15, 30, 45};

    tstiop__my_struct_e__t se = {
        .a = 10,
        .b = IOP_UNION(tstiop__my_union_a, ua, 42),
        .c = {
            .b = IOP_ARRAY(val, countof(val)),
        },
    };

    tstiop__my_struct_a__t sa = {
        .a = 42,
        .b = 5,
        .c_of_my_struct_a = 120,
        .d = 230,
        .e = 540,
        .f = 2000,
        .g = 10000,
        .h = 20000,
        .i = IOP_DATA((void *)"foo", 3),
        //.i = IOP_DATA_EMPTY,
        //.j = CLSTR_IMMED("bar"),
        .j = CLSTR_EMPTY,
        .k = MY_ENUM_A_B,
        .l = IOP_UNION(tstiop__my_union_a, ub, 42),
        .m = 3.14159265,
        .n = true,
        .xml_field = CLSTR_IMMED("<foo><bar/><foobar attr=\"value\">toto</foobar></foo>"),
    };

    clstr_t svals[] = {
        CLSTR_IMMED("foo"),
        CLSTR_IMMED("bar"),
        CLSTR_IMMED("foobar")
    };

    iop_data_t dvals[] = {
        IOP_DATA((void *)"Test", 4),
        IOP_DATA((void *)"Foo", 3),
        IOP_DATA((void *)"BAR", 3)
    };

    tstiop__my_struct_b__t bvals[] = {
        {
            .b = IOP_ARRAY(NULL, 0),
        },
        {
            .a = IOP_OPT(55),
            .b = IOP_ARRAY(NULL, 0),
        }
    };

    tstiop__my_struct_f__t sf = {
        .a = IOP_ARRAY(svals, countof(svals)),
        .b = IOP_ARRAY(dvals, countof(dvals)),
        .c = IOP_ARRAY(bvals, countof(bvals)),
    };

    //e_info("ua: %d -> %d", sa.l.iop_tag, sa.l.ua);
    sb_init(&sb);
    sb_adds(&sb, "<root "
            "xmlns:xsd=\"http://www.w3.org/2001/XMLSchema\" "
            "xmlns:xsi=\"http://www.w3.org/2001/XMLSchema-instance\" "
            ">\n");

    /* data packing */

    iop_xpack(&sb, &tstiop__my_struct_e__s, &se, verbose, true);
    sb_adds(&sb, "</root>\n");
    printf("*** Packed:\n%s\n\n*** Unpacked:\n", sb.data);

    {
        tstiop__my_struct_e__t se_ret;
        tstiop__my_struct_e__init(&se_ret);

        ret =  xmlr_setup(&xp, sb.data, sb.len)
            ?: iop_xunpack(xp, t_pool(), &tstiop__my_struct_e__s, &se_ret);

        if (ret < 0) {
            e_info("%s", xmlr_get_err());
            exit(1);
        } else {
            sb_reset(&sb);
            iop_xpack(&sb, &tstiop__my_struct_e__s, &se_ret, verbose, true);
            printf("%s\n\n", sb.data);
        }
        xmlr_close(xp);
    }

    /* XXX Second example */

    sb_reset(&sb);
    sb_adds(&sb, "<root "
            "xmlns:xsd=\"http://www.w3.org/2001/XMLSchema\" "
            "xmlns:xsi=\"http://www.w3.org/2001/XMLSchema-instance\" "
            ">\n");

    /* data packing */

    iop_xpack(&sb, &tstiop__my_struct_a__s, &sa, verbose, true);
    sb_adds(&sb, "</root>\n");
    printf("*** Packed 2:\n%s\n\n*** Unpacked 2:\n", sb.data);

    {
        tstiop__my_struct_a__t sa_ret;
        tstiop__my_struct_a__init(&sa_ret);

        ret =  xmlr_setup(&xp, sb.data, sb.len)
            ?: iop_xunpack(xp, t_pool(), &tstiop__my_struct_a__s, &sa_ret);
        if (ret < 0) {
            e_info("%s", xmlr_get_err());
            exit(1);
        } else {
            sb_reset(&sb);
            iop_xpack(&sb, &tstiop__my_struct_a__s, &sa_ret, verbose,  true);
            printf("%s\n\n", sb.data);
        }
        xmlr_close(xp);
    }

    /* XXX Third example */

    sb_reset(&sb);
    sb_adds(&sb, "<root "
            "xmlns:xsd=\"http://www.w3.org/2001/XMLSchema\" "
            "xmlns:xsi=\"http://www.w3.org/2001/XMLSchema-instance\" "
            ">\n");

    /* data packing */

    iop_xpack(&sb, &tstiop__my_struct_f__s, &sf, verbose, true);
    sb_adds(&sb, "<c />\n");
    sb_adds(&sb, "</root>\n");
    printf("*** Packed 3:\n%s\n\n*** Unpacked 3:\n", sb.data);

    {
        tstiop__my_struct_f__t sf_ret;
        tstiop__my_struct_f__init(&sf_ret);

        ret =  xmlr_setup(&xp, sb.data, sb.len)
            ?: iop_xunpack(xp, t_pool(), &tstiop__my_struct_f__s, &sf_ret);
        if (ret < 0) {
            e_info("%s", xmlr_get_err());
            exit(1);
        } else {
            sb_reset(&sb);
            iop_xpack(&sb, &tstiop__my_struct_f__s, &sf_ret, verbose, true);
            printf("%s\n\n", sb.data);
        }

        xmlr_close(xp);
    }

#if 0
    /* data wsdl */
    sb_reset(&sb);
    iop_xwsdl(&sb, &tstiop__my_mod_a__mod, "tstiop");
    printf("\n%s\n", sb.data);
#endif


    xmlr_delete(&xp);
    sb_wipe(&sb);
#undef xp
}
/* -}}}- */

/* ---- json test -{{{- */
static int jwrite(void *priv, const void *buf, int len) {
    return fwrite(buf, sizeof(char), len, stdout);
}
static void test_json(void)
{
    int ua_pos, res;
    tstiop__my_struct_a__t sa = {
        .a = 42,
        .b = 5,
        .c_of_my_struct_a = 120,
        .d = 230,
        .e = 540,
        .f = 2000,
        .g = 10000,
        .h = 20000,
        .i = IOP_DATA((void *)"foo", 3),
        //.i = IOP_DATA_EMPTY,
        .j = CLSTR_IMMED("baré© \" foo ."),
        //.j = CLSTR_EMPTY,
        .k = MY_ENUM_A_B,
        .l = IOP_UNION(tstiop__my_union_a, ub, 42),
        .m = 3.14159265,
        .n = true,
    };

    const char json_sa[] =
"        /* Json example */\n"
"        @j \"bar\" {\n"
"            \"a\": 42,\n"
"            \"b\": 50,\n"
"            cOfMyStructA: 30,\n"
"            \"d\": 40,\n"
"            \"e\": 50, //comment\n"
"            \"f\": 60,\n"
"            \"g\": 10d,\n"
"            \"h\": 1T,\n"
"            \"i\": \"foo\",\n"
//"            \"j\": \"bar\",\n"
"            \"k\": \"B\",\n"
"            l.us: \"union value\",\n"
//"            l: {us: \"union value normal\"},\n"
"            foo: {us: \"union value to skip\"},\n"
"            bar.us: \"union value to skip\",\n"
"            \"m\": 10.42\n,"
"            \"n\": true\n"
"        };\n"
    ;

    const char json_sf[] =
"        /* Json example */\n"
"        {\n"
"            a = [ \"foo\", \"bar\", ];\n"
"            b = [ \"foobar\", \"barfoo\", ];\n"
"            c = [ @a 10 {\n"
"               b = [ 1w, 1d, 1h, 1m, 1s, 1G, 1M, 1K, ];\n"
"            }];\n"
"            d = [ .us: \"foo\", .ub: true ];\n"
//"            d = [ {us: \"foo\"}, {ub: true} ];\n"
"        };;;\n"
    ;

    const char json_ua[] =
"        /* Json example */\n"
"        {\n"
"        us: \"foo\"\n"
"        },,\n"
"        /* Json example */\n"
"        {\n"
"        us: \"foo\"\n"
"        };;\n"
    ;


    sb_t sb;
    pstream_t ps;
    iop_json_lex_t *jll = iop_jlex_new(t_pool());

    sb_init(&sb);
    /* XXX packing example*/
    printf("\nPacking test:\n");
    tstiop__my_struct_a__jpack(&sa, &jwrite, NULL);

    printf("\nPacking test bis:\n");
    {
        tstiop__my_struct_g__t sg;
        tstiop__my_struct_g__init(&sg);
        tstiop__my_struct_g__jpack(&sg, &jwrite, NULL);
    }

    /* XXX First unpacking example */
    printf("\nFirst test:\n");
    {
        tstiop__my_struct_a__t sa_res;
        tstiop__my_struct_a__init(&sa_res);

        ps = ps_initstr(json_sa);
        iop_jlex_attach(jll, &ps);
        if ((tstiop__my_struct_a__junpack(jll, &sa_res, true)) == 0)
        {
            tstiop__my_struct_a__jpack(&sa_res, &jwrite, NULL);
        } else {
            sb_reset(&sb);
            iop_jlex_write_error(jll, &sb);
            e_info("XXX junpacking error: %s", sb.data);
        }
        iop_jlex_detach(jll);
    }


    /* XXX Second test */
    printf("\nSecond test:\n");
    {
        tstiop__my_struct_f__t sf_res;
        tstiop__my_struct_f__init(&sf_res);

        ps = ps_initstr(json_sf);
        iop_jlex_attach(jll, &ps);
        if ((tstiop__my_struct_f__junpack(jll, &sf_res, true)) == 0)
        {
            tstiop__my_struct_f__jpack(&sf_res, &jwrite, NULL);
        } else {
            char sbuf[256];
            iop_jlex_write_error_buf(jll, sbuf, 256);
            e_info("XXX junpacking error: %s", sbuf);
        }
        iop_jlex_detach(jll);
    }

    /* XXX Third test */
    printf("\nThird test:\n");
    ua_pos = 0;
    res = 1;
    ps = ps_init(json_ua, sizeof(json_ua) - 1);
    iop_jlex_attach(jll, &ps);
    for (int t = 1; res > 0 && t < 15; t++) {
        tstiop__my_union_a__t ua_res;
        tstiop__my_union_a__init(&ua_res);

        e_info("\tPass %d", t);

        if ((res = tstiop__my_union_a__junpack(jll, &ua_res, false)) > 0)
        {
            tstiop__my_union_a__jpack(&ua_res, &jwrite, NULL);
            ua_pos += res;
        } else {
            char sbuf[256];
            iop_jlex_write_error_buf(jll, sbuf, 256);
            e_info("XXX junpacking error: %s", sbuf);
        }
    }
    iop_jlex_detach(jll);

    sb_wipe(&sb);
    iop_jlex_delete(&jll);
}
/* -}}}- */

/* ---- iop std test -{{{- */
static void test_iop(void)
{
    t_scope;
    char dst[BUFSIZ];
    qv_t(i32) szs;
    int len, i = 0;

    qv_inita(i32, &szs, 1024);
    p_clear(dst, 1);

    {
        tstiop__my_struct_a__t sa = {
            .a = 42,
            .b = 5,
            .c_of_my_struct_a = 120,
            .d = 230,
            .e = 540,
            .f = 2000,
            .g = 10000,
            .h = 20000,
            .i = IOP_DATA((void *)"foo", 3),
            //.i = IOP_DATA_EMPTY,
            .j = CLSTR_IMMED("baré© \" foo ."),
            //.j = CLSTR_EMPTY,
            .k = MY_ENUM_A_B,
            .l = IOP_UNION(tstiop__my_union_a, ub, 42),
            .m = 3.14159265,
            .n = true,
        };

        tstiop__my_struct_a__t sa_res;
        tstiop__my_struct_a__init(&sa_res);

        len = iop_bpack_size(&tstiop__my_struct_a__s, &sa, &szs);
        e_info("%2d: Packing (packing size: %d) ...", ++i, len);
        iop_bpack(dst, &tstiop__my_struct_a__s, &sa, szs.tab);

        e_info("%2d: Unpacking ...", i);
        if (iop_bunpack(t_pool(), &tstiop__my_struct_a__s, &sa_res,
                        ps_init(dst, BUFSIZ), false) < 0)
        {
            e_info("XXX unpacking error!");
        }
        /* Test duplication */
        e_info("%2d: Duplicating ...", i);
        IGNORE(tstiop__my_struct_a__dup(t_pool(), &sa_res));
    }

    /* Test 2nd */
    {
        tstiop__my_struct_a_opt__t sa_opt;
        tstiop__my_struct_a_opt__t sa_opt_res;
        tstiop__my_struct_a_opt__init(&sa_opt);
        tstiop__my_struct_a_opt__init(&sa_opt_res);

        IOP_OPT_SET(sa_opt.a, 32);
        sa_opt.j = CLSTR_IMMED_V("foo");

        qv_clear(i32, &szs);
        len = iop_bpack_size(&tstiop__my_struct_a_opt__s, &sa_opt, &szs);
        e_info("%2d: Packing (packing size: %d) ...", ++i, len);
        iop_bpack(dst, &tstiop__my_struct_a_opt__s, &sa_opt, szs.tab);

        e_info("%2d: Unpacking ...", i);
        if (iop_bunpack(t_pool(), &tstiop__my_struct_a_opt__s, &sa_opt_res,
                        ps_init(dst, BUFSIZ), false) < 0)
        {
            e_info("XXX unpacking error!");
        }
        /* Test duplication */
        e_info("%2d: Duplicating ...", i);
        IGNORE(tstiop__my_struct_a_opt__dup(t_pool(), &sa_opt_res));
    }

    qv_wipe(i32, &szs);
}
/* -}}}- */

/* ---- constant folding test -{{{- */
static void test_cfold(void)
{
#define feed_num(_num) \
    if (err == 0 &&  (err = iop_cfolder_feed_number(folder, _num, true)) < 0) \
        e_info("Error when feeding with %d", _num)
#define feed_op(_op) \
    if (err == 0 && (err = iop_cfolder_feed_operator(folder, _op)) < 0) \
        e_info("Error when feeding with %c", _op)
#define result() \
    if (err == 0 && iop_cfolder_get_result(folder, &res) == 0) { \
        e_info("Operation result: %"PRIi64, (int64_t) res); \
    } else { \
        e_info("Constant folder error"); \
    } \
    iop_cfolder_init(folder); \
    err = 0;

    iop_cfolder_t *folder = iop_cfolder_new();
    uint64_t res;
    int err = 0;

#if 1
    feed_num(10);
    feed_op('+');
    feed_num(2);
    feed_op('*');
    feed_num(3);
    feed_op('*');
    feed_num(4);
    feed_op('-');
    feed_num(10);
    e_info("Expected : 24");
    result();
#endif
#if 1
    feed_num(10);
    feed_op('*');
    feed_num(2);
    feed_op('+');
    feed_num(3);
    feed_op('+');
    feed_num(4);
    feed_op('*');
    feed_num(10);
    e_info("Expected : 63");
    result();
#endif
#if 1
    feed_num(8);
    feed_op('+');
    feed_num(4);
    feed_op('+');
    feed_op('-');
    feed_num(2);
    feed_op('+');
    feed_num(2);
    feed_op('*');
    feed_op('-');
    feed_num(5);
    feed_op('/');
    feed_num(2);
    feed_op('+');
    feed_num(1);
    e_info("Expected : 6");
    result();
#endif
#if 1
    feed_num(32);
    feed_op('/');
    feed_num(4);
    feed_op(CF_OP_EXP);
    feed_num(2);
    feed_op('/');
    feed_num(2);
    e_info("Expected : 1");
    result();
#endif
#if 1
    feed_num(8);
    feed_op('/');
    feed_num(4);
    feed_op('/');
    feed_num(2);
    e_info("Expected : 1");
    result();
#endif
#if 1
    feed_num(8);
    feed_op('/');
    feed_op('(');
    feed_num(4);
    feed_op('/');
    feed_num(2);
    feed_op(')');
    e_info("Expected : 4");
    result();
#endif
#if 1
    feed_num(4);
    feed_op(CF_OP_EXP);
    feed_num(3);
    feed_op(CF_OP_EXP);
    feed_num(2);
    e_info("Expected : 262144");
    result();
#endif
#if 1
    feed_num(4);
    feed_op('+');
    feed_op('-');
    feed_num(2);
    feed_op(CF_OP_EXP);
    feed_num(2);
    e_info("Expected : 0");
    result();
#endif
#if 1
    feed_num(1);
    feed_op('+');
    feed_num(4);
    feed_op(CF_OP_EXP);
    feed_num(3);
    feed_op(CF_OP_EXP);
    feed_num(1);
    feed_op('+');
    feed_num(1);
    feed_op('-');
    feed_num(1);
    e_info("Expected : 65");
    result();
#endif
#if 1
    feed_num(0xfffff);
    feed_op('&');
    feed_num(32);
    feed_op(CF_OP_LSHIFT);
    feed_num(2);
    feed_op('|');
    feed_num(3);
    e_info("Expected : 131");
    result();
#endif

    iop_cfolder_delete(&folder);
#undef feed_num
#undef feed_op
}
/* -}}}- */

/* IOP testing */
int main(int argc, char **argv)
{
    int c = parseopt(argc - 1, argv + 1, popt, 0);
    if (c < 0 || opts.help)
        makeusage(c < 0, argv[0], usage[0], usage + 1, popt);


    if (opts.soap) {
        test_soap();
    } else
    if (opts.unions) {
        test_union();
    } else
    if (opts.json) {
        test_json();
    } else
    if (opts.cfold) {
        test_cfold();
    } else
    if (opts.iopstd) {
        test_iop();
    } else {
        makeusage(c < 0, argv[0], usage[0], usage + 1, popt);
    }

    return EXIT_SUCCESS;
}
