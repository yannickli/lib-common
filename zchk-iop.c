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
#include "iop/tstiop_inheritance.iop.h"
#include "xmlr.h"
#include "zchk-iop-ressources.h"

/* {{{ IOP testing helpers */

static int iop_xml_test_struct(const iop_struct_t *st, void *v, const char *info)
{
    t_scope;
    int len;
    lstr_t s;
    uint8_t buf1[20], buf2[20];
    void *res = NULL;
    int ret;
    sb_t sb;

    /* XXX: Use a small t_sb here to force a realloc during (un)packing and
     *      detect possible illegal usage of the t_pool in the (un)packing
     *      functions. */
    t_sb_init(&sb, 100);

    sb_adds(&sb, "<root xmlns:xsd=\"http://www.w3.org/2001/XMLSchema\" "
            "xmlns:xsi=\"http://www.w3.org/2001/XMLSchema-instance\"");
    if (iop_struct_is_class(st)) {
        const iop_struct_t *real_st = *(const iop_struct_t **)v;

        sb_addf(&sb, " xsi:type=\"tns:%*pM\"",
                LSTR_FMT_ARG(real_st->fullname));
    }
    sb_addc(&sb, '>');
    len = sb.len;
    iop_xpack(&sb, st, v, false, true);
    sb_adds(&sb, "</root>");

    s = t_lstr_dups(sb.data + len, sb.len - len - 7);

    /* unpacking */
    Z_ASSERT_N(xmlr_setup(&xmlr_g, sb.data, sb.len));
    ret = iop_xunpack_ptr(xmlr_g, t_pool(), st, &res);
    Z_ASSERT_N(ret, "XML unpacking failure (%s, %s): %s", st->fullname.s,
               info, xmlr_get_err());

    /* pack again ! */
    t_sb_init(&sb, 10);
    iop_xpack(&sb, st, res, false, true);

    /* check packing equality */
    Z_ASSERT_LSTREQUAL(s, LSTR_SB_V(&sb),
                       "XML packing/unpacking doesn't match! (%s, %s)",
                       st->fullname.s, info);

    /* In case of, check hashes equality */
    iop_hash_sha1(st, v,   buf1, 0);
    iop_hash_sha1(st, res, buf2, 0);
    Z_ASSERT_EQUAL(buf1, sizeof(buf1), buf2, sizeof(buf2),
                   "XML packing/unpacking hashes don't match! (%s, %s)",
                   st->fullname.s, info);

    xmlr_close(&xmlr_g);
    Z_HELPER_END;
}

static int iop_xml_test_struct_invalid(const iop_struct_t *st, void *v,
                                       const char *info)
{
    t_scope;
    void *res = NULL;
    sb_t sb;

    /* XXX: Use a small t_sb here to force a realloc during (un)packing and
     *      detect possible illegal usage of the t_pool in the (un)packing
     *      functions. */
    t_sb_init(&sb, 100);

    sb_adds(&sb, "<root xmlns:xsd=\"http://www.w3.org/2001/XMLSchema\" "
            "xmlns:xsi=\"http://www.w3.org/2001/XMLSchema-instance\"");
    if (iop_struct_is_class(st)) {
        const iop_struct_t *real_st = *(const iop_struct_t **)v;

        sb_addf(&sb, " xsi:type=\"tns:%*pM\"",
                LSTR_FMT_ARG(real_st->fullname));
    }
    sb_addc(&sb, '>');
    iop_xpack(&sb, st, v, false, true);
    sb_adds(&sb, "</root>");

    /* unpacking */
    Z_ASSERT_N(xmlr_setup(&xmlr_g, sb.data, sb.len));
    Z_ASSERT_NEG(iop_xunpack_ptr(xmlr_g, t_pool(), st, &res),
                 "XML unpacking unexpected success (%s, %s)", st->fullname.s,
                 info);

    xmlr_close(&xmlr_g);
    Z_HELPER_END;
}

static int iop_json_test_struct(const iop_struct_t *st, void *v,
                                const char *info)
{
    iop_json_lex_t jll;
    pstream_t ps;
    int strict = 0;
    uint8_t buf1[20], buf2[20];

    iop_jlex_init(t_pool(), &jll);
    jll.flags = IOP_UNPACK_IGNORE_UNKNOWN;

    while (strict < 3) {
        t_scope;
        sb_t sb;
        void *res = NULL;
        int ret;

        /* XXX: Use a small t_sb here to force a realloc during (un)packing
         *      and detect possible illegal usage of the t_pool in the
         *      (un)packing functions. */
        t_sb_init(&sb, 10);

        /* packing */
        Z_ASSERT_N(iop_jpack(st, v, iop_sb_write, &sb, strict),
                   "JSon packing failure! (%s, %s)", st->fullname.s, info);

        /* unpacking */
        ps = ps_initsb(&sb);
        iop_jlex_attach(&jll, &ps);
        if ((ret = iop_junpack_ptr(&jll, st, &res, true)) < 0) {
            t_sb_init(&sb, 10);
            iop_jlex_write_error(&jll, &sb);
        }
        Z_ASSERT_N(ret, "JSon unpacking error (%s, %s): %s", st->fullname.s,
                   info, sb.data);
        iop_jlex_detach(&jll);

        /* check hashes equality */
        iop_hash_sha1(st, v,   buf1, 0);
        iop_hash_sha1(st, res, buf2, 0);
        Z_ASSERT_EQUAL(buf1, sizeof(buf1), buf2, sizeof(buf2),
                       "JSON %spacking/unpacking hashes don't match! (%s, %s)",
                       (strict ? "strict " : ""),
                       st->fullname.s, info);

        strict++;
    }

    iop_jlex_wipe(&jll);

    Z_HELPER_END;
}

