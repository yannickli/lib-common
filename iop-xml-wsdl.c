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

#include <lib-common/xmlpp.h>
#include <lib-common/container.h>
#include "iop.h"
#include <lib-common/iop-helpers.inl.c>

static inline uint32_t
qhash_iop_type_hash(const qhash_t *qh, const iop_struct_t *st)
{
    return qhash_hash_ptr(qh, st);
}
static inline bool
qhash_iop_type_equal(const qhash_t *qh,
                     const iop_struct_t *st1, const iop_struct_t *st2)
{
    return st1 == st2;
}

qvector_t(iop_type, const iop_struct_t *);
qvector_t(iop_enum, const iop_enum_t *);
qh_kptr_ckey_t(iop_type, iop_struct_t,
               qhash_iop_type_hash, qhash_iop_type_equal);


typedef struct wsdlpp_t {
    qh_t(xwsdl_impl) *impl;
    bool    wauth;
    bool    wenums;

    xmlpp_t pp;
} wsdlpp_t;

#define wpp_add_comment(wpp, prev, fmt, ...) \
    do {                                                            \
        wsdlpp_t *_wpp = (wpp);                                     \
        sb_addf(_wpp->pp.buf, prev"%*s<!-- "fmt" -->",              \
                2 * _wpp->pp.stack.len, "", ##__VA_ARGS__);         \
    } while (0)

#define WSDL_CHECK_SKIP(wpp, alias, rpc) \
    ({                                                                      \
        if (wpp->impl                                                       \
        &&  qh_find(xwsdl_impl, wpp->impl,                                  \
                    (alias->tag << 16) | rpc->tag) < 0)                     \
        {                                                                   \
            continue;                                                       \
        }                                                                   \
    })

static void iop_xwsdl_put_attr_value(wsdlpp_t *wpp, iop_type_t type,
                                     const iop_field_attr_t *attr)
{
    switch (type) {
      case IOP_T_I8: case IOP_T_I16: case IOP_T_I32: case IOP_T_I64:
        xmlpp_putattrfmt(&wpp->pp, "value", "%jd", attr->args[0].v.i64);
        break;

      case IOP_T_U8: case IOP_T_U16: case IOP_T_U32: case IOP_T_U64:
        xmlpp_putattrfmt(&wpp->pp, "value", "%ju",
                         (uint64_t)attr->args[0].v.i64);
        break;

      case IOP_T_DOUBLE:
        xmlpp_putattrfmt(&wpp->pp, "value", "%.17e", attr->args[0].v.d);
        break;

      default:
        e_panic("should not happen");
    }
}

static void put_type_constraints(wsdlpp_t *wpp, iop_type_t type,
                                 const iop_field_attrs_t *attrs)
{
    for (int i = 0; i < attrs->attrs_len; i++) {
        const iop_field_attr_t *attr = &attrs->attrs[i];

        switch (attr->type) {
          case IOP_FIELD_MIN:
            xmlpp_opentag(&wpp->pp, "minInclusive");
            iop_xwsdl_put_attr_value(wpp, type, attr);
            break;

          case IOP_FIELD_MAX:
            xmlpp_opentag(&wpp->pp, "maxInclusive");
            iop_xwsdl_put_attr_value(wpp, type, attr);
            break;

          case IOP_FIELD_NON_EMPTY:
            xmlpp_opentag(&wpp->pp, "minLength");
            xmlpp_putattr(&wpp->pp, "value", "1");
            break;

          case IOP_FIELD_MIN_LENGTH:
            xmlpp_opentag(&wpp->pp, "minLength");
            xmlpp_putattrfmt(&wpp->pp, "value", "%jd",
                             attr->args[0].v.i64);
            break;

          case IOP_FIELD_MAX_LENGTH:
            xmlpp_opentag(&wpp->pp, "maxLength");
            xmlpp_putattrfmt(&wpp->pp, "value", "%jd",
                             attr->args[0].v.i64);
            break;

          case IOP_FIELD_PATTERN:
            xmlpp_opentag(&wpp->pp, "pattern");
            xmlpp_putattrfmt(&wpp->pp, "value", "(%*pM)*",
                             LSTR_FMT_ARG(attr->args[0].v.s));
            break;

          default:
            continue;
        }
        xmlpp_closetag(&wpp->pp);

        /* TODO non-zero isn't yet supported in WSDL */
    }
}

static void put_int_type(wsdlpp_t *wpp, iop_type_t type, const char *name,
                         const iop_field_attrs_t *attrs)
{
    static char const * const xs_types[] = {
        [IOP_T_I8]   = "byte",
        [IOP_T_U8]   = "unsignedByte",
        [IOP_T_I16]  = "short",
        [IOP_T_U16]  = "unsignedShort",
        [IOP_T_I32]  = "int",
        [IOP_T_ENUM] = "int",
        [IOP_T_U32]  = "unsignedInt",
        [IOP_T_I64]  = "long",
        [IOP_T_U64]  = "unsignedLong",
    };

#define mk_pattern(sz)  "0x0*[0-9a-fA-F]{1,"#sz"}"
    static char const * const hex_patterns[] = {
        [IOP_T_I8]   = mk_pattern(2),
        [IOP_T_U8]   = mk_pattern(2),
        [IOP_T_I16]  = mk_pattern(4),
        [IOP_T_U16]  = mk_pattern(4),
        [IOP_T_I32]  = mk_pattern(8),
        [IOP_T_ENUM] = mk_pattern(8),
        [IOP_T_U32]  = mk_pattern(8),
        [IOP_T_I64]  = mk_pattern(16),
        [IOP_T_U64]  = mk_pattern(16),
    };
#undef mk_pattern

    xmlpp_t *pp = &wpp->pp;

    xmlpp_opentag(pp, "simpleType");
    if (name) {
        xmlpp_putattr(pp, "name", name);
    }
    xmlpp_opentag(pp, "union");
    xmlpp_opentag(pp, "simpleType");
    xmlpp_opentag(pp, "restriction");
    xmlpp_putattr(pp, "base", xs_types[type]);

    if (attrs) {
        /* If given, constraints are applied only on the “integer” part of the
         * type, not on the hexadecimal pattern. */
        put_type_constraints(wpp, type, attrs);
    }

    xmlpp_closentag(pp, 2);
    xmlpp_opentag(pp, "simpleType");
    xmlpp_opentag(pp, "restriction");
    xmlpp_putattr(pp, "base", "string");
    xmlpp_opentag(pp, "pattern");
    xmlpp_putattr(pp, "value", hex_patterns[type]);
    /* TODO support constraints on hexadecimal pattern */

    xmlpp_closentag(pp, 5);
}

static void iop_xwsdl_put_enum(wsdlpp_t *wpp, const iop_enum_t *e)
{
    xmlpp_opentag(&wpp->pp, "simpleType");
    /* TODO maybe use a fullname (pkg.name) */
    xmlpp_putattr(&wpp->pp, "name", e->name.s);
    xmlpp_opentag(&wpp->pp, "union");

    xmlpp_opentag(&wpp->pp, "simpleType");
    xmlpp_opentag(&wpp->pp, "restriction");
    xmlpp_putattr(&wpp->pp, "base", "string");

    for (int i = 0; i < e->enum_len; i++) {
        xmlpp_opentag(&wpp->pp, "enumeration");
        xmlpp_putattr(&wpp->pp, "value", e->names[i].s);
        xmlpp_closetag(&wpp->pp);
    }
    xmlpp_closentag(&wpp->pp, 2);

    xmlpp_opentag(&wpp->pp, "simpleType");
    xmlpp_opentag(&wpp->pp, "restriction");
    xmlpp_putattr(&wpp->pp, "base", "tns:intersec.int");

    xmlpp_closentag(&wpp->pp, 4);
}

static void iop_xwsdl_put_occurs(wsdlpp_t *wpp, const iop_struct_t *st,
                                 const iop_field_t *f)
{
    const iop_field_attrs_t *attrs = iop_field_get_attrs(st, f);
    unsigned min = 0, max = 0;

    if (attrs
    &&  (attrs->flags & (  (1 << IOP_FIELD_MIN_OCCURS)
                         | (1 << IOP_FIELD_MAX_OCCURS))))
    {
        for (int i = 0; i < attrs->attrs_len; i++) {
            const iop_field_attr_t *attr = &attrs->attrs[i];

            if (attr->type == IOP_FIELD_MIN_OCCURS) {
                assert (attr->args[0].v.i64 >= 0);
                min = (unsigned)attr->args[0].v.i64;
            } else
            if (attr->type == IOP_FIELD_MAX_OCCURS) {
                assert (attr->args[0].v.i64 > 0);
                max = (unsigned)attr->args[0].v.i64;
            }
        }
    }

    xmlpp_putattrfmt(&wpp->pp, "minOccurs", "%u", min);
    if (max > 0) {
        xmlpp_putattrfmt(&wpp->pp, "maxOccurs", "%u", max);
    } else {
        xmlpp_putattr(&wpp->pp, "maxOccurs", "unbounded");
    }
}

static void iop_xwsdl_put_constraints(wsdlpp_t *wpp, const iop_struct_t *st,
                                      const iop_field_t *f)
{
    static char const * const types[] = {
        [IOP_T_I8]     = "tns:intersec.byte",
        [IOP_T_U8]     = "tns:intersec.unsignedByte",
        [IOP_T_I16]    = "tns:intersec.short",
        [IOP_T_U16]    = "tns:intersec.unsignedShort",
        [IOP_T_I32]    = "tns:intersec.int",
        [IOP_T_ENUM]   = "tns:intersec.int",
        [IOP_T_U32]    = "tns:intersec.unsignedInt",
        [IOP_T_I64]    = "tns:intersec.long",
        [IOP_T_U64]    = "tns:intersec.unsignedLong",
        [IOP_T_BOOL]   = "boolean",
        [IOP_T_DOUBLE] = "double",
        [IOP_T_STRING] = "string",
        [IOP_T_DATA]   = "base64Binary",
    };

    const iop_field_attrs_t *attrs = iop_field_get_attrs(st, f);
    unsigned restriction_flags = 0;

    SET_BIT(&restriction_flags, IOP_FIELD_MIN);
    SET_BIT(&restriction_flags, IOP_FIELD_MAX);
    SET_BIT(&restriction_flags, IOP_FIELD_NON_EMPTY);
    SET_BIT(&restriction_flags, IOP_FIELD_MIN_LENGTH);
    SET_BIT(&restriction_flags, IOP_FIELD_MAX_LENGTH);
    SET_BIT(&restriction_flags, IOP_FIELD_PATTERN);

    if (!attrs || !(attrs->flags & restriction_flags)) {
        /* No specific restrictions, so we just use the generic types */
        xmlpp_putattr(&wpp->pp, "type", types[f->type]);
        return;
    } else {
        /* When we have have constraints, we need to dump a custom type */
        switch (f->type) {
          case IOP_T_I8:  case IOP_T_U8:
          case IOP_T_I16: case IOP_T_U16:
          case IOP_T_I32: case IOP_T_U32:
          case IOP_T_ENUM:
          case IOP_T_I64: case IOP_T_U64:
            /* Integer values are handled separately */
            put_int_type(wpp, f->type, NULL, attrs);
            return;

          case IOP_T_DOUBLE:
          case IOP_T_STRING:
          case IOP_T_DATA:
            xmlpp_opentag(&wpp->pp, "simpleType");
            xmlpp_opentag(&wpp->pp, "restriction");
            xmlpp_putattr(&wpp->pp, "base", types[f->type]);

            put_type_constraints(wpp, f->type, attrs);

            xmlpp_closentag(&wpp->pp, 2);
            break;

          default:
            e_panic("unsupported type with constraints: %d", f->type);
        }
    }

#undef ATTR_TOUCH_TYPES
}

static void iop_xwsdl_put_type(wsdlpp_t *wpp, const iop_struct_t *st)
{
    xmlpp_opentag(&wpp->pp, "complexType");
    xmlpp_putattr(&wpp->pp, "name", st->fullname.s);
    if (st == &iop__void__s) {
        xmlpp_closetag(&wpp->pp);
        return;
    }
    if (st->is_union) {
        xmlpp_opentag(&wpp->pp, "choice");
    } else {
        xmlpp_opentag(&wpp->pp, "sequence");
    }
    for (int i = 0; i < st->fields_len; i++) {
        const iop_field_t *f   = &st->fields[i];
        const iop_field_attrs_t *attrs;

        attrs = iop_field_get_attrs(st, f);
        if (attrs && TST_BIT(&attrs->flags, IOP_FIELD_PRIVATE))
            continue;

        switch (f->type) {
            case IOP_T_XML:
              /* xsd:any with the attribute processContents="skip" means "any
               * valid xml element" */
              xmlpp_opentag(&wpp->pp, "any");
              xmlpp_putattr(&wpp->pp, "processContents", "skip");
              break;
            default:
              xmlpp_opentag(&wpp->pp, "element");
        }

        xmlpp_putattr(&wpp->pp, "name", f->name.s);
        switch (f->repeat) {
          case IOP_R_DEFVAL:
          case IOP_R_OPTIONAL:
            xmlpp_putattr(&wpp->pp, "minOccurs", "0");
            break;
          case IOP_R_REPEATED:
            iop_xwsdl_put_occurs(wpp, st, f);
            break;
          default:
            break;
        }

        if ((1 << f->type) & IOP_STRUCTS_OK) {
            const iop_struct_t *desc = f->u1.st_desc;
            xmlpp_putattrfmt(&wpp->pp, "type", "tns:%s", desc->fullname.s);
        } else
        if (wpp->wenums && f->type == IOP_T_ENUM) {
            const iop_enum_t *desc = f->u1.en_desc;
            /* TODO maybe use a fullname (pkg.name) */
            xmlpp_putattrfmt(&wpp->pp, "type", "tns:%s", desc->name.s);
        } else
        if (f->type != IOP_T_XML) {
            iop_xwsdl_put_constraints(wpp, st, f);

        }
        xmlpp_closetag(&wpp->pp);
        if (f->repeat == IOP_R_DEFVAL) {
            switch (f->type) {
              case IOP_T_BOOL:
                wpp_add_comment(wpp, "\n", "default: %s=%s",
                                f->name.s, f->u1.defval_u64 ? "true" : "false");
                break;
              case IOP_T_I8: case IOP_T_I16: case IOP_T_I32: case IOP_T_I64:
                wpp_add_comment(wpp, "\n", "default: %s=%jd",
                                f->name.s, f->u1.defval_u64);
                break;
              case IOP_T_ENUM:
                wpp_add_comment(wpp, "\n", "default: %s=%d",
                                f->name.s, f->u0.defval_enum);
                break;
              case IOP_T_U8: case IOP_T_U16: case IOP_T_U32: case IOP_T_U64:
                wpp_add_comment(wpp, "\n", "default: %s=%ju",
                                f->name.s, f->u1.defval_u64);
                break;
              case IOP_T_DOUBLE:
                wpp_add_comment(wpp, "\n", "default: %s=%.17e",
                                f->name.s, f->u1.defval_d);
                break;
              case IOP_T_STRING:
                wpp_add_comment(wpp, "\n", "default: %s=%s",
                                f->name.s, (const char *)f->u1.defval_data);
                break;
            }
        }
    }
    xmlpp_closentag(&wpp->pp, 2);
}

static void
iop_xwsdl_scan_types(wsdlpp_t *wpp, const iop_struct_t *st,
                     qh_t(iop_type) *type_idx,
                     qv_t(iop_type) *types, qv_t(iop_enum) *enums)
{
    if (qh_add(iop_type, type_idx, st))
        return;

    for (int i = 0; i < st->fields_len; i++) {
        const iop_field_t *field = &st->fields[i];
        const iop_field_attrs_t *attrs;

        attrs = iop_field_get_attrs(st, field);
        if (attrs && TST_BIT(&attrs->flags, IOP_FIELD_PRIVATE))
            continue;

        if ((1 << field->type) & IOP_STRUCTS_OK) {
            iop_xwsdl_scan_types(wpp, field->u1.st_desc, type_idx, types, enums);
        } else
        if (field->type == IOP_T_ENUM && wpp->wenums) {
            if (qh_add(iop_type, type_idx, field->u1.st_desc))
                continue;

            qv_append(iop_enum, enums, field->u1.en_desc);
        }
    }

    qv_append(iop_type, types, st);
}

static void
iop_xwsdl_put_types(wsdlpp_t *wpp, const iop_mod_t *mod, const char *ns)
{
    qh_t(iop_type) h;
    qv_t(iop_type) a;
    qv_t(iop_enum) b;

    qh_init(iop_type,  &h, false);
    qv_inita(iop_type, &a, 1024);
    qv_inita(iop_enum, &b, 1024);

    for (int i = 0; i < mod->ifaces_len; i++) {
        const iop_iface_alias_t *alias = &mod->ifaces[i];

        for (int j = 0; j < alias->iface->funs_len; j++) {
            const iop_rpc_t *rpc = &alias->iface->funs[j];
            const iop_struct_t *st;

            /* Check implementation table */
            WSDL_CHECK_SKIP(wpp, alias, rpc);

            iop_xwsdl_scan_types(wpp, rpc->args, &h, &a, &b);
            if (!rpc->async) {
                iop_xwsdl_scan_types(wpp, rpc->result, &h, &a, &b);
            }

            if (rpc->exn == &iop__void__s)
                continue;
            st = rpc->exn;
            if (st->is_union) {
                /* XXX An union is understood as a sequence of exceptions */
                for (int k = 0; k < st->fields_len; k++) {
                    const iop_field_t *f = st->fields + k;
                    assert(f->type == IOP_T_STRUCT);
                    iop_xwsdl_scan_types(wpp, f->u1.st_desc, &h, &a, &b);
                }
            } else {
                iop_xwsdl_scan_types(wpp, st, &h, &a, &b);
            }
        }
    }

    wpp_add_comment(wpp, "\n\n", "WSDL types");
    xmlpp_opentag(&wpp->pp, "types");
    xmlpp_opentag(&wpp->pp, "schema");
    xmlpp_putattr(&wpp->pp, "targetNamespace", ns);
    xmlpp_putattr(&wpp->pp, "xmlns", "http://www.w3.org/2001/XMLSchema");

    /* Dump the generic Intersec integer types */
    put_int_type(wpp, IOP_T_I8,  "intersec.byte",          NULL);
    put_int_type(wpp, IOP_T_U8,  "intersec.unsignedByte",  NULL);
    put_int_type(wpp, IOP_T_I16, "intersec.short",         NULL);
    put_int_type(wpp, IOP_T_U16, "intersec.unsignedShort", NULL);
    put_int_type(wpp, IOP_T_I32, "intersec.int",           NULL);
    put_int_type(wpp, IOP_T_U32, "intersec.unsignedInt",   NULL);
    put_int_type(wpp, IOP_T_I64, "intersec.long",          NULL);
    put_int_type(wpp, IOP_T_U64, "intersec.unsignedLong",  NULL);

    /* Dump the authentification header type */
    if (wpp->wauth) {
        xmlpp_opentag(&wpp->pp, "element");
        xmlpp_putattr(&wpp->pp, "name", "callerIdentity");
        xmlpp_opentag(&wpp->pp, "complexType");
        xmlpp_opentag(&wpp->pp, "sequence");
        xmlpp_opentag(&wpp->pp, "element");
        xmlpp_putattr(&wpp->pp, "name", "login");
        xmlpp_putattr(&wpp->pp, "type", "string");
        xmlpp_closetag(&wpp->pp);
        xmlpp_opentag(&wpp->pp, "element");
        xmlpp_putattr(&wpp->pp, "name", "password");
        xmlpp_putattr(&wpp->pp, "type", "string");
        xmlpp_closentag(&wpp->pp, 4);
    }

    /* Enums */
    for (int i = b.len - 1; i >= 0; i--) {
        iop_xwsdl_put_enum(wpp, b.tab[i]);
    }

    /* Structs */
    for (int i = a.len - 1; i >= 0; i--) {
        iop_xwsdl_put_type(wpp, a.tab[i]);
    }

    /* For each function dump the alias for Args/Res */
    wpp_add_comment(wpp, "\n\n", "RPC arguments types");
    for (int i = 0; i < mod->ifaces_len; i++) {
        const iop_iface_alias_t *alias = &mod->ifaces[i];

        for (int j = 0; j < alias->iface->funs_len; j++) {
            const iop_rpc_t *rpc = &alias->iface->funs[j];

            /* Check implementation table */
            WSDL_CHECK_SKIP(wpp, alias, rpc);

            xmlpp_opentag(&wpp->pp, "element");
            xmlpp_putattrfmt(&wpp->pp, "name", "%s.%sReq", alias->name.s,
                             rpc->name.s);
            xmlpp_putattrfmt(&wpp->pp, "type", "tns:%s",
                             rpc->args->fullname.s);
            xmlpp_closetag(&wpp->pp);

            if (!rpc->async) {
                xmlpp_opentag(&wpp->pp, "element");
                xmlpp_putattrfmt(&wpp->pp, "name", "%s.%sRes", alias->name.s,
                                 rpc->name.s);
                xmlpp_putattrfmt(&wpp->pp, "type", "tns:%s",
                                 rpc->result->fullname.s);
                xmlpp_closetag(&wpp->pp);
            }

            if (rpc->exn == &iop__void__s)
                continue;
            if (rpc->exn->is_union) {
                /* XXX An union is understood as a sequence of exceptions */
                for (int k = 0; k < rpc->exn->fields_len; k++) {
                    const iop_field_t *f   = rpc->exn->fields + k;
                    const iop_struct_t *st = f->u1.st_desc;
                    assert(f->type == IOP_T_STRUCT);
                    xmlpp_opentag(&wpp->pp, "element");
                    xmlpp_putattrfmt(&wpp->pp, "name", "%s.%s.%sFault",
                                     alias->name.s, rpc->name.s, f->name.s);
                    xmlpp_putattrfmt(&wpp->pp, "type", "tns:%s",
                                     st->fullname.s);
                    xmlpp_closetag(&wpp->pp);
                }
            } else {
                xmlpp_opentag(&wpp->pp, "element");
                xmlpp_putattrfmt(&wpp->pp, "name", "%s.%s.Fault",
                                 alias->name.s, rpc->name.s);
                xmlpp_putattrfmt(&wpp->pp, "type", "tns:%s",
                                 rpc->exn->fullname.s);
                xmlpp_closetag(&wpp->pp);
            }
        }
    }
    xmlpp_closentag(&wpp->pp, 2);

    qv_wipe(iop_type, &a);
    qv_wipe(iop_enum, &b);
    qh_wipe(iop_type, &h);
}

static void iop_xwsdl_put_msg(wsdlpp_t *wpp, const char *iname,
                              const char *rname, const char *type)
{
    xmlpp_opentag(&wpp->pp, "message");
    xmlpp_putattrfmt(&wpp->pp, "name", "%s.%s%s", iname, rname, type);
    xmlpp_opentag(&wpp->pp, "part");
    xmlpp_putattr(&wpp->pp, "name", "parameters");
    xmlpp_putattrfmt(&wpp->pp, "element", "tns:%s.%s%s", iname, rname, type);
    xmlpp_closentag(&wpp->pp, 2);
}

static void iop_xwsdl_put_msgs(wsdlpp_t *wpp, const iop_mod_t *mod)
{
    wpp_add_comment(wpp, "\n\n", "WSDL Messages");
    /* Dump the authentification header message */
    if (wpp->wauth) {
        xmlpp_opentag(&wpp->pp, "message");
        xmlpp_putattr(&wpp->pp, "name", "identityHeader");
        xmlpp_opentag(&wpp->pp, "part");
        xmlpp_putattr(&wpp->pp, "name", "requestHeader");
        xmlpp_putattr(&wpp->pp, "element", "tns:callerIdentity");
        xmlpp_closentag(&wpp->pp, 2);
    }
    for (int i = 0; i < mod->ifaces_len; i++) {
        const iop_iface_alias_t *alias = &mod->ifaces[i];
        const iop_iface_t *iface = mod->ifaces[i].iface;

        for (int j = 0; j < iface->funs_len; j++) {
            const iop_rpc_t *rpc = &iface->funs[j];

            /* Check implementation table */
            WSDL_CHECK_SKIP(wpp, alias, rpc);

            iop_xwsdl_put_msg(wpp, mod->ifaces[i].name.s, rpc->name.s, "Req");

            if (!rpc->async) {
                iop_xwsdl_put_msg(wpp, mod->ifaces[i].name.s, rpc->name.s,
                                  "Res");
            }

            if (rpc->exn == &iop__void__s)
                continue;
            if (rpc->exn->is_union) {
                /* XXX An union is understood as a sequence of exceptions */
                for (int k = 0; k < rpc->exn->fields_len; k++) {
                    const iop_field_t *f   = rpc->exn->fields + k;
                    char suffix[32];
                    assert(f->type == IOP_T_STRUCT);

                    snprintf(suffix, sizeof(suffix), ".%sFault", f->name.s);
                    iop_xwsdl_put_msg(wpp, mod->ifaces[i].name.s, rpc->name.s,
                                      suffix);
                }
            } else {
                iop_xwsdl_put_msg(wpp, mod->ifaces[i].name.s, rpc->name.s,
                                  ".Fault");
            }
        }
    }
}

static void iop_xwsdl_put_ports(wsdlpp_t *wpp, const iop_mod_t *mod)
{
    wpp_add_comment(wpp, "\n\n", "WSDL Ports");

    xmlpp_opentag(&wpp->pp, "portType");
    xmlpp_putattrfmt(&wpp->pp, "name", "%sPortType", mod->fullname.s);

    for (int i = 0; i < mod->ifaces_len; i++) {
        const iop_iface_alias_t *alias = &mod->ifaces[i];

        for (int j = 0; j < alias->iface->funs_len; j++) {
            const iop_rpc_t *rpc = &alias->iface->funs[j];

            /* Check implementation table */
            WSDL_CHECK_SKIP(wpp, alias, rpc);

            xmlpp_opentag(&wpp->pp, "operation");
            xmlpp_putattrfmt(&wpp->pp, "name", "%s.%s", alias->name.s,
                             rpc->name.s);
            xmlpp_opentag(&wpp->pp, "input");
            xmlpp_putattrfmt(&wpp->pp, "message", "tns:%s.%sReq",
                             alias->name.s, rpc->name.s);
            if (!rpc->async) {
                xmlpp_opensib(&wpp->pp, "output");
                xmlpp_putattrfmt(&wpp->pp, "message", "tns:%s.%sRes",
                                 alias->name.s, rpc->name.s);
            }

            if (rpc->exn == &iop__void__s) {
                xmlpp_closentag(&wpp->pp, 2);
                continue;
            }
            if (rpc->exn->is_union) {
                /* XXX An union is understood as a sequence of exceptions */
                for (int k = 0; k < rpc->exn->fields_len; k++) {
                    const iop_field_t *f   = rpc->exn->fields + k;
                    assert(f->type == IOP_T_STRUCT);

                    xmlpp_opensib(&wpp->pp, "fault");
                    xmlpp_putattrfmt(&wpp->pp, "message", "tns:%s.%s.%sFault",
                                     alias->name.s, rpc->name.s, f->name.s);
                    xmlpp_putattrfmt(&wpp->pp, "name", "%s.%s.%sFault",
                                     alias->name.s, rpc->name.s, f->name.s);
                }
            } else {
                xmlpp_opensib(&wpp->pp, "fault");
                xmlpp_putattrfmt(&wpp->pp, "message", "tns:%s.%s.Fault",
                                 alias->name.s, rpc->name.s);
                xmlpp_putattrfmt(&wpp->pp, "name", "%s.%s.Fault", alias->name.s,
                                 rpc->name.s);
            }
            xmlpp_closentag(&wpp->pp, 2);
        }
    }
    xmlpp_closetag(&wpp->pp);
}

static void iop_xwsdl_put_bindings(wsdlpp_t *wpp, const iop_mod_t *mod)
{
    wpp_add_comment(wpp, "\n\n", "WSDL Bindings");
    xmlpp_opentag(&wpp->pp, "binding");
    xmlpp_putattrfmt(&wpp->pp, "name", "%sBinding", mod->fullname.s);
    xmlpp_putattrfmt(&wpp->pp, "type", "tns:%sPortType", mod->fullname.s);

    xmlpp_opentag(&wpp->pp, "soap:binding");
    xmlpp_putattr(&wpp->pp, "style", "document");
    xmlpp_putattr(&wpp->pp, "transport", "http://schemas.xmlsoap.org/soap/http");
    xmlpp_closetag(&wpp->pp);

    for (int i = 0; i < mod->ifaces_len; i++) {
        const iop_iface_alias_t *alias = &mod->ifaces[i];

        for (int j = 0; j < alias->iface->funs_len; j++) {
            const iop_rpc_t *rpc = &alias->iface->funs[j];

            /* Check implementation table */
            WSDL_CHECK_SKIP(wpp, alias, rpc);

            xmlpp_opentag(&wpp->pp, "operation");
            xmlpp_putattrfmt(&wpp->pp, "name", "%s.%s", alias->name.s,
                             rpc->name.s);

            xmlpp_opentag(&wpp->pp, "soap:operation");
            xmlpp_putattr(&wpp->pp, "soapAction", "");
            xmlpp_opensib(&wpp->pp, "input");
            /* Dump the authentification header binding */
            if (wpp->wauth) {
                xmlpp_opentag(&wpp->pp, "soap:header");
                xmlpp_putattr(&wpp->pp, "use", "literal");
                xmlpp_putattr(&wpp->pp, "message", "tns:identityHeader");
                xmlpp_putattr(&wpp->pp, "part", "requestHeader");
                xmlpp_closetag(&wpp->pp);
            }
            xmlpp_opentag(&wpp->pp, "soap:body");
            xmlpp_putattr(&wpp->pp, "use", "literal");
            xmlpp_closetag(&wpp->pp);
            if (!rpc->async) {
                xmlpp_opensib(&wpp->pp, "output");
                xmlpp_opentag(&wpp->pp, "soap:body");
                xmlpp_putattr(&wpp->pp, "use", "literal");
                xmlpp_closetag(&wpp->pp);
            }

            if (rpc->exn == &iop__void__s) {
                xmlpp_closentag(&wpp->pp, 2);
                continue;
            }
            if (rpc->exn->is_union) {
                /* XXX An union is understood as a sequence of exceptions */
                for (int k = 0; k < rpc->exn->fields_len; k++) {
                    const iop_field_t *f   = rpc->exn->fields + k;
                    assert(f->type == IOP_T_STRUCT);
                    xmlpp_opensib(&wpp->pp, "fault");
                    xmlpp_putattrfmt(&wpp->pp, "name", "%s.%s.%sFault",
                                     alias->name.s, rpc->name.s, f->name.s);
                    xmlpp_opentag(&wpp->pp, "soap:fault");
                    xmlpp_putattrfmt(&wpp->pp, "name", "%s.%s.%sFault",
                                     alias->name.s, rpc->name.s, f->name.s);
                    xmlpp_putattr(&wpp->pp, "use", "literal");
                    xmlpp_closetag(&wpp->pp);
                }
                xmlpp_closetag(&wpp->pp);
            } else {
                xmlpp_opensib(&wpp->pp, "fault");
                xmlpp_putattrfmt(&wpp->pp, "name", "%s.%s.Fault",
                                 alias->name.s, rpc->name.s);
                xmlpp_opentag(&wpp->pp, "soap:fault");
                xmlpp_putattrfmt(&wpp->pp, "name", "%s.%s.Fault",
                                 alias->name.s, rpc->name.s);
                xmlpp_putattr(&wpp->pp, "use", "literal");
                xmlpp_closentag(&wpp->pp, 2);
            }
            xmlpp_closetag(&wpp->pp);
        }
    }
    xmlpp_closetag(&wpp->pp);
}

static void
iop_xwsdl_put_service(wsdlpp_t *wpp, const iop_mod_t *mod, const char *addr)
{
    wpp_add_comment(wpp, "\n\n", "WSDL Services");

    xmlpp_opentag(&wpp->pp, "service");
    xmlpp_putattr(&wpp->pp, "name", mod->fullname.s);

    xmlpp_opentag(&wpp->pp, "port");
    xmlpp_putattrfmt(&wpp->pp, "name", "%sEndPoint", mod->fullname.s);
    xmlpp_putattrfmt(&wpp->pp, "binding", "tns:%sBinding", mod->fullname.s);
    if (addr) {
        xmlpp_opentag(&wpp->pp, "soap:address");
        xmlpp_putattr(&wpp->pp, "location", addr);
        xmlpp_closetag(&wpp->pp);
    }
    xmlpp_closentag(&wpp->pp, 2);
}

void iop_xwsdl(sb_t *sb, const iop_mod_t *mod, qh_t(xwsdl_impl) *impl,
               const char *ns, const char *addr, bool with_auth,
               bool with_enums)
{
    wsdlpp_t wpp;

    wpp.impl   = impl;
    wpp.wauth  = with_auth;
    wpp.wenums = with_enums;

    xmlpp_open_banner(&wpp.pp, sb);
    xmlpp_opentag(&wpp.pp, "definitions");
    xmlpp_nl(&wpp.pp);
    xmlpp_putattr(&wpp.pp, "xmlns",      "http://schemas.xmlsoap.org/wsdl/");
    xmlpp_nl(&wpp.pp);
    xmlpp_putattr(&wpp.pp, "xmlns:soap", "http://schemas.xmlsoap.org/wsdl/soap/");
    xmlpp_nl(&wpp.pp);
    xmlpp_putattr(&wpp.pp, "xmlns:tns",  ns);
    xmlpp_nl(&wpp.pp);
    xmlpp_putattr(&wpp.pp, "targetNamespace", ns);
    iop_xwsdl_put_types(&wpp, mod, ns);
    iop_xwsdl_put_msgs(&wpp, mod);
    iop_xwsdl_put_ports(&wpp, mod);
    iop_xwsdl_put_bindings(&wpp, mod);
    iop_xwsdl_put_service(&wpp, mod, addr);
    xmlpp_close(&wpp.pp);
}
