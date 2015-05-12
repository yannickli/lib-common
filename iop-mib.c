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

#include "iop-mib.h"

#define LVL1 "    "
#define LVL2 LVL1 LVL1
#define LVL3 LVL2 LVL1
#define LVL4 LVL2 LVL2
#define LVL5 LVL3 LVL2

static struct {
    logger_t   logger;
    qh_t(lstr) unicity_conformance_objects;
    qh_t(lstr) unicity_conformance_notifs;
    qv_t(lstr) conformance_objects;
    qv_t(lstr) conformance_notifs;
} mib_g = {
#define _G  mib_g
    .logger = LOGGER_INIT_INHERITS(NULL, "iop2mib"),
};

/* {{{ Helpers */

static lstr_t t_get_short_name(const lstr_t fullname, bool down)
{
    lstr_t name = t_lstr_dup(fullname);
    pstream_t obj_name = ps_initlstr(&name);
    lstr_t out;

    if (ps_skip_afterlastchr(&obj_name, '.') < 0) {
        logger_fatal(&_G.logger, "fullname `%*pM` should be at least "
                     "composed by `pkg.name`", LSTR_FMT_ARG(fullname));
    }
    out = t_lstr_fmt("%s", obj_name.s);
    /* First letter must be down */
    if (down) {
        out.v[0] = tolower((unsigned char)out.v[0]);
    }

    return out;
}

static const
iop_snmp_attrs_t *mib_field_get_snmp_attr(const iop_field_attrs_t attrs)
{
    for (int i = 0; i < attrs.attrs_len; i++) {
        if (attrs.attrs[i].type == IOP_FIELD_SNMP_INFO) {
            iop_field_attr_arg_t const *arg = attrs.attrs[i].args;

            return (iop_snmp_attrs_t*)arg->v.p;
        }
    }
    logger_fatal(&_G.logger,
                 "all snmpObj fields should contain at least a brief");
}

static lstr_t get_type_to_lstr(iop_type_t type)
{
    switch (type) {
      case IOP_T_STRING:
        return LSTR("OCTET STRING");
      case IOP_T_I8:
      case IOP_T_I16:
      case IOP_T_I32:
        return LSTR("Integer32");
      case IOP_T_BOOL:
        return LSTR("BOOLEAN");
      default:
        logger_fatal(&_G.logger, "type not handled");
    }
}

static lstr_t t_mib_field_get_help(const iop_field_attrs_t *attrs)
{
    const iop_field_attr_t *attr = attrs->attrs;

    for (int i = 0; i < attrs->attrs_len; i++) {
        if (attr[i].type == IOP_FIELD_ATTR_HELP) {
            const iop_help_t *help = attr[i].args->v.p;

            return t_lstr_cat3(help->brief, help->details, help->warning);
        }
    }
    logger_fatal(&_G.logger, "each field needs a description");
}

static lstr_t t_mib_rpc_get_help(const iop_rpc_attrs_t *attrs)
{
    const iop_rpc_attr_t *attr = attrs->attrs;

    for (int i = 0; i < attrs->attrs_len; i++) {
        if (attr[i].type == IOP_RPC_ATTR_HELP) {
            const iop_help_t *help = attr[i].args->v.p;

            return t_lstr_cat3(help->brief, help->details, help->warning);
        }
    }
    logger_fatal(&_G.logger, "each rpc needs a description");
}

/* }}} */
/* {{{ Header/Footer */

static void mib_open_banner(sb_t *buf, lstr_t name)
{
    t_scope;
    lstr_t up_name = t_lstr_dup(name);

    lstr_ascii_toupper(&up_name);

    sb_addf(buf,
            "INTERSEC-%*pM-MIB DEFINITIONS ::= BEGIN\n\n"
            "IMPORTS\n"
            LVL1 "MODULE-COMPLIANCE, OBJECT-GROUP, "
            "NOTIFICATION-GROUP FROM SNMPv2-CONF\n"
            LVL1 "MODULE-IDENTITY, OBJECT-TYPE, NOTIFICATION-TYPE, "
            "Integer32 FROM SNMPv2-SMI\n"
            LVL1 "%*pM FROM INTERSEC-MIB;\n\n",
            LSTR_FMT_ARG(up_name), LSTR_FMT_ARG(name));
}