static int iop_json_test_struct_invalid(const iop_struct_t *st, void *v,
                                        const char *info)
{
    iop_json_lex_t jll;
    pstream_t ps;
    int strict = 0;

    iop_jlex_init(t_pool(), &jll);
    jll.flags = IOP_UNPACK_IGNORE_UNKNOWN;

    while (strict < 3) {
        t_scope;
        sb_t sb;
        void *res = NULL;
        int ret;

        /* XXX: Use a small t_sb here to force a realloc during (un)packing
         *      and detect possible illegal usage of the t_pool in the
         *      (un)packing functions. */
        t_sb_init(&sb, 10);

        /* packing */
        Z_ASSERT_N(iop_jpack(st, v, iop_sb_write, &sb, strict),
                   "JSon packing failure! (%s, %s)", st->fullname.s, info);

        /* unpacking */
        ps = ps_initsb(&sb);
        iop_jlex_attach(&jll, &ps);
        ret = iop_junpack_ptr(&jll, st, &res, true);
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
    void *res = NULL;
    int ret;
    uint8_t buf1[20], buf2[20];
    sb_t sb;

    /* XXX: Use a small t_sb here to force a realloc during (un)packing and
     *      detect possible illegal usage of the t_pool in the (un)packing
     *      functions. */
    t_sb_init(&sb, 10);

    iop_jlex_init(t_pool(), &jll);
    jll.flags = IOP_UNPACK_IGNORE_UNKNOWN;

    ps = ps_initstr(json);
    iop_jlex_attach(&jll, &ps);
    if ((ret = iop_junpack_ptr(&jll, st, &res, true)) < 0)
        iop_jlex_write_error(&jll, &sb);
    Z_ASSERT_N(ret, "JSon unpacking error (%s, %s): %s", st->fullname.s, info,
               sb.data);
    iop_jlex_detach(&jll);

    /* visualize result */
    if (e_is_traced(1))
        iop_jtrace_(1, __FILE__, __LINE__, __func__, NULL, st, res);

    /* check hashes equality */
    iop_hash_sha1(st, res,      buf1, 0);
    iop_hash_sha1(st, expected, buf2, 0);
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
    void *res = NULL;
    int ret;
    sb_t sb;

    /* XXX: Use a small t_sb here to force a realloc during (un)packing and
     *      detect possible illegal usage of the t_pool in the (un)packing
     *      functions. */
    t_sb_init(&sb, 10);

    iop_jlex_init(t_pool(), &jll);
    jll.flags = IOP_UNPACK_IGNORE_UNKNOWN;

    ps = ps_initstr(json);
    iop_jlex_attach(&jll, &ps);

    if ((ret = iop_junpack_ptr(&jll, st, &res, true)) < 0)
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
    void *res = NULL;
    uint8_t buf1[20], buf2[20];
    qv_t(i32) szs;
    int len;
    byte *dst;

    /* XXX: Use a small t_qv here to force a realloc during (un)packing and
     *      detect possible illegal usage of the t_pool in the (un)packing
     *      functions. */
    t_qv_init(i32, &szs, 2);

    /* packing */
    Z_ASSERT_N((len = iop_bpack_size_flags(st, v, flags, &szs)),
               "invalid structure size (%s, %s)", st->fullname.s, info);
    dst = t_new(byte, len);
    iop_bpack(dst, st, v, szs.tab);

    /* unpacking */
    ret = iop_bunpack_ptr(t_pool(), st, &res, ps_init(dst, len), false);
    Z_ASSERT_N(ret, "IOP unpacking error (%s, %s, %s)",
               st->fullname.s, info, iop_get_err());

    /* check hashes equality */
    iop_hash_sha1(st, v,   buf1, 0);
    iop_hash_sha1(st, res, buf2, 0);
    Z_ASSERT_EQUAL(buf1, sizeof(buf1), buf2, sizeof(buf2),
                   "IOP packing/unpacking hashes don't match! (%s, %s)",
                   st->fullname.s, info);

    /* check equality */
    Z_ASSERT_IOPEQUAL_DESC(st, v, res);

    /* test duplication */
    Z_ASSERT_NULL(iop_dup(NULL, st, NULL, NULL));
    Z_ASSERT_P(res = iop_dup(t_pool(), st, v, NULL),
               "IOP duplication error! (%s, %s)", st->fullname.s, info);

    /* check equality */
    Z_ASSERT_IOPEQUAL_DESC(st, v, res);

    /* check hashes equality */
    iop_hash_sha1(st, res, buf2, 0);
    Z_ASSERT_EQUAL(buf1, sizeof(buf1), buf2, sizeof(buf2),
                   "IOP duplication hashes don't match! (%s, %s)",
                   st->fullname.s, info);

    /* test copy */
    iop_copy(t_pool(), st, (void **)&res, NULL, NULL);
    Z_ASSERT_NULL(res);
    iop_copy(t_pool(), st, (void **)&res, v, NULL);

    /* check equality */
    Z_ASSERT_IOPEQUAL_DESC(st, v, res);

    /* check hashes equality */
    iop_hash_sha1(st, res, buf2, 0);
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
    void *res = NULL;
    qv_t(i32) szs;
    int len, ret;
    byte *dst;

    /* XXX: Use a small t_qv here to force a realloc during (un)packing and
     *      detect possible illegal usage of the t_pool in the (un)packing
     *      functions. */
    t_qv_init(i32, &szs, 2);

    /* packing */
    Z_ASSERT_N((len = iop_bpack_size(st, v, &szs)),
               "invalid structure size (%s, %s)", st->fullname.s, info);
    dst = t_new(byte, len);
    iop_bpack(dst, st, v, szs.tab);

    /* unpacking */
    ret = iop_bunpack_ptr(t_pool(), st, &res, ps_init(dst, len), false);
    Z_ASSERT_NEG(ret, "IOP unpacking unexpected success (%s, %s)",
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
        qv_t(cstr) ressources_str;
        qv_t(i32) ressources_int;
        lstr_t path = t_lstr_cat(z_cmddir_g,
                                 LSTR("zchk-iop-plugin"SO_FILEEXT));

        Z_ASSERT(dso = iop_dso_open(path.s));
        Z_ASSERT_N(qm_find(iop_struct, &dso->struct_h, &LSTR_IMMED_V("ic.Hdr")));

        Z_ASSERT_P(st = iop_dso_find_type(dso, LSTR("ic.SimpleHdr")));
        Z_ASSERT(st != &ic__simple_hdr__s);

        t_qv_init(cstr, &ressources_str, 0);
        iop_dso_for_each_ressource(dso, str, ressource) {
            qv_append(cstr, &ressources_str, *ressource);
        }
        Z_ASSERT_EQ(ressources_str.len, 2, "loading ressources failed");
        Z_ASSERT_ZERO(strcmp(ressources_str.tab[0], z_ressource_str_a));
        Z_ASSERT_ZERO(strcmp(ressources_str.tab[1], z_ressource_str_b));

        t_qv_init(i32, &ressources_int, 0);
        iop_dso_for_each_ressource(dso, int, ressource) {
            qv_append(i32, &ressources_int, *ressource);
        }
        Z_ASSERT_EQ(ressources_int.len, 2, "loading ressources failed");
        Z_ASSERT_EQ(ressources_int.tab[0], z_ressources_int_1);
        Z_ASSERT_EQ(ressources_int.tab[1], z_ressources_int_2);

        iop_dso_close(&dso);
    } Z_TEST_END;
    /* }}} */
    Z_TEST(hash_sha1, "test wether iop_hash_sha1 is stable wrt ABI change") { /* {{{ */
        t_scope;

        int  i_10 = 10, i_11 = 11;
        long j_10 = 10;

        struct tstiop__hash_v1__t v1 = {
            .b  = OPT(true),
            .i  = IOP_ARRAY(&i_10, 1),
            .s  = LSTR_IMMED("foo bar baz"),
        };

        struct tstiop__hash_v2__t v2 = {
            .b  = OPT(true),
            .i  = IOP_ARRAY(&j_10, 1),
            .s  = LSTR_IMMED("foo bar baz"),
        };

        struct tstiop__hash_v1__t v1_not_same = {
            .b  = OPT(true),
            .i  = IOP_ARRAY(&i_11, 1),
            .s  = LSTR_IMMED("foo bar baz"),
        };

        const iop_struct_t *stv1;
        const iop_struct_t *stv2;

        iop_dso_t *dso;
        lstr_t path = t_lstr_cat(z_cmddir_g,
                                 LSTR("zchk-tstiop-plugin"SO_FILEEXT));
        uint8_t buf1[20], buf2[20];

        dso = iop_dso_open(path.s);
        if (dso == NULL)
            Z_SKIP("unable to load zchk-tstiop-plugin, TOOLS repo?");

        Z_ASSERT_P(stv1 = iop_dso_find_type(dso, LSTR("tstiop.HashV1")));
        Z_ASSERT_P(stv2 = iop_dso_find_type(dso, LSTR("tstiop.HashV2")));

        iop_hash_sha1(stv1, &v1, buf1, 0);
        iop_hash_sha1(stv2, &v2, buf2, 0);
        Z_ASSERT_EQUAL(buf1, sizeof(buf1), buf2, sizeof(buf2));

        iop_hash_sha1(stv1, &v1_not_same, buf2, 0);
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

#define result(_res, _signed) \
        do {                                                            \
            uint64_t cres;                                              \
            bool is_signed;                                             \
            \
            Z_ASSERT_N(iop_cfolder_get_result(&cfolder, &cres, &is_signed),\
                       "constant folder error");                        \
            Z_ASSERT_EQ((int64_t)cres, (int64_t)_res);                  \
            Z_ASSERT_EQ(is_signed, _signed);                            \
            iop_cfolder_wipe(&cfolder);                                 \
            iop_cfolder_init(&cfolder);                                 \
        } while (false)

#define error()                                                         \
        do {                                                            \
            uint64_t cres;                                              \
                                                                        \
            Z_ASSERT_NEG(iop_cfolder_get_result(&cfolder, &cres, NULL));\
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
        result(24, false);

        feed_num(10);
        feed_op('*');
        feed_num(2);
        feed_op('+');
        feed_num(3);
        feed_op('+');
        feed_num(4);
        feed_op('*');
        feed_num(10);
        result(63, false);

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
        result(6, false);

        feed_num(32);
        feed_op('/');
        feed_num(4);
        feed_op(CF_OP_EXP);
        feed_num(2);
        feed_op('/');
        feed_num(2);
        result(1, false);

        feed_num(8);
        feed_op('/');
        feed_num(4);
        feed_op('/');
        feed_num(2);
        result(1, false);

        feed_num(8);
        feed_op('/');
        feed_op('(');
        feed_num(4);
        feed_op('/');
        feed_num(2);
        feed_op(')');
        result(4, false);

        feed_num(4);
        feed_op(CF_OP_EXP);
        feed_num(3);
        feed_op(CF_OP_EXP);
        feed_num(2);
        result(262144, false);

        feed_num(4);
        feed_op('+');
        feed_op('-');
        feed_num(2);
        feed_op(CF_OP_EXP);
        feed_num(2);
        result(0, false);

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
        result(65, false);

        feed_num(0xfffff);
        feed_op('&');
        feed_num(32);
        feed_op(CF_OP_LSHIFT);
        feed_num(2);
        feed_op('|');
        feed_num(3);
        result(131, false);

        feed_num(63);
        feed_op('-');
        feed_num(64);
        result(-1, true);

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

        feed_num(2);
        feed_op(CF_OP_EXP);
        feed_num(63);
        feed_op('-');
        feed_num(1);
        result(INT64_MAX, false);

        feed_num(-2);
        feed_op(CF_OP_EXP);
        feed_num(63);
        result(INT64_MIN, true);

        feed_num(1);
        feed_op(CF_OP_EXP);
        feed_num(INT64_MAX);
        result(1, false);

        feed_num(-1);
        feed_op(CF_OP_EXP);
        feed_num(INT64_MAX);
        result(-1, true);

        feed_num(-1);
        feed_op(CF_OP_EXP);
        feed_num(0);
        result(1, false);

        feed_num(-1);
        feed_op(CF_OP_EXP);
        feed_num(INT64_MAX - 1);
        result(1, false);

        feed_num(2);
        feed_op(CF_OP_EXP);
        feed_num(INT64_MAX);
        error();

        feed_num(-2);
        feed_op(CF_OP_EXP);
        feed_num(INT64_MAX);
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
                                 LSTR("zchk-tstiop-plugin"SO_FILEEXT));

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
                Z_ASSERT_LSTREQUAL(v, LSTR("foo"));
              }
              IOP_UNION_DEFAULT() {
                Z_ASSERT(false, "default case shouldn't be reached");
              }
            }

            Z_ASSERT_P(usvp = tstiop__my_union_a__get(&us, us));
            Z_ASSERT_LSTREQUAL(*usvp, LSTR("foo"));

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
                                 LSTR("zchk-tstiop-plugin"SO_FILEEXT));

        int32_t val[] = {15, 30, 45};

        tstiop__my_struct_e__t se = {
            .a = 10,
            .b = IOP_UNION(tstiop__my_union_a, ua, 42),
            .c = { .b = IOP_ARRAY(val, countof(val)), },
        };

        uint64_t uval[] = {UINT64_MAX, INT64_MAX, 0};

        tstiop__my_class2__t cls2;

        tstiop__my_union_a__t un = IOP_UNION(tstiop__my_union_a, ua, 1);

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
            .i = LSTR_IMMED("foo"),
            .j = LSTR_EMPTY,
            .k = MY_ENUM_A_B,
            .l = IOP_UNION(tstiop__my_union_a, ub, 42),
            .lr = &un,
            .cls2 = &cls2,
            .m = 3.14159265,
            .n = true,
            .xml_field = LSTR_IMMED("<foo><bar/><foobar "
                                    "attr=\"value\">toto</foobar></foo>"),
        };

        lstr_t svals[] = {
            LSTR_IMMED("foo"), LSTR_IMMED("bar"), LSTR_IMMED("foobar"),
        };

        lstr_t dvals[] = {
            LSTR_IMMED("Test"), LSTR_IMMED("Foo"), LSTR_IMMED("BAR"),
        };

        tstiop__my_struct_b__t bvals[] = {
            { .b = IOP_ARRAY(NULL, 0), },
            { .a = OPT(55), .b = IOP_ARRAY(NULL, 0), }
        };

        tstiop__my_struct_f__t sf = {
            .a = IOP_ARRAY(svals, countof(svals)),
            .b = IOP_ARRAY(dvals, countof(dvals)),
            .c = IOP_ARRAY(bvals, countof(bvals)),
        };

        const iop_struct_t *st_se, *st_sa, *st_sf, *st_cs, *st_sa_opt;
        const iop_struct_t *st_cls2;

        if ((dso = iop_dso_open(path.s)) == NULL)
            Z_SKIP("unable to load zchk-tstiop-plugin, TOOLS repo?");

        Z_ASSERT_P(st_se = iop_dso_find_type(dso, LSTR("tstiop.MyStructE")));
        Z_ASSERT_P(st_sa = iop_dso_find_type(dso, LSTR("tstiop.MyStructA")));
        Z_ASSERT_P(st_sf = iop_dso_find_type(dso, LSTR("tstiop.MyStructF")));
        Z_ASSERT_P(st_cs = iop_dso_find_type(dso, LSTR("tstiop.ConstraintS")));
        Z_ASSERT_P(st_sa_opt = iop_dso_find_type(dso, LSTR("tstiop.MyStructAOpt")));
        Z_ASSERT_P(st_cls2 = iop_dso_find_type(dso, LSTR("tstiop.MyClass2")));

        iop_init(st_cls2, &cls2);

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
            lstr_t foo = LSTR("foo");
            lstr_t bar = LSTR("bar");
            tstiop__my_struct_f__t sf_ret;
            SB_1k(sb);
            qm_t(part) parts;

            qm_init_cached(part, &parts);
            qm_add(part, &parts, &foo, LSTR("part cid foo"));
            qm_add(part, &parts, &bar, LSTR("part cid bar"));

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

            Z_ASSERT(OPT_ISSET(sa_opt.a));
            Z_ASSERT_EQ(OPT_VAL(sa_opt.a), 42);

            Z_ASSERT(OPT_ISSET(sa_opt.b));
            Z_ASSERT_EQ(OPT_VAL(sa_opt.b), 0x10U);

            Z_ASSERT(OPT_ISSET(sa_opt.e));
            Z_ASSERT_EQ(OPT_VAL(sa_opt.e), -42);

            Z_ASSERT(OPT_ISSET(sa_opt.f));
            Z_ASSERT_EQ(OPT_VAL(sa_opt.f), 0x42);
        }

        { /* Test PRIVATE */
            t_scope;
            tstiop__constraint_s__t cs;
            SB_1k(sb);
            byte *res;
            int ret;
            lstr_t strings[] = {
                LSTR("foo5"),
                LSTR("foo6"),
            };

            iop_init(st_cs, &cs);
            cs.s.tab = strings;
            cs.s.len = 2;
            Z_HELPER_RUN(iop_xml_test_struct(st_cs, &cs, "cs"));

            OPT_SET(cs.priv, true);
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
            Z_ASSERT(!OPT_ISSET(cs.priv));
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

        tstiop__my_class2__t cls2;

        tstiop__my_union_a__t un = IOP_UNION(tstiop__my_union_a, ua, 1);

        tstiop__my_struct_a__t sa = {
            .a = 42,
            .b = 5,
            .c_of_my_struct_a = 120,
            .d = 230,
            .e = 540,
            .f = 2000,
            .g = 10000,
            .h = 20000,
            .i = LSTR_IMMED("foo"),
            .j = LSTR_IMMED("baré© \" foo ."),
            .xml_field = LSTR_IMMED("<foo />"),
            .k = MY_ENUM_A_B,
            .l = IOP_UNION(tstiop__my_union_a, ub, 42),
            .lr = &un,
            .cls2 = &cls2,
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
            .i = LSTR_EMPTY,
            .j = LSTR_EMPTY,
            .xml_field = LSTR_EMPTY,
            .k = MY_ENUM_A_B,
            .l = IOP_UNION(tstiop__my_union_a, ub, 42),
            .lr = &un,
            .cls2 = &cls2,
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
            "    lr.ua: 1,\n"
            "    cls2: {\n"
            "        \"_class\": \"tstiop.MyClass2\",\n"
            "        \"int1\": 1,\n"
            "        \"int2\": 2\n"
            "    },\n"
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
            "    lr: {ua: 1},\n"
            "    cls2: {\n"
            "        \"_class\": \"tstiop.MyClass2\",\n"
            "        \"int1\": 1,\n"
            "        \"int2\": 2\n"
            "    },\n"
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
            .i = LSTR_IMMED("foo"),
            .j = LSTR_IMMED("bar"),
            .xml_field = LSTR_EMPTY,
            .k = MY_ENUM_A_B,
            .l = IOP_UNION(tstiop__my_union_a, us, LSTR_IMMED("union value")),
            .lr = &un,
            .cls2 = &cls2,
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

        lstr_t bvals[] = {
            LSTR_IMMED("foobar"),
            LSTR_IMMED("barfoo"),
        };

        int b2vals[] = { 86400*7, 86400, 3600, 60, 1, 1<<30, 1<<20, 1<<10 };

        tstiop__my_struct_b__t cvals[] = {
            {
                .a = OPT(10),
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
        const char json_si_p3[] = "{u = [ \"9223372036854775808\" ]; };" ;

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
                                              LSTR("foo"))),
                .btab = IOP_ARRAY(j_bvals, countof(j_bvals)),
            },
        };

        const char json_sa_opt[] = "{ a: 42, o: null }";

        tstiop__my_struct_a_opt__t json_sa_opt_res = {
            .a = OPT(42),
        };


        iop_dso_t *dso;
        lstr_t path = t_lstr_cat(z_cmddir_g,
                                 LSTR("zchk-tstiop-plugin"SO_FILEEXT));


        const iop_struct_t *st_sa, *st_sf, *st_si, *st_sk, *st_sa_opt;
        const iop_struct_t *st_cls2;

        if ((dso = iop_dso_open(path.s)) == NULL)
            Z_SKIP("unable to load zchk-tstiop-plugin, TOOLS repo?");

        Z_ASSERT_P(st_sa = iop_dso_find_type(dso, LSTR("tstiop.MyStructA")));
        Z_ASSERT_P(st_sf = iop_dso_find_type(dso, LSTR("tstiop.MyStructF")));
        Z_ASSERT_P(st_si = iop_dso_find_type(dso, LSTR("tstiop.MyStructI")));
        Z_ASSERT_P(st_sk = iop_dso_find_type(dso, LSTR("tstiop.MyStructK")));
        Z_ASSERT_P(st_sa_opt = iop_dso_find_type(dso, LSTR("tstiop.MyStructAOpt")));
        Z_ASSERT_P(st_cls2 = iop_dso_find_type(dso, LSTR("tstiop.MyClass2")));

        iop_init(st_cls2, &cls2);
        cls2.int1 = 1;
        cls2.int2 = 2;

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
        Z_HELPER_RUN(iop_json_test_unpack(st_si, json_si_p3, true,
                                          "json_si_p3"));

        Z_HELPER_RUN(iop_json_test_unpack(st_si, json_si_n1, false,
                                          "json_si_n1"));
        Z_HELPER_RUN(iop_json_test_unpack(st_si, json_si_n2, false,
                                          "json_si_n2"));

        iop_dso_close(&dso);
    } Z_TEST_END
    /* }}} */
    Z_TEST(std, "test IOP std (un)packer") { /* {{{ */
        t_scope;

        tstiop__my_class2__t cls2;

        tstiop__my_union_a__t un = IOP_UNION(tstiop__my_union_a, ua, 1);

        tstiop__my_struct_a__t sa = {
            .a = 42,
            .b = 5,
            .c_of_my_struct_a = 120,
            .d = 230,
            .e = 540,
            .f = 2000,
            .g = 10000,
            .h = 20000,
            .i = LSTR_IMMED("foo"),
            .j = LSTR_IMMED("baré© \" foo ."),
            .k = MY_ENUM_A_B,
            .l = IOP_UNION(tstiop__my_union_a, ub, 42),
            .lr = &un,
            .cls2 = &cls2,
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
            .i = LSTR_EMPTY,
            .j = LSTR_EMPTY,
            .k = MY_ENUM_A_B,
            .l = IOP_UNION(tstiop__my_union_a, ub, 42),
            .lr = &un,
            .cls2 = &cls2,
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
                                 LSTR("zchk-tstiop-plugin"SO_FILEEXT));


        const iop_struct_t *st_sa, *st_sa_opt, *st_se, *st_cls2;


        if ((dso = iop_dso_open(path.s)) == NULL)
            Z_SKIP("unable to load zchk-tstiop-plugin, TOOLS repo?");

        Z_ASSERT_P(st_sa = iop_dso_find_type(dso, LSTR("tstiop.MyStructA")));
        Z_ASSERT_P(st_sa_opt = iop_dso_find_type(dso, LSTR("tstiop.MyStructAOpt")));
        Z_ASSERT_P(st_se = iop_dso_find_type(dso, LSTR("tstiop.MyStructE")));
        Z_ASSERT_P(st_cls2 = iop_dso_find_type(dso, LSTR("tstiop.MyClass2")));

        iop_init(st_cls2, &cls2);

        Z_ASSERT_N(iop_check_constraints(st_sa, &sa));
        Z_ASSERT_N(iop_check_constraints(st_sa, &sa2));

        Z_HELPER_RUN(iop_std_test_struct(st_sa, &sa,  "sa"));
        Z_HELPER_RUN(iop_std_test_struct(st_sa, &sa2, "sa2"));
        Z_HELPER_RUN(iop_std_test_struct(st_se, &se, "se"));

        iop_init(st_sa_opt, &sa_opt);
        OPT_SET(sa_opt.a, 32);
        sa_opt.j = LSTR("foo");
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
                                 LSTR("zchk-tstiop-plugin"SO_FILEEXT));
        unsigned seed = (unsigned)time(NULL);



        if ((dso = iop_dso_open(path.s)) == NULL)
            Z_SKIP("unable to load zchk-tstiop-plugin, TOOLS repo?");

        Z_ASSERT_P(st = iop_dso_find_type(dso, LSTR("tstiop.Repeated")));

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
                              LSTR("samples/repeated.ibp"));

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
                                 LSTR("zchk-tstiop-plugin"SO_FILEEXT));


        const iop_struct_t *st_sg;
        qv_t(i32) szs;
        int len;
        lstr_t s;
        const unsigned flags = IOP_BPACK_SKIP_DEFVAL;

        if ((dso = iop_dso_open(path.s)) == NULL)
            Z_SKIP("unable to load zchk-tstiop-plugin, TOOLS repo?");

        Z_ASSERT_P(st_sg = iop_dso_find_type(dso, LSTR("tstiop.MyStructG")));

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
        sg.j = LSTR("plop");
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
                                 LSTR("zchk-tstiop-plugin"SO_FILEEXT));


        const iop_struct_t *st_sg, *st_sa_opt, *st_ua, *st_sr;

        if ((dso = iop_dso_open(path.s)) == NULL)
            Z_SKIP("unable to load zchk-tstiop-plugin, TOOLS repo?");

        Z_ASSERT_P(st_sg = iop_dso_find_type(dso, LSTR("tstiop.MyStructG")));
        Z_ASSERT_P(st_sr = iop_dso_find_type(dso, LSTR("tstiop.Repeated")));
        Z_ASSERT_P(st_sa_opt = iop_dso_find_type(dso, LSTR("tstiop.MyStructAOpt")));
        Z_ASSERT_P(st_ua = iop_dso_find_type(dso, LSTR("tstiop.MyUnionA")));

        /* Test with all the default values */
        iop_init(st_sg, &sg_a);
        iop_init(st_sg, &sg_b);
        Z_ASSERT_IOPEQUAL_DESC(st_sg, &sg_a, &sg_b);

        /* Change some fields and test */
        sg_a.b++;
        Z_ASSERT(!iop_equals(st_sg, &sg_a, &sg_b));

        sg_a.b--;
        sg_b.j = LSTR("not equal");
        Z_ASSERT(!iop_equals(st_sg, &sg_a, &sg_b));

        /* Use a more complex structure */
        iop_init(st_sa_opt, &sa_opt_a);
        iop_init(st_sa_opt, &sa_opt_b);
        Z_ASSERT_IOPEQUAL_DESC(st_sa_opt, &sa_opt_a, &sa_opt_b);

        OPT_SET(sa_opt_a.a, 42);
        OPT_SET(sa_opt_b.a, 42);
        sa_opt_a.j = LSTR("plop");
        sa_opt_b.j = LSTR("plop");
        Z_ASSERT_IOPEQUAL_DESC(st_sa_opt, &sa_opt_a, &sa_opt_b);

        OPT_CLR(sa_opt_b.a);
        Z_ASSERT(!iop_equals(st_sa_opt, &sa_opt_a, &sa_opt_b));

        OPT_SET(sa_opt_b.a, 42);
        sa_opt_b.j = LSTR_NULL_V;
        Z_ASSERT(!iop_equals(st_sa_opt, &sa_opt_a, &sa_opt_b));

        sa_opt_b.j = LSTR("plop2");
        Z_ASSERT(!iop_equals(st_sa_opt, &sa_opt_a, &sa_opt_b));

        sa_opt_b.j = LSTR("plop");
        ua_a = IOP_UNION(tstiop__my_union_a, ua, 1);
        ua_b = IOP_UNION(tstiop__my_union_a, ua, 1);
        sa_opt_a.l = &ua_a;
        sa_opt_b.l = &ua_b;
        Z_ASSERT_IOPEQUAL_DESC(st_sa_opt, &sa_opt_a, &sa_opt_b);

        sa_opt_b.l = NULL;
        Z_ASSERT(!iop_equals(st_sa_opt, &sa_opt_a, &sa_opt_b));

        ua_b = IOP_UNION(tstiop__my_union_a, ub, 1);
        sa_opt_b.l = &ua_b;
        Z_ASSERT(!iop_equals(st_sa_opt, &sa_opt_a, &sa_opt_b));

        /* test with non initialized optional fields values */
        iop_init(st_sa_opt, &sa_opt_a);
        iop_init(st_sa_opt, &sa_opt_b);
        sa_opt_a.a.v = 42;
        Z_ASSERT_IOPEQUAL_DESC(st_sa_opt, &sa_opt_a, &sa_opt_b);

        /* Now test with some arrays */
        {
            lstr_t strs[] = { LSTR_IMMED("a"), LSTR_IMMED("b") };
            uint8_t uints[] = { 1, 2, 3, 4 };
            uint8_t uints2[] = { 1, 2, 4, 4 };

            iop_init(st_sr, &sr_a);
            iop_init(st_sr, &sr_b);
            Z_ASSERT_IOPEQUAL_DESC(st_sr, &sr_a, &sr_b);

            sr_a.s.tab = strs, sr_a.s.len = countof(strs);
            sr_b.s.tab = strs, sr_b.s.len = countof(strs);
            sr_a.u8.tab = uints, sr_a.u8.len = countof(uints);
            sr_b.u8.tab = uints, sr_b.u8.len = countof(uints);
            Z_ASSERT_IOPEQUAL_DESC(st_sr, &sr_a, &sr_b);

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
                                 LSTR("zchk-tstiop-plugin"SO_FILEEXT));

        const iop_struct_t *st_sl;

        if ((dso = iop_dso_open(path.s)) == NULL)
            Z_SKIP("unable to load zchk-tstiop-plugin, TOOLS repo?");

        Z_ASSERT_P(st_sl = iop_dso_find_type(dso, LSTR("tstiop.MyStructL")));

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
        tstiop_inheritance__c1__t c;

        lstr_t strings[] = {
            LSTR("fooBAR_1"),
            LSTR("foobar_2"),
            LSTR("foo3"),
            LSTR("foo4"),
            LSTR("foo5"),
            LSTR("foo6"),
        };

        lstr_t bad_strings[] = {
            LSTR("abcd[]"),
            LSTR("a b c"),
        };

        int8_t   i8tab[] = { INT8_MIN,  INT8_MAX,  3, 4, 5, 6 };
        int16_t i16tab[] = { INT16_MIN, INT16_MAX, 3, 4, 5, 6 };
        int32_t i32tab[] = { INT32_MIN, INT32_MAX, 3, 4, 5, 6 };
        int64_t i64tab[] = { INT64_MIN, INT64_MAX, 3, 4, 5, 6 };

        iop_dso_t *dso;
        lstr_t path = t_lstr_cat(z_cmddir_g,
                                 LSTR("zchk-tstiop-plugin"SO_FILEEXT));

        const iop_struct_t *st_s, *st_u, *st_c;

        if ((dso = iop_dso_open(path.s)) == NULL)
            Z_SKIP("unable to load zchk-tstiop-plugin, TOOLS repo?");

        Z_ASSERT_P(st_s = iop_dso_find_type(dso, LSTR("tstiop.ConstraintS")));
        Z_ASSERT_P(st_u = iop_dso_find_type(dso, LSTR("tstiop.ConstraintU")));
        Z_ASSERT_P(st_c = iop_dso_find_type(dso,
                                            LSTR("tstiop_inheritance.C1")));

        iop_init(st_u, &u);
        iop_init(st_s, &s);

