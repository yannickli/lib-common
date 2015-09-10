/**************************************************************************/
/*                                                                        */
/*  Copyright (C) 2004-2015 INTERSEC SA                                   */
/*                                                                        */
/*  Should you receive a copy of this source code, you must check you     */
/*  have a proper, written authorization of INTERSEC to hold it. If you   */
/*  don't have such an authorization, you must DELETE all source code     */
/*  files in your possession, and inform INTERSEC of the fact you obtain  */
/*  these files. Should you not comply to these terms, you can be         */
/*  prosecuted in the extent permitted by applicable law.                 */
/*                                                                        */
/**************************************************************************/

#include "iop.h"
#include "iop-snmp-doc.h"

#include <sysexits.h>
#include <lib-common/parseopt.h>

static struct {
    logger_t   logger;
    bool       help;
} doc_g = {
#define _G  doc_g
    .logger = LOGGER_INIT_INHERITS(NULL, "snmp-doc"),
};

/* {{{ Helpers */

static lstr_t t_split_camelcase_word(lstr_t s)
{
    t_SB(buf, s.len + 1);

    sb_addc(&buf, toupper(s.s[0]));
    for (int i = 1; i < s.len; i++) {
        if (i && isupper(s.s[i])) {
            sb_addc(&buf, ' ');
        }
        sb_addc(&buf, s.s[i]);
    }

    return LSTR_SB_V(&buf);
}

static lstr_t t_get_short_name(const lstr_t fullname)
{
    lstr_t name = t_lstr_dup(fullname);
    pstream_t obj_name = ps_initlstr(&name);

    if (ps_skip_afterlastchr(&obj_name, '.') < 0) {
        logger_panic(&_G.logger, "fullname `%*pM` should be at least "
                     "composed by `pkg.name`", LSTR_FMT_ARG(fullname));
    }
    return LSTR_PS_V(&obj_name);
}

static lstr_t t_get_name_full_up(const lstr_t fullname)
{
    lstr_t out = t_lstr_dup(fullname);

    for (int i = 0; i < out.len ; i++) {
        out.v[i] = toupper((unsigned char)out.v[i]);
    }
    return out;
}

static lstr_t t_field_get_help(const iop_field_attrs_t *attrs)
{
    const iop_field_attr_t *attr = attrs->attrs;

    for (int i = 0; i < attrs->attrs_len; i++) {
        if (attr[i].type == IOP_FIELD_ATTR_HELP) {
            const iop_help_t *help = attr[i].args->v.p;

            return t_lstr_cat3(help->brief, help->details,
                               help->warning);
        }
    }
    return LSTR_EMPTY_V;
}

static lstr_t t_rpc_get_help(const iop_rpc_attrs_t *attrs)
{
    const iop_rpc_attr_t *attr = attrs->attrs;

    for (int i = 0; i < attrs->attrs_len; i++) {
        if (attr[i].type == IOP_RPC_ATTR_HELP) {
            const iop_help_t *help = attr[i].args->v.p;

            return t_lstr_cat3(help->brief, help->details,
                               help->warning);
        }
    }
    return LSTR_EMPTY_V;
}

static lstr_t t_struct_get_help(const iop_struct_attrs_t *attrs)
{
    const iop_struct_attr_t *attr = attrs->attrs;

    for (int i = 0; i < attrs->attrs_len; i++) {
        if (attr[i].type == IOP_STRUCT_ATTR_HELP) {
            const iop_help_t *help = attr[i].args->v.p;

            return t_lstr_cat3(help->brief, help->details,
                               help->warning);
        }
    }
    return LSTR_EMPTY_V;
}

static lstr_t t_field_get_help_without_dot(const iop_field_attrs_t *attrs)
{
    lstr_t help = t_field_get_help(attrs);

    if (lstr_endswithc(help, '.')) {
        t_SB(buf, help.len + 1);

        sb_addc(&buf, tolower(help.s[0]));

        for (int i = 1; i < help.len - 1; i++) {
            sb_addc(&buf, help.s[i]);
        }

        return LSTR_SB_V(&buf);
    }

    return help;
}


