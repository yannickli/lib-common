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
    int ret;

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

    Z_ASSERT_N(xmlr_setup(&xmlr_g, sb.data, sb.len));
    ret = iop_xunpack(xmlr_g, t_pool(), st, res);
    Z_ASSERT_N(ret, "XML unpacking failure (%s, %s): %s", st->fullname.s,
               info, xmlr_get_err());

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

static int iop_xml_test_struct_invalid(const iop_struct_t *st, void *v,
                                       const char *info)
{
    byte *res;

    t_scope;
    SB_8k(sb);

    sb_adds(&sb, "<root xmlns:xsd=\"http://www.w3.org/2001/XMLSchema\" "
            "xmlns:xsi=\"http://www.w3.org/2001/XMLSchema-instance\">");
    iop_xpack(&sb, st, v, false, true);
    sb_adds(&sb, "</root>");

    /* unpacking */
    res = t_new(byte, ROUND_UP(st->size, 8));
    iop_init(st, res);

    Z_ASSERT_N(xmlr_setup(&xmlr_g, sb.data, sb.len));
    Z_ASSERT_NEG(iop_xunpack(xmlr_g, t_pool(), st, res),
                 "XML unpacking unexpected success (%s, %s)", st->fullname.s,
                 info);

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
    jll.flags = IOP_UNPACK_IGNORE_UNKNOWN;
    res = t_new_raw(byte, ROUND_UP(st->size, 8));

    while (strict < 3) {
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

__unused__
static int iop_json_test_struct_invalid(const iop_struct_t *st, void *v,
                                        const char *info)
{
    t_scope;
    byte *res;
    iop_json_lex_t jll;
    pstream_t ps;
    int strict = 0;
    SB_8k(sb);

    iop_jlex_init(t_pool(), &jll);
    jll.flags = IOP_UNPACK_IGNORE_UNKNOWN;
    res = t_new(byte, ROUND_UP(st->size, 8));

    while (strict < 3) {
        int ret;

        /* packing */
        sb_reset(&sb);
        Z_ASSERT_N(iop_jpack(st, v, iop_sb_write, &sb, strict),
                   "JSon packing failure! (%s, %s)", st->fullname.s, info);

        /* unpacking */
        iop_init(st, res);

        ps = ps_initsb(&sb);
        iop_jlex_attach(&jll, &ps);
        ret = iop_junpack(&jll, st, res, true);
        Z_ASSERT_NEG(ret, "JSon unpacking unexpected success (%s, %s)",
                     st->fullname.s, info);
        iop_jlex_detach(&jll);

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
    jll.flags = IOP_UNPACK_IGNORE_UNKNOWN;
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

static int iop_json_test_unpack(const iop_struct_t *st, const char *json,
                                bool valid, const char *info)
{
    t_scope;
    iop_json_lex_t jll;
    pstream_t ps;
    byte *res;
    int ret;
    SB_1k(sb);

    iop_jlex_init(t_pool(), &jll);
    jll.flags = IOP_UNPACK_IGNORE_UNKNOWN;
    res = t_new(byte, ROUND_UP(st->size, 8));

    iop_init(st, res);
    ps = ps_initstr(json);
    iop_jlex_attach(&jll, &ps);

    if ((ret = iop_junpack(&jll, st, res, true)) < 0)
        iop_jlex_write_error(&jll, &sb);
    if (valid) {
        Z_ASSERT_N(ret, "JSon unpacking error (%s, %s): %s", st->fullname.s,
                   info, sb.data);
    } else {
        Z_ASSERT_NEG(ret, "JSon unpacking unexpected success (%s, %s)",
                     st->fullname.s, info);
    }
    iop_jlex_detach(&jll);

    iop_jlex_wipe(&jll);

    Z_HELPER_END;
}

static int iop_std_test_struct_flags(const iop_struct_t *st, void *v,
                                     const unsigned flags, const char *info)
{
    t_scope;
    int ret;
    byte *res;
    uint8_t buf1[20], buf2[20];
    qv_t(i32) szs;
    int len;
    byte *dst;

    t_qv_init(i32, &szs, 1024);

    /* packing */
    Z_ASSERT_N((len = iop_bpack_size_flags(st, v, flags, &szs)),
               "invalid structure size (%s, %s)", st->fullname.s, info);
    dst = t_new(byte, len);
    iop_bpack(dst, st, v, szs.tab);

    /* unpacking */
    res = t_new(byte, ROUND_UP(st->size, 8));
    iop_init(st, res);

    ret = iop_bunpack(t_pool(), st, res, ps_init(dst, len), false);
    Z_ASSERT_N(ret, "IOP unpacking error (%s, %s, %s)", st->fullname.s, info,
               iop_get_err());

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
#define iop_std_test_struct(st, v, info) \
    iop_std_test_struct_flags(st, v, 0, info)

static int iop_std_test_struct_invalid(const iop_struct_t *st, void *v,
                                       const char *info)
{
    t_scope;
    byte *res;
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

    Z_ASSERT_NEG(iop_bunpack(t_pool(), st, res, ps_init(dst, len), false),
               "IOP unpacking unexpected success (%s, %s)", st->fullname.s,
               info);

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
#define feed_num(_num)                                                  \
        Z_ASSERT_N(iop_cfolder_feed_number(&cfolder, _num, true),       \
                   "error when feeding %jd", (int64_t)_num)
#define feed_op(_op)                                                    \
        Z_ASSERT_N(iop_cfolder_feed_operator(&cfolder, _op),            \
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

#define error()                                                         \
        do {                                                            \
            uint64_t cres;                                              \
                                                                        \
            Z_ASSERT_NEG(iop_cfolder_get_result(&cfolder, &cres));      \
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

        feed_num(1);
        feed_op('/');
        feed_num(0);
        error();

        feed_num(1);
        feed_op('%');
        feed_num(0);
        error();

        feed_num(INT64_MIN);
        feed_op('/');
        feed_num(-1);
        error();

        iop_cfolder_wipe(&cfolder);
#undef feed_num
#undef feed_op
#undef result
#undef error
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

        const iop_struct_t *st_se, *st_sa, *st_sf, *st_cs, *st_sa_opt;

        if ((dso = iop_dso_open(path.s)) == NULL)
            Z_SKIP("unable to load zchk-tstiop-plugin, TOOLS repo?");

        Z_ASSERT_P(st_se = iop_dso_find_type(dso, LSTR_IMMED_V("tstiop.MyStructE")));
        Z_ASSERT_P(st_sa = iop_dso_find_type(dso, LSTR_IMMED_V("tstiop.MyStructA")));
        Z_ASSERT_P(st_sf = iop_dso_find_type(dso, LSTR_IMMED_V("tstiop.MyStructF")));
        Z_ASSERT_P(st_cs = iop_dso_find_type(dso, LSTR_IMMED_V("tstiop.ConstraintS")));
        Z_ASSERT_P(st_sa_opt = iop_dso_find_type(dso, LSTR_IMMED_V("tstiop.MyStructAOpt")));

        /* We test that packing and unpacking of XML structures is stable */
        Z_HELPER_RUN(iop_xml_test_struct(st_se, &se, "se"));
        Z_HELPER_RUN(iop_xml_test_struct(st_sa, &sa, "sa"));
        Z_HELPER_RUN(iop_xml_test_struct(st_sf, &sf, "sf"));

        { /* IOP_XUNPACK_IGNORE_UNKNOWN */
            t_scope;
            tstiop__my_struct_f__t sf_ret;
            SB_1k(sb);

            sb_adds(&sb, "<root "
                    "xmlns:xsd=\"http://www.w3.org/2001/XMLSchema\" "
                    "xmlns:xsi=\"http://www.w3.org/2001/XMLSchema-instance\" "
                    ">\n");
            sb_adds(&sb,
                    "<unk1></unk1>"
                    "<a>foo</a><a>bar</a><a>foobar</a>"
                    "<b>VGVzdA==</b><b>Rm9v</b><b>QkFS</b>"
                    "<c><unk2>foo</unk2></c><c><a>55</a><unk3 /></c><c />"
                    "<c><a>55</a><b>2</b><unk3 /></c>"
                    "<unk4>foo</unk4>");
            sb_adds(&sb, "</root>\n");

            iop_init(st_sf, &sf_ret);
            Z_ASSERT_N(xmlr_setup(&xmlr_g, sb.data, sb.len));
            Z_ASSERT_NEG(iop_xunpack(xmlr_g, t_pool(), st_sf, &sf_ret),
                         "unexpected successful unpacking");
            xmlr_close(&xmlr_g);

            iop_init(st_sf, &sf_ret);
            Z_ASSERT_N(xmlr_setup(&xmlr_g, sb.data, sb.len));
            Z_ASSERT_N(iop_xunpack_flags(xmlr_g, t_pool(), st_sf, &sf_ret,
                                         IOP_UNPACK_IGNORE_UNKNOWN),
                       "unexpected unpacking failure using IGNORE_UNKNOWN");
            xmlr_close(&xmlr_g);
        }

        {
            t_scope;
            tstiop__my_struct_f__t sf_ret;
            SB_1k(sb);
            qm_t(part) parts;

            qm_init(part, &parts, true);
            qm_add(part, &parts, &LSTR_IMMED_V("foo"), LSTR_IMMED_V("part cid foo"));
            qm_add(part, &parts, &LSTR_IMMED_V("bar"), LSTR_IMMED_V("part cid bar"));

            sb_adds(&sb, "<root "
                    "xmlns:xsd=\"http://www.w3.org/2001/XMLSchema\" "
                    "xmlns:xsi=\"http://www.w3.org/2001/XMLSchema-instance\" "
                    ">\n");
            sb_adds(&sb,
                    "<a></a><a/><a>foo</a>"
                    "<a href=\'cid:foo\'/>"
                    "<a><inc:Include href=\'cid:bar\' xmlns:inc=\"url\" /></a>"
                    "<b>VGVzdA==</b>"
                    "<b href=\'cid:foo\'/>");
            sb_adds(&sb, "</root>\n");

            iop_init(st_sf, &sf_ret);
            Z_ASSERT_N(xmlr_setup(&xmlr_g, sb.data, sb.len));
            Z_ASSERT_NEG(iop_xunpack(xmlr_g, t_pool(), st_sf, &sf_ret),
                         "unexpected successful unpacking");
            xmlr_close(&xmlr_g);

            iop_init(st_sf, &sf_ret);
            Z_ASSERT_N(xmlr_setup(&xmlr_g, sb.data, sb.len));
            Z_ASSERT_N(iop_xunpack_parts(xmlr_g, t_pool(), st_sf, &sf_ret,
                                         0, &parts),
                       "unexpected unpacking failure with parts");
            xmlr_close(&xmlr_g);

            qm_wipe(part, &parts);
        }

        { /* Test numeric values */
            t_scope;
            tstiop__my_struct_a_opt__t sa_opt;
            SB_1k(sb);

            sb_adds(&sb, "<root "
                    "xmlns:xsd=\"http://www.w3.org/2001/XMLSchema\" "
                    "xmlns:xsi=\"http://www.w3.org/2001/XMLSchema-instance\" "
                    ">\n");
            sb_adds(&sb,
                    "<a>42</a>"
                    "<b>0x10</b>"
                    "<e>-42</e>"
                    "<f>0x42</f>");
            sb_adds(&sb, "</root>\n");

            iop_init(st_sa_opt, &sa_opt);
            Z_ASSERT_N(xmlr_setup(&xmlr_g, sb.data, sb.len));
            Z_ASSERT_N(iop_xunpack(xmlr_g, t_pool(), st_sa_opt, &sa_opt));
            xmlr_close(&xmlr_g);

            Z_ASSERT(IOP_OPT_ISSET(sa_opt.a));
            Z_ASSERT_EQ(IOP_OPT_VAL(sa_opt.a), 42);

            Z_ASSERT(IOP_OPT_ISSET(sa_opt.b));
            Z_ASSERT_EQ(IOP_OPT_VAL(sa_opt.b), 0x10U);

            Z_ASSERT(IOP_OPT_ISSET(sa_opt.e));
            Z_ASSERT_EQ(IOP_OPT_VAL(sa_opt.e), -42);

            Z_ASSERT(IOP_OPT_ISSET(sa_opt.f));
            Z_ASSERT_EQ(IOP_OPT_VAL(sa_opt.f), 0x42);
        }

        { /* Test PRIVATE */
            t_scope;
            tstiop__constraint_s__t cs;
            SB_1k(sb);
            byte *res;
            int ret;
            lstr_t strings[] = {
                LSTR_IMMED_V("foo5"),
                LSTR_IMMED_V("foo6"),
            };

            iop_init(st_cs, &cs);
            cs.s.tab = strings;
            cs.s.len = 2;
            Z_HELPER_RUN(iop_xml_test_struct(st_cs, &cs, "cs"));

            IOP_OPT_SET(cs.priv, true);
            cs.priv2 = false;
            Z_HELPER_RUN(iop_xml_test_struct(st_cs, &cs, "cs"));

            /* packing (private values should be skipped) */
            sb_adds(&sb, "<root xmlns:xsd=\"http://www.w3.org/2001/XMLSchema\" "
                    "xmlns:xsi=\"http://www.w3.org/2001/XMLSchema-instance\">");
            iop_xpack_flags(&sb, st_cs, &cs, IOP_XPACK_SKIP_PRIVATE);
            sb_adds(&sb, "</root>");

            Z_ASSERT_NULL(strstr(sb.data, "<priv>"));
            Z_ASSERT_NULL(strstr(sb.data, "<priv2>"));

            /* unpacking should work (private values are gone) */
            res = t_new(byte, ROUND_UP(st_cs->size, 8));
            iop_init(st_cs, res);

            Z_ASSERT_N(xmlr_setup(&xmlr_g, sb.data, sb.len));
            ret = iop_xunpack_flags(xmlr_g, t_pool(), st_cs, &cs,
                                    IOP_UNPACK_FORBID_PRIVATE);
            Z_ASSERT_N(ret, "XML unpacking failure (%s, %s): %s",
                       st_cs->fullname.s, "st_cs", xmlr_get_err());
            Z_ASSERT(!IOP_OPT_ISSET(cs.priv));
            Z_ASSERT(cs.priv2);

            /* now test that unpacking only works when private values are not
             * specified */
            sb_reset(&sb);
            sb_adds(&sb, "<root "
                    "xmlns:xsd=\"http://www.w3.org/2001/XMLSchema\" "
                    "xmlns:xsi=\"http://www.w3.org/2001/XMLSchema-instance\" "
                    ">\n");
            sb_adds(&sb,
                    "<s>abcd</s>"
                    "<s>abcd</s>");
            sb_adds(&sb, "</root>\n");

            iop_init(st_cs, &cs);
            Z_ASSERT_N(xmlr_setup(&xmlr_g, sb.data, sb.len));
            Z_ASSERT_N(iop_xunpack_flags(xmlr_g, t_pool(), st_cs, &cs,
                                         IOP_UNPACK_FORBID_PRIVATE));
            xmlr_close(&xmlr_g);

            sb_reset(&sb);
            sb_adds(&sb, "<root "
                    "xmlns:xsd=\"http://www.w3.org/2001/XMLSchema\" "
                    "xmlns:xsi=\"http://www.w3.org/2001/XMLSchema-instance\" "
                    ">\n");
            sb_adds(&sb,
                    "<s>abcd</s>"
                    "<s>abcd</s>"
                    "<priv>true</priv>");
            sb_adds(&sb, "</root>\n");

            iop_init(st_cs, &cs);
            Z_ASSERT_N(xmlr_setup(&xmlr_g, sb.data, sb.len));
            Z_ASSERT_NEG(iop_xunpack_flags(xmlr_g, t_pool(), st_cs, &cs,
                                           IOP_UNPACK_FORBID_PRIVATE));
            xmlr_close(&xmlr_g);

            sb_reset(&sb);
            sb_adds(&sb, "<root "
                    "xmlns:xsd=\"http://www.w3.org/2001/XMLSchema\" "
                    "xmlns:xsi=\"http://www.w3.org/2001/XMLSchema-instance\" "
                    ">\n");
            sb_adds(&sb,
                    "<s>abcd</s>"
                    "<s>abcd</s>"
                    "<priv2>true</priv2>");
            sb_adds(&sb, "</root>\n");

            iop_init(st_cs, &cs);
            Z_ASSERT_N(xmlr_setup(&xmlr_g, sb.data, sb.len));
            Z_ASSERT_NEG(iop_xunpack_flags(xmlr_g, t_pool(), st_cs, &cs,
                                           IOP_UNPACK_FORBID_PRIVATE));
            xmlr_close(&xmlr_g);
        }

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
            .p = '.',
            .q = '!',
            .r = '*',
            .s = '+',
            .t = '\t',
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
            .p = '.',
            .q = '!',
            .r = '*',
            .s = '+',
            .t = '\t',
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
            "    \"p\": c\'.\',\n"
            "    \"q\": c\'\\041\',\n"
            "    \"r\": c\'\\x2A\',\n"
            "    \"s\": c\'\\u002B\',\n"
            "    \"t\": c\'\\t\'\n"
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
            "    \"skipMe\": 42,\n"
            "    \"skipMe2\": null,\n"
            "    \"skipMe3\": { foo: [1, 2, 3, {bar: \"plop\"}] },\n"
            "    \"xmlField\": \"\",\n"
            "    \"k\": \"B\",\n"
            "    l: {us: \"union value\"},\n"
            "    foo: {us: \"union value to skip\"},\n"
            "    bar.us: \"union value to skip\",\n"
            "    \"m\": 0.42\n,"
            "    \"n\": true,\n"
            "    \"p\": c\'.\',\n"
            "    \"q\": c\'\\041\',\n"
            "    \"r\": c\'\\x2A\',\n"
            "    \"s\": c\'\\u002B\',\n"
            "    \"t\": c\'\\t\'\n"
            "};\n"
            "// last line contains a comment and no \\n"
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
            .p = '.',
            .q = '!',
            .r = '*',
            .s = '+',
            .t = '\t',
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

#define IVALS -1*10-(-10-1), 0x10|0x11, ((0x1f + 010)- 0X1E -5-8) *(2+2),   \
    0-1, ((0x1f + 010) - 0X1E - 5 - 8) * (2 +2),                            \
    ~0xffffffffffffff00 + POW(3, 4) - (1 << 2), (2 * 3 + 1) << 2,           \
    POW(2, (5+(-2))), 1, -1, +1, 1+1, -1+1, +1+1
#define DVALS .5, +.5, -.5, 0.5, +0.5, -0.5, 5.5, 0.2e2, 0x1P10
#define EVALS  EC(A), EC(A) | EC(B) | EC(C) | EC(D) | EC(E), (1 << 5) - 1

#define EC(s)       #s
#define POW(a,b)    a ** b
        const char json_si[] =
            "/* Json example */\n"
            "{\n"
            "    i = [ " xstr(IVALS) " ];\n"
            "    d = [ " xstr(DVALS) " ];\n"
            "    e = [ " xstr(EVALS) " ];\n"
            "};;;\n"
            ;

        const char json_si_p1[] = "{l = [ -0x7fffffffffffffff + (-1) ]; };" ;
        const char json_si_p2[] = "{u = [  0xffffffffffffffff +   0  ]; };" ;

        const char json_si_n1[] = "{l = [ -0x7fffffffffffffff + (-2) ]; };" ;
        const char json_si_n2[] = "{u = [  0xffffffffffffffff +   1  ]; };" ;
#undef EC
#undef POW

#define EC(s)       MY_ENUM_C_ ##s
#define POW(a,b)    (int)pow(a,b)
        int                     i_ivals[] = { IVALS };
        double                  i_dvals[] = { DVALS };
        tstiop__my_enum_c__t    i_evals[] = { EVALS };
#undef EC
#undef POW
        const tstiop__my_struct_i__t json_si_res = {
            .i = IOP_ARRAY(i_ivals, countof(i_ivals)),
            .d = IOP_ARRAY(i_dvals, countof(i_dvals)),
            .e = IOP_ARRAY(i_evals, countof(i_evals)),
        };

        const char json_sk[] =
            "/* Json example */\n"
            "{\n"
            "    j = @cval 2 { \n"
            "                  b.a.us = \"foo\";\n"
            "                  btab = [ .bval: 0xf + 1, .a.ua: 2*8 ];\n"
            "                };\n"
            "};;;\n"
            ;

        tstiop__my_union_b__t j_bvals[] = {
            IOP_UNION(tstiop__my_union_b, bval, 16),
            IOP_UNION(tstiop__my_union_b, a,
                      (IOP_UNION(tstiop__my_union_a, ua, 16))),
        };

        const tstiop__my_struct_k__t json_sk_res = {
            .j = {
                .cval   = 2,
                .b      = IOP_UNION(tstiop__my_union_b, a,
                                    IOP_UNION(tstiop__my_union_a, us,
                                              LSTR_IMMED_V("foo"))),
                .btab = IOP_ARRAY(j_bvals, countof(j_bvals)),
            },
        };

        const char json_sa_opt[] = "{ a: 42, o: null }";

        tstiop__my_struct_a_opt__t json_sa_opt_res = {
            .a = IOP_OPT(42),
        };


        iop_dso_t *dso;
        lstr_t path = t_lstr_cat(z_cmddir_g,
                                 LSTR_IMMED_V("zchk-tstiop-plugin.so"));


        const iop_struct_t *st_sa, *st_sf, *st_si, *st_sk, *st_sa_opt;

        if ((dso = iop_dso_open(path.s)) == NULL)
            Z_SKIP("unable to load zchk-tstiop-plugin, TOOLS repo?");

        Z_ASSERT_P(st_sa = iop_dso_find_type(dso, LSTR_IMMED_V("tstiop.MyStructA")));
        Z_ASSERT_P(st_sf = iop_dso_find_type(dso, LSTR_IMMED_V("tstiop.MyStructF")));
        Z_ASSERT_P(st_si = iop_dso_find_type(dso, LSTR_IMMED_V("tstiop.MyStructI")));
        Z_ASSERT_P(st_sk = iop_dso_find_type(dso, LSTR_IMMED_V("tstiop.MyStructK")));
        Z_ASSERT_P(st_sa_opt = iop_dso_find_type(dso, LSTR_IMMED_V("tstiop.MyStructAOpt")));

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
        Z_HELPER_RUN(iop_json_test_json(st_sk, json_sk,  &json_sk_res,
                                        "json_sk"));
        Z_HELPER_RUN(iop_json_test_json(st_sa_opt, json_sa_opt,
                                        &json_sa_opt_res, "json_sa_opt"));

        Z_HELPER_RUN(iop_json_test_unpack(st_si, json_si_p1, true,
                                          "json_si_p1"));
        Z_HELPER_RUN(iop_json_test_unpack(st_si, json_si_p2, true,
                                          "json_si_p2"));

        Z_HELPER_RUN(iop_json_test_unpack(st_si, json_si_n1, false,
                                          "json_si_n1"));
        Z_HELPER_RUN(iop_json_test_unpack(st_si, json_si_n2, false,
                                          "json_si_n2"));

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

        int32_t val[] = {15, 30, 45};
        tstiop__my_struct_e__t se = {
            .a = 10,
            .b = IOP_UNION(tstiop__my_union_a, ua, 42),
            .c = { .b = IOP_ARRAY(val, countof(val)), },
        };

        iop_dso_t *dso;
        lstr_t path = t_lstr_cat(z_cmddir_g,
                                 LSTR_IMMED_V("zchk-tstiop-plugin.so"));


        const iop_struct_t *st_sa, *st_sa_opt, *st_se;


        if ((dso = iop_dso_open(path.s)) == NULL)
            Z_SKIP("unable to load zchk-tstiop-plugin, TOOLS repo?");

        Z_ASSERT_P(st_sa = iop_dso_find_type(dso, LSTR_IMMED_V("tstiop.MyStructA")));
        Z_ASSERT_P(st_sa_opt = iop_dso_find_type(dso, LSTR_IMMED_V("tstiop.MyStructAOpt")));
        Z_ASSERT_P(st_se = iop_dso_find_type(dso, LSTR_IMMED_V("tstiop.MyStructE")));

        Z_ASSERT_N(iop_check_constraints(st_sa, &sa));
        Z_ASSERT_N(iop_check_constraints(st_sa, &sa2));

        Z_HELPER_RUN(iop_std_test_struct(st_sa, &sa,  "sa"));
        Z_HELPER_RUN(iop_std_test_struct(st_sa, &sa2, "sa2"));
        Z_HELPER_RUN(iop_std_test_struct(st_se, &se, "se"));

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
        lstr_t s;
        const unsigned flags = IOP_BPACK_SKIP_DEFVAL;

        if ((dso = iop_dso_open(path.s)) == NULL)
            Z_SKIP("unable to load zchk-tstiop-plugin, TOOLS repo?");

        Z_ASSERT_P(st_sg = iop_dso_find_type(dso, LSTR_IMMED_V("tstiop.MyStructG")));

        t_qv_init(i32, &szs, 1024);

        /* test with all the default values */
        iop_init(st_sg, &sg);
        Z_ASSERT_EQ((len = iop_bpack_size_flags(st_sg, &sg, flags, &szs)), 0,
                    "sg-empty");
        Z_HELPER_RUN(iop_std_test_struct_flags(st_sg, &sg, flags,
                                               "sg-empty"));

        /* check that t_iop_bpack returns LSTR_EMPTY_V and not LSTR_NULL_V */
        s = t_iop_bpack_struct_flags(st_sg, &sg, flags);
        Z_ASSERT_P(s.s);
        Z_ASSERT_ZERO(s.len);

        /* test with a different string length */
        sg.j.len = sg.j.len - 1;
        Z_ASSERT_EQ((len = iop_bpack_size_flags(st_sg, &sg, flags, &szs)), 15,
                    "sg-string-len-diff");
        Z_HELPER_RUN(iop_std_test_struct_flags(st_sg, &sg, flags,
                                               "sg-string-len-diff"));

        /* test with a NULL string */
        sg.j = LSTR_NULL_V;
        Z_ASSERT_EQ((len = iop_bpack_size_flags(st_sg, &sg, flags, &szs)), 0,
                    "sg-string-null");

        /* test with a different string */
        sg.j = LSTR_IMMED_V("plop");
        Z_ASSERT_EQ((len = iop_bpack_size_flags(st_sg, &sg, flags, &szs)), 7,
                    "sg-string-diff");
        Z_HELPER_RUN(iop_std_test_struct_flags(st_sg, &sg, flags,
                                               "sg-string-diff"));

        /* test with different values at different places */
        sg.a = 42;
        sg.f = 12;
        sg.l = 10.6;
        Z_ASSERT_EQ((len = iop_bpack_size_flags(st_sg, &sg, flags, &szs)), 20,
                    "sg-diff");
        Z_HELPER_RUN(iop_std_test_struct_flags(st_sg, &sg, flags, "sg-diff"));

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
    Z_TEST(strict_enum, "test IOP strict enum (un)packing") { /* {{{ */
        t_scope;

        tstiop__my_enum_b__t bvals[] = {
            MY_ENUM_B_A, MY_ENUM_B_B, MY_ENUM_B_C
        };

        tstiop__my_struct_l__t sl1 = {
            .a      = MY_ENUM_A_A,
            .b      = MY_ENUM_B_B,
            .btab   = IOP_ARRAY(bvals, countof(bvals)),
            .c      = MY_ENUM_C_A | MY_ENUM_C_B,
        };

        tstiop__my_struct_l__t sl2 = {
            .a      = 10,
            .b      = MY_ENUM_B_B,
            .btab   = IOP_ARRAY(bvals, countof(bvals)),
            .c      = MY_ENUM_C_A | MY_ENUM_C_B,
        };

        tstiop__my_struct_l__t sl3 = {
            .a      = MY_ENUM_A_A,
            .b      = 10,
            .btab   = IOP_ARRAY(bvals, countof(bvals)),
            .c      = MY_ENUM_C_A | MY_ENUM_C_B,
        };

        const char json_sl_p1[] =
            "{\n"
            "     a     = 1 << \"C\";               \n"
            "     b     = \"C\";                    \n"
            "     btab  = [ \"A\", \"B\", \"C\" ];  \n"
            "     c     = 1 << \"C\";               \n"
            "};\n";

        const char json_sl_n1[] =
            "{\n"
            "     a     = 1 << \"C\";               \n"
            "     b     = 1 << \"C\";               \n"
            "     btab  = [ \"A\", \"B\", \"C\" ];  \n"
            "     c     = 1 << \"C\";               \n"
            "};\n";

        iop_dso_t *dso;
        lstr_t path = t_lstr_cat(z_cmddir_g,
                                 LSTR_IMMED_V("zchk-tstiop-plugin.so"));

        const iop_struct_t *st_sl;

        if ((dso = iop_dso_open(path.s)) == NULL)
            Z_SKIP("unable to load zchk-tstiop-plugin, TOOLS repo?");

        Z_ASSERT_P(st_sl = iop_dso_find_type(dso, LSTR_IMMED_V("tstiop.MyStructL")));

        Z_ASSERT_N(iop_check_constraints(st_sl, &sl1));
        Z_ASSERT_N(tstiop__my_struct_l__check(&sl2));
        Z_ASSERT_NEG(iop_check_constraints(st_sl, &sl3));
        e_trace(1, "%s", iop_get_err());

        Z_HELPER_RUN(iop_std_test_struct(st_sl, &sl1, "sl1"));
        Z_HELPER_RUN(iop_std_test_struct(st_sl, &sl2, "sl2"));
        Z_HELPER_RUN(iop_std_test_struct_invalid(st_sl, &sl3, "sl3"));

        Z_HELPER_RUN(iop_xml_test_struct(st_sl, &sl1, "sl1"));
        Z_HELPER_RUN(iop_xml_test_struct(st_sl, &sl2, "sl2"));
        Z_HELPER_RUN(iop_xml_test_struct_invalid(st_sl, &sl3, "sl3"));

        Z_HELPER_RUN(iop_json_test_unpack(st_sl, json_sl_p1, true,
                                          "json_sl_p1"));
        Z_HELPER_RUN(iop_json_test_unpack(st_sl, json_sl_n1, false,
                                          "json_sl_n1"));

        iop_dso_close(&dso);
    } Z_TEST_END
    /* }}} */
    Z_TEST(constraints, "test IOP constraints") { /* {{{ */
        t_scope;

        tstiop__constraint_u__t u;
        tstiop__constraint_s__t s;

        lstr_t strings[] = {
            LSTR_IMMED_V("fooBAR_1"),
            LSTR_IMMED_V("foobar_2"),
            LSTR_IMMED_V("foo3"),
            LSTR_IMMED_V("foo4"),
            LSTR_IMMED_V("foo5"),
            LSTR_IMMED_V("foo6"),
        };

        lstr_t bad_strings[] = {
            LSTR_IMMED_V("abcd[]"),
            LSTR_IMMED_V("a b c"),
        };

        int8_t   i8tab[] = { INT8_MIN,  INT8_MAX,  3, 4, 5, 6 };
        int16_t i16tab[] = { INT16_MIN, INT16_MAX, 3, 4, 5, 6 };
        int32_t i32tab[] = { INT32_MIN, INT32_MAX, 3, 4, 5, 6 };
        int64_t i64tab[] = { INT64_MIN, INT64_MAX, 3, 4, 5, 6 };

        iop_dso_t *dso;
        lstr_t path = t_lstr_cat(z_cmddir_g,
                                 LSTR_IMMED_V("zchk-tstiop-plugin.so"));

        const iop_struct_t *st_s, *st_u;

        if ((dso = iop_dso_open(path.s)) == NULL)
            Z_SKIP("unable to load zchk-tstiop-plugin, TOOLS repo?");

        Z_ASSERT_P(st_s = iop_dso_find_type(dso, LSTR_IMMED_V("tstiop.ConstraintS")));
        Z_ASSERT_P(st_u = iop_dso_find_type(dso, LSTR_IMMED_V("tstiop.ConstraintU")));

        tstiop__constraint_u__init(&u);
        tstiop__constraint_s__init(&s);

#define CHECK_VALID(st, v, info) \
        Z_ASSERT_N(iop_check_constraints((st), (v)));                   \
        Z_HELPER_RUN(iop_std_test_struct((st), (v), (info)));           \
        Z_HELPER_RUN(iop_xml_test_struct((st), (v), (info)));           \
        Z_HELPER_RUN(iop_json_test_struct((st), (v), (info)));          \

#define CHECK_INVALID(st, v, info) \
        Z_ASSERT_NEG(iop_check_constraints((st), (v)));                 \
        Z_HELPER_RUN(iop_std_test_struct_invalid((st), (v), (info)));   \
        Z_HELPER_RUN(iop_xml_test_struct_invalid((st), (v), (info)));   \
        Z_HELPER_RUN(iop_json_test_struct_invalid((st), (v), (info)));  \

#define CHECK_UNION(f, size) \
        u = IOP_UNION(tstiop__constraint_u, f, 1L << (size - 1));           \
        CHECK_VALID(st_u, &u, #f);                                          \
        u = IOP_UNION(tstiop__constraint_u, f, 1 + (1L << (size - 1)));     \
        CHECK_INVALID(st_u, &u, #f "_max");                                 \
        u = IOP_UNION(tstiop__constraint_u, f, 0);                          \
        CHECK_INVALID(st_u, &u, #f "_zero");                                \

        CHECK_UNION(u8,   8);
        CHECK_UNION(u16, 16);
        CHECK_UNION(u32, 32);
        CHECK_UNION(u64, 64);

        u = IOP_UNION(tstiop__constraint_u, s, LSTR_EMPTY_V);
        CHECK_INVALID(st_u, &u, "s_empty");
        u = IOP_UNION(tstiop__constraint_u, s, LSTR_NULL_V);
        CHECK_INVALID(st_u, &u, "s_null");
        u = IOP_UNION(tstiop__constraint_u, s, LSTR_IMMED_V("way_too_long"));
        CHECK_INVALID(st_u, &u, "s_maxlength");
        u = IOP_UNION(tstiop__constraint_u, s, LSTR_IMMED_V("ab.{}[]"));
        CHECK_INVALID(st_u, &u, "s_pattern");
        u = IOP_UNION(tstiop__constraint_u, s, LSTR_IMMED_V("ab.{}()"));
        CHECK_VALID(st_u, &u, "s");

        CHECK_INVALID(st_s, &s, "s_minoccurs");

        s.s.tab = bad_strings;
        s.s.len = countof(bad_strings);
        CHECK_INVALID(st_s, &s, "s_pattern");

        s.s.tab = strings;
        s.s.len = 1;
        CHECK_INVALID(st_s, &s, "s_minoccurs");
        s.s.len = countof(strings);
        CHECK_INVALID(st_s, &s, "s_maxoccurs");
        s.s.len = 2;
        CHECK_VALID(st_s, &s, "s");
        s.s.len = 5;
        CHECK_VALID(st_s, &s, "s");

#define CHECK_TAB(_f, _tab) \
        s._f.tab = _tab;                  \
        s._f.len = 6;                    \
        CHECK_INVALID(st_s, &s, "s");   \
        s._f.len = 5;                    \
        CHECK_INVALID(st_s, &s, "s");   \
        s._f.tab[0]++;                   \
        CHECK_VALID(st_s, &s, "s");     \

        CHECK_TAB(i8,   i8tab);
        CHECK_TAB(i16,  i16tab);
        CHECK_TAB(i32,  i32tab);
        CHECK_TAB(i64,  i64tab);

#undef CHECK_TAB
#undef CHECK_UNION
#undef CHECK_VALID
#undef CHECK_INVALID

        iop_dso_close(&dso);
    } Z_TEST_END
    /* }}} */
    Z_TEST(iop_sort, "test IOP structures/unions sorting") { /* {{{ */
        qv_t(my_struct_a) vec;
        tstiop__my_struct_a__t a;
        qv_t(my_struct_a_opt) vec2;
        tstiop__my_struct_a_opt__t a2;
        tstiop__my_struct_b__t b1, b2;
        tstiop__my_struct_m__t m;
        qv_t(my_struct_m)  mvec;

        qv_init(my_struct_a, &vec);
        tstiop__my_struct_a__init(&a);

        a.e = 1;
        a.j = LSTR_IMMED_V("xyz");
        a.l = IOP_UNION(tstiop__my_union_a, ua, 111);
        qv_append(my_struct_a, &vec, a);
        a.e = 2;
        a.j = LSTR_IMMED_V("abc");
        a.l = IOP_UNION(tstiop__my_union_a, ua, 666);
        qv_append(my_struct_a, &vec, a);
        a.e = 3;
        a.j = LSTR_IMMED_V("Jkl");
        a.l = IOP_UNION(tstiop__my_union_a, ua, 222);
        qv_append(my_struct_a, &vec, a);
        a.e = 3;
        a.j = LSTR_IMMED_V("jKl");
        a.l = IOP_UNION(tstiop__my_union_a, ub, 23);
        qv_append(my_struct_a, &vec, a);
        a.e = 3;
        a.j = LSTR_IMMED_V("jkL");
        a.l = IOP_UNION(tstiop__my_union_a, ub, 42);
        qv_append(my_struct_a, &vec, a);

#define TST_SORT_VEC(p, f)  tstiop__my_struct_a__sort(vec.tab, vec.len, p, f)

        /* reverse sort on short e */
        Z_ASSERT_N(TST_SORT_VEC(LSTR_IMMED_V("e"), IOP_SORT_REVERSE));
        Z_ASSERT_EQ(vec.tab[0].e, 3);
        Z_ASSERT_EQ(vec.tab[4].e, 1);

        /* sort on string j */
        Z_ASSERT_N(TST_SORT_VEC(LSTR_IMMED_V("j"), 0));
        Z_ASSERT_LSTREQUAL(vec.tab[0].j, LSTR_IMMED_V("abc"));
        Z_ASSERT_LSTREQUAL(vec.tab[4].j, LSTR_IMMED_V("xyz"));

        /* sort on union l */
        Z_ASSERT_N(TST_SORT_VEC(LSTR_IMMED_V("l"), 0));
        Z_ASSERT_P(IOP_UNION_GET(tstiop__my_union_a, &vec.tab[0].l, ua));
        Z_ASSERT_P(IOP_UNION_GET(tstiop__my_union_a, &vec.tab[4].l, ub));

        /* sort on int ua, member of union l */
        Z_ASSERT_N(TST_SORT_VEC(LSTR_IMMED_V("l.ua"), 0));
        Z_ASSERT_P(IOP_UNION_GET(tstiop__my_union_a, &vec.tab[0].l, ua));
        Z_ASSERT_P(IOP_UNION_GET(tstiop__my_union_a, &vec.tab[1].l, ua));
        Z_ASSERT_P(IOP_UNION_GET(tstiop__my_union_a, &vec.tab[2].l, ua));
        Z_ASSERT_EQ(vec.tab[0].l.ua, 111);
        Z_ASSERT_EQ(vec.tab[1].l.ua, 222);
        Z_ASSERT_EQ(vec.tab[2].l.ua, 666);

        /* reverse sort on int ua, member of union l */
        Z_ASSERT_N(TST_SORT_VEC(LSTR_IMMED_V("l.ua"), IOP_SORT_REVERSE));
        Z_ASSERT_P(IOP_UNION_GET(tstiop__my_union_a, &vec.tab[0].l, ua));
        Z_ASSERT_P(IOP_UNION_GET(tstiop__my_union_a, &vec.tab[1].l, ua));
        Z_ASSERT_P(IOP_UNION_GET(tstiop__my_union_a, &vec.tab[2].l, ua));
        Z_ASSERT_EQ(vec.tab[0].l.ua, 666);
        Z_ASSERT_EQ(vec.tab[1].l.ua, 222);
        Z_ASSERT_EQ(vec.tab[2].l.ua, 111);

        /* sort on int ua, member of union l, put other union members first */
        Z_ASSERT_N(TST_SORT_VEC(LSTR_IMMED_V("l.ua"), IOP_SORT_NULL_FIRST));
        Z_ASSERT_P(IOP_UNION_GET(tstiop__my_union_a, &vec.tab[0].l, ub));
        Z_ASSERT_P(IOP_UNION_GET(tstiop__my_union_a, &vec.tab[1].l, ub));
        Z_ASSERT_EQ(vec.tab[2].l.ua, 111);
        Z_ASSERT_EQ(vec.tab[3].l.ua, 222);
        Z_ASSERT_EQ(vec.tab[4].l.ua, 666);

        /* reverse sort on int ua, member of union l, put other union members
         * first */
        Z_ASSERT_N(TST_SORT_VEC(LSTR_IMMED_V("l.ua"),
                                IOP_SORT_NULL_FIRST | IOP_SORT_REVERSE));
        Z_ASSERT_P(IOP_UNION_GET(tstiop__my_union_a, &vec.tab[0].l, ub));
        Z_ASSERT_P(IOP_UNION_GET(tstiop__my_union_a, &vec.tab[1].l, ub));
        Z_ASSERT_EQ(vec.tab[2].l.ua, 666);
        Z_ASSERT_EQ(vec.tab[3].l.ua, 222);
        Z_ASSERT_EQ(vec.tab[4].l.ua, 111);

        /* sort on byte ub, member of union l, put other union members first */
        Z_ASSERT_N(TST_SORT_VEC(LSTR_IMMED_V("l.ub"), IOP_SORT_NULL_FIRST));
        Z_ASSERT_P(IOP_UNION_GET(tstiop__my_union_a, &vec.tab[0].l, ua));
        Z_ASSERT_P(IOP_UNION_GET(tstiop__my_union_a, &vec.tab[1].l, ua));
        Z_ASSERT_P(IOP_UNION_GET(tstiop__my_union_a, &vec.tab[2].l, ua));
        Z_ASSERT_EQ(vec.tab[3].l.ua, 23);
        Z_ASSERT_EQ(vec.tab[4].l.ua, 42);

        /* error: empty field path */
        Z_ASSERT_NEG(TST_SORT_VEC(LSTR_IMMED_V(""), 0));
        /* error: invalid field path */
        Z_ASSERT_NEG(TST_SORT_VEC(LSTR_IMMED_V("."), 0));
        /* error: bar field does not exist */
        Z_ASSERT_NEG(TST_SORT_VEC(LSTR_IMMED_V("bar"), 0));
        /* error: htab is a repeated field */
        Z_ASSERT_NEG(TST_SORT_VEC(LSTR_IMMED_V("htab"), 0));

        qv_wipe(my_struct_a, &vec);
#undef TST_SORT_VEC

        qv_init(my_struct_a_opt, &vec2);
        tstiop__my_struct_a_opt__init(&a2);

        qv_append(my_struct_a_opt, &vec2, a2);
        IOP_OPT_SET(a2.a, 42);
        qv_append(my_struct_a_opt, &vec2, a2);
        IOP_OPT_SET(a2.a, 43);
        qv_append(my_struct_a_opt, &vec2, a2);
        IOP_OPT_CLR(a2.a);
        a2.j = LSTR_IMMED_V("abc");
        a2.l = &IOP_UNION(tstiop__my_union_a, ua, 222);
        qv_append(my_struct_a_opt, &vec2, a2);
        a2.j = LSTR_IMMED_V("def");
        a2.l = &IOP_UNION(tstiop__my_union_a, ub, 222);
        qv_append(my_struct_a_opt, &vec2, a2);
        a2.l = &IOP_UNION(tstiop__my_union_a, us, LSTR_IMMED_V("xyz"));
        qv_append(my_struct_a_opt, &vec2, a2);

        tstiop__my_struct_b__init(&b1);
        IOP_OPT_SET(b1.a, 42);
        a2.o = &b1;
        qv_append(my_struct_a_opt, &vec2, a2);

        tstiop__my_struct_b__init(&b2);
        IOP_OPT_SET(b2.a, 72);
        a2.o = &b2;
        qv_append(my_struct_a_opt, &vec2, a2);

#define TST_SORT_VEC(p, f)  tstiop__my_struct_a_opt__sort(vec2.tab, vec2.len, p, f)

        /* sort on optional int a */
        Z_ASSERT_N(TST_SORT_VEC(LSTR_IMMED_V("a"), 0));
        Z_ASSERT_EQ(IOP_OPT_VAL(vec2.tab[0].a), 42);
        Z_ASSERT_EQ(IOP_OPT_VAL(vec2.tab[1].a), 43);
        Z_ASSERT(!IOP_OPT_ISSET(vec2.tab[2].a));
        Z_ASSERT(!IOP_OPT_ISSET(vec2.tab[3].a));
        Z_ASSERT(!IOP_OPT_ISSET(vec2.tab[4].a));
        Z_ASSERT(!IOP_OPT_ISSET(vec2.tab[5].a));

        /* sort on optional string j */
        Z_ASSERT_N(TST_SORT_VEC(LSTR_IMMED_V("j"), 0));
        Z_ASSERT_LSTREQUAL(vec2.tab[0].j, LSTR_IMMED_V("abc"));
        Z_ASSERT_LSTREQUAL(vec2.tab[1].j, LSTR_IMMED_V("def"));

        /* sort on optional union l */
        Z_ASSERT_N(TST_SORT_VEC(LSTR_IMMED_V("l"), 0));
        Z_ASSERT_EQ(vec2.tab[0].l->ua, 222);

        /* sort on optional int a, member of optional struct MyStructB o */
        Z_ASSERT_N(TST_SORT_VEC(LSTR_IMMED_V("o.a"), 0));
        Z_ASSERT_EQ(IOP_OPT_VAL(vec2.tab[0].o->a), 42);
        Z_ASSERT_EQ(IOP_OPT_VAL(vec2.tab[1].o->a), 72);

        /* error: cannot sort on struct */
        Z_ASSERT_NEG(TST_SORT_VEC(LSTR_IMMED_V("o"), 0));

        qv_wipe(my_struct_a_opt, &vec2);
#undef TST_SORT_VEC

        qv_init(my_struct_m, &mvec);
        tstiop__my_struct_m__init(&m);

        m.k.j.cval = 5;
        m.k.j.b = IOP_UNION(tstiop__my_union_b, bval, 55);
        qv_append(my_struct_m, &mvec, m);
        m.k.j.cval = 4;
        m.k.j.b = IOP_UNION(tstiop__my_union_b, bval, 44);
        qv_append(my_struct_m, &mvec, m);
        m.k.j.cval = 3;
        m.k.j.b = IOP_UNION(tstiop__my_union_b, bval, 33);
        qv_append(my_struct_m, &mvec, m);

#define TST_SORT_VEC(p, f)  tstiop__my_struct_m__sort(mvec.tab, mvec.len, p, f)

        /* sort on int cval from MyStructJ j from MyStructK k */
        Z_ASSERT_N(TST_SORT_VEC(LSTR_IMMED_V("k.j.cval"), 0));
        Z_ASSERT_EQ(mvec.tab[0].k.j.cval, 3);
        Z_ASSERT_EQ(mvec.tab[1].k.j.cval, 4);
        Z_ASSERT_EQ(mvec.tab[2].k.j.cval, 5);

        /* sort on int bval from MyUnionB b from MyStructJ j from MyStructK k
         */
        Z_ASSERT_N(TST_SORT_VEC(LSTR_IMMED_V("k.j.b.bval"), 0));
        Z_ASSERT_EQ(mvec.tab[0].k.j.b.bval, 33);
        Z_ASSERT_EQ(mvec.tab[1].k.j.b.bval, 44);
        Z_ASSERT_EQ(mvec.tab[2].k.j.b.bval, 55);

        qv_wipe(my_struct_m, &mvec);
#undef TST_SORT_VEC

    } Z_TEST_END;
    /* }}} */
    Z_TEST(iop_copy_inv_tab, "iop_copy(): invalid tab pointer when len == 0") { /* {{{ */
        t_scope;

        tstiop__my_struct_b__t sb, *sb_dup;

        iop_dso_t *dso;
        lstr_t path = t_lstr_cat(z_cmddir_g,
                                 LSTR_IMMED_V("zchk-tstiop-plugin.so"));


        const iop_struct_t *st_sb;

        if ((dso = iop_dso_open(path.s)) == NULL)
            Z_SKIP("unable to load zchk-tstiop-plugin, TOOLS repo?");

        Z_ASSERT_P(st_sb = iop_dso_find_type(dso, LSTR_IMMED_V("tstiop.MyStructB")));

        iop_init(st_sb, &sb);
        sb.b.tab = (void *)0x42;
        sb.b.len = 0;

        sb_dup = iop_dup(NULL, st_sb, &sb);
        Z_ASSERT_NULL(sb_dup->b.tab);
        Z_ASSERT_ZERO(sb_dup->b.len);

        p_delete(&sb_dup);

        iop_dso_close(&dso);
    } Z_TEST_END
    /* }}} */
    Z_TEST(iop_get_field_len, "test iop_get_field_len") { /* {{{ */
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

        iop_dso_t *dso;
        lstr_t path = t_lstr_cat(z_cmddir_g,
                                 LSTR_IMMED_V("zchk-tstiop-plugin.so"));
        const iop_struct_t *st_sa;
        qv_t(i32) szs;
        int len;
        byte *dst;
        pstream_t ps;

        if ((dso = iop_dso_open(path.s)) == NULL)
            Z_SKIP("unable to load zchk-tstiop-plugin, TOOLS repo?");

        Z_ASSERT_P(st_sa = iop_dso_find_type(dso, LSTR_IMMED_V("tstiop.MyStructA")));

        t_qv_init(i32, &szs, 1024);

        /* packing */
        Z_ASSERT_N((len = iop_bpack_size(st_sa, &sa, &szs)),
                   "invalid structure size (%s)", st_sa->fullname.s);
        dst = t_new(byte, len);
        iop_bpack(dst, st_sa, &sa, szs.tab);

        ps = ps_init(dst, len);
        while (!ps_done(&ps)) {
            Z_ASSERT_GT(len = iop_get_field_len(ps), 0);
            Z_ASSERT_N(ps_skip(&ps, len));
        }

        iop_dso_close(&dso);
    } Z_TEST_END
    /* }}} */
} Z_GROUP_END