#define CHECK_VALID(st, v, info) \
        Z_ASSERT_N(iop_check_constraints((st), (v)));                   \
        Z_HELPER_RUN(iop_std_test_struct((st), (v), (info)));           \
        Z_HELPER_RUN(iop_xml_test_struct((st), (v), (info)));           \
        Z_HELPER_RUN(iop_json_test_struct((st), (v), (info)));

#define CHECK_INVALID(st, v, info) \
        Z_ASSERT_NEG(iop_check_constraints((st), (v)));                 \
        Z_HELPER_RUN(iop_std_test_struct_invalid((st), (v), (info)));   \
        Z_HELPER_RUN(iop_xml_test_struct_invalid((st), (v), (info)));   \
        Z_HELPER_RUN(iop_json_test_struct_invalid((st), (v), (info)));

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
        u = IOP_UNION(tstiop__constraint_u, s, LSTR("way_too_long"));
        CHECK_INVALID(st_u, &u, "s_maxlength");
        u = IOP_UNION(tstiop__constraint_u, s, LSTR("ab.{}[]"));
        CHECK_INVALID(st_u, &u, "s_pattern");
        u = IOP_UNION(tstiop__constraint_u, s, LSTR("ab.{}()"));
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

        /* With inheritance */
        iop_init(st_c, &c);
        CHECK_VALID(st_c, &c, "c");
        c.a = 0;
        CHECK_INVALID(st_c, &c, "c");
        c.a = 2;
        c.c = 0;
        CHECK_INVALID(st_c, &c, "c");
        c.c = 3;
        CHECK_VALID(st_c, &c, "c");

#undef CHECK_TAB
#undef CHECK_UNION
#undef CHECK_VALID
#undef CHECK_INVALID

        iop_dso_close(&dso);
    } Z_TEST_END
    /* }}} */
    Z_TEST(iop_sort, "test IOP structures/unions sorting") { /* {{{ */
        t_scope;
        qv_t(my_struct_a) vec;
        tstiop__my_union_a__t un[5];
        tstiop__my_struct_a__t a;
        qv_t(my_struct_a_opt) vec2;
        tstiop__my_struct_a_opt__t a2;
        tstiop__my_struct_b__t b1, b2;
        tstiop__my_struct_m__t m;
        tstiop__my_class2__t cls2;
        qv_t(my_struct_m)  mvec;
        qv_t(my_class2) cls2_vec;

        qv_init(my_struct_a, &vec);
        tstiop__my_struct_a__init(&a);
        tstiop__my_class2__init(&cls2);

        un[0] = IOP_UNION(tstiop__my_union_a, ub, 42);
        a.e = 1;
        a.j = LSTR("xyz");
        a.l = IOP_UNION(tstiop__my_union_a, ua, 111);
        a.lr = &un[0];
        cls2.int1 = 10;
        cls2.int2 = 100;
        a.cls2 = tstiop__my_class2__dup(t_pool(), &cls2);
        qv_append(my_struct_a, &vec, a);

        un[1] = IOP_UNION(tstiop__my_union_a, ub, 23);
        a.e = 2;
        a.j = LSTR("abc");
        a.l = IOP_UNION(tstiop__my_union_a, ua, 666);
        a.lr = &un[1];
        cls2.int1 = 15;
        cls2.int2 = 95;
        a.cls2 = tstiop__my_class2__dup(t_pool(), &cls2);
        qv_append(my_struct_a, &vec, a);

        un[2] = IOP_UNION(tstiop__my_union_a, ua, 222);
        a.e = 3;
        a.j = LSTR("Jkl");
        a.l = IOP_UNION(tstiop__my_union_a, ua, 222);
        a.lr = &un[2];
        cls2.int1 = 13;
        cls2.int2 = 98;
        a.cls2 = tstiop__my_class2__dup(t_pool(), &cls2);
        qv_append(my_struct_a, &vec, a);

        un[3] = IOP_UNION(tstiop__my_union_a, ua, 666);
        a.e = 3;
        a.j = LSTR("jKl");
        a.l = IOP_UNION(tstiop__my_union_a, ub, 23);
        a.lr = &un[3];
        cls2.int1 = 14;
        cls2.int2 = 96;
        a.cls2 = tstiop__my_class2__dup(t_pool(), &cls2);
        qv_append(my_struct_a, &vec, a);

        un[4] = IOP_UNION(tstiop__my_union_a, ua, 111);
        a.e = 3;
        a.j = LSTR("jkL");
        a.l = IOP_UNION(tstiop__my_union_a, ub, 42);
        a.lr = &un[4];
        cls2.int1 = 16;
        cls2.int2 = 97;
        a.cls2 = tstiop__my_class2__dup(t_pool(), &cls2);
        qv_append(my_struct_a, &vec, a);

#define TST_SORT_VEC(p, f)  tstiop__my_struct_a__sort(vec.tab, vec.len, p, f)

        /* reverse sort on short e */
        Z_ASSERT_N(TST_SORT_VEC(LSTR("e"), IOP_SORT_REVERSE));
        Z_ASSERT_EQ(vec.tab[0].e, 3);
        Z_ASSERT_EQ(vec.tab[4].e, 1);

        /* sort on string j */
        Z_ASSERT_N(TST_SORT_VEC(LSTR("j"), 0));
        Z_ASSERT_LSTREQUAL(vec.tab[0].j, LSTR("abc"));
        Z_ASSERT_LSTREQUAL(vec.tab[4].j, LSTR("xyz"));

        /* sort on union l */
        Z_ASSERT_N(TST_SORT_VEC(LSTR("l"), 0));
        Z_ASSERT_P(IOP_UNION_GET(tstiop__my_union_a, &vec.tab[0].l, ua));
        Z_ASSERT_P(IOP_UNION_GET(tstiop__my_union_a, &vec.tab[4].l, ub));

        /* sort on int ua, member of union l */
        Z_ASSERT_N(TST_SORT_VEC(LSTR("l.ua"), 0));
        Z_ASSERT_P(IOP_UNION_GET(tstiop__my_union_a, &vec.tab[0].l, ua));
        Z_ASSERT_P(IOP_UNION_GET(tstiop__my_union_a, &vec.tab[1].l, ua));
        Z_ASSERT_P(IOP_UNION_GET(tstiop__my_union_a, &vec.tab[2].l, ua));
        Z_ASSERT_EQ(vec.tab[0].l.ua, 111);
        Z_ASSERT_EQ(vec.tab[1].l.ua, 222);
        Z_ASSERT_EQ(vec.tab[2].l.ua, 666);

        /* reverse sort on int ua, member of union l */
        Z_ASSERT_N(TST_SORT_VEC(LSTR("l.ua"), IOP_SORT_REVERSE));
        Z_ASSERT_P(IOP_UNION_GET(tstiop__my_union_a, &vec.tab[0].l, ua));
        Z_ASSERT_P(IOP_UNION_GET(tstiop__my_union_a, &vec.tab[1].l, ua));
        Z_ASSERT_P(IOP_UNION_GET(tstiop__my_union_a, &vec.tab[2].l, ua));
        Z_ASSERT_EQ(vec.tab[0].l.ua, 666);
        Z_ASSERT_EQ(vec.tab[1].l.ua, 222);
        Z_ASSERT_EQ(vec.tab[2].l.ua, 111);

        /* sort on int ua, member of union l, put other union members first */
        Z_ASSERT_N(TST_SORT_VEC(LSTR("l.ua"), IOP_SORT_NULL_FIRST));
        Z_ASSERT_P(IOP_UNION_GET(tstiop__my_union_a, &vec.tab[0].l, ub));
        Z_ASSERT_P(IOP_UNION_GET(tstiop__my_union_a, &vec.tab[1].l, ub));
        Z_ASSERT_EQ(vec.tab[2].l.ua, 111);
        Z_ASSERT_EQ(vec.tab[3].l.ua, 222);
        Z_ASSERT_EQ(vec.tab[4].l.ua, 666);

        /* reverse sort on int ua, member of union l, put other union members
         * first */
        Z_ASSERT_N(TST_SORT_VEC(LSTR("l.ua"),
                                IOP_SORT_NULL_FIRST | IOP_SORT_REVERSE));
        Z_ASSERT_P(IOP_UNION_GET(tstiop__my_union_a, &vec.tab[0].l, ub));
        Z_ASSERT_P(IOP_UNION_GET(tstiop__my_union_a, &vec.tab[1].l, ub));
        Z_ASSERT_EQ(vec.tab[2].l.ua, 666);
        Z_ASSERT_EQ(vec.tab[3].l.ua, 222);
        Z_ASSERT_EQ(vec.tab[4].l.ua, 111);

        /* sort on byte ub, member of union l, put other union members first */
        Z_ASSERT_N(TST_SORT_VEC(LSTR("l.ub"), IOP_SORT_NULL_FIRST));
        Z_ASSERT_P(IOP_UNION_GET(tstiop__my_union_a, &vec.tab[0].l, ua));
        Z_ASSERT_P(IOP_UNION_GET(tstiop__my_union_a, &vec.tab[1].l, ua));
        Z_ASSERT_P(IOP_UNION_GET(tstiop__my_union_a, &vec.tab[2].l, ua));
        Z_ASSERT_EQ(vec.tab[3].l.ua, 23);
        Z_ASSERT_EQ(vec.tab[4].l.ua, 42);

        /* sort on union lr */
        Z_ASSERT_N(TST_SORT_VEC(LSTR("lr"), 0));
        Z_ASSERT_P(IOP_UNION_GET(tstiop__my_union_a, vec.tab[0].lr, ua));
        Z_ASSERT_P(IOP_UNION_GET(tstiop__my_union_a, vec.tab[4].lr, ub));

        /* sort on int ua, member of union lr */
        Z_ASSERT_N(TST_SORT_VEC(LSTR("lr.ua"), 0));
        Z_ASSERT_P(IOP_UNION_GET(tstiop__my_union_a, vec.tab[0].lr, ua));
        Z_ASSERT_P(IOP_UNION_GET(tstiop__my_union_a, vec.tab[1].lr, ua));
        Z_ASSERT_P(IOP_UNION_GET(tstiop__my_union_a, vec.tab[2].lr, ua));
        Z_ASSERT_EQ(vec.tab[0].lr->ua, 111);
        Z_ASSERT_EQ(vec.tab[1].lr->ua, 222);
        Z_ASSERT_EQ(vec.tab[2].lr->ua, 666);

        /* reverse sort on int ua, member of union lr */
        Z_ASSERT_N(TST_SORT_VEC(LSTR("lr.ua"), IOP_SORT_REVERSE));
        Z_ASSERT_P(IOP_UNION_GET(tstiop__my_union_a, vec.tab[0].lr, ua));
        Z_ASSERT_P(IOP_UNION_GET(tstiop__my_union_a, vec.tab[1].lr, ua));
        Z_ASSERT_P(IOP_UNION_GET(tstiop__my_union_a, vec.tab[2].lr, ua));
        Z_ASSERT_EQ(vec.tab[0].lr->ua, 666);
        Z_ASSERT_EQ(vec.tab[1].lr->ua, 222);
        Z_ASSERT_EQ(vec.tab[2].lr->ua, 111);

        /* sort on int ua, member of union lr, put other union members first */
        Z_ASSERT_N(TST_SORT_VEC(LSTR("lr.ua"), IOP_SORT_NULL_FIRST));
        Z_ASSERT_P(IOP_UNION_GET(tstiop__my_union_a, vec.tab[0].lr, ub));
        Z_ASSERT_P(IOP_UNION_GET(tstiop__my_union_a, vec.tab[1].lr, ub));
        Z_ASSERT_EQ(vec.tab[2].lr->ua, 111);
        Z_ASSERT_EQ(vec.tab[3].lr->ua, 222);
        Z_ASSERT_EQ(vec.tab[4].lr->ua, 666);

        /* reverse sort on int ua, member of union lr, put other union members
         * first */
        Z_ASSERT_N(TST_SORT_VEC(LSTR("lr.ua"),
                                IOP_SORT_NULL_FIRST | IOP_SORT_REVERSE));
        Z_ASSERT_P(IOP_UNION_GET(tstiop__my_union_a, vec.tab[0].lr, ub));
        Z_ASSERT_P(IOP_UNION_GET(tstiop__my_union_a, vec.tab[1].lr, ub));
        Z_ASSERT_EQ(vec.tab[2].lr->ua, 666);
        Z_ASSERT_EQ(vec.tab[3].lr->ua, 222);
        Z_ASSERT_EQ(vec.tab[4].lr->ua, 111);

        /* sort on byte ub, member of union lr, put other union members first */
        Z_ASSERT_N(TST_SORT_VEC(LSTR("lr.ub"), IOP_SORT_NULL_FIRST));
        Z_ASSERT_P(IOP_UNION_GET(tstiop__my_union_a, vec.tab[0].lr, ua));
        Z_ASSERT_P(IOP_UNION_GET(tstiop__my_union_a, vec.tab[1].lr, ua));
        Z_ASSERT_P(IOP_UNION_GET(tstiop__my_union_a, vec.tab[2].lr, ua));
        Z_ASSERT_EQ(vec.tab[3].lr->ua, 23);
        Z_ASSERT_EQ(vec.tab[4].lr->ua, 42);

        /* sort on class members */
        Z_ASSERT_N(TST_SORT_VEC(LSTR("cls2.int1"), 0));
        Z_ASSERT_EQ(vec.tab[0].cls2->int1, 10);
        Z_ASSERT_EQ(vec.tab[1].cls2->int1, 13);
        Z_ASSERT_EQ(vec.tab[2].cls2->int1, 14);
        Z_ASSERT_EQ(vec.tab[3].cls2->int1, 15);
        Z_ASSERT_EQ(vec.tab[4].cls2->int1, 16);
        Z_ASSERT_N(TST_SORT_VEC(LSTR("cls2.int2"), 0));
        Z_ASSERT_EQ(vec.tab[0].cls2->int2, 95);
        Z_ASSERT_EQ(vec.tab[1].cls2->int2, 96);
        Z_ASSERT_EQ(vec.tab[2].cls2->int2, 97);
        Z_ASSERT_EQ(vec.tab[3].cls2->int2, 98);
        Z_ASSERT_EQ(vec.tab[4].cls2->int2, 100);

        /* error: empty field path */
        Z_ASSERT_NEG(TST_SORT_VEC(LSTR(""), 0));
        /* error: invalid field path */
        Z_ASSERT_NEG(TST_SORT_VEC(LSTR("."), 0));
        /* error: bar field does not exist */
        Z_ASSERT_NEG(TST_SORT_VEC(LSTR("bar"), 0));
        /* error: htab is a repeated field */
        Z_ASSERT_NEG(TST_SORT_VEC(LSTR("htab"), 0));

        qv_wipe(my_struct_a, &vec);
#undef TST_SORT_VEC

        qv_init(my_struct_a_opt, &vec2);
        tstiop__my_struct_a_opt__init(&a2);

        qv_append(my_struct_a_opt, &vec2, a2);
        OPT_SET(a2.a, 42);
        qv_append(my_struct_a_opt, &vec2, a2);
        OPT_SET(a2.a, 43);
        qv_append(my_struct_a_opt, &vec2, a2);
        OPT_CLR(a2.a);
        a2.j = LSTR("abc");
        a2.l = &IOP_UNION(tstiop__my_union_a, ua, 222);
        qv_append(my_struct_a_opt, &vec2, a2);
        a2.j = LSTR("def");
        a2.l = &IOP_UNION(tstiop__my_union_a, ub, 222);
        qv_append(my_struct_a_opt, &vec2, a2);
        a2.l = &IOP_UNION(tstiop__my_union_a, us, LSTR("xyz"));
        qv_append(my_struct_a_opt, &vec2, a2);

        tstiop__my_struct_b__init(&b1);
        OPT_SET(b1.a, 42);
        a2.o = &b1;
        qv_append(my_struct_a_opt, &vec2, a2);

        tstiop__my_struct_b__init(&b2);
        OPT_SET(b2.a, 72);
        a2.o = &b2;
        qv_append(my_struct_a_opt, &vec2, a2);

#define TST_SORT_VEC(p, f)  tstiop__my_struct_a_opt__sort(vec2.tab, vec2.len, p, f)

        /* sort on optional int a */
        Z_ASSERT_N(TST_SORT_VEC(LSTR("a"), 0));
        Z_ASSERT_EQ(OPT_VAL(vec2.tab[0].a), 42);
        Z_ASSERT_EQ(OPT_VAL(vec2.tab[1].a), 43);
        Z_ASSERT(!OPT_ISSET(vec2.tab[2].a));
        Z_ASSERT(!OPT_ISSET(vec2.tab[3].a));
        Z_ASSERT(!OPT_ISSET(vec2.tab[4].a));
        Z_ASSERT(!OPT_ISSET(vec2.tab[5].a));

        /* sort on optional string j */
        Z_ASSERT_N(TST_SORT_VEC(LSTR("j"), 0));
        Z_ASSERT_LSTREQUAL(vec2.tab[0].j, LSTR("abc"));
        Z_ASSERT_LSTREQUAL(vec2.tab[1].j, LSTR("def"));

        /* sort on optional union l */
        Z_ASSERT_N(TST_SORT_VEC(LSTR("l"), 0));
        Z_ASSERT_P(vec2.tab[0].l);
        Z_ASSERT_EQ(vec2.tab[0].l->ua, 222);

        /* sort on optional int a, member of optional struct MyStructB o */
        Z_ASSERT_N(TST_SORT_VEC(LSTR("o.a"), 0));
        Z_ASSERT_EQ(OPT_VAL(vec2.tab[0].o->a), 42);
        Z_ASSERT_EQ(OPT_VAL(vec2.tab[1].o->a), 72);

        /* error: cannot sort on struct */
        Z_ASSERT_NEG(TST_SORT_VEC(LSTR("o"), 0));

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
        Z_ASSERT_N(TST_SORT_VEC(LSTR("k.j.cval"), 0));
        Z_ASSERT_EQ(mvec.tab[0].k.j.cval, 3);
        Z_ASSERT_EQ(mvec.tab[1].k.j.cval, 4);
        Z_ASSERT_EQ(mvec.tab[2].k.j.cval, 5);

        /* sort on int bval from MyUnionB b from MyStructJ j from MyStructK k
         */
        Z_ASSERT_N(TST_SORT_VEC(LSTR("k.j.b.bval"), 0));
        Z_ASSERT_EQ(mvec.tab[0].k.j.b.bval, 33);
        Z_ASSERT_EQ(mvec.tab[1].k.j.b.bval, 44);
        Z_ASSERT_EQ(mvec.tab[2].k.j.b.bval, 55);

        qv_wipe(my_struct_m, &mvec);
#undef TST_SORT_VEC

        t_qv_init(my_class2, &cls2_vec, 3);

        cls2.int1 = 3;
        cls2.int2 = 4;
        qv_append(my_class2, &cls2_vec,
                  tstiop__my_class2__dup(t_pool(), &cls2));
        cls2.int1 = 2;
        cls2.int2 = 5;
        qv_append(my_class2, &cls2_vec,
                  tstiop__my_class2__dup(t_pool(), &cls2));
        cls2.int1 = 1;
        cls2.int2 = 6;
        qv_append(my_class2, &cls2_vec,
                  tstiop__my_class2__dup(t_pool(), &cls2));

#define TST_SORT_VEC(p, f)  \
        tstiop__my_class2__sort(cls2_vec.tab, cls2_vec.len, p, f)

        Z_ASSERT_N(TST_SORT_VEC(LSTR("int1"), 0));
        Z_ASSERT_EQ(cls2_vec.tab[0]->int1, 1);
        Z_ASSERT_EQ(cls2_vec.tab[1]->int1, 2);
        Z_ASSERT_EQ(cls2_vec.tab[2]->int1, 3);

        Z_ASSERT_N(TST_SORT_VEC(LSTR("int2"), 0));
        Z_ASSERT_EQ(cls2_vec.tab[0]->int2, 4);
        Z_ASSERT_EQ(cls2_vec.tab[1]->int2, 5);
        Z_ASSERT_EQ(cls2_vec.tab[2]->int2, 6);
#undef TST_SORT_VEC

    } Z_TEST_END;
    /* }}} */
    Z_TEST(iop_msort, "test IOP structures/unions multi sorting") { /* {{{ */
        t_scope;
        qv_t(my_struct_a) original;
        qv_t(my_struct_a) sorted;
        qv_t(iop_sort) params;

        t_qv_init(my_struct_a, &original, 3);
        t_qv_init(my_struct_a, &sorted, 3);
        t_qv_init(iop_sort, &params, 2);

        qv_growlen(my_struct_a, &original, 3);
        tstiop__my_struct_a__init(&original.tab[0]);
        tstiop__my_struct_a__init(&original.tab[1]);
        tstiop__my_struct_a__init(&original.tab[2]);

        original.tab[0].a = 1;
        original.tab[1].a = 2;
        original.tab[2].a = 3;

        original.tab[0].b = 1;
        original.tab[1].b = 1;
        original.tab[2].b = 2;

        original.tab[0].d = 3;
        original.tab[1].d = 2;
        original.tab[2].d = 1;


#define ADD_PARAM(_field, _flags)  do {                                      \
        qv_append(iop_sort, &params, ((iop_sort_t){                          \
            .field_path = LSTR(_field),                                      \
            .flags = _flags,                                                 \
        }));                                                                 \
    } while (0)