static const iop_field_t *iop_get_field_match_oid(const iop_struct_t *st,
                                                  uint8_t tag)
{
    for (int i = 0; i < st->fields_len; i++) {
        const iop_field_t *field = &st->fields[i];

        if (field->tag == tag) {
            return field;
        }
    }
    logger_panic(&_G.logger, "no field matches wanted OID %u", tag);
}


static lstr_t t_struct_build_oid(qv_t(u16) oids,
                                 const iop_struct_t *snmp_obj)
{
    t_SB(sb, 128);

    assert (iop_struct_is_snmp_obj(snmp_obj) ||
            iop_struct_is_snmp_tbl(snmp_obj));

    do {
        qv_append(u16, &oids, snmp_obj->snmp_attrs->oid);
    } while ((snmp_obj = snmp_obj->snmp_attrs->parent));

    qv_for_each_pos_rev(u16, pos, &oids) {
        sb_addf(&sb, ".%d", oids.tab[pos]);
    }
    return LSTR_SB_V(&sb);
}

static lstr_t t_notif_build_oid(const iop_struct_t *notif,
                                const iop_iface_t *parent)
{
    qv_t(u16) oids;

    t_qv_init(u16, &oids, 16);
    qv_append(u16, &oids, notif->snmp_attrs->oid);
    qv_append(u16, &oids, parent->snmp_iface_attrs->oid);

    return t_struct_build_oid(oids, parent->snmp_iface_attrs->parent);
}

static lstr_t t_field_build_oid(const iop_field_t *field,
                                const iop_struct_t *parent)
{
    qv_t(u16) oids;

    t_qv_init(u16, &oids, 16);
    qv_append(u16, &oids, field->tag);

    return t_struct_build_oid(oids, parent);
}

static const
iop_snmp_attrs_t *doc_field_get_snmp_attr(const iop_field_attrs_t attrs)
{
    for (int i = 0; i < attrs.attrs_len; i++) {
        if (attrs.attrs[i].type == IOP_FIELD_SNMP_INFO) {
            iop_field_attr_arg_t const *arg = attrs.attrs[i].args;

            return (iop_snmp_attrs_t*)arg->v.p;
        }
    }
    logger_panic(&_G.logger, "all snmpObj fields should have snmp attribute");
}

/* }}} */
/* {{{ Alarms */

static void doc_put_alarms_header(sb_t *buf, lstr_t name_full_up)
{
    sb_addf(buf,
            "=== +ALM-%*pM+: Alarms generated by the %*pM ===\n\n"
            "[cols=\"1,5<asciidoc\",options=\"header\"]\n"
            "|===\n"
            "|Features No    | Description, Rationale and Notes\n",
            LSTR_FMT_ARG(name_full_up), LSTR_FMT_ARG(name_full_up));
}

static void doc_put_arg_field(sb_t *buf, const iop_field_t *field,
                          const iop_struct_t *parent, uint16_t oid)
{
    sb_addf(buf,
            "- <<%*pM, %*pM>> (%*pM): %*pM",
            LSTR_FMT_ARG(field->name),
            LSTR_FMT_ARG(field->name),
            LSTR_FMT_ARG(t_field_build_oid(field, parent)),
            LSTR_FMT_ARG(t_field_get_help_without_dot(
                iop_get_field_attr_match_oid(parent, oid))));
}

