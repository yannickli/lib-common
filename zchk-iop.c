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

#include "z.h"
#include "iop.h"
#include "iop/tstiop.iop.h"
#include "ic.iop.h"
#include "xmlr.h"

/* {{{ IOP testing helpers */

static int iop_xml_test_struct(const iop_struct_t *st, void *v, const char *info)
{
    int len;
    lstr_t s;
    uint8_t buf1[20], buf2[20];
    byte *res;

    t_scope;
    SB_8k(sb);

    sb_adds(&sb, "<root xmlns:xsd=\"http://www.w3.org/2001/XMLSchema\" "
            "xmlns:xsi=\"http://www.w3.org/2001/XMLSchema-instance\">");
    len = sb.len;
    iop_xpack(&sb, st, v, false, true);
    sb_adds(&sb, "</root>");

    s = t_lstr_dups(sb.data + len, sb.len - len - 7);

    /* unpacking */
    res = t_new_raw(byte, ROUND_UP(st->size, 8));

    Z_ASSERT_N((xmlr_setup(&xmlr_g, sb.data, sb.len) ?:
                iop_xunpack(xmlr_g, t_pool(), st, res)),
               "XML unpacking failure (%s, %s): %s", st->fullname.s, info,
               xmlr_get_err());

    /* pack again ! */
    sb_reset(&sb);
    iop_xpack(&sb, st, res, false, true);

    /* check packing equality */
    Z_ASSERT_LSTREQUAL(s, LSTR_SB_V(&sb),
                       "XML packing/unpacking doesn't match! (%s, %s)",
                       st->fullname.s, info);

    /* In case of, check hashes equality */
    iop_hash_sha1(st, v,   buf1);
    iop_hash_sha1(st, res, buf2);
    Z_ASSERT_EQUAL(buf1, sizeof(buf1), buf2, sizeof(buf2),
                   "XML packing/unpacking hashes don't match! (%s, %s)",
                   st->fullname.s, info);

    xmlr_close(&xmlr_g);
    Z_HELPER_END;
}

static int iop_json_test_struct(const iop_struct_t *st, void *v,
                                const char *info)
{
    t_scope;
    byte *res;
    iop_json_lex_t jll;
    pstream_t ps;
    int strict = 0;
    uint8_t buf1[20], buf2[20];
    SB_8k(sb);

    iop_jlex_init(t_pool(), &jll);
    res = t_new_raw(byte, ROUND_UP(st->size, 8));

    while (strict < 2) {
        int ret;

        /* packing */
        sb_reset(&sb);
        Z_ASSERT_N(iop_jpack(st, v, iop_sb_write, &sb, strict),
                   "JSon packing failure! (%s, %s)", st->fullname.s, info);

        /* unpacking */
        ps = ps_initsb(&sb);
        iop_jlex_attach(&jll, &ps);
        if ((ret = iop_junpack(&jll, st, res, true)) < 0) {
            sb_reset(&sb);
            iop_jlex_write_error(&jll, &sb);
        }
        Z_ASSERT_N(ret, "JSon unpacking error (%s, %s): %s", st->fullname.s,
                   info, sb.data);
        iop_jlex_detach(&jll);

        /* check hashes equality */
        iop_hash_sha1(st, v,   buf1);
        iop_hash_sha1(st, res, buf2);
        Z_ASSERT_EQUAL(buf1, sizeof(buf1), buf2, sizeof(buf2),
                       "JSON %spacking/unpacking hashes don't match! (%s, %s)",
                       (strict ? "strict " : ""),
                       st->fullname.s, info);

        strict++;
    }

    iop_jlex_wipe(&jll);

    Z_HELPER_END;
}

static int iop_json_test_json(const iop_struct_t *st, const char *json, const
                              void *expected, const char *info)
{
    t_scope;
    iop_json_lex_t jll;
    pstream_t ps;
    byte *res;
    int ret;
    uint8_t buf1[20], buf2[20];
    SB_1k(sb);

    iop_jlex_init(t_pool(), &jll);
    res = t_new_raw(byte, ROUND_UP(st->size, 8));

    ps = ps_initstr(json);
    iop_jlex_attach(&jll, &ps);
    if ((ret = iop_junpack(&jll, st, res, true)) < 0)
        iop_jlex_write_error(&jll, &sb);
    Z_ASSERT_N(ret, "JSon unpacking error (%s, %s): %s", st->fullname.s, info,
               sb.data);
    iop_jlex_detach(&jll);

    /* visualize result */
    if (e_is_traced(1))
        iop_jtrace_(1, __FILE__, __LINE__, __func__, NULL, st, res);

    /* check hashes equality */
    iop_hash_sha1(st, res,      buf1);
    iop_hash_sha1(st, expected, buf2);
    Z_ASSERT_EQUAL(buf1, sizeof(buf1), buf2, sizeof(buf2),
                   "JSON unpacking hashes don't match! (%s, %s)",
                   st->fullname.s, info);

    iop_jlex_wipe(&jll);

    Z_HELPER_END;
}