#define SORT_AND_CHECK(p1, p2, p3)  do {                                     \
        Z_ASSERT_ZERO(tstiop__my_struct_a__msort(sorted.tab, sorted.len,     \
                                                 &params));                  \
        Z_ASSERT_EQ(sorted.tab[0].a, original.tab[p1].a);                    \
        Z_ASSERT_EQ(sorted.tab[1].a, original.tab[p2].a);                    \
        Z_ASSERT_EQ(sorted.tab[2].a, original.tab[p3].a);                    \
        Z_ASSERT_EQ(sorted.tab[0].b, original.tab[p1].b);                    \
        Z_ASSERT_EQ(sorted.tab[1].b, original.tab[p2].b);                    \
        Z_ASSERT_EQ(sorted.tab[2].b, original.tab[p3].b);                    \
        Z_ASSERT_EQ(sorted.tab[0].d, original.tab[p1].d);                    \
        Z_ASSERT_EQ(sorted.tab[1].d, original.tab[p2].d);                    \
        Z_ASSERT_EQ(sorted.tab[2].d, original.tab[p3].d);                    \
    } while (0)

        /* Simple sort */
        qv_copy(my_struct_a, &sorted, &original);
        qv_clear(iop_sort, &params);
        ADD_PARAM("a", IOP_SORT_REVERSE);
        SORT_AND_CHECK(2, 1, 0);

        /* Double sort */
        qv_clear(iop_sort, &params);
        ADD_PARAM("b", 0);
        ADD_PARAM("d", 0);
        SORT_AND_CHECK(1, 0, 2);

        /* Double sort reverse on first */
        qv_clear(iop_sort, &params);
        ADD_PARAM("b", IOP_SORT_REVERSE);
        ADD_PARAM("d", 0);
        SORT_AND_CHECK(2, 1, 0);

        /* Double sort reverse on last */
        qv_clear(iop_sort, &params);
        ADD_PARAM("b", 0);
        ADD_PARAM("d", IOP_SORT_REVERSE);
        SORT_AND_CHECK(0, 1, 2);