static void mib_close_banner(sb_t *buf)
{
            sb_adds(buf, "\n\n-- vim:syntax=mib\n");
}

/* }}} */
/* {{{ Object Identifier */

static void mib_put_snmp_obj(sb_t *buf, const iop_struct_t *snmp_obj)
{
    t_scope;
    const iop_snmp_attrs_t *snmp_attrs = snmp_obj->snmp_attrs;
    /* Having no 'parent' means that in the IOP code the snmpObj was
     * inherited from 'Intersec' */
    lstr_t parent;

    parent = snmp_attrs->parent ?
        t_get_short_name(snmp_attrs->parent->fullname, true) :
        LSTR("intersec");

    sb_addf(buf,
            "%*pM" LVL1 "OBJECT IDENTIFIER ::= { %*pM %d }\n",
            LSTR_FMT_ARG(t_get_short_name(snmp_obj->fullname, true)),
            LSTR_FMT_ARG(parent), snmp_attrs->oid);
}

static void mib_put_snmp_iface(sb_t *buf, const iop_iface_t *snmp_iface)
{
    t_scope;
    const iop_snmp_attrs_t *snmp_attrs = snmp_iface->snmp_iface_attrs;

    if (!snmp_attrs->parent) {
        logger_fatal(&_G.logger, "any snmpIface should have a parent");
    }

    sb_addf(buf,
            "%*pM" LVL1 "OBJECT IDENTIFIER ::= { %*pM %d }\n",
            LSTR_FMT_ARG(t_get_short_name(snmp_iface->fullname, true)),
            LSTR_FMT_ARG(t_get_short_name(snmp_attrs->parent->fullname, true)),
            snmp_attrs->oid);
}

static void mib_put_object_identifier(sb_t *buf, const iop_pkg_t *pkg)
{
    sb_addf(buf, "-- {{{ Top Level Structure\n\n");

    for (const iop_struct_t *const *it = pkg->structs; *it; it++) {
        const iop_struct_t *desc = *it;

        if (!iop_struct_is_snmp_obj(desc)) {
            continue;
        }
        mib_put_snmp_obj(buf, desc);
    }
    for (const iop_iface_t *const *it = pkg->ifaces; *it; it++) {
        const iop_iface_t *iface = *it;

        if (!iop_iface_is_snmp_iface(iface)) {
            continue;
        }
        mib_put_snmp_iface(buf, iface);
    }
    sb_addf(buf, "\n-- }}}\n");
}

/* }}} */
/* {{{ SnmpObj fields */

static void mib_put_field(sb_t *buf, lstr_t name, int pos,
                          const iop_struct_t *st)
{
    t_scope;
    const iop_field_attrs_t field_attrs = st->fields_attrs[pos];
    const iop_snmp_attrs_t *snmp_attrs;

    snmp_attrs = mib_field_get_snmp_attr(field_attrs);
    sb_addf(buf,
            "\n%*pM OBJECT-TYPE\n"
            LVL1 "SYNTAX %*pM\n"
            LVL1 "MAX-ACCESS read-only\n"
            LVL1 "STATUS current\n"
            LVL1 "DESCRIPTION\n"
            LVL2 "\"%*pM\"\n"
            LVL1 "::= { %*pM %d }\n",
            LSTR_FMT_ARG(name),
            LSTR_FMT_ARG(get_type_to_lstr(snmp_attrs->type)),
            LSTR_FMT_ARG(t_mib_field_get_help(&field_attrs)),
            LSTR_FMT_ARG(t_get_short_name(snmp_attrs->parent->fullname, true)),
            snmp_attrs->oid);

    if (qh_add(lstr, &_G.unicity_conformance_objects, &name) < 0) {
        logger_fatal(&_G.logger,
                     "conflicting field name `%*pM`", LSTR_FMT_ARG(name));
    }
    qv_append(lstr, &_G.conformance_objects, name);

    /* TODO: for brief lstr_len/80 then cut the lstr */
}