static int iop_std_test_struct(const iop_struct_t *st, void *v,
                               const char *info)
{
    t_scope;
    byte *res;
    uint8_t buf1[20], buf2[20];
    qv_t(i32) szs;
    int len;
    byte *dst;

    t_qv_init(i32, &szs, 1024);

    /* packing */
    Z_ASSERT_N((len = iop_bpack_size(st, v, &szs)),
               "invalid structure size (%s, %s)", st->fullname.s, info);
    dst = t_new(byte, len);
    iop_bpack(dst, st, v, szs.tab);

    /* unpacking */
    res = t_new(byte, ROUND_UP(st->size, 8));
    iop_init(st, res);

    Z_ASSERT_N(iop_bunpack(t_pool(), st, res, ps_init(dst, len), false),
               "IOP unpacking error (%s, %s)", st->fullname.s, info);

    /* check hashes equality */
    iop_hash_sha1(st, v,   buf1);
    iop_hash_sha1(st, res, buf2);
    Z_ASSERT_EQUAL(buf1, sizeof(buf1), buf2, sizeof(buf2),
                   "IOP packing/unpacking hashes don't match! (%s, %s)",
                   st->fullname.s, info);

    /* check equality */
    Z_ASSERT(iop_equals(st, v, res));

    /* test duplication */
    Z_ASSERT_NULL(iop_dup(NULL, st, NULL));
    Z_ASSERT_P(res = iop_dup(t_pool(), st, v),
               "IOP duplication error! (%s, %s)", st->fullname.s, info);

    /* check hashes equality */
    iop_hash_sha1(st, res, buf2);
    Z_ASSERT_EQUAL(buf1, sizeof(buf1), buf2, sizeof(buf2),
                   "IOP duplication hashes don't match! (%s, %s)",
                   st->fullname.s, info);

    /* test copy */
    iop_copy(t_pool(), st, (void **)&res, NULL);
    Z_ASSERT_NULL(res);
    iop_copy(t_pool(), st, (void **)&res, v);

    /* check hashes equality */
    iop_hash_sha1(st, res, buf2);
    Z_ASSERT_EQUAL(buf1, sizeof(buf1), buf2, sizeof(buf2),
                   "IOP copy hashes don't match! (%s, %s)",
                   st->fullname.s, info);

    Z_HELPER_END;
}

/* }}} */