#undef ADD_PARAM
#undef SORT_AND_CHECK

    } Z_TEST_END;
    /* }}} */
    Z_TEST(iop_copy_inv_tab, "iop_copy(): invalid tab pointer when len == 0") { /* {{{ */
        t_scope;

        tstiop__my_struct_b__t sb, *sb_dup;

        iop_dso_t *dso;
        lstr_t path = t_lstr_cat(z_cmddir_g,
                                 LSTR("zchk-tstiop-plugin"SO_FILEEXT));


        const iop_struct_t *st_sb;

        if ((dso = iop_dso_open(path.s)) == NULL)
            Z_SKIP("unable to load zchk-tstiop-plugin, TOOLS repo?");

        Z_ASSERT_P(st_sb = iop_dso_find_type(dso, LSTR("tstiop.MyStructB")));

        iop_init(st_sb, &sb);
        sb.b.tab = (void *)0x42;
        sb.b.len = 0;

        sb_dup = iop_dup(NULL, st_sb, &sb, NULL);
        Z_ASSERT_NULL(sb_dup->b.tab);
        Z_ASSERT_ZERO(sb_dup->b.len);

        p_delete(&sb_dup);

        iop_dso_close(&dso);
    } Z_TEST_END
    /* }}} */
    Z_TEST(iop_signature, "iop_compute/check_signature()") { /* {{{ */
        t_scope;
        tstiop__my_hashed__t a;
        tstiop__my_hashed_extended__t b;

        tstiop__my_hashed__init(&a);
        tstiop__my_hashed_extended__init(&b);

        Z_ASSERT_N(iop_check_signature(&tstiop__my_hashed__s, &a,
            t_iop_compute_signature(&tstiop__my_hashed__s, &a, 0), 0));
        Z_ASSERT_N(iop_check_signature(&tstiop__my_hashed__s, &a,
            t_iop_compute_signature(&tstiop__my_hashed__s, &a,
                                      IOP_HASH_SKIP_MISSING),
            IOP_HASH_SKIP_MISSING));
        Z_ASSERT_N(iop_check_signature(&tstiop__my_hashed__s, &a,
            t_iop_compute_signature(&tstiop__my_hashed__s, &a,
                                      IOP_HASH_SKIP_DEFAULT),
            IOP_HASH_SKIP_DEFAULT));

        Z_ASSERT_N(iop_check_signature(&tstiop__my_hashed__s, &a,
            t_iop_compute_signature(&tstiop__my_hashed_extended__s, &b,
                                      IOP_HASH_SKIP_MISSING |
                                      IOP_HASH_SKIP_DEFAULT),
            IOP_HASH_SKIP_MISSING | IOP_HASH_SKIP_DEFAULT));

        Z_ASSERT_NEG(iop_check_signature(&tstiop__my_hashed__s, &a,
            t_iop_compute_signature(&tstiop__my_hashed_extended__s, &b, 0),
            0));

        b.s = LSTR("default-value");
        Z_ASSERT_N(iop_check_signature(&tstiop__my_hashed__s, &a,
            t_iop_compute_signature(&tstiop__my_hashed_extended__s, &b,
                                      IOP_HASH_SKIP_MISSING |
                                      IOP_HASH_SKIP_DEFAULT),
            IOP_HASH_SKIP_MISSING | IOP_HASH_SKIP_DEFAULT));

        Z_ASSERT_NEG(iop_check_signature(&tstiop__my_hashed__s, &a,
            t_iop_compute_signature(&tstiop__my_hashed_extended__s, &b,
                                      IOP_HASH_SKIP_MISSING),
            IOP_HASH_SKIP_MISSING));

        Z_ASSERT_NEG(iop_check_signature(&tstiop__my_hashed__s, &a,
            t_iop_compute_signature(&tstiop__my_hashed_extended__s, &b,
                                      IOP_HASH_SKIP_DEFAULT),
            IOP_HASH_SKIP_DEFAULT));

        OPT_SET(b.b, 0);
        Z_ASSERT_NEG(iop_check_signature(&tstiop__my_hashed__s, &a,
            t_iop_compute_signature(&tstiop__my_hashed_extended__s, &b,
                                      IOP_HASH_SKIP_MISSING),
            IOP_HASH_SKIP_MISSING));

        OPT_CLR(b.b);
        b.c.tab = t_new(int, 1);
        b.c.len = 1;
        Z_ASSERT_NEG(iop_check_signature(&tstiop__my_hashed__s, &a,
            t_iop_compute_signature(&tstiop__my_hashed_extended__s, &b,
                                      IOP_HASH_SKIP_MISSING |
                                      IOP_HASH_SKIP_DEFAULT),
            IOP_HASH_SKIP_MISSING | IOP_HASH_SKIP_DEFAULT));

        b.c.len = 0;
        Z_ASSERT_N(iop_check_signature(&tstiop__my_hashed__s, &a,
            t_iop_compute_signature(&tstiop__my_hashed_extended__s, &b,
                                      IOP_HASH_SKIP_MISSING |
                                      IOP_HASH_SKIP_DEFAULT),
            IOP_HASH_SKIP_MISSING | IOP_HASH_SKIP_DEFAULT));

        b.d = 11;
        Z_ASSERT_NEG(iop_check_signature(&tstiop__my_hashed__s, &a,
            t_iop_compute_signature(&tstiop__my_hashed_extended__s, &b,
                                      IOP_HASH_SKIP_MISSING |
                                      IOP_HASH_SKIP_DEFAULT),
            IOP_HASH_SKIP_MISSING | IOP_HASH_SKIP_DEFAULT));
        Z_ASSERT_NEG(iop_check_signature(&tstiop__my_hashed__s, &a,
            t_iop_compute_signature(&tstiop__my_hashed_extended__s, &b, 0),
            0));
    } Z_TEST_END;
    /* }}} */
    Z_TEST(inheritance_basics, "test inheritance basic properties") { /* {{{ */
#define CHECK_PARENT(_type, _class_id)  \
        do {                                                                 \
            const iop_class_attrs_t *attrs;                                  \
                                                                             \
            attrs = tstiop_inheritance__##_type##__s.class_attrs;            \
            Z_ASSERT_P(attrs);                                               \
            Z_ASSERT_EQ(attrs->class_id, _class_id);                         \
            Z_ASSERT_NULL(attrs->parent);                                    \
        } while (0)

#define CHECK_CHILD(_type, _class_id, _parent)  \
        do {                                                                 \
            const iop_class_attrs_t *attrs;                                  \
                                                                             \
            attrs = tstiop_inheritance__##_type##__s.class_attrs;            \
            Z_ASSERT_EQ(attrs->class_id, _class_id);                         \
            Z_ASSERT(attrs->parent ==  &tstiop_inheritance__##_parent##__s); \
        } while (0)

        CHECK_PARENT(a1, 0);
        CHECK_CHILD(b1,  1, a1);
        CHECK_CHILD(b2,  65535, a1);
        CHECK_CHILD(c1,  3, b2);
        CHECK_CHILD(c2,  4, b2);

        CHECK_PARENT(a2, 0);
        CHECK_CHILD(b3,  1, a2);
        CHECK_CHILD(c3,  2, b3);
        CHECK_CHILD(c4,  3, b3);

        CHECK_PARENT(a3, 0);
        CHECK_CHILD(b4,  1, a3);
#undef CHECK_PARENT
#undef CHECK_CHILD
    } Z_TEST_END
    /* }}} */
    Z_TEST(inheritance_switch, "test IOP_(OBJ|CLASS)_SWITCH helpers") { /* {{{ */
        tstiop_inheritance__c1__t c1;
        bool matched = false;

        tstiop_inheritance__c1__init(&c1);
        Z_ASSERT_EQ(IOP_OBJ_CLASS_ID(&c1), 3);
        IOP_OBJ_EXACT_SWITCH(&c1) {
          IOP_OBJ_CASE(tstiop_inheritance__a1, &c1, a1) {
            Z_ASSERT(false);
          }
          IOP_OBJ_CASE(tstiop_inheritance__b1, &c1, b1) {
            Z_ASSERT(false);
          }
          IOP_OBJ_CASE(tstiop_inheritance__b2, &c1, b2) {
            Z_ASSERT(false);
          }
          IOP_OBJ_CASE(tstiop_inheritance__c1, &c1, ok) {
            Z_ASSERT_P(ok);
            Z_ASSERT(!matched);
            matched = true;
          }
          IOP_OBJ_CASE(tstiop_inheritance__c2, &c1, c2) {
            Z_ASSERT(false);
          }
          IOP_OBJ_EXACT_DEFAULT() {
            Z_ASSERT(false);
          }
        }
        Z_ASSERT(matched);

        matched = false;
        IOP_OBJ_EXACT_SWITCH(&c1) {
          IOP_OBJ_CASE(tstiop_inheritance__a1, &c1, a1) {
            Z_ASSERT(false);
          }
          IOP_OBJ_CASE(tstiop_inheritance__b1, &c1, b1) {
            Z_ASSERT(false);
          }
          IOP_OBJ_CASE(tstiop_inheritance__b2, &c1, b2) {
            Z_ASSERT(false);
          }
          IOP_OBJ_CASE(tstiop_inheritance__c2, &c1, c2) {
            Z_ASSERT(false);
          }
          IOP_OBJ_EXACT_DEFAULT() {
            Z_ASSERT(!matched);
            matched = true;
          }
        }
        Z_ASSERT(matched);

        matched = false;
        IOP_CLASS_EXACT_SWITCH(&tstiop_inheritance__c1__s) {
          case IOP_CLASS_ID(tstiop_inheritance__a1):
            Z_ASSERT(false);
            break;

          case IOP_CLASS_ID(tstiop_inheritance__b1):
            Z_ASSERT(false);
            break;

          case IOP_CLASS_ID(tstiop_inheritance__b2):
            Z_ASSERT(false);
            break;

          case IOP_CLASS_ID(tstiop_inheritance__c1):
            matched = true;
            break;

          case IOP_CLASS_ID(tstiop_inheritance__c2):
            Z_ASSERT(false);
            break;

          default:
            Z_ASSERT(false);
            break;
        }
        Z_ASSERT(matched);

        matched = false;
        IOP_OBJ_SWITCH(c1, &c1) {
          IOP_OBJ_CASE(tstiop_inheritance__a1, &c1, a1) {
            Z_ASSERT(false);
          }
          IOP_OBJ_CASE(tstiop_inheritance__b1, &c1, b1) {
            Z_ASSERT(false);
          }
          IOP_OBJ_CASE(tstiop_inheritance__b2, &c1, b2) {
            Z_ASSERT(false);
          }
          IOP_OBJ_CASE(tstiop_inheritance__c1, &c1, ok) {
            Z_ASSERT_P(ok);
            Z_ASSERT(!matched);
            matched = true;
          }
          IOP_OBJ_CASE(tstiop_inheritance__c2, &c1, c2) {
            Z_ASSERT(false);
          }
          IOP_OBJ_DEFAULT(c1) {
            Z_ASSERT(false);
          }
        }
        Z_ASSERT(matched);

        matched = false;
        IOP_CLASS_SWITCH(c1, c1.__vptr) {
          IOP_CLASS_CASE(tstiop_inheritance__a1) {
            Z_ASSERT(false);
          }
          IOP_CLASS_CASE(tstiop_inheritance__b1) {
            Z_ASSERT(false);
          }
          IOP_CLASS_CASE(tstiop_inheritance__b2) {
            Z_ASSERT(false);
          }
          IOP_CLASS_CASE(tstiop_inheritance__c1) {
            Z_ASSERT(!matched);
            matched = true;
          }
          IOP_CLASS_CASE(tstiop_inheritance__c2) {
            Z_ASSERT(false);
          }
          IOP_CLASS_DEFAULT(c1) {
            Z_ASSERT(false);
          }
        }
        Z_ASSERT(matched);

        matched = false;
        IOP_OBJ_SWITCH(c1, &c1) {
          IOP_OBJ_CASE(tstiop_inheritance__a1, &c1, a1) {
            Z_ASSERT(false);
          }
          IOP_OBJ_CASE(tstiop_inheritance__b1, &c1, b1) {
            Z_ASSERT(false);
          }
          IOP_OBJ_CASE(tstiop_inheritance__b2, &c1, b2) {
            Z_ASSERT(b2);
            Z_ASSERT(!matched);
            matched = true;
          }
          IOP_OBJ_CASE(tstiop_inheritance__c2, &c1, c2) {
            Z_ASSERT(false);
          }
          IOP_OBJ_DEFAULT(c1) {
            Z_ASSERT(false);
          }
        }
        Z_ASSERT(matched);

        matched = false;
        IOP_OBJ_SWITCH(c1, &c1) {
          IOP_OBJ_CASE(tstiop_inheritance__a1, &c1, a1) {
            Z_ASSERT(a1);
            Z_ASSERT(!matched);
            matched = true;
          }
          IOP_OBJ_CASE(tstiop_inheritance__b1, &c1, b1) {
            Z_ASSERT(false);
          }
          IOP_OBJ_CASE(tstiop_inheritance__c2, &c1, c2) {
            Z_ASSERT(false);
          }
          IOP_OBJ_DEFAULT(c1) {
            Z_ASSERT(false);
          }
        }
        Z_ASSERT(matched);

        matched = false;
        IOP_OBJ_SWITCH(c1, &c1) {
          IOP_OBJ_CASE(tstiop_inheritance__b1, &c1, b1) {
            Z_ASSERT(false);
          }
          IOP_OBJ_CASE(tstiop_inheritance__c2, &c1, c2) {
            Z_ASSERT(false);
          }
          IOP_OBJ_DEFAULT(c1) {
            Z_ASSERT(!matched);
            matched = true;
          }
        }
        Z_ASSERT(matched);
    } Z_TEST_END
    /* }}} */
    Z_TEST(inheritance_fields_init, "test fields initialization") { /* {{{ */
        {
            tstiop_inheritance__b1__t b1;

            tstiop_inheritance__b1__init(&b1);
            Z_ASSERT_EQ(b1.a, 1);
            Z_ASSERT_LSTREQUAL(b1.b, LSTR("b"));
        }
        {
            tstiop_inheritance__c1__t c1;

            tstiop_inheritance__c1__init(&c1);
            Z_ASSERT_EQ(c1.a, 1);
            Z_ASSERT_EQ(c1.b, true);
            Z_ASSERT_EQ(c1.c, (uint32_t)3);
        }
        {
            tstiop_inheritance__c2__t c2;

            tstiop_inheritance__c2__init(&c2);
            Z_ASSERT_EQ(c2.a, 1);
            Z_ASSERT_EQ(c2.b, true);
            Z_ASSERT_EQ(c2.c, 4);
        }
        {
            tstiop_inheritance__c3__t c3;

            tstiop_inheritance__c3__init(&c3);
            Z_ASSERT_LSTREQUAL(c3.a, LSTR("A2"));
            Z_ASSERT_EQ(c3.b, 5);
            Z_ASSERT_EQ(c3.c, 6);
        }
        {
            tstiop_inheritance__c4__t c4;

            tstiop_inheritance__c4__init(&c4);
            Z_ASSERT_LSTREQUAL(c4.a, LSTR("A2"));
            Z_ASSERT_EQ(c4.b, 5);
            Z_ASSERT_EQ(c4.c, false);
        }
    } Z_TEST_END
    /* }}} */
    Z_TEST(inheritance_casts, "test inheritance casts") { /* {{{ */
        tstiop_inheritance__c2__t  c2;
        tstiop_inheritance__c2__t *c2p;
        tstiop_inheritance__b2__t *b2p;
        uint8_t buf_b2p[20], buf_c2p[20];

        IOP_REGISTER_PACKAGES(&tstiop_inheritance__pkg);

#define CHECK_IS_A(_type1, _type2, _res)  \
        do {                                                                 \
            tstiop_inheritance__##_type1##__t obj;                           \
                                                                             \
            tstiop_inheritance__##_type1##__init(&obj);                      \
            Z_ASSERT(iop_obj_is_a(&obj,                                      \
                                  tstiop_inheritance__##_type2) == _res);    \
            Z_ASSERT(iop_obj_dynvcast(tstiop_inheritance__##_type2, &obj)    \
                     == (_res ? (void *)&obj : NULL));                       \
            Z_ASSERT(iop_obj_dynccast(tstiop_inheritance__##_type2, &obj)    \
                     == (_res ? (const void *)&obj : NULL));                 \
        } while (0)

        CHECK_IS_A(a1, a1, true);
        CHECK_IS_A(b1, a1, true);
        CHECK_IS_A(b1, b1, true);
        CHECK_IS_A(b2, a1, true);
        CHECK_IS_A(c1, b2, true);
        CHECK_IS_A(c1, a1, true);
        CHECK_IS_A(c2, b2, true);
        CHECK_IS_A(c2, a1, true);
        CHECK_IS_A(c3, b3, true);
        CHECK_IS_A(c3, a2, true);
        CHECK_IS_A(c4, b3, true);
        CHECK_IS_A(c4, a2, true);

        CHECK_IS_A(a1, b1, false);
        CHECK_IS_A(a1, a2, false);
        CHECK_IS_A(c1, c2, false);
#undef CHECK_IS_A

        /* Initialize a C2 class */
        tstiop_inheritance__c2__init(&c2);
        c2.a = 11111;
        c2.c = 500;

        /* Cast it in B2, and change some values */
        b2p = iop_obj_vcast(tstiop_inheritance__b2, &c2);
        Z_ASSERT_IOPEQUAL(tstiop_inheritance__b2, b2p, &c2.super);
        Z_HELPER_RUN(iop_std_test_struct(&tstiop_inheritance__b2__s, b2p,
                                         "b2p"));
        Z_ASSERT_EQ(b2p->a, 11111);
        Z_ASSERT_EQ(b2p->b, true);
        b2p->a = 22222;
        b2p->b = false;

        /* Re-cast it in C2, and check fields equality */
        c2p = iop_obj_vcast(tstiop_inheritance__c2, b2p);
        Z_ASSERT_IOPEQUAL(tstiop_inheritance__b2, b2p, &c2.super);
        Z_HELPER_RUN(iop_std_test_struct(&tstiop_inheritance__c2__s, c2p,
                                         "c2p"));
        Z_ASSERT_EQ(c2p->a, 22222);
        Z_ASSERT_EQ(c2p->b, false);
        Z_ASSERT_EQ(c2p->c, 500);

        /* Test that hashes of b2p and c2p are the sames */
        iop_hash_sha1(&tstiop_inheritance__b2__s, b2p, buf_b2p, 0);
        iop_hash_sha1(&tstiop_inheritance__c2__s, c2p, buf_c2p, 0);
        Z_ASSERT_EQUAL(buf_b2p, sizeof(buf_b2p), buf_c2p, sizeof(buf_c2p));
    } Z_TEST_END
    /* }}} */
    Z_TEST(inheritance_static, "test static class members") { /* {{{ */
        const iop_value_t *cvar;

#define CHECK_STATIC_STR(_type, _varname, _value)  \
        do {                                                                 \
            tstiop_inheritance__##_type##__t obj;                            \
                                                                             \
            tstiop_inheritance__##_type##__init(&obj);                       \
            cvar = iop_get_cvar_cst(&obj, _varname);                         \
            Z_ASSERT_P(cvar);                                                \
            Z_ASSERT_LSTREQUAL(cvar->s, LSTR(_value));                       \
        } while (0)

        CHECK_STATIC_STR(a1, "staticStr",  "a1");
        CHECK_STATIC_STR(b1, "staticStr",  "a1");
        CHECK_STATIC_STR(b2, "staticStr",  "a1");
        CHECK_STATIC_STR(c1, "staticStr",  "a1");
        CHECK_STATIC_STR(c2, "staticStr",  "c2");
        CHECK_STATIC_STR(c2, "staticStr1", "staticStr1");
        CHECK_STATIC_STR(c2, "staticStr2", "staticStr2");
        CHECK_STATIC_STR(c2, "staticStr3", "staticStr3");
        CHECK_STATIC_STR(c2, "staticStr4", "staticStr4");
        CHECK_STATIC_STR(c2, "staticStr5", "staticStr5");
        CHECK_STATIC_STR(c2, "staticStr6", "staticStr6");
        CHECK_STATIC_STR(c3, "staticStr",  "c3");
#undef CHECK_STATIC_STR

#define CHECK_STATIC(_type, _varname, _field, _value)  \
        do {                                                                 \
            tstiop_inheritance__##_type##__t obj;                            \
                                                                             \
            tstiop_inheritance__##_type##__init(&obj);                       \
            cvar = iop_get_cvar_cst(&obj, _varname);                         \
            Z_ASSERT_P(cvar);                                                \
            Z_ASSERT_EQ(cvar->_field, _value);                               \
        } while (0)

        CHECK_STATIC(a1, "staticEnum", i, MY_ENUM_A_B);
        CHECK_STATIC(b1, "staticInt", i, 12);
        CHECK_STATIC(c4, "staticInt", u, (uint64_t)44);
        CHECK_STATIC(b4, "staticInt", u, (uint64_t)4);

        CHECK_STATIC(b2, "staticBool", b, true);
        CHECK_STATIC(c1, "staticBool", b, false);
        CHECK_STATIC(c2, "staticBool", b, true);

        CHECK_STATIC(b3, "staticDouble", d, 23.0);
        CHECK_STATIC(c3, "staticDouble", d, 33.0);
        CHECK_STATIC(c4, "staticDouble", d, 23.0);
#undef CHECK_STATIC

#define CHECK_STATIC_UNDEFINED(_type, _varname)  \
        do {                                                                 \
            tstiop_inheritance__##_type##__t obj;                            \
                                                                             \
            tstiop_inheritance__##_type##__init(&obj);                       \
            Z_ASSERT_NULL(iop_get_cvar_cst(&obj, _varname));                 \
        } while (0)

        CHECK_STATIC_UNDEFINED(a1, "undefined");
        CHECK_STATIC_UNDEFINED(a1, "staticInt");
        CHECK_STATIC_UNDEFINED(a1, "staticBool");
        CHECK_STATIC_UNDEFINED(a1, "staticDouble");
        CHECK_STATIC_UNDEFINED(b1, "staticBool");
        CHECK_STATIC_UNDEFINED(b3, "staticBool");
#undef CHECK_STATIC_UNDEFINED

        {
            tstiop_inheritance__a3__t a3;

            a3.__vptr = &tstiop_inheritance__a3__s;
            Z_ASSERT_NULL(iop_get_cvar_cst(&a3, "staticInt"));
        }

        {
            tstiop_inheritance__a1__t a1;
            tstiop_inheritance__b1__t b1;

            a1.__vptr = &tstiop_inheritance__a1__s;
            b1.__vptr = &tstiop_inheritance__b1__s;
            Z_ASSERT(iop_get_cvar_cst(&a1, "staticStr"));
            Z_ASSERT(iop_get_cvar_cst(&b1, "staticStr"));
            cvar = iop_get_class_cvar_cst(&a1, "staticStr");
            Z_ASSERT_P(cvar);
            Z_ASSERT_LSTREQUAL(cvar->s, LSTR("a1"));
            Z_ASSERT_NULL(iop_get_class_cvar_cst(&b1, "staticStr"));
        }
    } Z_TEST_END
    /* }}} */
    Z_TEST(inheritance_equals, "test iop_equals/hash with inheritance") { /* {{{ */
        t_scope;
        tstiop_inheritance__c2__t c2_1_1, c2_1_2, c2_1_3;
        tstiop_inheritance__c2__t c2_2_1, c2_2_2, c2_2_3;
        tstiop_inheritance__b2__t b2_1, b2_2;
        tstiop_inheritance__class_container__t cc_1, cc_2;

        IOP_REGISTER_PACKAGES(&tstiop_inheritance__pkg);

        tstiop_inheritance__c2__init(&c2_1_1);
        tstiop_inheritance__c2__init(&c2_1_2);
        tstiop_inheritance__c2__init(&c2_1_3);
        tstiop_inheritance__c2__init(&c2_2_1);
        tstiop_inheritance__c2__init(&c2_2_2);
        tstiop_inheritance__c2__init(&c2_2_3);

        tstiop_inheritance__b2__init(&b2_1);
        tstiop_inheritance__b2__init(&b2_2);

        tstiop_inheritance__class_container__init(&cc_1);
        tstiop_inheritance__class_container__init(&cc_2);

        /* These tests rely on the fact that there are no hash collisions in
         * the test samples, which is the case.
         *
         * They are actually doing much more than just testing
         * iop_equals/hash: packing/unpacking in binary/json/xml is also
         * tested.
         */
#define CHECK_EQUALS(_type, _v1, _v2, _res)  \
        do {                                                                 \
            uint8_t buf1[20], buf2[20];                                      \
                                                                             \
            Z_ASSERT(iop_equals(&tstiop_inheritance__##_type##__s,           \
                                _v1, _v2) == _res);                          \
            iop_hash_sha1(&tstiop_inheritance__##_type##__s, _v1, buf1, 0);  \
            iop_hash_sha1(&tstiop_inheritance__##_type##__s, _v2, buf2, 0);  \
            Z_ASSERT(lstr_equal2(                                            \
                LSTR_INIT_V((const char *)buf1, sizeof(buf1)),               \
                LSTR_INIT_V((const char *)buf2, sizeof(buf2))) == _res);     \
            Z_HELPER_RUN(iop_std_test_struct(                                \
                &tstiop_inheritance__##_type##__s, _v1, TOSTR(_v1)));        \
            Z_HELPER_RUN(iop_std_test_struct(                                \
                &tstiop_inheritance__##_type##__s, _v2, TOSTR(_v2)));        \
            Z_HELPER_RUN(iop_json_test_struct(                               \
                &tstiop_inheritance__##_type##__s, _v1, TOSTR(_v1)));        \
            Z_HELPER_RUN(iop_json_test_struct(                               \
                &tstiop_inheritance__##_type##__s, _v2, TOSTR(_v2)));        \
            Z_HELPER_RUN(iop_xml_test_struct(                                \
                &tstiop_inheritance__##_type##__s, _v1, TOSTR(_v1)));        \
            Z_HELPER_RUN(iop_xml_test_struct(                                \
                &tstiop_inheritance__##_type##__s, _v2, TOSTR(_v2)));        \
        } while (0)

        /* ---- Tests with "simple" classes --- */
        CHECK_EQUALS(c2, &c2_1_1, &c2_2_1, true);

        /* Modify a field of "level A1" */
        c2_1_1.a = 2;
        CHECK_EQUALS(c2, &c2_1_1, &c2_2_1, false);
        c2_2_1.a = 2;
        CHECK_EQUALS(c2, &c2_1_1, &c2_2_1, true);

        /* Modify a field of "level B2" */
        c2_1_1.b = false;
        CHECK_EQUALS(c2, &c2_1_1, &c2_2_1, false);
        c2_2_1.b = false;
        CHECK_EQUALS(c2, &c2_1_1, &c2_2_1, true);

        /* Modify a field of "level C2" */
        c2_1_1.c = 8;
        CHECK_EQUALS(c2, &c2_1_1, &c2_2_1, false);
        c2_2_1.c = 8;
        CHECK_EQUALS(c2, &c2_1_1, &c2_2_1, true);

        /* ---- Test when modifying a non-scalar field ---- */
        {
            t_scope;
            tstiop_inheritance__c2__t *c2_1_4;

            /* With iop_dup */
            c2_1_1.a3 = t_lstr_fmt("a");
            c2_1_4 = tstiop_inheritance__c2__dup(t_pool(), &c2_1_1);
            CHECK_EQUALS(c2, &c2_1_1, c2_1_4, true);
            c2_1_1.a3.v[0] = 'b';
            CHECK_EQUALS(c2, &c2_1_1, c2_1_4, false);
            c2_1_4->a3.v[0] = 'b';
            CHECK_EQUALS(c2, &c2_1_1, c2_1_4, true);

            /* And with iop_copy */
            c2_1_1.a3 = t_lstr_fmt("c");
            tstiop_inheritance__c2__copy(t_pool(), &c2_1_4, &c2_1_1);
            CHECK_EQUALS(c2, &c2_1_1, c2_1_4, true);
            c2_1_1.a3.v[0] = 'd';
            CHECK_EQUALS(c2, &c2_1_1, c2_1_4, false);
            c2_1_4->a3.v[0] = 'd';
            CHECK_EQUALS(c2, &c2_1_1, c2_1_4, true);

            c2_1_1.a3 = LSTR_NULL_V;
        }

        /* ---- Tests with a class container --- */
        cc_1.a1 = iop_obj_vcast(tstiop_inheritance__a1, &c2_1_1);
        cc_2.a1 = iop_obj_vcast(tstiop_inheritance__a1, &c2_2_1);
        CHECK_EQUALS(class_container, &cc_1, &cc_2, true);

        /* Test mandatory field a1 */
        cc_1.a1->a = 3;
        CHECK_EQUALS(class_container, &cc_1, &cc_2, false);
        cc_2.a1->a = 3;
        CHECK_EQUALS(class_container, &cc_1, &cc_2, true);

        /* Test optional field b2 */
        cc_1.b2 = &b2_1;
        CHECK_EQUALS(class_container, &cc_1, &cc_2, false);
        cc_2.b2 = &b2_2;
        CHECK_EQUALS(class_container, &cc_1, &cc_2, true);
        cc_1.b2->a = 4;
        CHECK_EQUALS(class_container, &cc_1, &cc_2, false);
        cc_2.b2->a = 4;
        CHECK_EQUALS(class_container, &cc_1, &cc_2, true);
        cc_2.b2 = iop_obj_vcast(tstiop_inheritance__b2, &c2_2_1);
        CHECK_EQUALS(class_container, &cc_1, &cc_2, false);
        cc_2.b2 = &b2_2;

        /* Test repeated field c2 */
        cc_1.c2.tab = t_new(tstiop_inheritance__c2__t *, 2);
        cc_1.c2.tab[0] = &c2_1_2;
        cc_1.c2.len = 1;
        CHECK_EQUALS(class_container, &cc_1, &cc_2, false);
        cc_2.c2.tab = t_new(tstiop_inheritance__c2__t *, 2);
        cc_2.c2.tab[0] = &c2_2_2;
        cc_2.c2.len = 1;
        CHECK_EQUALS(class_container, &cc_1, &cc_2, true);
        c2_1_2.b = false;
        CHECK_EQUALS(class_container, &cc_1, &cc_2, false);
        c2_2_2.b = false;
        CHECK_EQUALS(class_container, &cc_1, &cc_2, true);
        cc_1.c2.tab[1] = &c2_1_3;
        cc_1.c2.len = 2;
        CHECK_EQUALS(class_container, &cc_1, &cc_2, false);
        cc_2.c2.tab[1] = &c2_2_3;
        cc_2.c2.len = 2;
        CHECK_EQUALS(class_container, &cc_1, &cc_2, true);
        c2_1_3.a = 5;
        CHECK_EQUALS(class_container, &cc_1, &cc_2, false);
        c2_2_3.a = 5;
        CHECK_EQUALS(class_container, &cc_1, &cc_2, true);
#undef CHECK_EQUALS

    } Z_TEST_END
    /* }}} */
    Z_TEST(inheritance_json, "test json unpacking inheritance") { /* {{{ */
        /* These tests are meant to check json unpacking in some unusual
         * conditions.
         * Packing and unpacking in usual conditions (ie. valid json packed by
         * our packer) is already stressed by the other tests.
         */
        t_scope;
        tstiop_inheritance__c1__t *c1 = NULL;
        tstiop_inheritance__d1__t *d1 = NULL;
        tstiop_inheritance__b2__t *b2 = NULL;
        tstiop_inheritance__a3__t *a3 = NULL;
        tstiop_inheritance__b4__t *b4 = NULL;
        tstiop_inheritance__class_container__t  *class_container  = NULL;
        tstiop_inheritance__class_container2__t *class_container2 = NULL;
        SB_1k(err);

        IOP_REGISTER_PACKAGES(&tstiop_inheritance__pkg);

#define CHECK_OK(_type, _filename)  \
        do {                                                                 \
            Z_ASSERT_N(t_tstiop_inheritance__##_type##__junpack_ptr_file(    \
                       t_fmt("%*pM/iop/" _filename,                          \
                             LSTR_FMT_ARG(z_cmddir_g)), &_type, 0, &err),    \
                       "junpack failed: %s", err.data);                      \
        } while (0)

        /* Test that fields can be in any order */
        CHECK_OK(c1, "tstiop_inheritance_valid1.json");
        Z_ASSERT(c1->__vptr == &tstiop_inheritance__c1__s);
        Z_ASSERT_EQ(c1->a,   2);
        Z_ASSERT_EQ(c1->a2, 12);
        Z_ASSERT_EQ(c1->b,  false);
        Z_ASSERT_EQ(c1->c,  (uint32_t)5);

        /* Test with missing optional fields */
        CHECK_OK(c1, "tstiop_inheritance_valid2.json");
        Z_ASSERT(c1->__vptr == &tstiop_inheritance__c1__s);
        Z_ASSERT_EQ(c1->a,   1);
        Z_ASSERT_EQ(c1->a2, 12);
        Z_ASSERT_EQ(c1->b,  true);
        Z_ASSERT_EQ(c1->c,  (uint32_t)3);

        /* Test that "_class" field can be missing */
        CHECK_OK(d1, "tstiop_inheritance_valid3.json");
        Z_ASSERT(d1->__vptr == &tstiop_inheritance__d1__s);
        Z_ASSERT_EQ(d1->a,  -12);
        Z_ASSERT_EQ(d1->a2, -15);
        Z_ASSERT_EQ(d1->b,  true);
        Z_ASSERT_EQ(d1->c,  (uint32_t)153);

        /* Test that missing mandatory class fields are OK if this class have
         * only optional fields.
         * Also check prefixed syntax on a class field. */
        CHECK_OK(class_container2, "tstiop_inheritance_valid4.json");
        Z_ASSERT_P(class_container2->a1);
        Z_ASSERT(class_container2->a1->__vptr == &tstiop_inheritance__a1__s);
        Z_ASSERT_EQ(class_container2->a1->a2, 10);
        Z_ASSERT_P(class_container2->b3);
        Z_ASSERT(class_container2->b3->__vptr == &tstiop_inheritance__b3__s);
        Z_ASSERT_LSTREQUAL(class_container2->b3->a, LSTR("A2"));
        Z_ASSERT_EQ(class_container2->b3->b, 5);
        Z_ASSERT(class_container2->a3->__vptr == &tstiop_inheritance__b4__s);
        b4 = iop_obj_vcast(tstiop_inheritance__b4, class_container2->a3);
        Z_ASSERT_EQ(b4->a3, 6);
        Z_ASSERT_EQ(b4->b4, 7);

        /* Test that "_class" field can be given using prefixed syntax */
        CHECK_OK(c1, "tstiop_inheritance_valid5.json");
        Z_ASSERT(c1->__vptr == &tstiop_inheritance__c1__s);
        Z_ASSERT_EQ(c1->a,  -480);
        Z_ASSERT_EQ(c1->a2, -479);
        Z_ASSERT_EQ(c1->b,  false);
        Z_ASSERT_EQ(c1->c,  (uint32_t)478);
#undef CHECK_OK

#define CHECK_FAIL(_type, _filename, _err)  \
        do {                                                                 \
            sb_reset(&err);                                                  \
            Z_ASSERT_NEG(t_tstiop_inheritance__##_type##__junpack_ptr_file(  \
                         t_fmt("%*pM/iop/" _filename,                        \
                               LSTR_FMT_ARG(z_cmddir_g)), &_type, 0, &err)); \
            Z_ASSERT(strstr(err.data, _err));                                \
        } while (0)

        /* Test that when the "_class" is missing, the expected type is the
         * wanted one */
        CHECK_FAIL(b2, "tstiop_inheritance_invalid1.json",
                   "expected field of struct tstiop_inheritance.B2, got "
                   "`\"c\"'");

        /* Test that the "_class" field is mandatory for abstract classes */
        CHECK_FAIL(a3, "tstiop_inheritance_invalid1.json",
                   "expected `_class' field, got `}'");

        /* Test with an unknown "_class" */
        CHECK_FAIL(c1, "tstiop_inheritance_invalid2.json",
                   "expected a child of `tstiop_inheritance.C1'");

        /* Test with an incompatible "_class" */
        CHECK_FAIL(c1, "tstiop_inheritance_invalid3.json",
                   "expected a child of `tstiop_inheritance.C1'");

        /* Test with a missing mandatory field */
        CHECK_FAIL(c1, "tstiop_inheritance_invalid4.json",
                   "member `tstiop_inheritance.A1:a2' is missing");
        CHECK_FAIL(class_container, "tstiop_inheritance_invalid5.json",
                   "member `tstiop_inheritance.ClassContainer:a1' is missing");
        CHECK_FAIL(class_container, "tstiop_inheritance_invalid6.json",
                   "member `tstiop_inheritance.ClassContainer:a1' is missing");

        /* Unpacking of abstract classes is forbidden */
        CHECK_FAIL(a3, "tstiop_inheritance_invalid7.json",
                   "expected a non-abstract class");

        /* Check that missing mandatory class fields, for classes having only
         * optional fields, is KO if this class is abstract (while it's ok if
         * it's not abstract, cf. test above).
         */
        CHECK_FAIL(class_container2, "tstiop_inheritance_invalid8.json",
                   "member `tstiop_inheritance.ClassContainer2:a3' is "
                   "missing");
#undef CHECK_FAIL
    } Z_TEST_END
    /* }}} */
    Z_TEST(inheritance_xml, "test inheritance and xml") { /* {{{ */
        /* These tests are meant to check XML unpacking in some unusual
         * conditions.
         * Packing and unpacking in usual conditions (ie. valid XML packed by
         * our packer) is already stressed by the other tests.
         */
        t_scope;
        lstr_t file;
        tstiop_inheritance__c2__t *c2 = NULL;
        tstiop_inheritance__c3__t *c3 = NULL;
        tstiop_inheritance__a3__t *a3 = NULL;

        IOP_REGISTER_PACKAGES(&tstiop_inheritance__pkg);

#define MAP(_filename)  \
        do {                                                                 \
            Z_ASSERT_N(lstr_init_from_file(&file,                            \
                           t_fmt("%*pM/iop/" _filename,                      \
                                 LSTR_FMT_ARG(z_cmddir_g)),                  \
                           PROT_READ, MAP_SHARED));                          \
        } while (0)

#define UNPACK_OK(_filename, _type)  \
        do {                                                                 \
            MAP(_filename);                                                  \
            Z_ASSERT_N(xmlr_setup(&xmlr_g, file.s, file.len));               \
            Z_ASSERT_N(t_iop_xunpack_ptr(xmlr_g,                             \
                                         &tstiop_inheritance__##_type##__s,  \
                                         (void **)&_type),                   \
                       "XML unpacking failure: %s", xmlr_get_err());         \
            lstr_wipe(&file);                                                \
        } while (0)

#define UNPACK_FAIL(_filename, _type, _err)  \
        do {                                                                 \
            MAP(_filename);                                                  \
            Z_ASSERT_N(xmlr_setup(&xmlr_g, file.s, file.len));               \
            Z_ASSERT_NEG(t_iop_xunpack_ptr(xmlr_g,                           \
                                &tstiop_inheritance__##_type##__s,           \
                                (void **)&_type));                           \
            Z_ASSERT(strstr(xmlr_get_err(), _err));                          \
            lstr_wipe(&file);                                                \
        } while (0)

        /* Test that 'xsi:type' can be missing, if the packed object is of
         * the expected type. */
        UNPACK_OK("tstiop_inheritance_valid1.xml", c2);
        Z_ASSERT(c2->__vptr == &tstiop_inheritance__c2__s);
        Z_ASSERT_EQ(c2->a,  15);
        Z_ASSERT_EQ(c2->a2, 16);
        Z_ASSERT_EQ(c2->b,  false);
        Z_ASSERT_EQ(c2->c,  18);

        /* Test with missing optional fields */
        UNPACK_OK("tstiop_inheritance_valid2.xml", c3);
        Z_ASSERT(c3->__vptr == &tstiop_inheritance__c3__s);
        Z_ASSERT_LSTREQUAL(c3->a, LSTR("I am the only field"));
        Z_ASSERT_EQ(c3->b, 5);
        Z_ASSERT_EQ(c3->c, 6);

        /* Test with no field at all (all are optional) */
        UNPACK_OK("tstiop_inheritance_valid3.xml", c3);
        Z_ASSERT(c3->__vptr == &tstiop_inheritance__c3__s);
        Z_ASSERT_LSTREQUAL(c3->a, LSTR("A2"));
        Z_ASSERT_EQ(c3->b, 5);
        Z_ASSERT_EQ(c3->c, 6);

        /* Test with fields in bad order */
        UNPACK_FAIL("tstiop_inheritance_invalid1.xml", c2,
                    "near /root/a: unknown tag <a>");
        UNPACK_FAIL("tstiop_inheritance_invalid2.xml", c2,
                    "near /root/b: missing mandatory tag <a2>");

        /* Test with an unknown field */
        UNPACK_FAIL("tstiop_inheritance_invalid3.xml", c2,
                    "near /root/toto: unknown tag <toto>");

        /* Test with a missing mandatory field */
        UNPACK_FAIL("tstiop_inheritance_invalid4.xml", c2,
                    "near /root: missing mandatory tag <a2>");

        /* Test with an unknown/incompatible class */
        UNPACK_FAIL("tstiop_inheritance_invalid5.xml", c2,
                    "near /root: class `tstiop_inheritance.Toto' not found");
        UNPACK_FAIL("tstiop_inheritance_invalid6.xml", c2,
                    "near /root: class `tstiop_inheritance.C1' is not a "
                    "child of `tstiop_inheritance.C2'");
        UNPACK_FAIL("tstiop_inheritance_invalid7.xml", a3,
                    "near /root: class `tstiop_inheritance.A3' is an "
                    "abstract class");

        /* 'xsi:type' is mandatory for abstract classes */
        UNPACK_FAIL("tstiop_inheritance_invalid8.xml", a3,
                    "near /root: type attribute not found (mandatory for "
                    "abstract classes)");
#undef UNPACK_OK
#undef UNPACK_FAIL
#undef MAP
    } Z_TEST_END
    /* }}} */
    Z_TEST(iop_references, "test iop references") { /* {{{ */
        t_scope;
        SB_1k(err);
        tstiop__my_referenced_struct__t rs = { .a = 666 };
        tstiop__my_referenced_union__t ru;
        tstiop__my_ref_union__t uu;
        tstiop__my_ref_union__t us;
        tstiop__my_ref_struct__t s = {
            .s = &rs,
            .u = &ru
        };

        uu = IOP_UNION(tstiop__my_ref_union, u, &ru);
        us = IOP_UNION(tstiop__my_ref_union, s, &rs);
        ru = IOP_UNION(tstiop__my_referenced_union, b, 42);

#define XUNPACK_OK(_type, _str)                                              \
        do {                                                                 \
            void *_type = NULL;                                              \
                                                                             \
            Z_ASSERT_N(xmlr_setup(&xmlr_g, _str, strlen(_str)));             \
            Z_ASSERT_N(t_iop_xunpack_ptr(xmlr_g, &tstiop__##_type##__s,      \
                                         &_type),                            \
                       "XML unpacking failure: %s", xmlr_get_err());         \
        } while (0)

#define XUNPACK_FAIL(_type, _str, _err)                                      \
        do {                                                                 \
            void *_type = NULL;                                              \
                                                                             \
            Z_ASSERT_N(xmlr_setup(&xmlr_g, _str, strlen(_str)));             \
            Z_ASSERT_NEG(t_iop_xunpack_ptr(xmlr_g, &tstiop__##_type##__s,    \
                                           &_type));                         \
            Z_ASSERT(strstr(xmlr_get_err(), _err), "%s", xmlr_get_err());    \
        } while (0)

#define JUNPACK_FAIL(_type, _str, _err)                                      \
        do {                                                                 \
            void *_type = NULL;                                              \
            pstream_t ps = ps_initstr(_str);                                 \
                                                                             \
            sb_reset(&err);                                                  \
            Z_ASSERT_NEG(t_iop_junpack_ptr_ps(&ps, &tstiop__##_type##__s,    \
                                              &_type, 0, &err));             \
            Z_ASSERT(strstr(err.data, _err), "%s", err.data);                \
        } while (0)

        Z_HELPER_RUN(iop_std_test_struct(&tstiop__my_ref_struct__s, &s, "s"));
        Z_HELPER_RUN(iop_json_test_struct(&tstiop__my_ref_struct__s, &s, "s"));
        Z_HELPER_RUN(iop_xml_test_struct(&tstiop__my_ref_struct__s, &s, "s"));
        XUNPACK_OK(my_ref_struct,
                   "<MyRefStruct><s><a>2</a></s><u><b>1</b></u></MyRefStruct>");
        XUNPACK_FAIL(my_ref_struct,
                     "<MyRefStruct><u><b>1</b></u></MyRefStruct>",
                     "missing mandatory tag <s>");
        XUNPACK_FAIL(my_ref_struct,
                     "<MyRefStruct><u><b>1</b></u></MyRefStruct>",
                     "missing mandatory tag <s>");
        XUNPACK_FAIL(my_ref_struct,
                     "<MyRefStruct><s></s></MyRefStruct>",
                     "missing mandatory tag <a>");
        Z_ASSERT_IOPJSONEQUAL(tstiop__my_ref_struct, &s,
                              LSTR("{ u: { b: 42 }, s: { a: 666 } }"));
        Z_ASSERT_IOPJSONEQUAL(tstiop__my_ref_struct, &s,
                              LSTR("{ u.b: 42, s: { a: 666 } }"));
        JUNPACK_FAIL(my_ref_struct, "{ u: { b: 1 } }",
                     "member `tstiop.MyRefStruct:s' is missing");
        JUNPACK_FAIL(my_ref_struct, "{ s: { a: 1 } }",
                     "member `tstiop.MyRefStruct:u' is missing");

        Z_HELPER_RUN(iop_std_test_struct(&tstiop__my_ref_union__s, &uu, "uu"));
        Z_HELPER_RUN(iop_json_test_struct(&tstiop__my_ref_union__s, &uu, "uu"));
        Z_HELPER_RUN(iop_xml_test_struct(&tstiop__my_ref_union__s, &uu, "uu"));
        Z_HELPER_RUN(iop_std_test_struct(&tstiop__my_ref_union__s, &us, "us"));
        Z_HELPER_RUN(iop_json_test_struct(&tstiop__my_ref_union__s, &us, "us"));
        Z_HELPER_RUN(iop_xml_test_struct(&tstiop__my_ref_union__s, &us, "us"));
        XUNPACK_OK(my_ref_union, "<MyRefUnion><s><a>2</a></s></MyRefUnion>");
        XUNPACK_OK(my_ref_union, "<MyRefUnion><u><b>2</b></u></MyRefUnion>");
        XUNPACK_FAIL(my_ref_union, "<MyRefUnion></MyRefUnion>",
                     "node has no children");
        XUNPACK_FAIL(my_ref_union, "<MyRefUnion><u></u></MyRefUnion>",
                     "node has no children");
        XUNPACK_FAIL(my_ref_union,
                     "<MyRefUnion><s><a>2</a></s><u><b>1</b></u></MyRefUnion>",
                     "closing tag expected");
        Z_ASSERT_IOPJSONEQUAL(tstiop__my_ref_union, &uu,
                              LSTR("{ u: { b: 42 } }"));
        Z_ASSERT_IOPJSONEQUAL(tstiop__my_ref_union, &uu,
                              LSTR("{ u.b: 42 }"));
        Z_ASSERT_IOPJSONEQUAL(tstiop__my_ref_union, &us,
                              LSTR("{ s: { a: 666 } }"));

#undef JUNPACK_FAIL
#undef XUNPACK_OK
#undef XUNPACK_FAIL
    } Z_TEST_END
    /* }}} */
    Z_TEST(iop_get_field_len, "test iop_get_field_len") { /* {{{ */
        t_scope;

        tstiop__my_class2__t cls2;
        tstiop__my_union_a__t ua = IOP_UNION(tstiop__my_union_a, ua, 1);
        tstiop__my_struct_a__t sa = {
            .a = 42,
            .b = 5,
            .c_of_my_struct_a = 120,
            .d = 230,
            .e = 540,
            .f = 2000,
            .g = 10000,
            .h = 20000,
            .i = LSTR_IMMED("foo"),
            .j = LSTR_IMMED("baré© \" foo ."),
            .k = MY_ENUM_A_B,
            .l = IOP_UNION(tstiop__my_union_a, ub, 42),
            .lr = &ua,
            .cls2 = &cls2,
            .m = 3.14159265,
            .n = true,
        };

        iop_dso_t *dso;
        lstr_t path = t_lstr_cat(z_cmddir_g,
                                 LSTR("zchk-tstiop-plugin"SO_FILEEXT));
        const iop_struct_t *st_sa, *st_cls2;
        qv_t(i32) szs;
        int len;
        byte *dst;
        pstream_t ps;

        if ((dso = iop_dso_open(path.s)) == NULL)
            Z_SKIP("unable to load zchk-tstiop-plugin, TOOLS repo?");

        Z_ASSERT_P(st_sa = iop_dso_find_type(dso, LSTR("tstiop.MyStructA")));
        Z_ASSERT_P(st_cls2 = iop_dso_find_type(dso, LSTR("tstiop.MyClass2")));

        t_qv_init(i32, &szs, 1024);
        iop_init(st_cls2, &cls2);

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
    Z_TEST(iop_class_for_each_field, "test iop_class_for_each_field") { /* {{{ */
        tstiop__my_class1__t cls1;
        tstiop__my_class2__t cls2;
        tstiop__my_class3__t cls3;
        const iop_struct_t *st;
        const iop_struct_t *st2;
        const iop_field_t  *f;
        const iop_field_t  *f2;
        int i = 0;

        tstiop__my_class1__init(&cls1);
        tstiop__my_class2__init(&cls2);
        tstiop__my_class3__init(&cls3);

#define TEST_FIELD(_f, _type, _name, _st, _class)                       \
        do {                                                            \
            Z_ASSERT_EQ(_f->type, IOP_T_##_type);                       \
            Z_ASSERT_LSTREQUAL(_f->name, LSTR(_name));                  \
            Z_ASSERT(_st == _class.__vptr);                             \
        } while (0)

        iop_obj_for_each_field(f, st, &cls3) {
            switch (i) {
              case 0:
                TEST_FIELD(f, I32, "int3", st, cls3);
                break;
              case 1:
                TEST_FIELD(f, BOOL, "bool1", st, cls3);
                break;
              case 2:
                TEST_FIELD(f, I32, "int2", st, cls2);
                break;
              case 3:
                TEST_FIELD(f, I32, "int1", st, cls1);
                break;
              default:
                Z_ASSERT(false);
            }
            i++;
        }
        Z_ASSERT_EQ(i, 4);

        i = 0;
        iop_obj_for_each_field(f, st, &cls2) {
            switch (i) {
              case 0:
                TEST_FIELD(f, I32, "int2", st, cls2);
                break;
              case 1:
                TEST_FIELD(f, I32, "int1", st, cls1);
                break;
              default:
                Z_ASSERT(false);
            }
            i++;
        }
        Z_ASSERT_EQ(i, 2);

        i = 0;
        iop_obj_for_each_field(f, st, &cls1) {
            TEST_FIELD(f, I32, "int1", st, cls1);
            Z_ASSERT_EQ(i, 0);
            i++;
        }

        /* Imbrication */
        i = 0;
        iop_obj_for_each_field(f, st, &cls3) {
            int j = 0;

            iop_obj_for_each_field(f2, st2, &cls1) {
                TEST_FIELD(f2, I32, "int1", st2, cls1);
                Z_ASSERT_EQ(j, 0);
                j++;
            }

            switch (i) {
              case 0:
                TEST_FIELD(f, I32, "int3", st, cls3);
                break;
              case 1:
                TEST_FIELD(f, BOOL, "bool1", st, cls3);
                break;
              case 2:
                TEST_FIELD(f, I32, "int2", st, cls2);
                break;
              case 3:
                TEST_FIELD(f, I32, "int1", st, cls1);
                break;
              default:
                Z_ASSERT(false);
            }
            i++;
        }

#undef TEST_FIELD

    } Z_TEST_END
    /* }}} */
    Z_TEST(iop_get_field, "test iop_get_field function") { /* {{{ */
        tstiop__my_struct_a__t struct_a;
        tstiop__my_struct_b__t struct_b;
        tstiop__my_struct_c__t struct_c;
        tstiop__my_struct_e__t struct_e;
        tstiop__my_class3__t cls3;
        tstiop__my_struct_a_opt__t struct_a_opt;
        tstiop__my_ref_struct__t struct_ref;
        const iop_field_t *iop_field;
        const void *out = NULL;

        tstiop__my_struct_a__init(&struct_a);
        tstiop__my_struct_b__init(&struct_b);
        tstiop__my_struct_c__init(&struct_c);
        tstiop__my_struct_e__init(&struct_e);
        tstiop__my_class3__init(&cls3);
        tstiop__my_struct_a_opt__init(&struct_a_opt);
        tstiop__my_ref_struct__init(&struct_ref);
        cls3.int3 = 10;
        cls3.int2 = 5;
        cls3.int1 = 2;
        cls3.bool1 = true;
        struct_a.a = 15;
        struct_a.j = LSTR("toto");
        struct_a.l = IOP_UNION(tstiop__my_union_a, ua, 25);
        struct_a.cls2 = iop_obj_vcast(tstiop__my_class2, &cls3);
        struct_c.b = &struct_c;
        struct_a_opt.l = &IOP_UNION(tstiop__my_union_a, ua, 10);
        OPT_SET(struct_e.c.a, 42);

        Z_ASSERT_NULL(iop_get_field(&struct_a, &tstiop__my_struct_a__s,
                                    LSTR("unknown_field"), NULL));
        Z_ASSERT_NULL(iop_get_field(&struct_a, &tstiop__my_struct_a__s,
                                    LSTR(""), NULL));
        Z_ASSERT_NULL(iop_get_field(&struct_a, &tstiop__my_struct_a__s,
                                    LSTR("."), NULL));
        Z_ASSERT_NULL(iop_get_field(&struct_a, &tstiop__my_struct_a__s,
                                    LSTR(".a"), NULL));
        Z_ASSERT_P(iop_get_field(&struct_a, &tstiop__my_struct_a__s,
                                 LSTR("l."), NULL));
        Z_ASSERT_NULL(iop_get_field(&struct_a, &tstiop__my_struct_a__s,
                                    LSTR("l.."), NULL));

        iop_field = iop_get_field(&struct_a, &tstiop__my_struct_a__s,
                                  LSTR("a"), &out);
        Z_ASSERT_P(iop_field);
        Z_ASSERT_P(out);
        Z_ASSERT_EQ(*(int *)out, struct_a.a);

        iop_field = iop_get_field(&struct_a, &tstiop__my_struct_a__s,
                                  LSTR("l"), &out);
        Z_ASSERT_P(iop_field);
        Z_ASSERT_P(out);

        iop_field = iop_get_field(&struct_a, &tstiop__my_struct_a__s,
                                  LSTR("l.ua"), &out);
        Z_ASSERT_P(iop_field);
        Z_ASSERT_P(out);
        Z_ASSERT_EQ(*(int *)out, struct_a.l.ua);

        iop_field = iop_get_field(&struct_a, &tstiop__my_struct_a__s,
                                  LSTR("cls2"), &out);
        Z_ASSERT_P(iop_field);
        Z_ASSERT_P(out);

        iop_field = iop_get_field(&struct_a, &tstiop__my_struct_a__s,
                                  LSTR("cls2.int2"), &out);
        Z_ASSERT_P(iop_field);
        Z_ASSERT_P(out);

        iop_field = iop_get_field(&struct_a, &tstiop__my_struct_a__s,
                                  LSTR("cls2.int1"), &out);
        Z_ASSERT_P(iop_field);
        Z_ASSERT_P(out);

        iop_field = iop_get_field(&struct_a, &tstiop__my_struct_a__s,
                                  LSTR("cls2.bool1"), &out);
        Z_ASSERT_P(iop_field);
        Z_ASSERT_P(out);

        iop_field = iop_get_field(&struct_a, &tstiop__my_struct_a__s,
                                  LSTR("j"), &out);
        Z_ASSERT_P(iop_field);
        Z_ASSERT_P(out);

        Z_ASSERT_NULL(iop_get_field(&struct_a, &tstiop__my_struct_a__s,
                                    LSTR("cls2.bool10"), NULL));

        iop_field = iop_get_field(&struct_e, &tstiop__my_struct_e__s,
                                  LSTR("c"), &out);
        Z_ASSERT_P(iop_field);
        Z_ASSERT_P(out);

        iop_field = iop_get_field(&struct_e, &tstiop__my_struct_e__s,
                                  LSTR("c.a"), &out);
        Z_ASSERT_P(iop_field);
        Z_ASSERT_P(out);

        iop_field = iop_get_field(&struct_b, &tstiop__my_struct_b__s,
                                  LSTR("a"), &out);
        Z_ASSERT_P(iop_field);
        Z_ASSERT_P(out);

        Z_ASSERT_NULL(iop_get_field(&struct_a, &tstiop__my_struct_a__s,
                                    LSTR("a.b"), NULL));

        iop_field = iop_get_field(&struct_a_opt, &tstiop__my_struct_a_opt__s,
                                  LSTR("l.ua"), &out);
        Z_ASSERT_P(iop_field);
        Z_ASSERT_P(out);

        iop_field = iop_get_field(&struct_c, &tstiop__my_struct_c__s,
                                  LSTR("b.a"), &out);
        Z_ASSERT_P(iop_field);
        Z_ASSERT_P(out);

        iop_field = iop_get_field(&struct_a, &tstiop__my_struct_a__s,
                                  LSTR("lr.ua"), &out);
        Z_ASSERT_P(iop_field);
        Z_ASSERT_P(out);

        iop_field = iop_get_field(&struct_ref, &tstiop__my_ref_struct__s,
                                  LSTR("s.a"), &out);
        Z_ASSERT_P(iop_field);
        Z_ASSERT_NULL(out);

        iop_field = iop_get_field(&struct_c, &tstiop__my_struct_c__s,
                                  LSTR("b.b.a"), &out);
        Z_ASSERT_P(iop_field);
        Z_ASSERT_P(out);
    } Z_TEST_END
    /* }}} */
    Z_TEST(iop_value_from_field, "test iop_value_from_field") { /* {{{ */
        tstiop__my_struct_g__t sg;
        const iop_struct_t *st;
        const iop_field_t *field;
        iop_value_t value;

        tstiop__my_struct_g__init(&sg);

        st = &tstiop__my_struct_g__s;

#define TEST_FIELD(_n, _type, _u, _res)                                    \
        field = &st->fields[_n];                                           \
        Z_ASSERT_N(iop_value_from_field((void *) &sg, field, &value));     \
        Z_ASSERT_EQ(value._u, (_type) _res)

        TEST_FIELD(0, int64_t, i, -1);
        TEST_FIELD(1, uint64_t, u, 2);
        TEST_FIELD(11, double, d, 10.5);

#undef TEST_FIELD

        field = &st->fields[9];
        Z_ASSERT_N(iop_value_from_field((void *) &sg, field, &value));
        Z_ASSERT_LSTREQUAL(value.s, LSTR("fo\"o?cbaré©"));

        /* test to get struct */
        {
            tstiop__my_struct_k__t sk;
            tstiop__my_struct_j__t *sj;

            tstiop__my_struct_k__init(&sk);

            sk.j.cval = 2314;
            st = &tstiop__my_struct_k__s;
            field = &st->fields[0];
            Z_ASSERT_N(iop_value_from_field((void *) &sk, field, &value));
            sj = value.s.data;
            Z_ASSERT_EQ(sj->cval, 2314);
        }

        /* test to get reference */
        {
            tstiop__my_ref_struct__t ref_st;
            tstiop__my_referenced_struct__t referenced_st;
            tstiop__my_referenced_struct__t *p;

            tstiop__my_ref_struct__init(&ref_st);
            tstiop__my_referenced_struct__init(&referenced_st);

            referenced_st.a = 23;
            ref_st.s = &referenced_st;

            st = &tstiop__my_ref_struct__s;
            field = &st->fields[0];
            Z_ASSERT_N(iop_value_from_field((void *) &ref_st, field, &value));
            p = value.s.data;
            Z_ASSERT_EQ(p->a, 23);
        }

        /* test to get optional */
        {
            tstiop__my_struct_b__t sb;

            tstiop__my_struct_b__init(&sb);
            OPT_SET(sb.a, 42);

            st = &tstiop__my_struct_b__s;
            field = &st->fields[0];
            Z_ASSERT_N(iop_value_from_field((void *) &sb, field, &value));
            Z_ASSERT_EQ(value.i, 42);
        }
    } Z_TEST_END
    /* }}} */
    Z_TEST(iop_value_to_field, "test iop_value_to_field") { /* {{{ */
        tstiop__my_struct_g__t sg;
        tstiop__my_struct_k__t sk;
        tstiop__my_struct_j__t sj;
        const iop_struct_t *st;
        const iop_field_t *field;
        iop_value_t value;

        tstiop__my_struct_g__init(&sg);
        tstiop__my_struct_k__init(&sk);
        tstiop__my_struct_j__init(&sj);

        st = &tstiop__my_struct_g__s;

        /* test with int */
        field = &st->fields[0];
        value.i = 2314;
        Z_ASSERT_N(iop_value_to_field((void *) &sg, field, &value));
        Z_ASSERT_EQ(sg.a, 2314);

        /* test with string */
        field = &st->fields[9];
        value.s = LSTR("fo\"o?cbaré©");
        Z_ASSERT_N(iop_value_to_field((void *) &sg, field, &value));
        Z_ASSERT_LSTREQUAL(sg.j, LSTR("fo\"o?cbaré©"));

        /* test struct */
        sj.cval = 42;
        value.p = &sj;
        st = &tstiop__my_struct_k__s;
        field = &st->fields[0];
        Z_ASSERT_N(iop_value_to_field((void *) &sk, field, &value));
        Z_ASSERT_EQ(sk.j.cval, 42);

        /* test to get reference */
        {
            t_scope;
            tstiop__my_ref_struct__t ref_st;
            tstiop__my_referenced_struct__t referenced_st;

            tstiop__my_ref_struct__init(&ref_st);
            tstiop__my_referenced_struct__init(&referenced_st);

            referenced_st.a = 23;
            ref_st.s = t_new(tstiop__my_referenced_struct__t, 1);
            tstiop__my_referenced_struct__init(ref_st.s);

            value.p = &referenced_st;

            st = &tstiop__my_ref_struct__s;
            field = &st->fields[0];
            Z_ASSERT_N(iop_value_to_field((void *) &ref_st, field, &value));
            Z_ASSERT_EQ(ref_st.s->a, 23);
        }

        /* test to get optional */
        {
            tstiop__my_struct_b__t sb;

            tstiop__my_struct_b__init(&sb);

            value.i = 42;
            st = &tstiop__my_struct_b__s;
            field = &st->fields[0];
            Z_ASSERT_N(iop_value_to_field((void *) &sb, field, &value));
            Z_ASSERT_EQ(*OPT_GET(&sb.a), 42);
        }
    } Z_TEST_END
    /* }}} */
    Z_TEST(iop_type_vector_to_iop_struct, "test IOP struct build") { /* {{{ */
        t_scope;
        SB_1k(json);
        SB_1k(err);
        iop_field_info_t info;
        qv_t(iop_field_info) fields_info;
        qv_t(lstr) fields_name;
        iop_struct_t *st;
        const iop_field_t *f;
        void *v, *vtest;
        pstream_t ps;

        qv_init(iop_field_info, &fields_info);
        qv_init(lstr, &fields_name);

#define POLULATE_FIELDSINFO(_type, _name, _repeated)                 \
        info.type = _type;                                           \
        info.name = _name;                                           \
        info.repeat = _repeated;                                     \
        qv_append(iop_field_info, &fields_info, info)

        POLULATE_FIELDSINFO(IOP_T_I8,     LSTR("f0"), IOP_R_REQUIRED);
        POLULATE_FIELDSINFO(IOP_T_I16,    LSTR("f1"), IOP_R_REQUIRED);
        POLULATE_FIELDSINFO(IOP_T_I32,    LSTR("f2"), IOP_R_REQUIRED);
        POLULATE_FIELDSINFO(IOP_T_I64,    LSTR("f3"), IOP_R_REQUIRED);
        POLULATE_FIELDSINFO(IOP_T_DOUBLE, LSTR("f4"), IOP_R_REQUIRED);
        POLULATE_FIELDSINFO(IOP_T_STRING, LSTR("f5"), IOP_R_REQUIRED);

        POLULATE_FIELDSINFO(IOP_T_I8,     LSTR("f6"), IOP_R_OPTIONAL);
        POLULATE_FIELDSINFO(IOP_T_I16,    LSTR("f7"), IOP_R_OPTIONAL);
        POLULATE_FIELDSINFO(IOP_T_I32,    LSTR("f8"), IOP_R_OPTIONAL);
        POLULATE_FIELDSINFO(IOP_T_I64,    LSTR("f9"), IOP_R_OPTIONAL);
        POLULATE_FIELDSINFO(IOP_T_DOUBLE, LSTR("f10"), IOP_R_OPTIONAL);
        POLULATE_FIELDSINFO(IOP_T_STRING, LSTR("f11"), IOP_R_OPTIONAL);

#undef POLULATE_FIELDSINFO

        st = iop_type_vector_to_iop_struct(NULL, LSTR("fullname"),
                                           &fields_info);

        Z_ASSERT_LSTREQUAL(st->fullname, LSTR("fullname"));
        Z_ASSERT_GE(st->size, 62);

#define GET_FIELD_FROM_NAME(_name)                                           \
        ({                                                                   \
            const iop_field_t *tmp_field = NULL;                             \
            const iop_struct_t *tmp_struct = NULL;                           \
            lstr_t name = LSTR(_name);                                       \
                                                                             \
            __iop_field_find_by_name(st, name.s, name.len,                   \
                                     &tmp_struct, &tmp_field);               \
            tmp_field;                                                       \
        })

        /* test fields */
        f = GET_FIELD_FROM_NAME("f0");
        Z_ASSERT_P(f);
        Z_ASSERT_EQ(f->type, IOP_T_I8);
        Z_ASSERT_EQ(f->size, 1);
        Z_ASSERT_EQ(f->tag, 1);

        f = GET_FIELD_FROM_NAME("f1");
        Z_ASSERT_P(f);
        Z_ASSERT_EQ(f->type, IOP_T_I16);
        Z_ASSERT_EQ(f->size, 2);
        Z_ASSERT_EQ(f->tag, 2);

        f = GET_FIELD_FROM_NAME("f2");
        Z_ASSERT_P(f);
        Z_ASSERT_EQ(f->type, IOP_T_I32);
        Z_ASSERT_EQ(f->size, 4);
        Z_ASSERT_EQ(f->tag, 3);

        f = GET_FIELD_FROM_NAME("f3");
        Z_ASSERT_P(f);
        Z_ASSERT_EQ(f->type, IOP_T_I64);
        Z_ASSERT_EQ(f->size, 8);
        Z_ASSERT_EQ(f->tag, 4);

        f = GET_FIELD_FROM_NAME("f4");
        Z_ASSERT_P(f);
        Z_ASSERT_EQ(f->type, IOP_T_DOUBLE);
        Z_ASSERT_EQ(f->size, 8);
        Z_ASSERT_EQ(f->tag, 5);

        f = GET_FIELD_FROM_NAME("f5");
        Z_ASSERT_P(f);
        Z_ASSERT_EQ(f->type, IOP_T_STRING);
        Z_ASSERT_EQ(f->size, sizeof(lstr_t));
        Z_ASSERT_EQ(f->tag, 6);

        f = GET_FIELD_FROM_NAME("f6");
        Z_ASSERT_P(f);
        Z_ASSERT_EQ(f->type, IOP_T_I8);
        Z_ASSERT_EQ(f->size, sizeof(opt_i8_t));
        Z_ASSERT_EQ(f->tag, 7);

        f = GET_FIELD_FROM_NAME("f7");
        Z_ASSERT_P(f);
        Z_ASSERT_EQ(f->type, IOP_T_I16);
        Z_ASSERT_EQ(f->size, sizeof(opt_i16_t));
        Z_ASSERT_EQ(f->tag, 8);

        f = GET_FIELD_FROM_NAME("f8");
        Z_ASSERT_P(f);
        Z_ASSERT_EQ(f->type, IOP_T_I32);
        Z_ASSERT_EQ(f->size, sizeof(opt_i32_t));
        Z_ASSERT_EQ(f->tag, 9);

        f = GET_FIELD_FROM_NAME("f9");
        Z_ASSERT_P(f);
        Z_ASSERT_EQ(f->type, IOP_T_I64);
        Z_ASSERT_EQ(f->size, sizeof(opt_i64_t));
        Z_ASSERT_EQ(f->tag, 10);

        f = GET_FIELD_FROM_NAME("f10");
        Z_ASSERT_P(f);
        Z_ASSERT_EQ(f->type, IOP_T_DOUBLE);
        Z_ASSERT_EQ(f->size, sizeof(opt_double_t));
        Z_ASSERT_EQ(f->tag, 11);

        f = GET_FIELD_FROM_NAME("f11");
        Z_ASSERT_P(f);
        Z_ASSERT_EQ(f->type, IOP_T_STRING);
        Z_ASSERT_EQ(f->size, sizeof(lstr_t));
        Z_ASSERT_EQ(f->tag, 12);

#undef GET_FIELD_FROM_NAME

        /* test if no field overlap another */
        for (int i = 0; i < st->fields_len - 1; i++) {
            Z_ASSERT(st->fields[i].data_offs + st->fields[i].size <=
                     st->fields[i + 1].data_offs);
        }

        /* test if the whole size of the structure is in a reasonable range. */
        {
            int sum_field_sizes = 0;
            int sum_max_field_sizes = 0;

            for (int i = 0; i < st->fields_len; i++) {
                sum_field_sizes += st->fields[i].size;
                sum_max_field_sizes +=
                    ROUND_UP(st->fields[i].size, sizeof(void *));
            }

            Z_ASSERT(st->size >= sum_field_sizes);
            Z_ASSERT(st->size <= sum_max_field_sizes);
        }

        /* ranges test */
        Z_ASSERT_EQ(st->fields_len, fields_info.len);
        Z_ASSERT_EQ(st->ranges[0], 0);
        Z_ASSERT_EQ(st->ranges[1], 1);
        Z_ASSERT_EQ(st->ranges[2], fields_info.len);

        /* test structure validity */
        v = t_iop_new_desc(st);
        Z_HELPER_RUN(iop_std_test_struct(st, v, ""));

        /* pack/unpack using JSON */
        Z_ASSERT_N(iop_sb_jpack(&json, st, v, 0));

        vtest = t_new_raw(byte, st->size);
        ps = ps_initsb(&json);
        Z_ASSERT_N(t_iop_junpack_ps(&ps, st, vtest, 0, &err));

        Z_ASSERT(iop_equals(st, v, vtest));

        /* clear */
        qv_wipe(iop_field_info, &fields_info);
        qv_wipe(lstr, &fields_name);
        p_delete(&st);
    } Z_TEST_END
    /* }}} */
    Z_TEST(iop_enum, "test iop enums") { /* {{{ */
        bool found = false;

        Z_ASSERT_EQ(tstiop__my_enum_a__from_str("A", -1, -1), MY_ENUM_A_A);
        Z_ASSERT_EQ(tstiop__my_enum_a__from_str("b", -1, -1), MY_ENUM_A_B);
        Z_ASSERT_EQ(tstiop__my_enum_a__from_str("c", -1, -1), MY_ENUM_A_C);

        Z_ASSERT_EQ(tstiop__my_enum_a__from_str2("A", -1, &found),
                    MY_ENUM_A_A);
        Z_ASSERT_EQ(tstiop__my_enum_a__from_str2("b", -1, &found),
                    MY_ENUM_A_B);
        Z_ASSERT_EQ(tstiop__my_enum_a__from_str2("c", -1, &found),
                    MY_ENUM_A_C);

        Z_ASSERT_EQ(tstiop__my_enum_a__from_lstr(LSTR("A"), &found),
                    MY_ENUM_A_A);
        Z_ASSERT_EQ(tstiop__my_enum_a__from_lstr(LSTR("b"), &found),
                    MY_ENUM_A_B);
        Z_ASSERT_EQ(tstiop__my_enum_a__from_lstr(LSTR("c"), &found),
                    MY_ENUM_A_C);
    } Z_TEST_END
    /* }}} */
    Z_TEST(iop_gen_attrs, "test iop generic attributes") { /* {{{ */
        iop_value_t value;
        iop_type_t type;

        /* enum */
        Z_ASSERT_N(iop_enum_get_gen_attr(&tstiop__my_enum_a__e,
                                         LSTR("test:gen1"), IOP_T_I8, NULL,
                                         &value));
        Z_ASSERT_EQ(value.i, 1);
        /* wrong type */
        Z_ASSERT_NEG(iop_enum_get_gen_attr(&tstiop__my_enum_a__e,
                                           LSTR("test:gen1"), IOP_T_STRING,
                                           &type, &value));
        Z_ASSERT_EQ(type, (iop_type_t)IOP_T_I64);
        Z_ASSERT_NEG(iop_enum_get_gen_attr(&tstiop__my_enum_a__e,
                                           LSTR("test:gen2"), IOP_T_I8, NULL,
                                           &value));

        /* enum values */
        Z_ASSERT_N(iop_enum_get_gen_attr_from_str(
            &tstiop__my_enum_a__e, LSTR("A"), LSTR("test:gen2"), IOP_T_DOUBLE,
            NULL, &value));
        Z_ASSERT_EQ(value.d, 2.2);
        Z_ASSERT_N(iop_enum_get_gen_attr_from_str(
            &tstiop__my_enum_a__e, LSTR("a"), LSTR("test:gen2"), IOP_T_DOUBLE,
            NULL, &value));
        Z_ASSERT_EQ(value.d, 2.2);
        Z_ASSERT_N(iop_enum_get_gen_attr_from_val(
            &tstiop__my_enum_a__e, 0, LSTR("test:gen2"), IOP_T_DOUBLE, NULL,
            &value));
        Z_ASSERT_EQ(value.d, 2.2);
        /* wrong type */
        Z_ASSERT_NEG(iop_enum_get_gen_attr_from_val(&tstiop__my_enum_a__e, 0,
                                                    LSTR("test:gen2"),
                                                    IOP_T_I64, &type, &value));
        Z_ASSERT_EQ(type, (iop_type_t)IOP_T_DOUBLE);

        Z_ASSERT_NEG(iop_enum_get_gen_attr_from_str(
            &tstiop__my_enum_a__e, LSTR("b"), LSTR("test:gen2"), IOP_T_I8,
            NULL, &value));
        Z_ASSERT_NEG(iop_enum_get_gen_attr_from_val(&tstiop__my_enum_a__e, 1,
                                                    LSTR("test:gen2"),
                                                    IOP_T_I8, NULL, &value));

        /* struct */
        Z_ASSERT_N(iop_struct_get_gen_attr(&tstiop__my_struct_a__s,
                                           LSTR("test:gen3"), IOP_T_STRING,
                                           NULL, &value));
        Z_ASSERT_LSTREQUAL(value.s, LSTR("3"));
        /* wrong type */
        Z_ASSERT_NEG(iop_struct_get_gen_attr(&tstiop__my_struct_a__s,
                                             LSTR("test:gen3"), IOP_T_I8,
                                             &type, &value));
        Z_ASSERT_EQ(type, (iop_type_t)IOP_T_STRING);
        Z_ASSERT_NEG(iop_struct_get_gen_attr(&tstiop__my_struct_a__s,
                                             LSTR("test:gen1"), IOP_T_I8,
                                             NULL, &value));

        /* struct field */
        Z_ASSERT_N(iop_field_by_name_get_gen_attr(
            &tstiop__my_struct_a__s, LSTR("a"), LSTR("test:gen4"), IOP_T_I16,
            NULL, &value));
        Z_ASSERT_EQ(value.i, 4);
        Z_ASSERT_NEG(iop_field_by_name_get_gen_attr(
            &tstiop__my_struct_a__s, LSTR("a"), LSTR("test:gen1"), IOP_T_I32,
            NULL, &value));

        /* iface */
        Z_ASSERT_N(iop_iface_get_gen_attr(&tstiop__my_iface_a__if,
                                          LSTR("test:gen5"), IOP_T_U8, NULL,
                                          &value));
        Z_ASSERT_EQ(value.i, 5);
        Z_ASSERT_NEG(iop_iface_get_gen_attr(&tstiop__my_iface_a__if,
                                            LSTR("test:gen1"), IOP_T_U16,
                                            NULL, &value));

        /* rpc */
        Z_ASSERT_N(iop_rpc_get_gen_attr(
            &tstiop__my_iface_a__if, tstiop__my_iface_a__fun_a__rpc,
            LSTR("test:gen6"), IOP_T_U32, NULL, &value));
        Z_ASSERT_EQ(value.i, 6);
        Z_ASSERT_NEG(iop_rpc_get_gen_attr(
            &tstiop__my_iface_a__if, tstiop__my_iface_a__fun_a__rpc,
            LSTR("test:gen1"), IOP_T_U64, NULL, &value));

        /* json object */
        Z_ASSERT_N(iop_struct_get_gen_attr(&tstiop__my_struct_a__s,
                                           LSTR("test:json"), IOP_T_STRING,
                                           NULL, &value));
        Z_ASSERT_STREQUAL(value.s.s,
            "{\"field\":{\"f1\":\"val1\",\"f2\":-1.00000000000000000e+02}}");
    } Z_TEST_END
    /* }}} */
    Z_TEST(iop_new, "test iop_new and sisters") { /* {{{ */
        t_scope;
        tstiop__my_struct_g__t  g;
        tstiop__my_struct_g__t *gp;

        tstiop__my_struct_g__init(&g);

        gp = mp_iop_new_desc(NULL, &tstiop__my_struct_g__s);
        Z_ASSERT(tstiop__my_struct_g__equals(&g, gp));
        p_delete(&gp);

        gp = t_iop_new_desc(&tstiop__my_struct_g__s);
        Z_ASSERT(tstiop__my_struct_g__equals(&g, gp));

        gp = mp_iop_new(NULL, tstiop__my_struct_g);
        Z_ASSERT(tstiop__my_struct_g__equals(&g, gp));
        p_delete(&gp);

        gp = iop_new(tstiop__my_struct_g);
        Z_ASSERT(tstiop__my_struct_g__equals(&g, gp));
        p_delete(&gp);

        gp = t_iop_new(tstiop__my_struct_g);
        Z_ASSERT(tstiop__my_struct_g__equals(&g, gp));
    } Z_TEST_END
    /* }}} */
    Z_TEST(nr_47521, "test bug while unpacking json with bunpack") { /* {{{ */
        /* test that bunpack does not crash when trying to unpack json */
        t_scope;
        SB_1k(sb);
        tstiop__my_struct_b__t b;
        tstiop__my_class1__t c;
        tstiop__my_class1__t *c_ptr;

        iop_init(&tstiop__my_struct_b__s, &b);
        Z_ASSERT_N(iop_sb_jpack(&sb, &tstiop__my_struct_b__s, &b, 0));
        Z_ASSERT_NEG(t_iop_bunpack(&LSTR_SB_V(&sb), tstiop, my_struct_b, &b));

        iop_init(&tstiop__my_class1__s, &c);
        Z_ASSERT_N(iop_sb_jpack(&sb, &tstiop__my_class1__s, &c, 0));
        Z_ASSERT_NEG(iop_bunpack_ptr(t_pool(), &tstiop__my_class1__s,
                                     (void **)&c_ptr, ps_initsb(&sb), false));
    } Z_TEST_END;
    /* }}} */
    Z_TEST(repeated_field_removal, "repeated field removal") { /* {{{ */
        t_scope;
        lstr_t data;
        pstream_t data_ps;
        struct_with_repeated_field__t st;
        struct_without_repeated_field__t *out = NULL;
        lstr_t tab[] = { LSTR_IMMED("toto"), LSTR_IMMED("foo") };

        Z_TEST_FLAGS("redmine_54728");

        struct_with_repeated_field__init(&st);
        st.a = 42;
        st.b.tab = tab;
        st.b.len = countof(tab);
        st.c = 999;

        data = t_iop_bpack_struct(&struct_with_repeated_field__s, &st);
        Z_ASSERT_P(data.s);
        data_ps = ps_initlstr(&data);
        Z_ASSERT_N(iop_bunpack_ptr(t_pool(),
                                   &struct_without_repeated_field__s,
                                   (void **)&out, data_ps, false),
                   "unexpected backward incompatibility for repeated field "
                   "removal: %s", iop_get_err());
        Z_ASSERT_EQ(st.a, out->a);
        Z_ASSERT_EQ(st.c, out->c);
    } Z_TEST_END;
    /* }}} */

} Z_GROUP_END