static void mib_put_fields(sb_t *buf, const iop_pkg_t *pkg)
{
    for (const iop_struct_t *const *it = pkg->structs; *it; it++) {
        const iop_struct_t *desc = *it;
        bool has_fields = desc->fields_len > 0;

        if (!iop_struct_is_snmp_obj(desc)) {
            continue;
        }

        if (has_fields) {
            t_scope;

            sb_addf(buf,
                "-- {{{ %*pM\n",
                LSTR_FMT_ARG(t_get_short_name(desc->fullname, false)));
        }

        /* deal with snmp fields */
        for (int i = 0; i < desc->fields_len; i++) {
            const iop_field_t field = desc->fields[i];
            if (!iop_field_has_snmp_info(&field)) {
                continue;
            }
            mib_put_field(buf, field.name, i, desc);
        }

        if (has_fields) {
            sb_addf(buf, "\n-- }}}\n");
        }
    }
}

/* }}} */
/* {{{ SnmpIface rpcs */

static void mib_put_rpc(sb_t *buf, int pos, const iop_rpc_t *rpc,
                        const iop_rpc_attrs_t attrs, lstr_t iface_name)
{
    t_scope;
    const iop_struct_t *st = rpc->args;

    sb_addf(buf,
            "\n%*pM NOTIFICATION-TYPE\n"
            LVL1 "OBJECTS { ", LSTR_FMT_ARG(rpc->name));

    for (int i = 0; i < st->fields_len; i++) {
        sb_addf(buf, "%*pM", LSTR_FMT_ARG(st->fields[i].name));
        if (i < st->fields_len - 1) {
            sb_addf(buf, ", ");
        }
    }
    sb_addf(buf, " }\n"
            LVL1 "STATUS current\n"
            LVL1 "DESCRIPTION\n"
            LVL2 "\"%*pM\"\n"
            LVL1 "::= { %*pM %d }\n",
            LSTR_FMT_ARG(t_mib_rpc_get_help(&attrs)),
            LSTR_FMT_ARG(t_get_short_name(iface_name, true)), pos + 1);

    /* TODO: for brief lstr_len/80 then cut the lstr */

    if (qh_add(lstr, &_G.unicity_conformance_notifs, &rpc->name) < 0) {
        logger_fatal(&_G.logger,
                     "conflicting notification name `%*pM`",
                     LSTR_FMT_ARG(rpc->name));
    }
    qv_append(lstr, &_G.conformance_notifs, rpc->name);
}

static void mib_put_rpcs(sb_t *buf, const iop_pkg_t *pkg)
{
    for (const iop_iface_t *const *it = pkg->ifaces; *it; it++) {
        const iop_iface_t *iface = *it;
        bool has_rpcs = iface->funs_len > 0;

        if (!iop_iface_is_snmp_iface(iface)) {
            continue;
        }

        if (has_rpcs) {
            t_scope;

            sb_addf(buf, "-- {{{ %*pM\n",
                    LSTR_FMT_ARG(t_get_short_name(iface->fullname, false)));
        }

        /* deal with snmp rpcs */
        for (int i = 0; i < iface->funs_len; i++) {
            const iop_rpc_t rpc = iface->funs[i];
            const iop_rpc_attrs_t attrs = iface->rpc_attrs[i];

            mib_put_rpc(buf, i, &rpc, attrs, iface->fullname);
        }

        if (has_rpcs) {
            sb_addf(buf, "\n-- }}}\n");
        }
    }
}

/* }}} */
/* {{{ Conformance Groups */

static void mib_put_compliance(sb_t *buf, lstr_t name)
{
    sb_addf(buf,
            "\n%*pMCompliance MODULE-COMPLIANCE\n"
            LVL1 "STATUS current\n"
            LVL1 "DESCRIPTION \"The compliance statement for %*pM entities\"\n"
            LVL1 "MODULE\n"
            LVL2 "MANDATORY-GROUPS { %*pMConformanceObject, "
            "%*pMConformanceNotification }\n"
            LVL1 "::= { %*pMIdentity 1 }\n",
            LSTR_FMT_ARG(name), LSTR_FMT_ARG(name),
            LSTR_FMT_ARG(name), LSTR_FMT_ARG(name),
            LSTR_FMT_ARG(name));
}