Z_GROUP_EXPORT(iop)
{
    Z_TEST(dso_open, "test wether iop_dso_open works and loads stuff") { /* {{{ */
        t_scope;

        iop_dso_t *dso;
        const iop_struct_t *st;
        lstr_t path = t_lstr_cat(z_cmddir_g,
                                 LSTR_IMMED_V("zchk-iop-plugin.so"));

        Z_ASSERT(dso = iop_dso_open(path.s));
        Z_ASSERT_N(qm_find(iop_struct, &dso->struct_h,
                           &LSTR_IMMED_V("ic.Hdr")));

        Z_ASSERT_P(st = iop_dso_find_type(dso, LSTR_IMMED_V("ic.SimpleHdr")));
        Z_ASSERT(st != &ic__simple_hdr__s);

        iop_dso_close(&dso);
    } Z_TEST_END;
    /* }}} */
    Z_TEST(hash_sha1, "test wether iop_hash_sha1 is stable wrt ABI change") { /* {{{ */
        t_scope;

        int  i_10 = 10, i_11 = 11;
        long j_10 = 10;

        struct tstiop__hash_v1__t v1 = {
            .b  = IOP_OPT(true),
            .i  = IOP_ARRAY(&i_10, 1),
            .s  = LSTR_IMMED("foo bar baz"),
        };

        struct tstiop__hash_v2__t v2 = {
            .b  = IOP_OPT(true),
            .i  = IOP_ARRAY(&j_10, 1),
            .s  = LSTR_IMMED("foo bar baz"),
        };

        struct tstiop__hash_v1__t v1_not_same = {
            .b  = IOP_OPT(true),
            .i  = IOP_ARRAY(&i_11, 1),
            .s  = LSTR_IMMED("foo bar baz"),
        };

        const iop_struct_t *stv1;
        const iop_struct_t *stv2;

        iop_dso_t *dso;
        lstr_t path = t_lstr_cat(z_cmddir_g,
                                 LSTR_IMMED_V("zchk-tstiop-plugin.so"));
        uint8_t buf1[20], buf2[20];

        dso = iop_dso_open(path.s);
        if (dso == NULL)
            Z_SKIP("unable to load zchk-tstiop-plugin, TOOLS repo?");

        Z_ASSERT_P(stv1 = iop_dso_find_type(dso, LSTR_IMMED_V("tstiop.HashV1")));
        Z_ASSERT_P(stv2 = iop_dso_find_type(dso, LSTR_IMMED_V("tstiop.HashV2")));

        iop_hash_sha1(stv1, &v1, buf1);
        iop_hash_sha1(stv2, &v2, buf2);
        Z_ASSERT_EQUAL(buf1, sizeof(buf1), buf2, sizeof(buf2));

        iop_hash_sha1(stv1, &v1_not_same, buf2);
        Z_ASSERT(memcmp(buf1, buf2, sizeof(buf1)) != 0);
        iop_dso_close(&dso);
    } Z_TEST_END;
    /* }}} */
    Z_TEST(constant_folder, "test the IOP constant folder") { /* {{{ */
#define feed_num(_num) \
        Z_ASSERT_N(iop_cfolder_feed_number(&cfolder, _num, true), \
                   "error when feeding %d", _num)
#define feed_op(_op) \
        Z_ASSERT_N(iop_cfolder_feed_operator(&cfolder, _op), \
                   "error when feeding with %d", _op)

#define result(_res) \
        do {                                                            \
            uint64_t cres;                                              \
            \
            Z_ASSERT_N(iop_cfolder_get_result(&cfolder, &cres),         \
                       "constant folder error");                        \
            Z_ASSERT_EQ((int64_t)cres, (int64_t)_res);                  \
            iop_cfolder_wipe(&cfolder);                                 \
            iop_cfolder_init(&cfolder);                                 \
        } while (false)

        iop_cfolder_t cfolder;

        iop_cfolder_init(&cfolder);

        feed_num(10);
        feed_op('+');
        feed_num(2);
        feed_op('*');
        feed_num(3);
        feed_op('*');
        feed_num(4);
        feed_op('-');
        feed_num(10);
        result(24);

        feed_num(10);
        feed_op('*');
        feed_num(2);
        feed_op('+');
        feed_num(3);
        feed_op('+');
        feed_num(4);
        feed_op('*');
        feed_num(10);
        result(63);

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
        result(6);

        feed_num(32);
        feed_op('/');
        feed_num(4);
        feed_op(CF_OP_EXP);
        feed_num(2);
        feed_op('/');
        feed_num(2);
        result(1);

        feed_num(8);
        feed_op('/');
        feed_num(4);
        feed_op('/');
        feed_num(2);
        result(1);

        feed_num(8);
        feed_op('/');
        feed_op('(');
        feed_num(4);
        feed_op('/');
        feed_num(2);
        feed_op(')');
        result(4);

        feed_num(4);
        feed_op(CF_OP_EXP);
        feed_num(3);
        feed_op(CF_OP_EXP);
        feed_num(2);
        result(262144);

        feed_num(4);
        feed_op('+');
        feed_op('-');
        feed_num(2);
        feed_op(CF_OP_EXP);
        feed_num(2);
        result(0);

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
        result(65);

        feed_num(0xfffff);
        feed_op('&');
        feed_num(32);
        feed_op(CF_OP_LSHIFT);
        feed_num(2);
        feed_op('|');
        feed_num(3);
        result(131);

        iop_cfolder_wipe(&cfolder);
#undef feed_num
#undef feed_op
#undef result
    } Z_TEST_END;
    /* }}} */
    Z_TEST(unions, "test IOP union helpers") { /* {{{ */
        t_scope;

        iop_dso_t *dso;
        lstr_t path = t_lstr_cat(z_cmddir_g,
                                 LSTR_IMMED_V("zchk-tstiop-plugin.so"));

        if ((dso = iop_dso_open(path.s)) == NULL)
            Z_SKIP("unable to load zchk-tstiop-plugin, TOOLS repo?");

        {
            tstiop__my_union_a__t ua = IOP_UNION(tstiop__my_union_a, ua, 42);
            int *uavp, uav = 0;

            IOP_UNION_SWITCH(&ua) {
              IOP_UNION_CASE(tstiop__my_union_a, &ua, ua, v) {
                Z_ASSERT_EQ(v, 42);
              }
              IOP_UNION_CASE_P(tstiop__my_union_a, &ua, ub, v) {
                Z_ASSERT(false, "shouldn't be reached");
              }
              IOP_UNION_CASE(tstiop__my_union_a, &ua, us, v) {
                Z_ASSERT(false, "shouldn't be reached");
              }
              IOP_UNION_DEFAULT() {
                Z_ASSERT(false, "default case shouldn't be reached");
              }
            }

            Z_ASSERT_P(uavp = tstiop__my_union_a__get(&ua, ua));
            Z_ASSERT_EQ(*uavp, 42);
            Z_ASSERT(IOP_UNION_COPY(uav, tstiop__my_union_a, &ua, ua));
            Z_ASSERT_EQ(uav, 42);

            Z_ASSERT_NULL(tstiop__my_union_a__get(&ua, ub));
            Z_ASSERT_NULL(tstiop__my_union_a__get(&ua, us));
        }

        {
            tstiop__my_union_a__t ub = IOP_UNION(tstiop__my_union_a, ub, 42);
            int8_t *ubvp;

            IOP_UNION_SWITCH(&ub) {
              IOP_UNION_CASE(tstiop__my_union_a, &ub, ua, v) {
                Z_ASSERT(false, "shouldn't be reached");
              }
              IOP_UNION_CASE_P(tstiop__my_union_a, &ub, ub, v) {
                Z_ASSERT_EQ(*v, 42);
              }
              IOP_UNION_CASE(tstiop__my_union_a, &ub, us, v) {
                Z_ASSERT(false, "shouldn't be reached");
              }
              IOP_UNION_DEFAULT() {
                Z_ASSERT(false, "default case shouldn't be reached");
              }
            }

            Z_ASSERT_P(ubvp = tstiop__my_union_a__get(&ub, ub));
            Z_ASSERT_EQ(*ubvp, 42);

            Z_ASSERT_NULL(tstiop__my_union_a__get(&ub, ua));
            Z_ASSERT_NULL(tstiop__my_union_a__get(&ub, us));
        }

        {
            tstiop__my_union_a__t us = IOP_UNION(tstiop__my_union_a, us,
                                                 LSTR_IMMED("foo"));
            lstr_t *usvp;

            IOP_UNION_SWITCH(&us) {
              IOP_UNION_CASE(tstiop__my_union_a, &us, ua, v) {
                Z_ASSERT(false, "shouldn't be reached");
              }
              IOP_UNION_CASE_P(tstiop__my_union_a, &us, ub, v) {
                Z_ASSERT(false, "shouldn't be reached");
              }
              IOP_UNION_CASE(tstiop__my_union_a, &us, us, v) {
                Z_ASSERT_LSTREQUAL(v, LSTR_IMMED_V("foo"));
              }
              IOP_UNION_DEFAULT() {
                Z_ASSERT(false, "default case shouldn't be reached");
              }
            }

            Z_ASSERT_P(usvp = tstiop__my_union_a__get(&us, us));
            Z_ASSERT_LSTREQUAL(*usvp, LSTR_IMMED_V("foo"));

            Z_ASSERT_NULL(tstiop__my_union_a__get(&us, ua));
            Z_ASSERT_NULL(tstiop__my_union_a__get(&us, ub));
        }

        iop_dso_close(&dso);
    } Z_TEST_END
    /* }}} */
    Z_TEST(soap, "test IOP SOAP (un)packer") { /* {{{ */
        t_scope;

        iop_dso_t *dso;
        lstr_t path = t_lstr_cat(z_cmddir_g,
                                 LSTR_IMMED_V("zchk-tstiop-plugin.so"));

        int32_t val[] = {15, 30, 45};

        tstiop__my_struct_e__t se = {
            .a = 10,
            .b = IOP_UNION(tstiop__my_union_a, ua, 42),
            .c = { .b = IOP_ARRAY(val, countof(val)), },
        };

        uint64_t uval[] = {UINT64_MAX, INT64_MAX, 0};

        tstiop__my_struct_a__t sa = {
            .a = 42,
            .b = 5,
            .c_of_my_struct_a = 120,
            .d = 230,
            .e = 540,
            .f = 2000,
            .g = 10000,
            .h = UINT64_MAX,
            .htab = IOP_ARRAY(uval, countof(uval)),
            .i = IOP_DATA((void *)"foo", 3),
            .j = LSTR_EMPTY,
            .k = MY_ENUM_A_B,
            .l = IOP_UNION(tstiop__my_union_a, ub, 42),
            .m = 3.14159265,
            .n = true,
            .xml_field = LSTR_IMMED("<foo><bar/><foobar "
                                    "attr=\"value\">toto</foobar></foo>"),
        };

        clstr_t svals[] = {
            LSTR_IMMED("foo"), LSTR_IMMED("bar"), LSTR_IMMED("foobar"),
        };

        iop_data_t dvals[] = {
            IOP_DATA((void *)"Test", 4), IOP_DATA((void *)"Foo", 3),
            IOP_DATA((void *)"BAR", 3),
        };

        tstiop__my_struct_b__t bvals[] = {
            { .b = IOP_ARRAY(NULL, 0), },
            { .a = IOP_OPT(55), .b = IOP_ARRAY(NULL, 0), }
        };

        tstiop__my_struct_f__t sf = {
            .a = IOP_ARRAY(svals, countof(svals)),
            .b = IOP_ARRAY(dvals, countof(dvals)),
            .c = IOP_ARRAY(bvals, countof(bvals)),
        };

        const iop_struct_t *st_se, *st_sa, *st_sf;

        if ((dso = iop_dso_open(path.s)) == NULL)
            Z_SKIP("unable to load zchk-tstiop-plugin, TOOLS repo?");

        Z_ASSERT_P(st_se = iop_dso_find_type(dso, LSTR_IMMED_V("tstiop.MyStructE")));
        Z_ASSERT_P(st_sa = iop_dso_find_type(dso, LSTR_IMMED_V("tstiop.MyStructA")));
        Z_ASSERT_P(st_sf = iop_dso_find_type(dso, LSTR_IMMED_V("tstiop.MyStructF")));

        /* We test that packing and unpacking of XML structures is stable */
        Z_HELPER_RUN(iop_xml_test_struct(st_se, &se, "se"));
        Z_HELPER_RUN(iop_xml_test_struct(st_sa, &sa, "sa"));
        Z_HELPER_RUN(iop_xml_test_struct(st_sf, &sf, "sf"));

        iop_dso_close(&dso);
    } Z_TEST_END
    /* }}} */
    Z_TEST(json, "test IOP JSon (un)packer") { /* {{{ */
        t_scope;

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
            .j = LSTR_IMMED("baré© \" foo ."),
            .xml_field = LSTR_IMMED("<foo />"),
            .k = MY_ENUM_A_B,
            .l = IOP_UNION(tstiop__my_union_a, ub, 42),
            .m = 3.14159265,
            .n = true,
            .o = '.',
            .p = '!',
            .q = '*',
            .r = '+',
            .s = '\t',
        };

        tstiop__my_struct_a__t sa2 = {
            .a = 42,
            .b = 5,
            .c_of_my_struct_a = 120,
            .d = 230,
            .e = 540,
            .f = 2000,
            .g = 10000,
            .h = 20000,
            .i = IOP_DATA_EMPTY,
            .j = LSTR_EMPTY,
            .xml_field = LSTR_EMPTY,
            .k = MY_ENUM_A_B,
            .l = IOP_UNION(tstiop__my_union_a, ub, 42),
            .m = 3.14159265,
            .n = true,
            .o = '.',
            .p = '!',
            .q = '*',
            .r = '+',
            .s = '\t',
        };

        const char json_sa[] =
            "/* Json example */\n"
            "@j \"bar\" {\n"
            "    \"a\": 42,\n"
            "    \"b\": 50,\n"
            "    cOfMyStructA: 30,\n"
            "    \"d\": 40,\n"
            "    \"e\": 50, //comment\n"
            "    \"f\": 60,\n"
            "    \"g\": 10d,\n"
            "    \"h\": 1T,\n"
            "    \"i\": \"Zm9v\",\n"
            "    \"xmlField\": \"\",\n"
            "    \"k\": \"B\",\n"
            "    l.us: \"union value\",\n"
            "    foo: {us: \"union value to skip\"},\n"
            "    bar.us: \"union value to skip\",\n"
            "    arraytoSkip: [ .blah: \"skip\", .foo: 42, 32; \"skipme\";\n"
            "                   { foo: 42 } ];"
            "    \"m\": .42,\n"
            "    \"n\": true,\n"
            "    \"o\": c\'.\',\n"
            "    \"p\": c\'\\041\',\n"
            "    \"q\": c\'\\x2A\',\n"
            "    \"r\": c\'\\u002B\',\n"
            "    \"s\": c\'\\t\'\n"
            "};\n"
            ;

        const char json_sa2[] =
            "/* Json example */\n"
            "@j \"bar\" {\n"
            "    \"a\": 42,\n"
            "    \"b\": 50,\n"
            "    cOfMyStructA: 30,\n"
            "    \"d\": 40,\n"
            "    \"e\": 50, //comment\n"
            "    \"f\": 60,\n"
            "    \"g\": 10d,\n"
            "    \"h\": 1T,\n"
            "    \"i\": \"Zm9v\",\n"
            "    \"xmlField\": \"\",\n"
            "    \"k\": \"B\",\n"
            "    l: {us: \"union value\"},\n"
            "    foo: {us: \"union value to skip\"},\n"
            "    bar.us: \"union value to skip\",\n"
            "    \"m\": 0.42\n,"
            "    \"n\": true,\n"
            "    \"o\": c\'.\',\n"
            "    \"p\": c\'\\041\',\n"
            "    \"q\": c\'\\x2A\',\n"
            "    \"r\": c\'\\u002B\',\n"
            "    \"s\": c\'\\t\'\n"
            "};\n"
            ;

        tstiop__my_struct_a__t json_sa_res = {
            .a = 42,
            .b = 50,
            .c_of_my_struct_a = 30,
            .d = 40,
            .e = 50,
            .f = 60,
            .g = 10 * 24 * 3600,
            .h = 1ULL << 40,
            .i = IOP_DATA((void *)"foo", 3),
            .j = LSTR_IMMED("bar"),
            .xml_field = LSTR_EMPTY,
            .k = MY_ENUM_A_B,
            .l = IOP_UNION(tstiop__my_union_a, us, LSTR_IMMED("union value")),
            .m = 0.42,
            .n = true,
            .o = '.',
            .p = '!',
            .q = '*',
            .r = '+',
            .s = '\t',
        };

        const char json_sf[] =
            "/* Json example */\n"
            "{\n"
            "    a = [ \"foo\", \"bar\", ];\n"
            "    b = [ \"Zm9vYmFy\", \"YmFyZm9v\", ];\n"
            "    c = [ @a 10 {\n"
            "       b = [ 1w, 1d, 1h, 1m, 1s, 1G, 1M, 1K, ];\n"
            "    }];\n"
            "    d = [ .us: \"foo\", .ub: true ];\n"
            "};;;\n"
            ;

        const char json_sf2[] =
            "/* Json example */\n"
            "{\n"
            "    a = [ \"foo\", \"bar\", ];\n"
            "    b = [ \"Zm9vYmFy\", \"YmFyZm9v\", ];\n"
            "    c = [ @a 10 {\n"
            "       b = [ 1w, 1d, 1h, 1m, 1s, 1G, 1M, 1K, ];\n"
            "    }];\n"
            "    d = [ {us: \"foo\"}, {ub: true} ];\n"
            "};;;\n"
            ;

        lstr_t avals[] = {
            LSTR_IMMED("foo"),
            LSTR_IMMED("bar"),
        };

        iop_data_t bvals[] = {
            IOP_DATA((void *)"foobar", 6),
            IOP_DATA((void *)"barfoo", 6),
        };

        int b2vals[] = { 86400*7, 86400, 3600, 60, 1, 1<<30, 1<<20, 1<<10 };

        tstiop__my_struct_b__t cvals[] = {
            {
                .a = IOP_OPT(10),
                .b = IOP_ARRAY(b2vals, countof(b2vals)),
            }
        };

        tstiop__my_union_a__t dvals[] = {
            IOP_UNION(tstiop__my_union_a, us, LSTR_IMMED("foo")),
            IOP_UNION(tstiop__my_union_a, ub, true),
        };

        const tstiop__my_struct_f__t json_sf_res = {
            .a = IOP_ARRAY(avals, countof(avals)),
            .b = IOP_ARRAY(bvals, countof(bvals)),
            .c = IOP_ARRAY(cvals, countof(cvals)),
            .d = IOP_ARRAY(dvals, countof(dvals)),
        };

#define xstr(...) str(__VA_ARGS__)
#define str(...)  #__VA_ARGS__

#define IVALS -1,0x10,2
#define DVALS .5, 0.5, 5.5, 0.2e2
#define EVALS 2,3,4

        const char json_si[] =
            "/* Json example */\n"
            "{\n"
            "    i = [ " xstr(IVALS) " ];\n"
            "    d = [ " xstr(DVALS) " ];\n"
            "    e = [ " xstr(EVALS) " ];\n"
            "};;;\n"
            ;

        int                     i_ivals[] = { IVALS };
        double                  i_dvals[] = { DVALS };
        tstiop__my_enum_c__t    i_evals[] = { EVALS };
        const tstiop__my_struct_i__t json_si_res = {
            .i = IOP_ARRAY(i_ivals, countof(i_ivals)),
            .d = IOP_ARRAY(i_dvals, countof(i_dvals)),
            .e = IOP_ARRAY(i_evals, countof(i_evals)),
        };


        iop_dso_t *dso;
        lstr_t path = t_lstr_cat(z_cmddir_g,
                                 LSTR_IMMED_V("zchk-tstiop-plugin.so"));


        const iop_struct_t *st_sa, *st_sf, *st_si;

        if ((dso = iop_dso_open(path.s)) == NULL)
            Z_SKIP("unable to load zchk-tstiop-plugin, TOOLS repo?");

        Z_ASSERT_P(st_sa = iop_dso_find_type(dso, LSTR_IMMED_V("tstiop.MyStructA")));
        Z_ASSERT_P(st_sf = iop_dso_find_type(dso, LSTR_IMMED_V("tstiop.MyStructF")));
        Z_ASSERT_P(st_si = iop_dso_find_type(dso, LSTR_IMMED_V("tstiop.MyStructI")));

        /* test packing/unpacking */
        Z_HELPER_RUN(iop_json_test_struct(st_sa, &sa,  "sa"));
        Z_HELPER_RUN(iop_json_test_struct(st_sa, &sa2, "sa2"));

        /* test unpacking */
        Z_HELPER_RUN(iop_json_test_json(st_sa, json_sa,  &json_sa_res,
                                        "json_sa"));
        Z_HELPER_RUN(iop_json_test_json(st_sa, json_sa2, &json_sa_res,
                                        "json_sa2"));
        Z_HELPER_RUN(iop_json_test_json(st_sf, json_sf,  &json_sf_res,
                                        "json_sf"));
        Z_HELPER_RUN(iop_json_test_json(st_sf, json_sf2, &json_sf_res,
                                        "json_sf2"));
        Z_HELPER_RUN(iop_json_test_json(st_si, json_si,  &json_si_res,
                                        "json_si"));

        iop_dso_close(&dso);
    } Z_TEST_END
    /* }}} */
    Z_TEST(std, "test IOP std (un)packer") { /* {{{ */
        t_scope;

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
            .j = CLSTR_IMMED("baré© \" foo ."),
            .k = MY_ENUM_A_B,
            .l = IOP_UNION(tstiop__my_union_a, ub, 42),
            .m = 3.14159265,
            .n = true,
        };

        tstiop__my_struct_a__t sa2 = {
            .a = 42,
            .b = 5,
            .c_of_my_struct_a = 120,
            .d = 230,
            .e = 540,
            .f = 2000,
            .g = 10000,
            .h = 20000,
            .i = IOP_DATA_EMPTY,
            .j = CLSTR_EMPTY,
            .k = MY_ENUM_A_B,
            .l = IOP_UNION(tstiop__my_union_a, ub, 42),
            .m = 3.14159265,
            .n = true,
        };
        tstiop__my_struct_a_opt__t sa_opt;

        iop_dso_t *dso;
        lstr_t path = t_lstr_cat(z_cmddir_g,
                                 LSTR_IMMED_V("zchk-tstiop-plugin.so"));


        const iop_struct_t *st_sa, *st_sa_opt;


        if ((dso = iop_dso_open(path.s)) == NULL)
            Z_SKIP("unable to load zchk-tstiop-plugin, TOOLS repo?");

        Z_ASSERT_P(st_sa = iop_dso_find_type(dso, LSTR_IMMED_V("tstiop.MyStructA")));
        Z_ASSERT_P(st_sa_opt = iop_dso_find_type(dso, LSTR_IMMED_V("tstiop.MyStructAOpt")));

        Z_HELPER_RUN(iop_std_test_struct(st_sa, &sa,  "sa"));
        Z_HELPER_RUN(iop_std_test_struct(st_sa, &sa2, "sa2"));

        iop_init(st_sa_opt, &sa_opt);
        IOP_OPT_SET(sa_opt.a, 32);
        sa_opt.j = LSTR_IMMED_V("foo");
        Z_HELPER_RUN(iop_std_test_struct(st_sa_opt, &sa_opt, "sa_opt"));

        iop_dso_close(&dso);
    } Z_TEST_END
    /* }}} */
    Z_TEST(roptimized, "test IOP std: optimized repeated fields") { /* {{{ */
        t_scope;

        tstiop__repeated__t sr;
        const iop_struct_t *st;

        int8_t   *i8;
        uint8_t  *u8;
        bool     *b;
        int16_t  *i16;
        uint16_t *u16;
        int32_t  *i32;

        lstr_t s[] = {
            LSTR_IMMED("foo"),
            LSTR_IMMED("bar"),
            LSTR_IMMED("foobar"),
        };

        iop_dso_t *dso;
        lstr_t path = t_lstr_cat(z_cmddir_g,
                                 LSTR_IMMED_V("zchk-tstiop-plugin.so"));
        unsigned seed = (unsigned)time(NULL);



        if ((dso = iop_dso_open(path.s)) == NULL)
            Z_SKIP("unable to load zchk-tstiop-plugin, TOOLS repo?");

        Z_ASSERT_P(st = iop_dso_find_type(dso, LSTR_IMMED_V("tstiop.Repeated")));

        /* initialize my arrays */
        {
            const int sz = 256;

            i8  = t_new_raw(int8_t, sz);
            u8  = t_new_raw(uint8_t, sz);
            i16 = t_new_raw(int16_t, sz);
            u16 = t_new_raw(uint16_t, sz);
            b   = t_new_raw(bool, sz);
            i32 = t_new_raw(int32_t, sz);

            for (int i = 0; i < sz; i++) {
                i8[i]  = (int8_t)i;
                u8[i]  = (uint8_t)i;
                i16[i] = (int16_t)i;
                u16[i] = (uint16_t)i;
                b[i]   = (bool)i;
                i32[i] = i;
            }
        }

        /* do some tests… */
#define SET(dst, f, _len)  ({ dst.f.tab = f; dst.f.len = (_len); })
#define SET_RAND(dst, f)   ({ dst.f.tab = f; dst.f.len = (rand() % 256); })
        iop_init(st, &sr);
        SET(sr, i8, 13);
        Z_HELPER_RUN(iop_std_test_struct(st, &sr,  "sr1"));

        iop_init(st, &sr);
        SET(sr, i8, 13);
        SET(sr, i32, 4);
        Z_HELPER_RUN(iop_std_test_struct(st, &sr,  "sr2"));

        srand(seed);
        e_trace(1, "rand seed: %u", seed);
        for (int i = 0; i < 256; i++ ) {
            iop_init(st, &sr);
            SET_RAND(sr, i8);
            SET_RAND(sr, u8);
            SET_RAND(sr, i16);
            SET_RAND(sr, u16);
            SET_RAND(sr, b);
            SET_RAND(sr, i32);
            SET(sr, s, rand() % (countof(s) + 1));
            Z_HELPER_RUN(iop_std_test_struct(st, &sr,  "sr_rand"));
        }

        /* Check the retro-compatibility */
        {
            void *map;
            int fd;
            struct stat sts;
            pstream_t ps;

            path = t_lstr_cat(z_cmddir_g,
                              LSTR_IMMED_V("samples/repeated.ibp"));

            /* map our data file */
            Z_ASSERT_N(fd = open(path.s, O_RDONLY), "open failed: %m");
            Z_ASSERT_N(fstat(fd, &sts), "fstat failed: %m");
            Z_ASSERT_GT(sts.st_size, 0);

            map = mmap(NULL, sts.st_size, PROT_READ, MAP_SHARED, fd, 0);
            Z_ASSERT(map != MAP_FAILED, "mmap failed: %m");
            close(fd);

            /* check the data */
            ps = ps_init(map, sts.st_size);
            while (ps_len(&ps) > 0) {
                t_scope;
                uint32_t dlen = 0;
                tstiop__repeated__t sr_res;

                Z_ASSERT_N(ps_get_cpu32(&ps, &dlen));
                Z_ASSERT(ps_has(&ps, dlen));

                iop_init(st, &sr);
                Z_ASSERT_N(iop_bunpack(t_pool(), st, &sr_res,
                                       __ps_get_ps(&ps, dlen), false),
                           "IOP unpacking error (%s) at offset %zu",
                           st->fullname.s, ps.b - (byte *)map);
            }

            munmap(map, sts.st_size);
        }

        iop_dso_close(&dso);
#undef SET
    } Z_TEST_END
    /* }}} */
    Z_TEST(defval, "test IOP std: do not pack default values") { /* {{{ */
        t_scope;

        tstiop__my_struct_g__t sg;

        iop_dso_t *dso;
        lstr_t path = t_lstr_cat(z_cmddir_g,
                                 LSTR_IMMED_V("zchk-tstiop-plugin.so"));


        const iop_struct_t *st_sg;
        qv_t(i32) szs;
        int len;

        if ((dso = iop_dso_open(path.s)) == NULL)
            Z_SKIP("unable to load zchk-tstiop-plugin, TOOLS repo?");

        Z_ASSERT_P(st_sg = iop_dso_find_type(dso, LSTR_IMMED_V("tstiop.MyStructG")));

        t_qv_init(i32, &szs, 1024);

        /* test with all the default values */
        iop_init(st_sg, &sg);
        Z_ASSERT_EQ((len = iop_bpack_size(st_sg, &sg, &szs)), 0, "sg-empty");
        Z_HELPER_RUN(iop_std_test_struct(st_sg, &sg,  "sg-empty"));

        /* test with a different string length */
        sg.j.len = sg.j.len - 1;
        Z_ASSERT_EQ((len = iop_bpack_size(st_sg, &sg, &szs)), 15,
                    "sg-string-len-diff");
        Z_HELPER_RUN(iop_std_test_struct(st_sg, &sg,  "sg-string-len-diff"));

        /* test with a NULL string */
        sg.j = LSTR_NULL_V;
        Z_ASSERT_EQ((len = iop_bpack_size(st_sg, &sg, &szs)), 0,
                    "sg-string-null");

        /* test with a different string */
        sg.j = LSTR_IMMED_V("plop");
        Z_ASSERT_EQ((len = iop_bpack_size(st_sg, &sg, &szs)), 7,
                    "sg-string-diff");
        Z_HELPER_RUN(iop_std_test_struct(st_sg, &sg,  "sg-string-diff"));

        /* test with different values at different places */
        sg.a = 42;
        sg.f = 12;
        sg.l = 10.6;
        Z_ASSERT_EQ((len = iop_bpack_size(st_sg, &sg, &szs)), 20, "sg-diff");
        Z_HELPER_RUN(iop_std_test_struct(st_sg, &sg,  "sg-diff"));

        iop_dso_close(&dso);
    } Z_TEST_END
    /* }}} */
    Z_TEST(equals, "test iop_equals()") { /* {{{ */
        t_scope;

        tstiop__my_struct_g__t sg_a, sg_b;
        tstiop__my_struct_a_opt__t sa_opt_a, sa_opt_b;
        tstiop__my_union_a__t ua_a, ua_b;
        tstiop__repeated__t sr_a, sr_b;

        iop_dso_t *dso;
        lstr_t path = t_lstr_cat(z_cmddir_g,
                                 LSTR_IMMED_V("zchk-tstiop-plugin.so"));


        const iop_struct_t *st_sg, *st_sa_opt, *st_ua, *st_sr;

        if ((dso = iop_dso_open(path.s)) == NULL)
            Z_SKIP("unable to load zchk-tstiop-plugin, TOOLS repo?");

        Z_ASSERT_P(st_sg = iop_dso_find_type(dso, LSTR_IMMED_V("tstiop.MyStructG")));
        Z_ASSERT_P(st_sr = iop_dso_find_type(dso, LSTR_IMMED_V("tstiop.Repeated")));
        Z_ASSERT_P(st_sa_opt = iop_dso_find_type(dso, LSTR_IMMED_V("tstiop.MyStructAOpt")));
        Z_ASSERT_P(st_ua = iop_dso_find_type(dso, LSTR_IMMED_V("tstiop.MyUnionA")));

        /* Test with all the default values */
        iop_init(st_sg, &sg_a);
        iop_init(st_sg, &sg_b);
        Z_ASSERT(iop_equals(st_sg, &sg_a, &sg_b));

        /* Change some fields and test */
        sg_a.b++;
        Z_ASSERT(!iop_equals(st_sg, &sg_a, &sg_b));

        sg_a.b--;
        sg_b.j = LSTR_IMMED_V("not equal");
        Z_ASSERT(!iop_equals(st_sg, &sg_a, &sg_b));

        /* Use a more complex structure */
        iop_init(st_sa_opt, &sa_opt_a);
        iop_init(st_sa_opt, &sa_opt_b);
        Z_ASSERT(iop_equals(st_sa_opt, &sa_opt_a, &sa_opt_b));

        IOP_OPT_SET(sa_opt_a.a, 42);
        IOP_OPT_SET(sa_opt_b.a, 42);
        sa_opt_a.j = LSTR_IMMED_V("plop");
        sa_opt_b.j = LSTR_IMMED_V("plop");
        Z_ASSERT(iop_equals(st_sa_opt, &sa_opt_a, &sa_opt_b));

        IOP_OPT_CLR(sa_opt_b.a);
        Z_ASSERT(!iop_equals(st_sa_opt, &sa_opt_a, &sa_opt_b));

        IOP_OPT_SET(sa_opt_b.a, 42);
        sa_opt_b.j = LSTR_NULL_V;
        Z_ASSERT(!iop_equals(st_sa_opt, &sa_opt_a, &sa_opt_b));

        sa_opt_b.j = LSTR_IMMED_V("plop2");
        Z_ASSERT(!iop_equals(st_sa_opt, &sa_opt_a, &sa_opt_b));

        sa_opt_b.j = LSTR_IMMED_V("plop");
        ua_a = IOP_UNION(tstiop__my_union_a, ua, 1);
        ua_b = IOP_UNION(tstiop__my_union_a, ua, 1);
        sa_opt_a.l = &ua_a;
        sa_opt_b.l = &ua_b;
        Z_ASSERT(iop_equals(st_sa_opt, &sa_opt_a, &sa_opt_b));

        sa_opt_b.l = NULL;
        Z_ASSERT(!iop_equals(st_sa_opt, &sa_opt_a, &sa_opt_b));

        ua_b = IOP_UNION(tstiop__my_union_a, ub, 1);
        sa_opt_b.l = &ua_b;
        Z_ASSERT(!iop_equals(st_sa_opt, &sa_opt_a, &sa_opt_b));

        /* test with non initialized optional fields values */
        iop_init(st_sa_opt, &sa_opt_a);
        iop_init(st_sa_opt, &sa_opt_b);
        sa_opt_a.a.v = 42;
        Z_ASSERT(iop_equals(st_sa_opt, &sa_opt_a, &sa_opt_b));

        /* Now test with some arrays */
        {
            lstr_t strs[] = { LSTR_IMMED("a"), LSTR_IMMED("b") };
            uint8_t uints[] = { 1, 2, 3, 4 };
            uint8_t uints2[] = { 1, 2, 4, 4 };

            iop_init(st_sr, &sr_a);
            iop_init(st_sr, &sr_b);
            Z_ASSERT(iop_equals(st_sr, &sr_a, &sr_b));

            sr_a.s.tab = strs, sr_a.s.len = countof(strs);
            sr_b.s.tab = strs, sr_b.s.len = countof(strs);
            sr_a.u8.tab = uints, sr_a.u8.len = countof(uints);
            sr_b.u8.tab = uints, sr_b.u8.len = countof(uints);
            Z_ASSERT(iop_equals(st_sr, &sr_a, &sr_b));

            sr_b.s.len--;
            Z_ASSERT(!iop_equals(st_sr, &sr_a, &sr_b));
            sr_b.s.len++;

            sr_b.u8.len--;
            Z_ASSERT(!iop_equals(st_sr, &sr_a, &sr_b));
            sr_b.u8.len++;

            sr_b.u8.tab = uints2;
            Z_ASSERT(!iop_equals(st_sr, &sr_a, &sr_b));
        }

        iop_dso_close(&dso);
    } Z_TEST_END
    /* }}} */
} Z_GROUP_END