static void doc_put_rpc(sb_t *buf, int tag, lstr_t iface_name,
                        const iop_rpc_t *rpc,
                        const iop_iface_t *parent)
{
    const iop_struct_t *st = rpc->args;
    lstr_t name = rpc->name;

    sb_addf(buf,
            "| ALM-%*pM-%u |\n"
            "*%*pM* (%*pM) +\n"
            "\n%*pM +\n"
            "\n*Parameters*\n\n",
            LSTR_FMT_ARG(iface_name), st->snmp_attrs->oid,
            LSTR_FMT_ARG(t_split_camelcase_word(name)),
            LSTR_FMT_ARG(t_notif_build_oid(st, parent)),
            LSTR_FMT_ARG(t_rpc_get_help(&parent->rpc_attrs[tag])));

    if (st->fields_len == 0) {
        sb_adds(buf, "*No parameter*\n");
        return;
    }
    /* Parameters */
    for (int i = 0; i < st->fields_len; i++) {
        const iop_field_t *field = &st->fields[i];
        const iop_snmp_attrs_t *attr;
        const iop_field_t *field_origin;

        if (!iop_field_has_snmp_info(field)) {
            continue;
        }

        if (!(attr = iop_get_snmp_attrs(&st->fields_attrs[i]))) {
            logger_panic(&_G.logger,
                         "no snmp attribute found for field `%*pM`",
                         LSTR_FMT_ARG(st->fields[i].name));
        }
        field_origin = iop_get_field_match_oid(attr->parent, attr->oid);
        doc_put_arg_field(buf, field_origin, attr->parent, attr->oid);

        if (i == st->fields_len - 1) {
            sb_adds(buf, ".\n");
        } else {
            sb_adds(buf, ";\n");
        }
    }
}

static void t_doc_put_alarms(sb_t *buf, const iop_pkg_t *pkg)
{
    for (const iop_iface_t *const *it = pkg->ifaces; *it; it++) {
        const iop_iface_t *iface = *it;
        lstr_t name_full_up = t_get_name_full_up(pkg->name);

        if (!iop_iface_is_snmp_iface(iface)) {
            continue;
        }

        if (iface->funs_len) {
            doc_put_alarms_header(buf, name_full_up);
        }
        for (int i = 0; i < iface->funs_len; i++) {
            doc_put_rpc(buf, i, name_full_up, &iface->funs[i], iface);
        }
        if (iface->funs_len) {
            sb_adds(buf, "|===\n");
        }
    }
}

/* }}} */
/* {{{ Objects */

static void doc_put_field_header(sb_t *buf)
{
    sb_adds(buf,
            "\n[cols=\"<20strong,20d,45d\",options=\"header\"]\n"
            "|===\n"
            "|Object\n"
            "|OID\n"
            "|Description\n\n");
}

static void t_doc_put_tbl(sb_t *buf, const iop_struct_t *st)
{
    qv_t(u16) oids;

    t_qv_init(u16, &oids, 16);
    sb_addf(buf,
            "|[[%*pM]]%*pM\n"
            "|32436%*pM\n"
            "|%*pM\n\n",
            LSTR_FMT_ARG(t_get_short_name(st->fullname)),
            LSTR_FMT_ARG(t_get_short_name(st->fullname)),
            LSTR_FMT_ARG(t_struct_build_oid(oids, st)),
            LSTR_FMT_ARG(t_struct_get_help(st->st_attrs)));
}

static void t_doc_put_field(sb_t *buf, int pos, const iop_struct_t *st)
{
    const iop_field_attrs_t field_attrs = st->fields_attrs[pos];
    const iop_field_t *field = &st->fields[pos];
    const iop_snmp_attrs_t *snmp_attrs;

    snmp_attrs = doc_field_get_snmp_attr(field_attrs);

    sb_addf(buf,
            "|[[%*pM]]%*pM\n"
            "|32436%*pM\n"
            "|%*pM.\n\n",
            LSTR_FMT_ARG(field->name),
            LSTR_FMT_ARG(field->name),
            LSTR_FMT_ARG(t_field_build_oid(field, st)),
            LSTR_FMT_ARG(t_field_get_help_without_dot(
                iop_get_field_attr_match_oid(st, snmp_attrs->oid))));
}

static void t_doc_put_fields(sb_t *buf, const iop_pkg_t *pkg)
{
    int compt = 0;

    for (const iop_struct_t *const *it = pkg->structs; *it; it++) {
        const iop_struct_t *desc = *it;

        if (!iop_struct_is_snmp_st(desc)) {
            continue;
        }

        if (iop_struct_is_snmp_tbl(desc)) {
            t_doc_put_tbl(buf, desc);
        }

        if (desc->fields_len > 0) {
            if (compt == 0) {
                doc_put_field_header(buf);
            }
            compt++;
            sb_addf(buf, "3+^s|*%*pM*\n\n",
                    LSTR_FMT_ARG(t_get_short_name(desc->fullname)));
        }

        /* deal with snmp fields */
        for (int i = 0; i < desc->fields_len; i++) {
            const iop_field_t field = desc->fields[i];

            if (!iop_field_has_snmp_info(&field)) {
                continue;
            }
            t_doc_put_field(buf, i, desc);
        }
    }
    if (compt > 0) {
        sb_adds(buf, "|===\n");
    }
}

