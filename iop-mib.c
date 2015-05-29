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

void iop_mib(sb_t *sb, lstr_t name, const iop_pkg_t *pkg)
{
    mib_open_banner(sb, name);
    mib_put_object_identifier(sb, pkg);
    mib_close_banner(sb);
}

#undef LVL1
#undef LVL2
#undef LVL3
#undef LVL4
#undef LVL5