static void mib_put_objects_conformance(sb_t *buf, lstr_t name)
{
    sb_addf(buf,
            "\n%*pMConformanceObject OBJECT-GROUP\n"
            LVL1 "OBJECTS { ", LSTR_FMT_ARG(name));

    qv_for_each_pos(lstr, pos, &_G.conformance_objects) {
        sb_addf(buf, "%*pM", LSTR_FMT_ARG(_G.conformance_objects.tab[pos]));
        if (pos < _G.conformance_objects.len - 1) {
            sb_addf(buf, ", ");
        }
    }
    sb_addf(buf, " }\n"
            LVL1 "STATUS current\n"
            LVL1 "DESCRIPTION\n"
            LVL2 "\"%*pM conformance objects\"\n"
            LVL1 "::= { %*pMIdentity 81 }\n",
            LSTR_FMT_ARG(name), LSTR_FMT_ARG(name));
}

static void mib_put_notifs_conformance(sb_t *buf, lstr_t name)
{
    sb_addf(buf,
            "\n%*pMConformanceNotification NOTIFICATION-GROUP\n"
            LVL1 "NOTIFICATIONS { ", LSTR_FMT_ARG(name));

    qv_for_each_pos(lstr, pos, &_G.conformance_notifs) {
        sb_addf(buf, "%*pM", LSTR_FMT_ARG(_G.conformance_notifs.tab[pos]));
        if (pos < _G.conformance_notifs.len - 1) {
            sb_addf(buf, ", ");
        }
    }
    sb_addf(buf, " }\n"
            LVL1 "STATUS current\n"
            LVL1 "DESCRIPTION\n"
            LVL2 "\"%*pM conformance notifications\"\n"
            LVL1 "::= { %*pMIdentity 80 }\n",
            LSTR_FMT_ARG(name), LSTR_FMT_ARG(name));
}

static void mib_put_compliance_fold(sb_t *buf, lstr_t name)
{
    if (_G.conformance_notifs.len == 0 && _G.conformance_objects.len == 0) {
        return;
    }

    sb_addf(buf, "-- {{{ Compliance\n");
    mib_put_compliance(buf, name);
    mib_put_notifs_conformance(buf, name);
    mib_put_objects_conformance(buf, name);
    sb_addf(buf, "\n-- }}}\n");
}

/* }}} */
/* {{{ Module */

static int iop_mib_initialize(void *arg)
{
    qh_init(lstr, &_G.unicity_conformance_objects);
    qh_init(lstr, &_G.unicity_conformance_notifs);
    qv_init(lstr, &_G.conformance_objects);
    qv_init(lstr, &_G.conformance_notifs);
    return 0;
}

static int iop_mib_shutdown(void)
{
    qh_wipe(lstr, &_G.unicity_conformance_objects);
    qh_wipe(lstr, &_G.unicity_conformance_notifs);
    qv_wipe(lstr, &_G.conformance_objects);
    qv_wipe(lstr, &_G.conformance_notifs);
    return 0;
}

MODULE_BEGIN(iop_mib)
MODULE_END()

/* }}} */

void iop_mib(sb_t *sb, lstr_t name, const iop_pkg_t *pkg)
{
    SB_8k(buffer);

    MODULE_REQUIRE(iop_mib);

    mib_open_banner(sb, name);
    mib_put_object_identifier(&buffer, pkg);
    mib_put_fields(&buffer, pkg);
    mib_put_rpcs(&buffer, pkg);
    mib_put_compliance_fold(sb, name);

    /* Concat both sb */
    sb_addsb(sb, &buffer);
    mib_close_banner(sb);

    MODULE_RELEASE(iop_mib);
}

#undef LVL1
#undef LVL2
#undef LVL3
#undef LVL4
#undef LVL5