/* }}} */
/* {{{ Parseopt */

static popt_t popt_g[] = {
    OPT_FLAG('h', "help", &_G.help, "show this help"),
    OPT_END(),
};

static void doc_parseopt(int argc, char **argv, lstr_t *output_notif,
                         lstr_t *output_object)
{
    const char *arg0 = NEXTARG(argc, argv);

    argc = parseopt(argc, argv, popt_g, 0);
    if (argc != 2 || _G.help) {
        makeusage(EX_USAGE, arg0, "<output-notifications-file> "
                  "<output-objects-file>", NULL, popt_g);
    }
    *output_notif  = LSTR(NEXTARG(argc, argv));
    *output_object = LSTR(NEXTARG(argc, argv));
}

/* }}} */

static void t_iop_write_snmp_doc(sb_t *notif_sb, sb_t *object_sb,
                                 qv_t(pkg) pkgs)
{
    qv_for_each_entry(pkg, pkg, &pkgs) {
        t_doc_put_alarms(notif_sb, pkg);
        t_doc_put_fields(object_sb, pkg);
    }
}

int iop_snmp_doc(int argc, char **argv, qv_t(pkg) pkgs)
{
    lstr_t path_notif = LSTR_NULL;
    lstr_t path_object = LSTR_NULL;
    SB_8k(notif_sb);
    SB_8k(object_sb);

    doc_parseopt(argc, argv, &path_notif, &path_object);
    t_iop_write_snmp_doc(&notif_sb, &object_sb, pkgs);

    if (sb_write_file(&notif_sb, path_notif.s) < 0) {
        logger_error(&_G.logger,
                     "couldn't write SNMP notification doc file `%*pM`: %m",
                     LSTR_FMT_ARG(path_notif));
        return -1;
    }
    if (sb_write_file(&object_sb, path_object.s) < 0) {
        logger_error(&_G.logger,
                     "couldn't write SNMP object doc file `%*pM`: %m",
                     LSTR_FMT_ARG(path_object));
        return -1;
    }

    return 0;
}

/* {{{ Tests */

/* LCOV_EXCL_START */

#include <lib-common/z.h>

#include "test-data/snmp/snmp_test_doc.iop.h"

static int z_check_wanted_file(lstr_t filename, sb_t *sb)
{
    char path[PATH_MAX];
    lstr_t file_map;

    snprintf(path, sizeof(path), "%*pM/test-data/snmp/docs/%*pM",
             LSTR_FMT_ARG(z_cmddir_g), LSTR_FMT_ARG(filename));

    sb_write_file(sb, t_fmt("/tmp/%*pM", LSTR_FMT_ARG(filename)));

    Z_ASSERT_N(lstr_init_from_file(&file_map, path, PROT_READ, MAP_SHARED));

    Z_ASSERT_LSTREQUAL(file_map, LSTR_SB_V(sb));

    lstr_wipe(&file_map);
    Z_HELPER_END;
}

Z_GROUP_EXPORT(iop_snmp_doc)
{
    Z_TEST(test_doc, "test generated doc") {
        t_scope;
        SB_1k(notifs_sb);
        SB_1k(objects_sb);
        qv_t(pkg) pkgs;

        qv_init(pkg, &pkgs);

        doc_register_pkg(&pkgs, snmp_test_doc);
        t_iop_write_snmp_doc(&notifs_sb, &objects_sb, pkgs);

        Z_HELPER_RUN(z_check_wanted_file(LSTR("ref-notif.inc.adoc"),
                                         &notifs_sb));
        Z_HELPER_RUN(z_check_wanted_file(LSTR("ref-object.inc.adoc"),
                                         &objects_sb));

        qv_wipe(pkg, &pkgs);
    } Z_TEST_END;

} Z_GROUP_END;

/* LCOV_EXCL_STOP */

/* }}} */
