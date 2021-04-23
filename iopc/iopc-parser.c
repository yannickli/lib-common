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

#include "iopc.h"
#include "iopctokens.h"

typedef struct iopc_parser_t {
    qv_t(iopc_token) tokens;
    struct lexdata *ld;

    const qv_t(cstr) *includes;
    const qm_t(env)  *env;
    const char *base;
    iop_cfolder_t *cfolder;
} iopc_parser_t;

qm_kptr_t(enums, char, const iopc_enum_field_t *, qhash_str_hash,
          qhash_str_equal);

static struct {
    qm_t(pkg)       mods;
    qm_t(enums)     enums;
    qm_t(enums)     enums_forbidden;
    qm_t(attr_desc) attrs;
} iopc_parser_g;
#define _G  iopc_parser_g

/* reserved keywords in field names */
static const char * const reserved_keywords[] = {
    /* C keywords */
    "auto", "bool", "break", "case", "char", "const", "continue", "default",
    "do", "double", "else", "enum", "extern", "float", "for", "goto", "if",
    "inline", "int", "long", "register", "restrict", "return", "short",
    "signed", "sizeof", "static", "struct", "switch", "typedef", "union",
    "unsigned", "void", "volatile", "while",
    /* Java and C++ keywords */
    "abstract", "assert", "boolean", "break", "byte", "case", "catch", "char",
    "const", "continue", "default", "do", "double", "else", "enum", "extends",
    "false", "final", "finally", "float", "for", "friend", "goto", "if",
    "implements", "import", "instanceof", "int", "interface", "long",
    "mutable", "namespace", "native", "null", "operator", "package",
    "private", "protected", "public", "return", "short", "static", "strictfp",
    "super", "switch", "synchronized", "template", "this", "throw", "throws",
    "transient", "true", "try", "typename", "virtual", "void", "volatile",
    "while",
    /* Language keywords */
    "in", "null", "out", "throw", "interface", "module", "package",
};

static const char * const avoid_keywords[] = {
    /* sadly already in use */
    "class", "new", "delete", "explicit",
};

static bool warn(qv_t(iopc_attr) *attrs, const char *category)
{
    lstr_t s = LSTR(category);

    if (!attrs)
        return true;

    qv_for_each_entry(iopc_attr, attr, attrs) {
        if (attr->desc->id != IOPC_ATTR_NOWARN) {
            continue;
        }

        qv_for_each_ptr(iopc_arg, arg, &attr->args) {
            if (lstr_equal2(arg->v.s, s)) {
                return false;
            }
        }
    }
    return true;
}

static void check_name(const char *name, iopc_loc_t loc,
                       qv_t(iopc_attr) *attrs)
{
    if (strchr(name, '_'))
        fatal_loc("%s contains a _", loc, name);
    for (int i = 0; i < countof(reserved_keywords); i++) {
        if (strequal(name, reserved_keywords[i]))
            fatal_loc("%s is a reserved keyword", loc, name);
    }
    if (warn(attrs, "keyword")) {
        for (int i = 0; i < countof(avoid_keywords); i++) {
            if (strequal(name, avoid_keywords[i]))
                warn_loc("%s is a keyword in some languages", loc, name);
        }
    }
}

static iopc_pkg_t *
iopc_try_file(iopc_parser_t *pp, const char *dir, iopc_path_t *path)
{
    struct stat st;
    char file[PATH_MAX];
    iopc_pkg_t *pkg;

    snprintf(file, sizeof(file), "%s/%s", dir, pretty_path(path));
    path_simplify(file);

    pkg = qm_get_def(pkg, &_G.mods, file, NULL);
    if (pkg) {
        return pkg;
    }
    if (pp->env) {
        const char *data = qm_get_def_safe(env, pp->env, file, NULL);

        if (data) {
            return iopc_parse_file(pp->includes, pp->env, file, data, false);
        }
    }
    if (stat(file, &st) == 0 && S_ISREG(st.st_mode)) {
        return iopc_parse_file(pp->includes, pp->env, file, NULL, false);
    }
    return NULL;
}

/* ----- attributes {{{*/

static const char *type_to_str(iopc_attr_type_t type)
{
    switch (type) {
      case IOPC_ATTR_T_INT:     return "integer";
      case IOPC_ATTR_T_BOOL:    return "boolean";
      case IOPC_ATTR_T_ENUM:    return "enum";
      case IOPC_ATTR_T_DOUBLE:  return "double";
      case IOPC_ATTR_T_STRING:  return "string";
      case IOPC_ATTR_T_DATA:    return "data";
      case IOPC_ATTR_T_UNION:   return "union";
      case IOPC_ATTR_T_STRUCT:  return "struct";
      case IOPC_ATTR_T_XML:     return "xml";
      case IOPC_ATTR_T_RPC:     return "rpc";
      case IOPC_ATTR_T_IFACE:   return "interface";
      case IOPC_ATTR_T_MOD:     return "module";
      case IOPC_ATTR_T_SNMP_IFACE:  return "snmpIface";
      case IOPC_ATTR_T_SNMP_OBJ:    return "snmpObj";
      case IOPC_ATTR_T_SNMP_TBL:    return "snmpTbl";
      default:                  fatal("invalid type %d", type);
    }
}

static void check_attr_type_decl(iopc_attr_t *attr, iopc_attr_type_t type)
{
    if (!TST_BIT(&attr->desc->flags, IOPC_ATTR_F_DECL)) {
        fatal_loc("attribute %*pM does not apply to declarations",
                  attr->loc,
                  LSTR_FMT_ARG(attr->desc->name));
    }

    if (!TST_BIT(&attr->desc->types, type)) {
        fatal_loc("attribute %*pM does not apply to %s",
                  attr->loc,
                  LSTR_FMT_ARG(attr->desc->name),
                  type_to_str(type));
    }
}

static void check_attr_type_field(iopc_attr_t *attr, iopc_field_t *f,
                                  bool tdef)
{
    const char *tstr = tdef ? "typedefs" : "fields";
    iopc_attr_type_t type;

    if (!(attr->desc->flags & IOPC_ATTR_F_FIELD_ALL)) {
        fatal_loc("attribute %*pM does not apply to %s", attr->loc,
                  LSTR_FMT_ARG(attr->desc->name), tstr);
    }

    switch (f->kind) {
      case IOP_T_DATA:      type = IOPC_ATTR_T_DATA; break;
      case IOP_T_DOUBLE:    type = IOPC_ATTR_T_DOUBLE; break;
      case IOP_T_STRING:    type = IOPC_ATTR_T_STRING; break;
      case IOP_T_XML:       type = IOPC_ATTR_T_XML; break;
      case IOP_T_STRUCT:    type = IOPC_ATTR_T_STRUCT; break;
      case IOP_T_UNION:     type = IOPC_ATTR_T_UNION; break;
      case IOP_T_ENUM:      type = IOPC_ATTR_T_ENUM; break;
      case IOP_T_BOOL:      type = IOPC_ATTR_T_BOOL; break;
      default:              type = IOPC_ATTR_T_INT; break;
    }

    if (f->kind == IOP_T_STRUCT && !f->struct_def) {
        /* struct or union or enum -> the typer will know the real type and
         * will check this attribute in iopc_check_field_attributes */
        return;
    }

    switch (f->repeat) {
      case IOP_R_REQUIRED:
        if (!TST_BIT(&attr->desc->flags, IOPC_ATTR_F_FIELD_REQUIRED)) {
            fatal_loc("attribute %*pM does not apply to required %s",
                      attr->loc, LSTR_FMT_ARG(attr->desc->name), tstr);
        }
        break;

      case IOP_R_DEFVAL:
        if (!TST_BIT(&attr->desc->flags, IOPC_ATTR_F_FIELD_DEFVAL)) {
            fatal_loc("attribute %*pM does not apply to %s "
                      "with default value", attr->loc,
                      LSTR_FMT_ARG(attr->desc->name), tstr);
        }
        break;

      case IOP_R_OPTIONAL:
        if (!TST_BIT(&attr->desc->flags, IOPC_ATTR_F_FIELD_OPTIONAL)) {
            fatal_loc("attribute %*pM does not apply to optional %s",
                      attr->loc, LSTR_FMT_ARG(attr->desc->name), tstr);
        }
        break;

      case IOP_R_REPEATED:
        if (!TST_BIT(&attr->desc->flags, IOPC_ATTR_F_FIELD_REPEATED)) {
            fatal_loc("attribute %*pM does not apply to repeated %s",
                      attr->loc, LSTR_FMT_ARG(attr->desc->name), tstr);
        }
        break;
    }

    if (!TST_BIT(&attr->desc->types, type)) {
        fatal_loc("attribute %*pM does not apply to %s",
                  attr->loc,
                  LSTR_FMT_ARG(attr->desc->name),
                  type_to_str(type));
    }

    /* Field snmp specific checks */
    if (attr->desc->id == IOPC_ATTR_SNMP_INDEX && !f->snmp_is_in_tbl) {
        fatal_loc("field '%s' does not support @snmpIndex attribute",
                  attr->loc, f->name);
    }
}

void iopc_check_field_attributes(iopc_field_t *f, bool tdef)
{
    const char *tstr = tdef ? "typedefs" : "fields";
    iopc_attr_type_t type;
    unsigned flags = 0;

    switch (f->kind) {
      case IOP_T_DATA:      type = IOPC_ATTR_T_DATA; break;
      case IOP_T_DOUBLE:    type = IOPC_ATTR_T_DOUBLE; break;
      case IOP_T_STRING:    type = IOPC_ATTR_T_STRING; break;
      case IOP_T_XML:       type = IOPC_ATTR_T_XML; break;
      case IOP_T_BOOL:      type = IOPC_ATTR_T_BOOL; break;
      case IOP_T_STRUCT:    type = IOPC_ATTR_T_STRUCT; break;
      case IOP_T_UNION:     type = IOPC_ATTR_T_UNION; break;
      case IOP_T_ENUM:      type = IOPC_ATTR_T_ENUM; break;
      default:              type = IOPC_ATTR_T_INT; break;
    }

    qv_for_each_entry(iopc_attr, attr, &f->attrs) {
        if (!TST_BIT(&attr->desc->types, type)) {
            fatal_loc("attribute %*pM does not apply to %s",
                      attr->loc,
                      LSTR_FMT_ARG(attr->desc->name),
                      type_to_str(type));
        }

        switch (f->repeat) {
          case IOP_R_REQUIRED:
            if (!TST_BIT(&attr->desc->flags, IOPC_ATTR_F_FIELD_REQUIRED)) {
                fatal_loc("attribute %*pM does not apply to required %s",
                          attr->loc, LSTR_FMT_ARG(attr->desc->name), tstr);
            }
            break;

          case IOP_R_DEFVAL:
            if (!TST_BIT(&attr->desc->flags, IOPC_ATTR_F_FIELD_DEFVAL)) {
                fatal_loc("attribute %*pM does not apply to %s "
                          "with default value",
                          attr->loc, LSTR_FMT_ARG(attr->desc->name), tstr);
            }
            break;

          case IOP_R_OPTIONAL:
            if (!TST_BIT(&attr->desc->flags, IOPC_ATTR_F_FIELD_OPTIONAL)) {
                fatal_loc("attribute %*pM does not apply to optional %s",
                          attr->loc, LSTR_FMT_ARG(attr->desc->name), tstr);
            }
            break;

          case IOP_R_REPEATED:
            if (!TST_BIT(&attr->desc->flags, IOPC_ATTR_F_FIELD_REPEATED)) {
                fatal_loc("attribute %*pM does not apply to repeated %s",
                          attr->loc, LSTR_FMT_ARG(attr->desc->name), tstr);
            }
            break;

          default:
            fatal_loc("unknown repeat kind for field `%s`", attr->loc, f->name);
        }

        /* Field specific checks */
        switch (attr->desc->id) {
          case IOPC_ATTR_ALLOW:
          case IOPC_ATTR_DISALLOW:
            SET_BIT(&flags, attr->desc->id);
            if (TST_BIT(&flags, IOPC_ATTR_ALLOW)
            &&  TST_BIT(&flags, IOPC_ATTR_DISALLOW))
            {
                fatal_loc("cannot use both @allow and @disallow on the same "
                          "field", attr->loc);
            }

            qv_for_each_ptr(iopc_arg, arg, &attr->args) {
                bool found = false;

                if (type == IOPC_ATTR_T_UNION) {
                    qv_for_each_entry(iopc_field, uf, &f->union_def->fields) {
                        if (strequal(uf->name, arg->v.s.s)) {
                            found = true;
                            break;
                        }
                    }
                } else
                if (type == IOPC_ATTR_T_ENUM) {
                    qv_for_each_entry(iopc_enum_field, ef,
                                      &f->enum_def->values)
                    {
                        if (strequal(ef->name, arg->v.s.s)) {
                            found = true;
                            break;
                        }
                    }
                }

                if (!found) {
                    fatal_loc("unknown field %*pM in %s",
                              attr->loc, LSTR_FMT_ARG(arg->v.s),
                              f->type_name);
                }
            }
            break;

          default:
            break;
        }
    }
}

static iopc_attr_desc_t *add_attr(iopc_attr_id_t id, const char *name)
{
    iopc_attr_desc_t d;
    int pos;

    iopc_attr_desc_init(&d);
    d.id    = id;
    d.name  = LSTR(name);
    pos = qm_put(attr_desc, &_G.attrs, &d.name, d, 0);

    if (pos & QHASH_COLLISION) {
        fatal("attribute %s already exists", name);
    }
    return &_G.attrs.values[pos];
}

static void init_attributes(void)
{
    iopc_attr_desc_t *d;

#define ADD_ATTR_ARG(_d, _s, _tok) \
    ({  \
        iopc_arg_desc_t arg;                                    \
        iopc_arg_desc_init(&arg);                               \
        arg.name = LSTR(_s);                                    \
        arg.type = _tok;                                        \
        qv_append(iopc_arg_desc, &_d->args, arg);               \
    })

    d = add_attr(IOPC_ATTR_CTYPE, "ctype");
    SET_BIT(&d->flags, IOPC_ATTR_F_DECL);
    SET_BIT(&d->flags, IOPC_ATTR_F_MULTI);
    SET_BIT(&d->types, IOPC_ATTR_T_STRUCT);
    SET_BIT(&d->types, IOPC_ATTR_T_UNION);
    SET_BIT(&d->types, IOPC_ATTR_T_ENUM);
    ADD_ATTR_ARG(d, "type", ITOK_IDENT);

    d = add_attr(IOPC_ATTR_NOWARN, "nowarn");
    d->flags |= IOPC_ATTR_F_FIELD_ALL;
    SET_BIT(&d->flags, IOPC_ATTR_F_DECL);
    SET_BIT(&d->flags, IOPC_ATTR_F_MULTI);
    d->types |= IOPC_ATTR_T_ALL;
    ADD_ATTR_ARG(d, "value", ITOK_IDENT);

    d = add_attr(IOPC_ATTR_PREFIX, "prefix");
    SET_BIT(&d->flags, IOPC_ATTR_F_DECL);
    SET_BIT(&d->types, IOPC_ATTR_T_ENUM);
    ADD_ATTR_ARG(d, "name", ITOK_IDENT);

    d = add_attr(IOPC_ATTR_STRICT, "strict");
    SET_BIT(&d->flags, IOPC_ATTR_F_DECL);
    SET_BIT(&d->flags, IOPC_ATTR_F_CONSTRAINT);
    SET_BIT(&d->types, IOPC_ATTR_T_ENUM);

    d = add_attr(IOPC_ATTR_MIN, "min");
    d->flags |= IOPC_ATTR_F_FIELD_ALL;
    SET_BIT(&d->flags, IOPC_ATTR_F_CONSTRAINT);
    SET_BIT(&d->types, IOPC_ATTR_T_INT);
    SET_BIT(&d->flags, IOPC_ATTR_F_MULTI);
    SET_BIT(&d->types, IOPC_ATTR_T_DOUBLE);
    ADD_ATTR_ARG(d, "value", ITOK_DOUBLE);

    d = add_attr(IOPC_ATTR_MAX, "max");
    d->flags |= IOPC_ATTR_F_FIELD_ALL;
    SET_BIT(&d->flags, IOPC_ATTR_F_CONSTRAINT);
    SET_BIT(&d->types, IOPC_ATTR_T_INT);
    SET_BIT(&d->flags, IOPC_ATTR_F_MULTI);
    SET_BIT(&d->types, IOPC_ATTR_T_DOUBLE);
    ADD_ATTR_ARG(d, "value", ITOK_DOUBLE);

    d = add_attr(IOPC_ATTR_MIN_LENGTH, "minLength");
    d->flags |= IOPC_ATTR_F_FIELD_ALL;
    SET_BIT(&d->flags, IOPC_ATTR_F_CONSTRAINT);
    SET_BIT(&d->types, IOPC_ATTR_T_STRING);
    SET_BIT(&d->flags, IOPC_ATTR_F_MULTI);
    SET_BIT(&d->types, IOPC_ATTR_T_DATA);
    ADD_ATTR_ARG(d, "value", ITOK_INTEGER);

    d = add_attr(IOPC_ATTR_MAX_LENGTH, "maxLength");
    d->flags |= IOPC_ATTR_F_FIELD_ALL;
    SET_BIT(&d->flags, IOPC_ATTR_F_CONSTRAINT);
    SET_BIT(&d->types, IOPC_ATTR_T_STRING);
    SET_BIT(&d->flags, IOPC_ATTR_F_MULTI);
    SET_BIT(&d->types, IOPC_ATTR_T_DATA);
    ADD_ATTR_ARG(d, "value", ITOK_INTEGER);

    d = add_attr(IOPC_ATTR_LENGTH, "length");
    d->flags |= IOPC_ATTR_F_FIELD_ALL;
    SET_BIT(&d->flags, IOPC_ATTR_F_CONSTRAINT);
    SET_BIT(&d->types, IOPC_ATTR_T_STRING);
    SET_BIT(&d->types, IOPC_ATTR_T_DATA);
    ADD_ATTR_ARG(d, "value", ITOK_INTEGER);

    d = add_attr(IOPC_ATTR_MIN_OCCURS, "minOccurs");
    SET_BIT(&d->flags, IOPC_ATTR_F_FIELD_REPEATED);
    SET_BIT(&d->flags, IOPC_ATTR_F_CONSTRAINT);
    SET_BIT(&d->flags, IOPC_ATTR_F_MULTI);
    d->types |= IOPC_ATTR_T_ALL;
    ADD_ATTR_ARG(d, "value", ITOK_INTEGER);

    d = add_attr(IOPC_ATTR_MAX_OCCURS, "maxOccurs");
    SET_BIT(&d->flags, IOPC_ATTR_F_FIELD_REPEATED);
    SET_BIT(&d->flags, IOPC_ATTR_F_CONSTRAINT);
    SET_BIT(&d->flags, IOPC_ATTR_F_MULTI);
    d->types |= IOPC_ATTR_T_ALL;
    ADD_ATTR_ARG(d, "value", ITOK_INTEGER);

    d = add_attr(IOPC_ATTR_CDATA, "cdata");
    d->flags |= IOPC_ATTR_F_FIELD_ALL;
    SET_BIT(&d->types, IOPC_ATTR_T_STRING);

    d = add_attr(IOPC_ATTR_NON_EMPTY, "nonEmpty");
    d->flags |= IOPC_ATTR_F_FIELD_ALL;
    SET_BIT(&d->flags, IOPC_ATTR_F_CONSTRAINT);
    SET_BIT(&d->types, IOPC_ATTR_T_STRING);
    SET_BIT(&d->types, IOPC_ATTR_T_DATA);
    SET_BIT(&d->types, IOPC_ATTR_T_XML);

    d = add_attr(IOPC_ATTR_NON_ZERO, "nonZero");
    d->flags |= IOPC_ATTR_F_FIELD_ALL;
    SET_BIT(&d->flags, IOPC_ATTR_F_CONSTRAINT);
    SET_BIT(&d->types, IOPC_ATTR_T_INT);
    SET_BIT(&d->types, IOPC_ATTR_T_DOUBLE);

    d = add_attr(IOPC_ATTR_PATTERN, "pattern");
    d->flags |= IOPC_ATTR_F_FIELD_ALL;
    SET_BIT(&d->flags, IOPC_ATTR_F_CONSTRAINT);
    SET_BIT(&d->types, IOPC_ATTR_T_STRING);
    ADD_ATTR_ARG(d, "value", ITOK_STRING);

    d = add_attr(IOPC_ATTR_PRIVATE, "private");
    d->flags |= IOPC_ATTR_F_FIELD_ALL_BUT_REQUIRED;
    d->types |= IOPC_ATTR_T_ALL;

    d = add_attr(IOPC_ATTR_ALIAS, "alias");
    SET_BIT(&d->flags, IOPC_ATTR_F_DECL);
    SET_BIT(&d->flags, IOPC_ATTR_F_MULTI);
    SET_BIT(&d->types, IOPC_ATTR_T_RPC);
    ADD_ATTR_ARG(d, "name", ITOK_IDENT);

    d = add_attr(IOPC_ATTR_NO_REORDER, "noReorder");
    SET_BIT(&d->flags, IOPC_ATTR_F_DECL);
    SET_BIT(&d->types, IOPC_ATTR_T_STRUCT);

    d = add_attr(IOPC_ATTR_ALLOW, "allow");
    d->flags |= IOPC_ATTR_F_FIELD_ALL;
    SET_BIT(&d->flags, IOPC_ATTR_F_CONSTRAINT);
    SET_BIT(&d->flags, IOPC_ATTR_F_MULTI);
    SET_BIT(&d->types, IOPC_ATTR_T_UNION);
    SET_BIT(&d->types, IOPC_ATTR_T_ENUM);
    ADD_ATTR_ARG(d, "field", ITOK_IDENT);

    d = add_attr(IOPC_ATTR_DISALLOW, "disallow");
    d->flags |= IOPC_ATTR_F_FIELD_ALL;
    SET_BIT(&d->flags, IOPC_ATTR_F_CONSTRAINT);
    SET_BIT(&d->flags, IOPC_ATTR_F_MULTI);
    SET_BIT(&d->types, IOPC_ATTR_T_UNION);
    SET_BIT(&d->types, IOPC_ATTR_T_ENUM);
    ADD_ATTR_ARG(d, "field", ITOK_IDENT);

    d = add_attr(IOPC_ATTR_GENERIC, "generic");
    d->flags |= IOPC_ATTR_F_FIELD_ALL;
    SET_BIT(&d->flags, IOPC_ATTR_F_MULTI);
    SET_BIT(&d->flags, IOPC_ATTR_F_DECL);
    d->types |= IOPC_ATTR_T_ALL;
    ADD_ATTR_ARG(d, "", ITOK_STRING);

    d = add_attr(IOPC_ATTR_DEPRECATED, "deprecated");
    d->flags |= IOPC_ATTR_F_FIELD_ALL;
    d->types |= IOPC_ATTR_T_ALL;
    SET_BIT(&d->flags, IOPC_ATTR_F_DECL);
    SET_BIT(&d->types, IOPC_ATTR_T_SNMP_IFACE);
    SET_BIT(&d->types, IOPC_ATTR_T_SNMP_OBJ);
    SET_BIT(&d->types, IOPC_ATTR_T_SNMP_TBL);

    d = add_attr(IOPC_ATTR_SNMP_PARAMS_FROM, "snmpParamsFrom");
    SET_BIT(&d->flags, IOPC_ATTR_F_MULTI);
    SET_BIT(&d->flags, IOPC_ATTR_F_DECL);
    SET_BIT(&d->types, IOPC_ATTR_T_SNMP_IFACE);
    ADD_ATTR_ARG(d, "param", ITOK_IDENT);

    d = add_attr(IOPC_ATTR_SNMP_PARAM, "snmpParam");
    SET_BIT(&d->flags, IOPC_ATTR_F_DECL);
    SET_BIT(&d->types, IOPC_ATTR_T_SNMP_OBJ);

    d = add_attr(IOPC_ATTR_SNMP_INDEX, "snmpIndex");
    d->flags |= IOPC_ATTR_F_FIELD_ALL;
    d->types |= IOPC_ATTR_T_ALL;
#undef ADD_ATTR_ARG
}

static int check_attr_multi(qv_t(iopc_attr) *attrs, iopc_attr_t *attr)
{
    qv_for_each_pos(iopc_attr, pos, attrs) {
        iopc_attr_t *a = attrs->tab[pos];
        if (a->desc == attr->desc) {
            /* Generic attributes share the same desc */
            if (a->desc->id == IOPC_ATTR_GENERIC) {
                if (lstr_equal2(a->real_name, attr->real_name)) {
                    fatal_loc("generic attribute '%*pM' must be unique for "
                              "each IOP object", attr->loc,
                              LSTR_FMT_ARG(attr->real_name));
                }
                return -1;
            }

            if (TST_BIT(&attr->desc->flags, IOPC_ATTR_F_MULTI)) {
                return pos;
            } else {
                fatal_loc("attribute %*pM must be unique", attr->loc,
                          LSTR_FMT_ARG(attr->desc->name));
            }
        }
    }
    return -1;
}

int
iopc_attr_check(const qv_t(iopc_attr) *attrs, iopc_attr_id_t attr_id,
                const qv_t(iopc_arg) **out)
{
    qv_for_each_entry(iopc_attr, e, attrs) {
        if (e->desc->id == attr_id) {
            if (out) {
                *out = &e->args;
            }
            return 0;
        }
    }
    return -1;
}

int t_iopc_attr_check_prefix(const qv_t(iopc_attr) *attrs, lstr_t *out)
{
    const qv_t(iopc_arg) *args = NULL;

    RETHROW(iopc_attr_check(attrs, IOPC_ATTR_PREFIX, &args));
    *out = t_lstr_dup(args->tab[0].v.s);
    return 0;
}

/*}}}*/
/*----- helpers -{{{-*/

static iopc_token_t *TK(iopc_parser_t *pp, int i)
{
    qv_t(iopc_token) *tks = &pp->tokens;

    while (i >= tks->len) {
        iopc_token_t *tk = iopc_next_token(pp->ld, false);
        if (!tk) {
            assert (tks->len && tks->tab[tks->len - 1]->token == ITOK_EOF);
            tk = iopc_token_dup(tks->tab[tks->len - 1]);
        }
        qv_append(iopc_token, tks, tk);
    }
    return tks->tab[i];
}

static void DROP(iopc_parser_t *pp, int len, int offset)
{
    qv_t(iopc_token) *tks = &pp->tokens;

    assert (offset < tks->len && len <= tks->len);
    for (int i = 0; i < len; i++) {
        iopc_token_delete(tks->tab + offset + i);
    }
    qv_splice(iopc_token, tks, offset, len, NULL, 0);
}
#define DROP(_pp, _len)  ((DROP)(_pp, _len, 0))

static bool CHECK(iopc_parser_t *pp, int i, iopc_tok_type_t token)
{
    iopc_token_t *tk = TK(pp, i);
    return tk->token == token;
}

static bool CHECK_NOEOF(iopc_parser_t *pp, int i, iopc_tok_type_t token)
{
    iopc_token_t *tk = TK(pp, i);
    if (tk->token == ITOK_EOF) {
        fatal_loc("unexpected end of file", tk->loc);
    }
    return tk->token == token;
}

static bool CHECK_KW(iopc_parser_t *pp, int i, const char *kw)
{
    iopc_token_t *tk = TK(pp, i);
    return tk->token == ITOK_IDENT && strequal(tk->b.data, kw);
}

static void WANT(iopc_parser_t *pp, int i, iopc_tok_type_t token)
{
    iopc_token_t *tk = TK(pp, i);

    if (tk->token != token) {
        t_scope;
        if (tk->token == ITOK_EOF) {
            fatal_loc("unexpected end of file", tk->loc);
        }
        fatal_loc("%s expected, but got %s instead",
                  tk->loc, t_pretty_token(token), t_pretty_token(tk->token));
    }
}

static bool SKIP(iopc_parser_t *pp, iopc_tok_type_t token)
{
    if (CHECK(pp, 0, token)) {
        DROP(pp, 1);
        return true;
    }
    return false;
}

static bool SKIP_KW(iopc_parser_t *pp, const char *kw)
{
    if (CHECK_KW(pp, 0, kw)) {
        DROP(pp, 1);
        return true;
    }
    return false;
}

static void EAT(iopc_parser_t *pp, iopc_tok_type_t token)
{
    WANT(pp, 0, token);
    DROP(pp, 1);
}

static void EAT_KW(iopc_parser_t *pp, const char *kw)
{
    iopc_token_t *tk = TK(pp, 0);

    if (!SKIP_KW(pp, kw)) {
        WANT(pp, 0, ITOK_IDENT);
        fatal_loc("%s expected, but got %s instead",
                  tk->loc, kw, tk->b.data);
    }
}

static inline char *dup_ident(iopc_token_t *tk)
{
    return p_dup(tk->b.data, tk->b.len + 1);
}
static inline const char *ident(iopc_token_t *tk)
{
    return tk->b.data;
}

static uint64_t
parse_constant_integer(iopc_parser_t *pp, int paren, bool *is_signed)
{
    uint64_t num = 0;
    int pos = 0;
    iopc_token_t *tk;
    int nparen = 1;

    for (;;) {
        tk = TK(pp, pos++);

        switch (tk->token) {
          case '-': case '+': case '*': case '/': case '~':
          case '&': case '|': case '%': case '^': case '(':
            if (tk->token == '(') {
                nparen++;
            }
            if (iop_cfolder_feed_operator(pp->cfolder,
                                          (iop_cfolder_op_t)tk->token) < 0)
            {
                fatal_loc("error when feeding the constant folder with `%c'",
                          tk->loc, tk->token);
            }
            break;

          case ')':
            nparen--;
            /* If we are in a function or in an attribute, check if it is the
             * end paren */
            if (paren == ')' && nparen == 0) {
                goto end;
            }

            if (iop_cfolder_feed_operator(pp->cfolder,
                                          (iop_cfolder_op_t)tk->token) < 0)
            {
                fatal_loc("error when feeding the constant folder with `%c'",
                          tk->loc, tk->token);
            }
            break;

          case ITOK_LSHIFT:
            if (iop_cfolder_feed_operator(pp->cfolder, CF_OP_LSHIFT) < 0)
                fatal_loc("error when feeding the constant folder with `<<'",
                          tk->loc);
            break;

          case ITOK_RSHIFT:
            if (iop_cfolder_feed_operator(pp->cfolder, CF_OP_RSHIFT) < 0)
                fatal_loc("error when feeding the constant folder with `>>'",
                          tk->loc);
            break;

          case ITOK_EXP:
            if (iop_cfolder_feed_operator(pp->cfolder, CF_OP_EXP) < 0)
                fatal_loc("error when feeding the constant folder with `**'",
                          tk->loc);
            break;

          case ITOK_INTEGER:
          case ITOK_BOOL:
            if (iop_cfolder_feed_number(pp->cfolder, tk->i, tk->i_is_signed) < 0)
            {
                if (tk->i_is_signed) {
                    fatal_loc("error when feeding the constant folder with `%"
                              PRIi64"'", tk->loc, (int64_t)tk->i);
                } else {
                    fatal_loc("error when feeding the constant folder with `%"
                              PRIu64"'", tk->loc, tk->i);
                }
            }
            break;

          case ITOK_IDENT:
            /* check for enum value or stop */
            {
                const iopc_enum_field_t *f;
                int qpos = qm_find(enums, &_G.enums, ident(tk));

                if (qpos < 0) {
                    /* XXX compatibility code which will be removed soon */
                    if ((qpos = qm_find(enums, &_G.enums_forbidden,
                                        ident(tk))) >= 0)
                    {
                        f = _G.enums_forbidden.values[qpos];
                        goto compatibility;
                    }
                    fatal_loc("unknown enumeration value `%s'", tk->loc,
                              ident(tk));
                }

                if (qm_find(enums, &_G.enums_forbidden, ident(tk)) >= 0) {
                    warn_loc("enum field identifier `%s` is ambiguous",
                             tk->loc, ident(tk));
                }

                f = _G.enums.values[qpos];

              compatibility:
                /* feed the enum value */
                if (iop_cfolder_feed_number(pp->cfolder, f->value, true) < 0)
                {
                    fatal_loc("error when feeding the constant folder with `%d'",
                              tk->loc, f->value);
                }
            }
            break;

          default:
            goto end;
        }
    }

  end:
    /* Let's try to get a result */
    if (iop_cfolder_get_result(pp->cfolder, &num, is_signed) < 0) {
        fatal_loc("invalid arithmetic expression", TK(pp, 0)->loc);
    }
    DROP(pp, pos - 1);

    return num;
}

/*-}}}-*/
/*----- doxygen -{{{-*/

#ifdef NDEBUG
  #define debug_dump_dox(X, Y)
#else
static void debug_dump_dox(qv_t(iopc_dox) comments, const char *name)
{
#define DEBUG_LVL  3

    if (!comments.len)
        return;

    e_trace(DEBUG_LVL, "BUILT DOX COMMENTS for %s", name);

    qv_for_each_ptr(iopc_dox, dox, &comments) {
        lstr_t type = iopc_dox_type_to_lstr(dox->type);

        e_trace(DEBUG_LVL, "type: %*pM", LSTR_FMT_ARG(type));
        e_trace(DEBUG_LVL, "desc: %*pM", LSTR_FMT_ARG(dox->desc));
        e_trace(DEBUG_LVL, "----------------------------------------");
    }
    e_trace(DEBUG_LVL, "****************************************");

#undef DEBUG_LVL
}
  #define debug_dump_dox(_c, _n)  ((debug_dump_dox)((_c),           \
      __builtin_types_compatible_p(typeof(_n), iopc_path_t *)       \
      ? pretty_path_dot((iopc_path_t *)(_n)) : (const char *)(_n)))
#endif

typedef enum iopc_dox_arg_dir_t {
    IOPC_DOX_ARG_DIR_IN,
    IOPC_DOX_ARG_DIR_OUT,
    IOPC_DOX_ARG_DIR_THROW,

    IOPC_DOX_ARG_DIR_count,
} iopc_dox_arg_dir_t;

static lstr_t iopc_dox_arg_dir_to_lstr(iopc_dox_arg_dir_t dir)
{
    switch (dir) {
      case IOPC_DOX_ARG_DIR_IN:    return LSTR("in");
      case IOPC_DOX_ARG_DIR_OUT:   return LSTR("out");
      case IOPC_DOX_ARG_DIR_THROW: return LSTR("throw");
      default:                     fatal("invalid doxygen arg dir %d", dir);
    }
}

static int iopc_dox_check_param_dir(lstr_t dir_name, iopc_dox_arg_dir_t *out)
{
    for (int i = 0; i < IOPC_DOX_ARG_DIR_count; i++) {
        if (lstr_equal2(dir_name, iopc_dox_arg_dir_to_lstr(i))) {
            *out = i;
            return 0;
        }
    }
    return -1;
}

lstr_t iopc_dox_type_to_lstr(iopc_dox_type_t type)
{
    switch (type) {
      case IOPC_DOX_TYPE_BRIEF:   return LSTR("brief");
      case IOPC_DOX_TYPE_DETAILS: return LSTR("details");
      case IOPC_DOX_TYPE_WARNING: return LSTR("warning");
      default:                    fatal("invalid doxygen type %d", type);
    }
}

/** XXX: @param[] description in appended to comments of the related
 *       parameter. It is a valid keyword in the input file, but is not put
 *       into output file.
 */
#define IOPC_DOX_TYPE_PARAM   IOPC_DOX_TYPE_count

static int iopc_dox_check_keyword(lstr_t keyword, int *type)
{
    for (int i = 0; i < IOPC_DOX_TYPE_count; i++) {
        if (lstr_equal2(keyword, iopc_dox_type_to_lstr(i))) {
            *type = i;
            return 0;
        }
    }
    if (lstr_equal2(keyword, LSTR("param"))) {
        *type = IOPC_DOX_TYPE_PARAM;
        return 0;
    }
    return -1;
}

iopc_dox_t *
iopc_dox_find_type(const qv_t(iopc_dox) *comments, iopc_dox_type_t type)
{
    qv_for_each_ptr(iopc_dox, p, comments) {
        if (p->type == type)
            return p;
    }
    return NULL;
}

static iopc_dox_t *
iopc_dox_add(qv_t(iopc_dox) *comments, iopc_dox_type_t type)
{
    iopc_dox_t *res = iopc_dox_init(qv_growlen(iopc_dox, comments, 1));
    res->type = type;
    return res;
}

static iopc_dox_t *
iopc_dox_find_type_or_create(qv_t(iopc_dox) *comments, iopc_dox_type_t type)
{
    iopc_dox_t *dox = iopc_dox_find_type(comments, type);

    if (!dox) {
        dox = iopc_dox_add(comments, type);
    }
    return dox;
}

static bool
iopc_dox_type_is_related(iopc_dox_type_t dox_type, iopc_attr_type_t attr_type)
{
    return dox_type != IOPC_DOX_TYPE_PARAM || attr_type == IOPC_ATTR_T_RPC;
}

static iopc_field_t *
iopc_dox_arg_find_in_fun(lstr_t name, iopc_dox_arg_dir_t dir,
                         const iopc_fun_t* fun)
{
    switch(dir) {
#define CASE_DIR(X, Y)                                                       \
      case IOPC_DOX_ARG_DIR_##X:                                             \
        if (!fun->Y##_is_anonymous) {                                        \
            if (name.len)                                                    \
                return NULL;                                                 \
            return fun->f##Y;                                                \
        }                                                                    \
        qv_for_each_entry(iopc_field, f, &fun->Y->fields) {                  \
            if (lstr_equal2(name, LSTR(f->name)))                            \
                return f;                                                    \
        }                                                                    \
        return NULL;

      CASE_DIR(IN, arg);
      CASE_DIR(OUT, res);
      CASE_DIR(THROW, exn);

      default: return NULL;

#undef CASE_DIR
    }
}

static void dox_chunk_params_args_validate(dox_chunk_t *chunk)
{
    if (chunk->params_args.len == 1 && chunk->paragraphs.len) {
        sb_skip(&chunk->paragraphs.tab[0], chunk->paragraph0_args_len);
        sb_ltrim(&chunk->paragraphs.tab[0]);
        chunk->paragraph0_args_len = 0;
    }
}

static void dox_chunk_autobrief_validate(dox_chunk_t *chunk)
{
    if (chunk->first_sentence_len
    &&  isspace(chunk->paragraphs.tab[0].data[chunk->first_sentence_len]))
    {
        pstream_t tmp = ps_initsb(&chunk->paragraphs.tab[0]);
        sb_t paragraph0_end;

        ps_skip(&tmp, chunk->first_sentence_len);
        ps_ltrim(&tmp);

        sb_init(&paragraph0_end);
        sb_add_ps(&paragraph0_end, tmp);

        sb_clip(&chunk->paragraphs.tab[0], chunk->first_sentence_len);

        qv_insert(sb, &chunk->paragraphs, 1, paragraph0_end);
        chunk->first_sentence_len = 0;
    }
}

static void dox_chunk_merge(dox_chunk_t *eating, dox_chunk_t *eaten)
{
    qv_for_each_entry(lstr, param, &eaten->params) {
        qv_append(lstr, &eating->params, param);
    }
    qv_for_each_entry(lstr, arg, &eaten->params_args) {
        qv_append(lstr, &eating->params_args, arg);
    }

    if (eating->paragraphs.len <= 1) {
        eating->paragraph0_args_len += eaten->paragraph0_args_len;
    }
    if (eating->paragraphs.len && eaten->paragraphs.len) {
        sb_addc(qv_last(sb, &eating->paragraphs), ' ');
        sb_addsb(qv_last(sb, &eating->paragraphs), &eaten->paragraphs.tab[0]);
        sb_wipe(&eaten->paragraphs.tab[0]);
        qv_skip(sb, &eaten->paragraphs, 1);
    }
    qv_for_each_entry(sb, paragraph, &eaten->paragraphs) {
        qv_append(sb, &eating->paragraphs, paragraph);
    }

    iopc_loc_merge(&eating->loc, eaten->loc);

    lstr_wipe(&eaten->keyword);
    qv_wipe(lstr, &eaten->params);
    qv_wipe(lstr, &eaten->params_args);
    qv_wipe(sb, &eaten->paragraphs);
}

static void dox_chunk_push_sb(dox_chunk_t *chunk, sb_t sb)
{
    if (chunk->paragraphs.len) {
        if (chunk->paragraphs.tab[0].len)
            sb_addc(&sb, ' ');
        sb_addsb(&sb, &chunk->paragraphs.tab[0]);
        sb_wipe(&chunk->paragraphs.tab[0]);
        chunk->paragraphs.tab[0] = sb;
    } else {
        qv_append(sb, &chunk->paragraphs, sb);
    }
}

static void dox_chunk_keyword_merge(dox_chunk_t *chunk)
{
    sb_t sb;

    if (!chunk->keyword.len)
        return;

    sb_init(&sb);
    sb_addc(&sb, '\\');
    sb_add_lstr(&sb, chunk->keyword);
    lstr_wipe(&chunk->keyword);
    dox_chunk_push_sb(chunk, sb);
}

static void dox_chunk_params_merge(dox_chunk_t *chunk)
{
    sb_t sb;

    if (!chunk->params.len)
        return;

    sb_init(&sb);

    sb_addc(&sb, '[');
    qv_for_each_ptr(lstr, s, &chunk->params) {
        sb_add_lstr(&sb, *s);
        if (s != qv_last(lstr, &chunk->params))
            sb_adds(&sb, ", ");
    }
    sb_addc(&sb, ']');
    dox_chunk_push_sb(chunk, sb);

    qv_deep_wipe(lstr, &chunk->params, lstr_wipe);
    qv_deep_wipe(lstr, &chunk->params_args, lstr_wipe);
    chunk->paragraph0_args_len = 0;
}

static int
read_dox(iopc_parser_t *pp, int tk_offset, qv_t(dox_chunk) *chunks, bool back,
         int ignore_token)
{
    const iopc_token_t *tk;
    dox_tok_t *dox;

    if (ignore_token && CHECK(pp, tk_offset, ignore_token))
        tk_offset++;

    tk = TK(pp, tk_offset);
    dox = tk->dox;

    if (!CHECK(pp, tk_offset, ITOK_DOX_COMMENT) || (back && !dox->is_back))
        return -1;

    /* XXX: when reading front, back comments are forced to be front
     *      with first chunk = "<" */
    if (!back && dox->is_back) {
        dox_chunk_t chunk;

        dox->is_back = false;
        dox_chunk_init(&chunk);
        sb_addc(sb_init(qv_growlen(sb, &chunk.paragraphs, 1)), '<');
        chunk.loc = tk->loc;
        chunk.loc.lmax = chunk.loc.lmin;
        qv_insert(dox_chunk, &dox->chunks, 0, chunk);
    }

    qv_for_each_ptr(dox_chunk, chunk, &dox->chunks) {
        /* this test is intented for first chunk of the current token */
        if (!chunk->keyword.len && chunks->len
        &&  chunk->loc.lmin - qv_last(dox_chunk, chunks)->loc.lmax < 2)
        {
            dox_chunk_merge(qv_last(dox_chunk, chunks), chunk);
        } else {
            qv_append(dox_chunk, chunks, *chunk);
        }
    }
    (DROP)(pp, 1, tk_offset);
    return 0;
}

static void read_dox_front(iopc_parser_t *pp, qv_t(dox_chunk) *chunks)
{
    int offset = 0;

    do {
        /* XXX: we ignore tags when reading doxygen front comments */
        if (CHECK(pp, offset, ITOK_INTEGER) && CHECK(pp, offset + 1, ':'))
            offset += 2;
    } while (read_dox(pp, offset, chunks, false, 0) >= 0);
}

static void
read_dox_back(iopc_parser_t *pp, qv_t(dox_chunk) *chunks, int ignore_token)
{
    while (read_dox(pp, 0, chunks, true, ignore_token) >= 0);
}

static
void iopc_dox_desc_append_paragraphs(lstr_t *desc, const qv_t(sb) *paragraphs)
{
    sb_t text;

    sb_inita(&text, 1024);
    sb_add_lstr(&text, *desc);
    qv_for_each_ptr(sb, paragraph, paragraphs) {
        if (text.len && paragraph->len) {
            sb_addc(&text, '\n');
        }
        sb_addsb(&text, paragraph);
    }
    lstr_wipe(desc);
    lstr_transfer_sb(desc, &text, false);
}

static void
iopc_dox_append_paragraphs_to_details(qv_t(iopc_dox) *comments,
                                      const qv_t(sb) *paragraphs)
{
    iopc_dox_t *dox;

    if (!paragraphs->len)
        return;

    dox = iopc_dox_find_type_or_create(comments, IOPC_DOX_TYPE_DETAILS);
    iopc_dox_desc_append_paragraphs(&dox->desc, paragraphs);
}

static void
iopc_dox_split_paragraphs(const qv_t(sb) *paragraphs,
                          qv_t(sb) *first, qv_t(sb) *others)
{
    if (first)
        qv_init_static(sb, first, paragraphs->tab, 1);

    if (others)
        qv_init_static(sb, others, paragraphs->tab + 1, paragraphs->len - 1);
}

static void
iopc_dox_append_paragraphs(qv_t(iopc_dox) *comments, lstr_t *desc,
                           const qv_t(sb) *paragraphs)
{
    qv_t(sb) first;
    qv_t(sb) others;

    if (!paragraphs->len)
        return;

    iopc_dox_split_paragraphs(paragraphs, &first, &others);

    iopc_dox_desc_append_paragraphs(desc, &first);
    iopc_dox_append_paragraphs_to_details(comments, &others);
}

/* XXX: the first paragraph of a chunk could be empty
 * and it is the sole paragraph of a chunk that can be empty
 * in case it is empty we must append the paragraphs to 'details'
 * but only if there are others paragraphs in order to avoid an empty
 * 'details'
 */
static int
iopc_dox_check_paragraphs(qv_t(iopc_dox) *comments,
                          const qv_t(sb) *paragraphs)
{
    if (!paragraphs->len)
        return -1;

    if (paragraphs->tab[0].len)
        return 0;

    if (paragraphs->len > 1) {
        iopc_dox_append_paragraphs_to_details(comments, paragraphs);
    }
    return -1;
}

static int
build_dox_param(const iopc_fun_t *owner, qv_t(iopc_dox) *res,
                dox_chunk_t *chunk)
{
    iopc_dox_arg_dir_t dir;

    if (!chunk->params.len) {
        warn_loc("doxygen param direction not specified", chunk->loc);
        return -1;
    }

    if (chunk->params.len > 1) {
        warn_loc("more than one doxygen param direction", chunk->loc);
        return -1;
    }

    if (iopc_dox_check_param_dir(chunk->params.tab[0], &dir) < 0)
    {
        warn_loc("unsupported doxygen param direction: `%*pM`", chunk->loc,
                 LSTR_FMT_ARG(chunk->params.tab[0]));
        return -1;
    }

#define TEST_ANONYMOUS(X, Y)  \
    (dir == IOPC_DOX_ARG_DIR_##X && !owner->Y##_is_anonymous)

    if (TEST_ANONYMOUS(IN, arg) || TEST_ANONYMOUS(OUT, res)
    ||  TEST_ANONYMOUS(THROW, exn))
    {
        qv_deep_wipe(lstr, &chunk->params_args, lstr_wipe);
        qv_append(lstr, &chunk->params_args, LSTR_EMPTY_V);
        chunk->paragraph0_args_len = 0;
    }
#undef TEST_ANONYMOUS

    dox_chunk_params_args_validate(chunk);

    if (iopc_dox_check_paragraphs(res, &chunk->paragraphs) < 0)
        return 0;

    qv_for_each_pos(lstr, i, &chunk->params_args) {
        lstr_t arg = chunk->params_args.tab[i];
        iopc_field_t *arg_field;
        qv_t(sb) arg_paragraphs;
        qv_t(sb) object_paragraphs;

        for (int j = i + 1; j < chunk->params_args.len; j++) {
            if (!lstr_equal2(arg, chunk->params_args.tab[j]))
                continue;
            fatal_loc("doxygen duplicated `%*pM` argument `%*pM`", chunk->loc,
                      LSTR_FMT_ARG(chunk->params.tab[0]), LSTR_FMT_ARG(arg));
        }

        arg_field = iopc_dox_arg_find_in_fun(arg, dir, owner);
        if (!arg_field) {
            fatal_loc("doxygen unrelated `%*pM` argument `%*pM` for RPC `%s`",
                      chunk->loc, LSTR_FMT_ARG(chunk->params.tab[0]),
                      LSTR_FMT_ARG(arg), owner->name);
        }

        iopc_dox_split_paragraphs(&chunk->paragraphs,
                                  &arg_paragraphs, &object_paragraphs);

        iopc_dox_append_paragraphs_to_details(&arg_field->comments,
                                              &arg_paragraphs);
        debug_dump_dox(arg_field->comments, arg_field->name);

        iopc_dox_append_paragraphs_to_details(res, &object_paragraphs);
    }

    return 0;
}

static qv_t(iopc_dox)
build_dox_(qv_t(dox_chunk) *chunks, const void *owner, int attr_type)
{
    qv_t(iopc_dox) res;

    qv_init(iopc_dox, &res);

    qv_for_each_ptr(dox_chunk, chunk, chunks) {
        iopc_dox_t *dox;
        int type = -1;

        if (chunk->keyword.len
        &&  iopc_dox_check_keyword(chunk->keyword, &type) >= 0
        &&  !iopc_dox_type_is_related(type, attr_type))
        {
            warn_loc("unrelated doxygen keyword: `%*pM`", chunk->loc,
                     LSTR_FMT_ARG(chunk->keyword));
            type = -1;
        }

        if (type == -1) {
            dox_chunk_params_merge(chunk);
            dox_chunk_keyword_merge(chunk);
        }

        if (iopc_dox_check_paragraphs(&res, &chunk->paragraphs) < 0)
            continue;

        if (type == IOPC_DOX_TYPE_PARAM) {
            if (build_dox_param(owner, &res, chunk) >= 0)
                continue;
            dox_chunk_params_merge(chunk);
            dox_chunk_keyword_merge(chunk);
            type = -1;
        }

        if (type >= 0) {
            dox = iopc_dox_find_type_or_create(&res, type);
            iopc_dox_append_paragraphs(&res, &dox->desc, &chunk->paragraphs);
        } else
        if (iopc_dox_find_type(&res, IOPC_DOX_TYPE_BRIEF)) {
            iopc_dox_append_paragraphs_to_details(&res, &chunk->paragraphs);
        } else {
            dox = iopc_dox_add(&res, IOPC_DOX_TYPE_BRIEF);
            dox_chunk_autobrief_validate(chunk);
            iopc_dox_append_paragraphs(&res, &dox->desc, &chunk->paragraphs);
        }
    }

    qv_deep_clear(dox_chunk, chunks, dox_chunk_wipe);
    return res;
}

#define build_dox(_chunks, _owner, _attr_type)                             \
    do {                                                                   \
        (_owner)->comments = build_dox_(_chunks, _owner, _attr_type);      \
        debug_dump_dox((_owner)->comments, (_owner)->name);                \
    } while (0)

#define build_dox_check_all(_chunks, _owner)  \
    do { build_dox(_chunks, _owner, -1); } while (0)

static iopc_attr_t *parse_attr(iopc_parser_t *pp);

static void iopc_add_attr(qv_t(iopc_attr) *attrs, iopc_attr_t **attrp)
{
    iopc_attr_t *attr = *attrp;
    int pos = check_attr_multi(attrs, attr);

    if (pos < 0 || attr->desc->args.len != 1) {
        qv_append(iopc_attr, attrs, attr);
    } else {
        qv_for_each_entry(iopc_arg, arg, &attr->args) {
            *qv_growlen(iopc_arg, &attrs->tab[pos]->args, 1) =
                iopc_arg_dup(&arg);
        }
        iopc_attr_delete(attrp);
    }
}

void iopc_field_add_attr(iopc_field_t *f, iopc_attr_t **attrp, bool tdef)
{
    check_attr_type_field(*attrp, f, tdef);
    iopc_add_attr(&f->attrs, attrp);
}

static void
check_dox_and_attrs(iopc_parser_t *pp, qv_t(dox_chunk) *chunks,
                    qv_t(iopc_attr) *attrs, int attr_type)
{
    qv_clear(iopc_attr, attrs);
    qv_deep_clear(dox_chunk, chunks, dox_chunk_wipe);
    for (;;) {
        if (CHECK(pp, 0, ITOK_ATTR)) {
            iopc_attr_t *attr = parse_attr(pp);

            if (attr_type >= 0) {
                check_attr_type_decl(attr, attr_type);
            }
            iopc_add_attr(attrs, &attr);
        } else
        if (read_dox(pp, 0, chunks, false, 0) < 0) {
            break;
        }
    }
    read_dox_front(pp, chunks);
}
#define check_dox_and_attrs(_pp, _chunks, _attrs)  \
    ((check_dox_and_attrs)((_pp), (_chunks), (_attrs), -1))

/*-}}}-*/
/*----- recursive descent parser -{{{-*/

static char *iopc_upper_ident(iopc_parser_t *pp)
{
    iopc_token_t *tk = TK(pp, 0);
    char *res;

    WANT(pp, 0, ITOK_IDENT);
    if (!isupper((unsigned char)ident(tk)[0])) {
        fatal_loc("first character must be uppercased (got %s)",
                  tk->loc, ident(tk));
    }
    res = dup_ident(tk);
    DROP(pp, 1);
    return res;
}

static char *iopc_aupper_ident(iopc_parser_t *pp)
{
    iopc_token_t *tk = TK(pp, 0);
    char *res;

    WANT(pp, 0, ITOK_IDENT);
    for (const char *s = ident(tk); *s; s++) {
        if (isdigit((unsigned char)*s) || *s == '_')
            continue;
        if (isupper((unsigned char)*s))
            continue;
        fatal_loc("this token should be all uppercase", tk->loc);
    }
    res = dup_ident(tk);
    DROP(pp, 1);
    return res;
}

static char *iopc_lower_ident(iopc_parser_t *pp)
{
    iopc_token_t *tk = TK(pp, 0);
    char *res;

    WANT(pp, 0, ITOK_IDENT);
    if (!islower((unsigned char)ident(tk)[0])) {
        fatal_loc("first character must be lowercased (got %s)",
                  tk->loc, ident(tk));
    }
    res = dup_ident(tk);
    DROP(pp, 1);
    return res;
}

static iopc_pkg_t *
check_path_exists(iopc_parser_t *pp, iopc_path_t *path)
{
    iopc_pkg_t *pkg;

    if (pp->base) {
        if ((pkg = iopc_try_file(pp, pp->base, path))) {
            return pkg;
        }
    }
    if (pp->includes) {
        qv_for_each_entry(cstr, include, pp->includes) {
            if ((pkg = iopc_try_file(pp, include, path))) {
                return pkg;
            }
        }
    }
    fatal_loc("unable to find file `%s` in the include path",
              path->loc, pretty_path(path));
}

static iopc_path_t *parse_path_aux(iopc_parser_t *pp, iopc_pkg_t **modp)
{
    iopc_path_t *path = iopc_path_new();
    iopc_token_t *tk0 = TK(pp, 0);

    path->loc = tk0->loc;
    qv_append(str, &path->bits, iopc_lower_ident(pp));

    while (CHECK(pp, 0, '.') && CHECK(pp, 1, ITOK_IDENT)) {
        iopc_token_t *tk1 = TK(pp, 1);

        if (!islower((unsigned char)ident(tk1)[0]))
            break;
        qv_append(str, &path->bits, dup_ident(tk1));
        iopc_loc_merge(&path->loc, tk1->loc);
        DROP(pp, 2);
    }

    if (modp)
        *modp = check_path_exists(pp, path);

    return path;
}

static iopc_path_t *parse_pkg_stmt(iopc_parser_t *pp)
{
    iopc_path_t *path;

    EAT_KW(pp, "package");
    path = parse_path_aux(pp, NULL);
    if (CHECK(pp, 0, '.'))
        WANT(pp, 1, ITOK_IDENT);
    EAT(pp, ';');
    return path;
}

static iopc_import_t *parse_import_stmt(iopc_parser_t *pp)
{
    iopc_import_t *import = iopc_import_new();
    iopc_token_t *tk;

    import->loc  = TK(pp, 0)->loc;
    EAT_KW(pp, "import");
    import->path = parse_path_aux(pp, &import->pkg);

    if (!CHECK(pp, 0, '.'))
        fatal_loc("no type is specified, missing `.*` ?", TK(pp, 0)->loc);
    DROP(pp, 1);

    if (CHECK(pp, 0, ITOK_IDENT)) {
        import->type = iopc_upper_ident(pp);
    } else
    if (!CHECK(pp, 0, '*')) {
        t_scope;
        tk = TK(pp, 0);
        fatal_loc("%s or %s expected, but got %s instead",
                  tk->loc, t_pretty_token('*'), t_pretty_token(ITOK_IDENT),
                  t_pretty_token(tk->token));
    } else {
        DROP(pp, 1);
    }
    WANT(pp, 0, ';');
    iopc_loc_merge(&import->loc, TK(pp, 0)->loc);
    DROP(pp, 1);

    return import;
}

static iop_type_t get_type_kind(iopc_token_t *tk)
{
    const char *id = ident(tk);
    int v = iopc_get_token(id, tk->b.len);

    if (v == IOPC_TK_unknown)
        return IOP_T_STRUCT;
    for (const char *p = id; *p; p++) {
        if (isupper((unsigned char)*p))
            return IOP_T_STRUCT;
    }
    switch (v) {
      case IOPC_TK_BYTE:   return IOP_T_I8;
      case IOPC_TK_UBYTE:  return IOP_T_U8;
      case IOPC_TK_SHORT:  return IOP_T_I16;
      case IOPC_TK_USHORT: return IOP_T_U16;
      case IOPC_TK_INT:    return IOP_T_I32;
      case IOPC_TK_UINT:   return IOP_T_U32;
      case IOPC_TK_LONG:   return IOP_T_I64;
      case IOPC_TK_ULONG:  return IOP_T_U64;
      case IOPC_TK_BOOL:   return IOP_T_BOOL;

      case IOPC_TK_BYTES:  return IOP_T_DATA;
      case IOPC_TK_DOUBLE: return IOP_T_DOUBLE;
      case IOPC_TK_STRING: return IOP_T_STRING;
      case IOPC_TK_XML:    return IOP_T_XML;
      default:
        return IOP_T_STRUCT;
    }
}

static void parse_struct_type(iopc_parser_t *pp, iopc_pkg_t **type_pkg,
                              iopc_path_t **path, char **name)
{
    iopc_token_t *tk = TK(pp, 0);

    WANT(pp, 0, ITOK_IDENT);
    if (islower((unsigned char)ident(tk)[0])) {
        *path = parse_path_aux(pp, type_pkg);
        EAT(pp, '.');
        WANT(pp, 0, ITOK_IDENT);
        tk = TK(pp, 0);
    }
    *name = iopc_upper_ident(pp);
}

static void check_snmp_obj_field_type(iopc_struct_t *st, iop_type_t kind)
{
    switch(kind) {
      case IOP_T_STRUCT:
      case IOP_T_STRING:
      case IOP_T_I8:
      case IOP_T_I16:
      case IOP_T_I32:
      case IOP_T_BOOL:
        return;
      case IOP_T_I64:
      case IOP_T_U8:
      case IOP_T_U16:
      case IOP_T_U32:
      case IOP_T_U64:
      default:
        fatal_loc("only int/string/boolean/enum types are handled for "
                  "snmp objects' fields", st->loc);
    }
}

static void parse_field_type(iopc_parser_t *pp, iopc_struct_t *st,
                             iopc_field_t *f)
{
    WANT(pp, 0, ITOK_IDENT);
    f->kind = get_type_kind(TK(pp, 0));

    /* in case of snmpObj structure, some field type are not handled */
    if (st && iopc_is_snmp_st(st->type)) {
        check_snmp_obj_field_type(st, f->kind);
    }
    if (f->kind == IOP_T_STRUCT) {
        parse_struct_type(pp, &f->type_pkg, &f->type_path, &f->type_name);
    } else {
        DROP(pp, 1);
    }

    switch (TK(pp, 0)->token) {
      case '?':
        if (f->is_static) {
            fatal_loc("optional static members are forbidden", f->loc);
        }
        f->repeat = IOP_R_OPTIONAL;
        DROP(pp, 1);
        break;

      case '&':
        if (f->is_static) {
            fatal_loc("referenced static members are forbidden", f->loc);
        }
        if (f->kind != IOP_T_STRUCT) {
            fatal_loc("references can only be applied to structures or unions",
                      f->loc);
        }
        f->repeat = IOP_R_REQUIRED;
        f->is_ref = true;
        DROP(pp, 1);
        break;

      case '[':
        if (f->is_static) {
            fatal_loc("repeated static members are forbidden", f->loc);
        }
        WANT(pp, 1, ']');
        f->repeat = IOP_R_REPEATED;
        DROP(pp, 2);
        break;

      default:
        f->repeat = IOP_R_REQUIRED;
        break;
    }
}

static void parse_field_defval(iopc_parser_t *pp, iopc_field_t *f, int paren)
{
    iopc_token_t *tk;

    EAT(pp, '=');
    tk = TK(pp, 0);

    if (f->repeat != IOP_R_REQUIRED) {
        fatal_loc("default values for non required fields makes no sense",
                  tk->loc);
    }
    f->repeat = IOP_R_DEFVAL;

    if (tk->b_is_char) {
        WANT(pp, 0, ITOK_STRING);
        f->defval.u64 = (uint64_t)tk->b.data[0];
        f->defval_type = IOPC_DEFVAL_INTEGER;
        DROP(pp, 1);
    } else
    if (CHECK(pp, 0, ITOK_STRING)) {
        f->defval.ptr = p_strdup(tk->b.data);
        f->defval_type = IOPC_DEFVAL_STRING;
        DROP(pp, 1);
    } else
    if (CHECK(pp, 0, ITOK_DOUBLE)) {
        f->defval.d = tk->d;
        f->defval_type = IOPC_DEFVAL_DOUBLE;
        DROP(pp, 1);
    } else {
        bool is_signed;

        f->defval.u64 = parse_constant_integer(pp, paren, &is_signed);
        f->defval_is_signed = is_signed;
        f->defval_type = IOPC_DEFVAL_INTEGER;
    }
}

static iopc_field_t *
parse_field_stmt(iopc_parser_t *pp, iopc_struct_t *st, qv_t(iopc_attr) *attrs,
                 qm_t(field) *fields, qv_t(i32) *tags, int *next_tag,
                 int paren, bool is_snmp_iface)
{
    iopc_loc_t    name_loc;
    iopc_field_t *f = NULL;
    iopc_token_t *tk;
    int           tag;

    f = iopc_field_new();
    f->loc = TK(pp, 0)->loc;

    f->snmp_is_in_tbl = iopc_is_snmp_tbl(st->type);

    if (SKIP_KW(pp, "static")) {
        if (!iopc_is_class(st->type)) {
            fatal_loc("static keyword is only authorized for class fields",
                      f->loc);
        }
        f->is_static = true;
    } else {
        /* Tag */
        if (CHECK(pp, 0, ITOK_INTEGER)) {
            WANT(pp, 1, ':');
            tk = TK(pp, 0);
            f->tag = tk->i;
            *next_tag = f->tag + 1;
            DROP(pp, 2);
            if (CHECK_KW(pp, 0, "static")) {
                fatal_loc("tag is not authorized for static class fields",
                          TK(pp, 0)->loc);
            }
        } else {
            f->tag = (*next_tag)++;
        }
        if (f->tag < 1) {
            fatal_loc("tag is too small (must be >= 1, got %d)",
                      TK(pp, 0)->loc, f->tag);
        }
        if (f->tag >= 0x8000) {
            fatal_loc("tag is too large (must be < 0x8000, got 0x%x)",
                      TK(pp, 0)->loc, f->tag);
        }
    }

    /* If the field is contained by a snmpIface rpc struct, it will have no
     * type (so no need to parse the type), and the flag snmp_is_from_param
     * needs to be set at true */
    if (is_snmp_iface) {
        f->snmp_is_from_param = true;
    } else {
        parse_field_type(pp, st, f);
    }

    WANT(pp, 0, ITOK_IDENT);
    f->name = dup_ident(TK(pp, 0));
    if (strchr(f->name, '_')) {
        fatal_loc("identifier '%s' contains a _", TK(pp, 0)->loc, f->name);
    }
    if (!islower(f->name[0])) {
        fatal_loc("first character must be lowercased (got %s)",
                 TK(pp, 0)->loc, f->name);
    }

    name_loc = TK(pp, 0)->loc;
    DROP(pp, 1);

    if (CHECK(pp, 0, '=')) {
        if (st->type == STRUCT_TYPE_UNION)
            fatal_loc("default values are forbidden in union types", f->loc);
        parse_field_defval(pp, f, paren);
        assert (f->defval_type);
    } else
    if (f->is_static && !st->is_abstract) {
        fatal_loc("static fields of non-abstract classes must have a default "
                  "value", f->loc);
    }

    /* XXX At this point, the default value (if there is one) has been read,
     * so the type of field is correct. If you depend on this type (for
     * example for check_attr_type_field()), your code should be below this
     * line. */

    qv_for_each_entry(iopc_attr, attr, attrs) {
        check_attr_type_field(attr, f, false);
        qv_append(iopc_attr, &f->attrs, attr);
    }

    /* Looks for blacklisted keyword (after attribute has been parsed) */
    check_name(f->name, name_loc, &f->attrs);

    iopc_loc_merge(&f->loc, TK(pp, 0)->loc);

    tag = f->tag;
    if (qm_add(field, fields, f->name, f)) {
        fatal_loc("field name `%s` is already in use", f->loc, f->name);
    }

    if (f->is_static) {
        qv_append(iopc_field, &st->static_fields, f);
        return f;
    }
    qv_append(iopc_field, &st->fields, f);

    qv_for_each_entry(i32, t, tags) {
        if (t == tag)
            fatal_loc("tag %d is used twice", f->loc, tag);
    }
    qv_append(i32, tags, tag);
    return f;
}

static void check_snmp_brief(qv_t(iopc_dox) comments, iopc_loc_t loc,
                             char *name, const char *type)
{
    qv_for_each_pos(iopc_dox, pos, &comments) {
        if (comments.tab[pos].type == IOPC_DOX_TYPE_BRIEF) {
            return;
        }
    }
    fatal_loc("%s `%s` needs a brief that would be used as a "
              "description in the generated MIB", loc, type, name);
}

static void parse_struct(iopc_parser_t *pp, iopc_struct_t *st, int sep,
                         int paren, bool is_snmp_iface)
{
    qm_t(field) fields = QM_INIT_CACHED(field, fields);
    int next_tag = 1;
    int next_pos = 1;
    bool previous_static = true;
    qv_t(i32) tags;
    qv_t(iopc_attr) attrs;
    qv_t(dox_chunk) chunks;

    qv_inita(i32, &tags, 1024);
    qv_inita(iopc_attr, &attrs, 16);
    qv_init(dox_chunk, &chunks);

    while (!CHECK_NOEOF(pp, 0, paren)) {
        iopc_field_t *f;

        check_dox_and_attrs(pp, &chunks, &attrs);
        f = parse_field_stmt(pp, st, &attrs, &fields, &tags, &next_tag,
                             paren, is_snmp_iface);
        if (f) {
            if (!previous_static && f->is_static) {
                if (iopc_g.v4) {
                    fatal_loc("all static attributes must be declared first",
                              TK(pp, 0)->loc);
                } else {
                    warn_loc("all static attributes must be declared first",
                             TK(pp, 0)->loc);
                }
            }
            previous_static = f->is_static;

            f->pos = next_pos++;
            read_dox_back(pp, &chunks, sep);
            build_dox_check_all(&chunks, f);

            if (iopc_is_snmp_st(st->type)) {
                check_snmp_brief(f->comments, f->loc, f->name, "field");
            }

        }
        if (CHECK(pp, 0, paren))
            break;
        EAT(pp, sep);
    }
    qm_wipe(field, &fields);
    qv_wipe(i32, &tags);
    qv_wipe(iopc_attr, &attrs);
    qv_deep_wipe(dox_chunk, &chunks, dox_chunk_wipe);
    iopc_loc_merge(&st->loc, TK(pp, 1)->loc);
}

static void
check_class_or_snmp_obj_id_range(iopc_parser_t *pp, int struct_id,
                                 int min, int max)
{
    if (struct_id < min) {
        fatal_loc("id is too small (must be >= %d, got %d)",
                  TK(pp, 0)->loc, min, struct_id);
    }
    if (struct_id > max) {
        fatal_loc("id is too large (must be <= %d, got %d)",
                  TK(pp, 0)->loc, max, struct_id);
    }
}

static void parse_check_class_snmp(iopc_parser_t *pp, bool is_class)
{
    if (!iopc_g.v2 && is_class) {
        fatal_loc("inheritance forbidden (use -2 parameter)",
                  TK(pp, 0)->loc);
    } else
    if (!iopc_g.v4 && !is_class) {
            fatal_loc("use of snmp forbidden (use -4 parameter)",
                      TK(pp, 0)->loc);
    }
}

static void parse_handle_class_snmp(iopc_parser_t *pp, iopc_struct_t *st,
                                    bool is_main_pkg)
{
    iopc_token_t *tk;
    bool is_class = iopc_is_class(st->type);

    assert (is_class || iopc_is_snmp_st(st->type));

    parse_check_class_snmp(pp, is_class);

    /* Parse struct id; This is optional for a struct without parent, and
     * in this case default is 0. */
    if (SKIP(pp, ':')) {
        int id, pkg_min, pkg_max, global_min;

        WANT(pp, 0, ITOK_INTEGER);
        tk = TK(pp, 0);
        st->class_id = tk->i; /* so st->snmp_obj_id is also set to tk->i */

        if (is_class) {
            id = st->class_id;
            pkg_min = iopc_g.class_id_min;
            pkg_max = iopc_g.class_id_max;
            global_min = 0;
        } else {
            id = st->oid;
            pkg_min = SNMP_OBJ_OID_MIN;
            pkg_max = SNMP_OBJ_OID_MAX;
            global_min = 1;
        }

        if (is_main_pkg) {
            check_class_or_snmp_obj_id_range(pp, id, pkg_min, pkg_max);
        } else {
            check_class_or_snmp_obj_id_range(pp, id, global_min, 0xFFFF);
        }

        DROP(pp, 1);

        /* Parse parent */
        if (SKIP(pp, ':')) {
            iopc_extends_t *xt = iopc_extends_new();

            xt->loc = TK(pp, 0)->loc;
            parse_struct_type(pp, &xt->pkg, &xt->path, &xt->name);
            iopc_loc_merge(&xt->loc, TK(pp, 0)->loc);

            /* Check if snmpObj parent is Intersec */
            xt->is_snmp_root = strequal(xt->name, "Intersec");

            qv_append(iopc_extends, &st->extends, xt);

            if (SKIP(pp, ',')) {
                fatal_loc("multiple inheritance is not supported",
                          TK(pp, 0)->loc);
            }
        } else
        if (iopc_is_snmp_st(st->type)) {
            fatal_loc("%s `%s` needs a snmpObj parent", TK(pp, 0)->loc,
                      iopc_struct_type_to_str(st->type), st->name);
        }
    } else
    if (iopc_is_snmp_st(st->type)) {
        fatal_loc("%s `%s` needs a snmpObj parent", TK(pp, 0)->loc,
                  iopc_struct_type_to_str(st->type), st->name);
    }
}

static iopc_struct_t *
parse_struct_class_union_snmp_stmt(iopc_parser_t *pp,
                                   iopc_struct_type_t type,
                                   bool is_abstract, bool is_local,
                                   bool is_main_pkg)
{
    iopc_struct_t *st = iopc_struct_new();

    st->is_visible = true;
    st->type = type;
    st->name = iopc_upper_ident(pp);
    st->loc = TK(pp, 0)->loc;
    st->is_abstract = is_abstract;
    st->is_local = is_local;

    if (iopc_is_class(st->type) || iopc_is_snmp_st(st->type)) {
        parse_handle_class_snmp(pp, st, is_main_pkg);
    }

    if (!iopc_is_class(st->type)) {
        if (is_abstract) {
            fatal_loc("only classes can be abstract", TK(pp, 0)->loc);
        }
        if (is_local) {
            fatal_loc("only classes can be local", TK(pp, 0)->loc);
        }
    }

    EAT(pp, '{');
    parse_struct(pp, st, ';', '}', false);
    EAT(pp, '}');
    EAT(pp, ';');
    return st;
}

static
iopc_enum_t *parse_enum_stmt(iopc_parser_t *pp, const qv_t(iopc_attr) *attrs)
{
    t_scope;
    iopc_enum_t *en = iopc_enum_new();
    int next_value  = 0;
    qv_t(i32) values;
    lstr_t ns;
    lstr_t prefix = LSTR_NULL;
    qv_t(dox_chunk) chunks;

    qv_init(dox_chunk, &chunks);

    en->is_visible = true;
    en->loc = TK(pp, 0)->loc;

    EAT_KW(pp, "enum");
    en->name = iopc_upper_ident(pp);
    EAT(pp, '{');

    t_iopc_attr_check_prefix(attrs, &prefix);
    lstr_ascii_toupper(&prefix);

    ns = t_camelcase_to_c(LSTR(en->name));
    lstr_ascii_toupper(&ns);

    if (lstr_equal2(ns, prefix))
        prefix = LSTR_NULL_V;

    t_qv_init(i32, &values, 1024);

    while (!CHECK_NOEOF(pp, 0, '}')) {
        iopc_enum_field_t *f = iopc_enum_field_new();
        char *ename;

        check_dox_and_attrs(pp, &chunks, &f->attrs);

        f->name  = iopc_aupper_ident(pp);
        f->loc   = TK(pp, 0)->loc;

        ename = asprintf("%*pM_%s", LSTR_FMT_ARG(ns), f->name);

        if (SKIP(pp, '=')) {
            next_value = parse_constant_integer(pp, '}', NULL);
        }

        qv_for_each_entry(iopc_attr, attr, &f->attrs) {
            if (attr->desc->id != IOPC_ATTR_GENERIC) {
                fatal_loc("invalid attribute %s on enum field", f->loc,
                          attr->desc->name.s);
            }
        }

        /* handle properly prefixed enums */
        if (prefix.s) {
            uint32_t qpos = qm_put(enums, &_G.enums_forbidden, ename, f, 0);

            if (qpos & QHASH_COLLISION) {
                p_delete(&ename);
            }
            ename = asprintf("%*pM_%s", LSTR_FMT_ARG(prefix), f->name);
        }

        /* Checks for name uniqueness */
        if (qm_add(enums, &_G.enums, ename, f)) {
            fatal_loc("enum field name `%s` is used twice", f->loc, f->name);
        }

        f->value = next_value++;
        qv_append(iopc_enum_field, &en->values, f);
        qv_for_each_entry(i32, v, &values) {
            if (v == f->value)
                fatal_loc("value %d is used twice", f->loc, f->value);
        }
        qv_append(i32, &values, f->value);

        read_dox_back(pp, &chunks, ',');
        build_dox_check_all(&chunks, f);

        if (SKIP(pp, ','))
            continue;

        fatal_loc("`,` expected on every line", f->loc);
    }

    qv_deep_wipe(dox_chunk, &chunks, dox_chunk_wipe);
    iopc_loc_merge(&en->loc, TK(pp, 1)->loc);

    WANT(pp, 1, ';');
    DROP(pp, 2);
    return en;
}

static iopc_field_t *
parse_typedef_stmt(iopc_parser_t *pp)
{
    iopc_field_t *f;

    EAT_KW(pp, "typedef");

    f = iopc_field_new();
    f->loc = TK(pp, 0)->loc;
    f->is_visible = true;
    parse_field_type(pp, NULL, f);

    f->name = iopc_upper_ident(pp);
    if (strchr(f->name, '_')) {
        fatal_loc("identifer '%s' contains a _", TK(pp, 0)->loc, f->name);
    }
    EAT(pp, ';');

    return f;
}

enum {
    IOP_F_ARGS = 0,
    IOP_F_RES  = 1,
    IOP_F_EXN  = 2,
};

static bool parse_function_desc(iopc_parser_t *pp, int what, iopc_fun_t *fun,
                                qv_t(dox_chunk) *chunks,
                                iopc_iface_type_t iface_type)
{
    static char const * const type_names[] = { "Args", "Res", "Exn", };
    static char const * const tokens[]     = { "in",   "out", "throw", };
    const char *type_name = type_names[what];
    const char *token = tokens[what];
    iopc_struct_t **sptr;
    iopc_field_t  **fptr;
    bool is_snmp_iface = iopc_is_snmp_iface(iface_type);

    read_dox_front(pp, chunks);

    if (!CHECK_KW(pp, 0, token)) {
        return false;
    }

    if (fun->fun_is_async && (what == IOP_F_EXN))
        fatal_loc("async functions cannot throw", TK(pp, 0)->loc);
    if (is_snmp_iface && (what == IOP_F_EXN || what == IOP_F_RES)) {
        fatal_loc("snmpIface cannot out and/or throw", TK(pp, 0)->loc);
    }

    DROP(pp, 1);
    if (SKIP(pp, '(')) {
        switch (what) {
          case IOP_F_ARGS:
            sptr = &fun->arg;
            fun->arg_is_anonymous = true;
            break;
          case IOP_F_RES:
            sptr = &fun->res;
            fun->res_is_anonymous = true;
            break;
          case IOP_F_EXN:
            sptr = &fun->exn;
            fun->exn_is_anonymous = true;
            break;
          default:
            abort();
        }

        *sptr = iopc_struct_new();
        (*sptr)->name = asprintf("%s%s", fun->name, type_name);
        (*sptr)->loc = TK(pp, 0)->loc;
        parse_struct(pp, *sptr, ',', ')', is_snmp_iface);
        EAT(pp, ')');
        read_dox_back(pp, chunks, 0);
        build_dox_check_all(chunks, *sptr);
    } else                          /* fname in void ... */
    if (CHECK_KW(pp, 0, "void")) {
        if (is_snmp_iface) {
            fatal_loc("void is not supported by snmpIface RPCs",
                      TK(pp, 0)->loc);
        }
        DROP(pp, 1);
    } else
    if (is_snmp_iface && SKIP_KW(pp, "null")) {
        fatal_loc("null is not supported by snmpIface RPCs",
                  TK(pp, 0)->loc);
    } else
    if ((what == IOP_F_RES) && SKIP_KW(pp, "null")) {
        fun->fun_is_async = true;
    } else
    if (is_snmp_iface) {
        fatal_loc("snmpIface RPC argument must be anonymous. example "
                  "`in (a, b, c);`", TK(pp, 0)->loc);
    } else {                        /* fname in Type ... */
        iop_type_t type = get_type_kind(TK(pp, 0));
        iopc_field_t *f;

        if (type != IOP_T_STRUCT)
            fatal_loc("a structure (or a union) type was expected here"
                      " (got %s)", TK(pp, 0)->loc, ident(TK(pp, 0)));
        /* We use a field to store the type of the function argument. */
        switch (what) {
          case IOP_F_ARGS:
            fptr = &fun->farg;
            fun->arg_is_anonymous = false;
            break;
          case IOP_F_RES:
            fptr = &fun->fres;
            fun->res_is_anonymous = false;
            break;
          case IOP_F_EXN:
            fptr = &fun->fexn;
            fun->exn_is_anonymous = false;
            break;
          default:
            abort();
        }

        f = iopc_field_new();
        f->name = asprintf("%s%s", fun->name, type_name);
        f->loc = TK(pp, 0)->loc;
        f->kind = IOP_T_STRUCT;

        parse_struct_type(pp, &f->type_pkg, &f->type_path, &f->type_name);

        read_dox_back(pp, chunks, 0);
        build_dox_check_all(chunks, f);

        iopc_loc_merge(&f->loc, TK(pp, 0)->loc);
        *fptr = f;
    }
    qv_deep_clear(dox_chunk, chunks, dox_chunk_wipe);
    return true;
}

static iopc_fun_t *
parse_function_stmt(iopc_parser_t *pp, qv_t(iopc_attr) *attrs,
                    qv_t(i32) *tags, int *next_tag,
                    iopc_iface_type_t type)
{
    iopc_fun_t *fun = NULL;
    iopc_token_t *tk;
    int tag;
    qv_t(dox_chunk) fun_chunks, arg_chunks;

    qv_init(dox_chunk, &fun_chunks);
    (check_dox_and_attrs)(pp, &fun_chunks, attrs, IOPC_ATTR_T_RPC);

    fun = iopc_fun_new();
    fun->loc = TK(pp, 0)->loc;
    if (CHECK(pp, 0, ITOK_INTEGER)) {
        WANT(pp, 1, ':');
        tk = TK(pp, 0);
        fun->tag = tk->i;
        *next_tag = fun->tag + 1;
        DROP(pp, 2);
    } else {
        fun->tag = (*next_tag)++;
    }
    if (fun->tag < 1) {
        fatal_loc("tag is too small (must be >= 1, got %d)",
                  TK(pp, 0)->loc, fun->tag);
    }
    if (fun->tag >= 0x8000) {
        fatal_loc("tag is too large (must be < 0x8000, got 0x%x)",
                  TK(pp, 0)->loc, fun->tag);
    }

    qv_splice(iopc_attr, &fun->attrs, fun->attrs.len, 0,
              attrs->tab, attrs->len);

    fun->name = iopc_lower_ident(pp);
    check_name(fun->name, TK(pp, 0)->loc, &fun->attrs);
    read_dox_back(pp, &fun_chunks, 0);

    qv_init(dox_chunk, &arg_chunks);

    /* Parse function desc */
    parse_function_desc(pp, IOP_F_ARGS, fun, &arg_chunks, type);

    /* XXX we use & to execute both function calls */
    if ((!parse_function_desc(pp, IOP_F_RES,  fun, &arg_chunks, type))
     &  (!parse_function_desc(pp, IOP_F_EXN,  fun, &arg_chunks, type)))
    {
        if (!iopc_is_snmp_iface(type)) {
            info_loc("function %s may be a candidate for async-ness",
                     fun->loc, fun->name);
        }
    }

    qv_deep_wipe(dox_chunk, &arg_chunks, dox_chunk_wipe);

    EAT(pp, ';');

    tag = fun->tag;
    for (int i = 0; i < tags->len; i++) {
        if (tags->tab[i] == tag)
            fatal_loc("tag %d is used twice", fun->loc, tag);
    }
    qv_append(i32, tags, tag);

    build_dox(&fun_chunks, fun, IOPC_ATTR_T_RPC);
    if (iopc_is_snmp_iface(type)) {
        check_snmp_brief(fun->comments, fun->loc, fun->name, "notification");
    }

    qv_deep_wipe(dox_chunk, &fun_chunks, dox_chunk_wipe);
    return fun;
}

static void parse_snmp_iface_parent(iopc_parser_t *pp, iopc_iface_t *iface,
                                    bool is_main_pkg)
{
   iopc_token_t *tk;

    /* Check OID */
    if (SKIP(pp, ':')) {
        WANT(pp, 0, ITOK_INTEGER);
        tk = TK (pp, 0);

        iface->oid = tk->i;

        if (is_main_pkg) {
            check_class_or_snmp_obj_id_range(pp, iface->oid,
                                             SNMP_IFACE_OID_MIN,
                                             SNMP_IFACE_OID_MAX);
        } else {
            check_class_or_snmp_obj_id_range(pp, iface->oid,
                                             0, 0xFFFF);
        }
        DROP(pp, 1);
    }

    /* Parse parent */
    if (SKIP(pp, ':')) {
        iopc_extends_t *xt = iopc_extends_new();

        xt->loc = TK(pp, 0)->loc;
        parse_struct_type(pp, &xt->pkg, &xt->path, &xt->name);
        iopc_loc_merge(&xt->loc, TK(pp, 0)->loc);

        qv_append(iopc_extends, &iface->extends, xt);

        if (SKIP(pp, ',')) {
            fatal_loc("multiple inheritance is not supported",
                      TK(pp, 0)->loc);
        }
    } else {
        fatal_loc("snmpIface `%s` needs a snmpObj parent", TK(pp, 0)->loc,
                  iface->name);
    }
}

static iopc_iface_t *parse_iface_stmt(iopc_parser_t *pp,
                                      iopc_iface_type_t type,
                                      const char *name, bool is_main_pkg)
{
    qm_t(fun) funs = QM_INIT_CACHED(fun, funs);
    qv_t(i32) tags;
    qv_t(iopc_attr) attrs;
    int next_tag = 1;
    iopc_iface_t *iface = iopc_iface_new();

    iface->loc = TK(pp, 0)->loc;
    iface->type = type;

    EAT_KW(pp, name);
    iface->name = iopc_upper_ident(pp);

    if (iopc_is_snmp_iface(type)) {
        parse_snmp_iface_parent(pp, iface, is_main_pkg);
    }

    EAT(pp, '{');

    qv_inita(i32, &tags, 1024);
    qv_inita(iopc_attr, &attrs, 16);

    while (!CHECK_NOEOF(pp, 0, '}')) {
        iopc_fun_t *fun;

        fun = parse_function_stmt(pp, &attrs, &tags, &next_tag, iface->type);
        if (!fun) {
            continue;
        }
        qv_append(iopc_fun, &iface->funs, fun);
        if (qm_add(fun, &funs, fun->name, fun)) {
            fatal_loc("a function `%s` already exists", fun->loc, fun->name);
        }
    }
    qm_wipe(fun, &funs);
    qv_wipe(i32, &tags);
    qv_wipe(iopc_attr, &attrs);

    iopc_loc_merge(&iface->loc, TK(pp, 1)->loc);
    WANT(pp, 1, ';');
    DROP(pp, 2);
    return iface;
}

static iopc_field_t *
parse_mod_field_stmt(iopc_parser_t *pp, iopc_struct_t *mod,
                     qm_t(field) *fields, qv_t(i32) *tags, int *next_tag)
{
    iopc_field_t *f = NULL;
    iopc_token_t *tk;
    int tag;

    f = iopc_field_new();
    f->loc = TK(pp, 0)->loc;
    if (CHECK(pp, 0, ITOK_INTEGER)) {
        WANT(pp, 1, ':');
        tk = TK(pp, 0);
        f->tag = tk->i;
        *next_tag = f->tag + 1;
        DROP(pp, 2);
    } else {
        f->tag = (*next_tag)++;
    }
    if (f->tag < 1) {
        fatal_loc("tag is too small (must be >= 1, got %d)",
                  TK(pp, 0)->loc, f->tag);
    }
    if (f->tag >= 0x8000) {
        fatal_loc("tag is too large (must be < 0x8000, got 0x%x)",
                  TK(pp, 0)->loc, f->tag);
    }

    parse_struct_type(pp, &f->type_pkg, &f->type_path, &f->type_name);
    f->name = iopc_lower_ident(pp);
    if (strchr(f->name, '_')) {
        fatal_loc("identifier '%s' contains a _", TK(pp, 0)->loc, f->name);
    }

    iopc_loc_merge(&f->loc, TK(pp, 0)->loc);
    qv_append(iopc_field, &mod->fields, f);

    tag = f->tag;
    if (qm_add(field, fields, f->name, f)) {
        fatal_loc("field name `%s` is already in use", f->loc, f->name);
    }
    for (int i = 0; i < tags->len; i++) {
        if (tags->tab[i] == tag)
            fatal_loc("tag %d is used twice", f->loc, tag);
    }
    qv_append(i32, tags, tag);
    return f;
}

static iopc_struct_t *parse_module_stmt(iopc_parser_t *pp)
{
    int next_tag = 1;
    qm_t(field) fields = QM_INIT_CACHED(field, fields);
    qv_t(i32) tags;
    iopc_struct_t *mod = iopc_struct_new();
    qv_t(dox_chunk) chunks;

    qv_init(dox_chunk, &chunks);

    mod->loc = TK(pp, 0)->loc;

    EAT_KW(pp, "module");
    mod->name = iopc_upper_ident(pp);

    if (SKIP(pp, ':')) {
        do {
            iopc_extends_t *xt = iopc_extends_new();

            xt->loc  = TK(pp, 0)->loc;
            parse_struct_type(pp, &xt->pkg, &xt->path, &xt->name);
            iopc_loc_merge(&xt->loc, TK(pp, 0)->loc);

            qv_append(iopc_extends, &mod->extends, xt);
        } while (SKIP(pp, ','));
        if (CHECK(pp, 0, ';'))
            goto empty_body;
    }
    EAT(pp, '{');

    qv_inita(i32, &tags, 1024);
    while (!CHECK_NOEOF(pp, 0, '}')) {
        iopc_field_t *f;

        read_dox_front(pp, &chunks);
        f = parse_mod_field_stmt(pp, mod, &fields, &tags, &next_tag);
        if (f) {
            read_dox_back(pp, &chunks, ';');
            build_dox_check_all(&chunks, f);
        }
        EAT(pp, ';');
    }
    qm_wipe(field, &fields);
    qv_wipe(i32, &tags);
    qv_deep_wipe(dox_chunk, &chunks, dox_chunk_wipe);
    DROP(pp, 1);

  empty_body:
    iopc_loc_merge(&mod->loc, TK(pp, 0)->loc);
    EAT(pp, ';');
    return mod;
}

static void parse_json_object(iopc_parser_t *pp, sb_t *sb, bool toplevel);
static void parse_json_array(iopc_parser_t *pp, sb_t *sb);

static void parse_json_value(iopc_parser_t *pp, sb_t *sb)
{
    SB_1k(tmp);
    iopc_token_t *tk = TK(pp, 0);

    switch (tk->token) {
      case ITOK_STRING:
        sb_add_slashes(&tmp, tk->b.data, tk->b.len,
                       "\a\b\e\t\n\v\f\r\"", "abetnvfr\"");
        sb_addf(sb, "\"%*pM\"",  SB_FMT_ARG(&tmp));
        break;

      case ITOK_INTEGER:
        if (tk->i_is_signed) {
            sb_addf(sb, "%jd", tk->i);
        } else {
            sb_addf(sb, "%ju", tk->i);
        }
        break;

      case ITOK_DOUBLE:
        sb_addf(sb, DOUBLE_FMT, tk->d);
        break;

      case ITOK_LBRACE:
        parse_json_object(pp, sb, false);
        return;

      case ITOK_LBRACKET:
        parse_json_array(pp, sb);
        return;

      case ITOK_BOOL:
        sb_addf(sb, "%s", tk->i ? "true" : "false");
        break;

      case ITOK_IDENT:
        if (CHECK_KW(pp, 0, "null")) {
            sb_adds(sb, "null");
        } else {
            fatal_loc("invalid identifier when parsing json value", tk->loc);
        }
        break;

      default:
        fatal_loc("invalid token when parsing json value", tk->loc);
        break;
    }

    DROP(pp, 1);
}

static void parse_json_array(iopc_parser_t *pp, sb_t *sb)
{
    EAT(pp, '[');
    sb_addc(sb, '[');

    if (CHECK_NOEOF(pp, 0, ']')) {
        goto end;
    }
    for (;;) {
        parse_json_value(pp, sb);

        if (!CHECK(pp, 0, ',')) {
            break;
        }
        DROP(pp, 1);
        if (CHECK_NOEOF(pp, 0, ']')) {
            break;
        }
        sb_addc(sb, ',');
    }
  end:
    EAT(pp, ']');
    sb_addc(sb, ']');
}

static void parse_json_object(iopc_parser_t *pp, sb_t *sb, bool toplevel)
{
    char end = toplevel ? ')' : '}';

    if (!toplevel) {
        EAT(pp, '{');
        sb_addc(sb, '{');
    }
    if (CHECK_NOEOF(pp, 0, end)) {
        goto end;
    }
    for (;;) {
        iopc_token_t *tk = TK(pp, 0);

        if (!CHECK(pp, 0, ITOK_IDENT)) {
            WANT(pp, 0, ITOK_STRING);
        }
        sb_addf(sb, "\"%*pM\"",  SB_FMT_ARG(&tk->b));
        DROP(pp, 1);

        if (CHECK(pp, 0, '=')) {
            DROP(pp, 1);
        } else {
            EAT(pp, ':');
        }
        sb_addc(sb, ':');

        parse_json_value(pp, sb);

        if (!CHECK(pp, 0, ',')) {
            break;
        }
        DROP(pp, 1);
        if (CHECK_NOEOF(pp, 0, end)) {
            break;
        }
        sb_addc(sb, ',');
    }
  end:
    if (!toplevel) {
        EAT(pp, '}');
        sb_addc(sb, '}');
    }
}

static lstr_t parse_gen_attr_arg(iopc_parser_t *pp, iopc_attr_t *attr,
                                 iopc_arg_desc_t *desc)
{
    SB_1k(sb);
    iopc_arg_t arg;
    lstr_t new_name;
    iopc_token_t *tk;

    assert (IOPC_ATTR_REPEATED_MONO_ARG(attr->desc));
    if (!expect(desc))
        return LSTR_NULL_V;

    iopc_arg_init(&arg);
    arg.desc = desc;
    arg.loc  = TK(pp, 0)->loc;

    WANT(pp, 0, ITOK_GEN_ATTR_NAME);
    new_name = lstr_dups(TK(pp, 0)->b.data, -1);
    DROP(pp, 1);

    EAT(pp, ',');

    tk = TK(pp, 0);
    arg.type = tk->token;

    if (CHECK(pp, 1, ':') || CHECK(pp, 1, '=')) {
        arg.type = ITOK_IDENT;
        sb_addc(&sb, '{');
        parse_json_object(pp, &sb, true);
        sb_addc(&sb, '}');
        lstr_transfer_sb(&arg.v.s, &sb, false);
        goto append;
    }

    switch (arg.type) {
      case ITOK_STRING:
        arg.v.s = lstr_dups(tk->b.data, -1);
        break;

      case ITOK_DOUBLE:
        arg.v.d = tk->d;
        break;

      case ITOK_INTEGER:
      case ITOK_BOOL:
        arg.v.i64 = tk->i;
        break;

      default:
        fatal("unable to parse value for generic argument '%*pM'",
              LSTR_FMT_ARG(new_name));
    }

    DROP(pp, 1);

  append:
    qv_append(iopc_arg, &attr->args, arg);
    return new_name;
}

static void parse_struct_snmp_from(iopc_parser_t *pp, iopc_pkg_t **pkg,
                                   iopc_path_t **path, char **name)
{
    pstream_t ps = ps_initstr(TK(pp, 0)->b.data);
    ctype_desc_t sep;

    ctype_desc_build(&sep, ".");

    if (ps_has_char_in_ctype(&ps, &sep)) {
        iopc_path_t *path_new = iopc_path_new();
        qv_t(lstr) words;

        qv_init(lstr, &words);
        path_new->loc = TK(pp, 0)->loc;

        /* Split the token */
        ps_split(ps, &sep, 0, &words);

        /* Get the path */
        for (int i = 0; i < words.len - 1; i++) {
            qv_append(str, &path_new->bits, lstr_dup(words.tab[i]).v);
        }
        if (pkg) {
            *pkg = check_path_exists(pp, path_new);
        }

        *path = path_new;
        *name = lstr_dup(words.tab[words.len - 1]).v;

        qv_wipe(lstr, &words);
        DROP(pp, 1);
    } else {
        parse_struct_type(pp, pkg, path, name);
    }
}

static void parse_snmp_attr_arg(iopc_parser_t *pp, iopc_attr_t *attr,
                                iopc_arg_desc_t *desc)
{
    iopc_arg_t arg;

    iopc_arg_init(&arg);
    arg.desc = desc;
    arg.loc  = TK(pp, 0)->loc;
    arg.v.s = lstr_dup(LSTR(TK(pp, 0)->b.data));
    e_trace(1, "%s=(id)%s", desc->name.s, arg.v.s.s);

    WANT(pp, 0, ITOK_IDENT);
    arg.type = ITOK_IDENT;
    qv_append(iopc_arg, &attr->args, arg);

    do {
        iopc_extends_t *xt = iopc_extends_new();

        xt->loc  = TK(pp, 0)->loc;
        parse_struct_snmp_from(pp, &xt->pkg, &xt->path, &xt->name);
        iopc_loc_merge(&xt->loc, TK(pp, 0)->loc);

        qv_append(iopc_extends, &attr->snmp_params_from, xt);
    } while (SKIP(pp, ','));
}

static void parse_attr_arg(iopc_parser_t *pp, iopc_attr_t *attr,
                           iopc_arg_desc_t *desc)
{
    iopc_arg_t arg;

    if (!desc) {
        /* expect named argument: arg=val */
        lstr_t  str;
        bool    found = false;

        WANT(pp, 0, ITOK_IDENT);
        str = LSTR(TK(pp, 0)->b.data);

        qv_for_each_ptr(iopc_arg_desc, d, &attr->desc->args) {
            if (lstr_equal(&str, &d->name)) {
                desc  = d;
                found = true;
                break;
            }
        }
        if (!found) {
            fatal_loc("incorrect argument name", TK(pp, 0)->loc);
        }
        DROP(pp, 1);
        EAT(pp, '=');
    }

    if (!IOPC_ATTR_REPEATED_MONO_ARG(attr->desc)) {
        qv_for_each_ptr(iopc_arg, a, &attr->args) {
            if (a->desc == desc) {
                fatal_loc("duplicated argument", TK(pp, 0)->loc);
            }
        }
    }

    iopc_arg_init(&arg);
    arg.desc = desc;
    arg.loc  = TK(pp, 0)->loc;

    if (desc->type == ITOK_DOUBLE) {
        if (CHECK(pp, 0, desc->type)) {
            arg.type = desc->type;
        } else {
            WANT(pp, 0, ITOK_INTEGER);
            arg.type = ITOK_INTEGER;
        }
    } else {
        WANT(pp, 0, desc->type);
        arg.type = desc->type;
    }

    switch (arg.type) {
      case ITOK_STRING:
        arg.v.s = lstr_dup(LSTR(TK(pp, 0)->b.data));
        e_trace(1, "%s=(str)%s", desc->name.s, arg.v.s.s);
        DROP(pp, 1);
        break;

      case ITOK_DOUBLE:
        arg.v.d = TK(pp,0)->d;
        e_trace(1, "%s=(double)%f", desc->name.s, arg.v.d);
        DROP(pp, 1);
        break;

      case ITOK_IDENT:
        arg.v.s = lstr_dup(LSTR(TK(pp, 0)->b.data));
        e_trace(1, "%s=(id)%s", desc->name.s, arg.v.s.s);
        DROP(pp, 1);
        break;

      case ITOK_INTEGER:
      case ITOK_BOOL:
        arg.v.i64 = parse_constant_integer(pp, ')', NULL);
        e_trace(1, "%s=(i64)%jd", desc->name.s, arg.v.i64);
        break;

      default:
        fatal("incorrect type for argument %*pM", LSTR_FMT_ARG(desc->name));
    }

    qv_append(iopc_arg, &attr->args, arg);
    return;
}

static lstr_t parse_attr_args(iopc_parser_t *pp, iopc_attr_t *attr)
{
    bool             explicit = false;
    iopc_arg_desc_t *desc = NULL;
    int i = 0;
    lstr_t new_name = LSTR_NULL;

    iopc_lexer_push_state_attr(pp->ld);

    if (CHECK(pp, 1, '='))
        explicit = true;

    while (!CHECK_NOEOF(pp, 0, ')')) {
        if (!explicit) {
            if (IOPC_ATTR_REPEATED_MONO_ARG(attr->desc)) {
                desc = &attr->desc->args.tab[0];
            } else
            if (i >= attr->desc->args.len) {
                fatal_loc("too many arguments", attr->loc);
            } else {
                desc = &attr->desc->args.tab[i++];
            }
        }

        if (attr->desc->id == IOPC_ATTR_GENERIC) {
            new_name = parse_gen_attr_arg(pp, attr, desc);
            WANT(pp, 0, ')');
            break;
        } else
        if (attr->desc->id == IOPC_ATTR_SNMP_PARAMS_FROM) {
            parse_snmp_attr_arg(pp, attr, desc);
            WANT(pp, 0, ')');
            break;
        } else {
            parse_attr_arg(pp, attr, desc);
            if (CHECK(pp, 0, ')')) {
                break;
            }
            EAT(pp, ',');
        }
    }
    iopc_lexer_pop_state(pp->ld);
    DROP(pp, 1);

    if (!IOPC_ATTR_REPEATED_MONO_ARG(attr->desc)
    &&  attr->args.len != attr->desc->args.len)
    {
        fatal_loc("wrong number of arguments", attr->loc);
    }

    if (attr->desc->id == IOPC_ATTR_MIN_OCCURS
    ||  attr->desc->id == IOPC_ATTR_MAX_OCCURS
    ||  attr->desc->id == IOPC_ATTR_MIN_LENGTH
    ||  attr->desc->id == IOPC_ATTR_MAX_LENGTH
    ||  attr->desc->id == IOPC_ATTR_LENGTH)
    {
        if (!attr->args.tab[0].v.i64) {
            fatal_loc("zero value invalid for attribute %*pM", attr->loc,
                      LSTR_FMT_ARG(attr->desc->name));
        }
    }

    if (attr->desc->id == IOPC_ATTR_CTYPE) {
        if (iopc_g.v4) {
            if (!lstr_endswith(attr->args.tab[0].v.s, LSTR("__t"))) {
                fatal_loc("invalid ctype %*pM: missing __t suffix", attr->loc,
                          LSTR_FMT_ARG(attr->args.tab[0].v.s));
            }
        } else {
            if (!lstr_endswith(attr->args.tab[0].v.s, LSTR("_t"))) {
                warn_loc("invalid ctype %*pM: missing _t suffix", attr->loc,
                         LSTR_FMT_ARG(attr->args.tab[0].v.s));
            }
        }
    }

    return new_name;
}

static iopc_attr_t *parse_attr(iopc_parser_t *pp)
{
    iopc_attr_t     *attr;
    lstr_t           name;
    int              pos;

    attr = iopc_attr_new();

    WANT(pp, 0, ITOK_ATTR);

    if (!iopc_g.v2)
        fatal_loc("attributes forbidden (use -2 parameter)", TK(pp, 0)->loc);

    attr->loc = TK(pp, 0)->loc;
    name = LSTR(ident(TK(pp, 0)));
    pos = qm_find(attr_desc, &_G.attrs, &name);
    if (pos < 0) {
        fatal_loc("incorrect attribute name", attr->loc);
    }
    attr->desc = &_G.attrs.values[pos];
    DROP(pp, 1);
    e_trace(1, "attribute %*pM", LSTR_FMT_ARG(attr->desc->name));

    /* Generic attributes */
    if (attr->desc->id == IOPC_ATTR_GENERIC) {
        assert(attr->desc->args.len == 1);

        attr->real_name = parse_attr_args(pp, attr);

        return attr;
    }

    if (!SKIP(pp, '(')) {
        if (attr->desc->args.len > 0) {
            fatal_loc("attribute arguments missing", attr->loc);
        }
        return attr;
    }
    if (attr->desc->args.len == 0) {
        fatal_loc("attribute should not have arguments", attr->loc);
    }

    parse_attr_args(pp, attr);
    return attr;
}


static void check_pkg_path(iopc_parser_t *pp, iopc_path_t *path, const char *base)
{
    int fd = iopc_lexer_fd(pp->ld);
    struct stat st1, st2;
    char buf[PATH_MAX];

    snprintf(buf, sizeof(buf), "%s/%s", base, pretty_path(path));
    path_simplify(buf);
    if (stat(buf, &st1))
        fatal_loc("incorrect package name", path->loc);
    if (fstat(fd, &st2))
        fatal("fstat error on fd %d", fd);
    if (st1.st_dev != st2.st_dev || st1.st_ino != st2.st_ino)
        fatal_loc("incorrect package name", path->loc);
}

static void add_iface(iopc_pkg_t *pkg, iopc_iface_t *iface,
                      qm_t(struct) *mod_inter, const char *obj)
{
    qv_append(iopc_iface, &pkg->ifaces, iface);
    if (qm_add(struct, mod_inter, iface->name, (iopc_struct_t *)iface)) {
        fatal_loc("%s named `%s` already exists", iface->loc,
                  obj, iface->name);
    }
}

/* Force struct, enum and union to have distinguished name (things qm)*/
/* Force module and interface to have distinguished name   (mod_inter qm)*/
static iopc_pkg_t *parse_package(iopc_parser_t *pp, char *file,
                                 iopc_file_t type, bool is_main_pkg)
{
    iopc_pkg_t *pkg = iopc_pkg_new();
    qm_t(struct) things = QM_INIT_CACHED(struct, things);
    qm_t(struct) mod_inter = QM_INIT_CACHED(struct, mod_inter);
    qv_t(iopc_attr) attrs;
    qv_t(dox_chunk) chunks;

    qv_init(dox_chunk, &chunks);

    read_dox_front(pp, &chunks);
    pkg->name = parse_pkg_stmt(pp);
    read_dox_back(pp, &chunks, 0);
    build_dox_check_all(&chunks, pkg);

    pkg->file = file;
    if (type != IOPC_FILE_STDIN) {
        char base[PATH_MAX];

        path_dirname(base, sizeof(base), file);
        for (int i = 0; i < pkg->name->bits.len - 1; i++) {
            path_join(base, sizeof(base), "..");
        }
        path_simplify(base);
        if (type == IOPC_FILE_FD) {
            check_pkg_path(pp, pkg->name, base);
        }
        pp->base = pkg->base = p_strdup(base);
        qm_add(pkg, &_G.mods, pkg->file, pkg);
    }

    while (CHECK_KW(pp, 0, "import")) {
        qv_append(iopc_import, &pkg->imports, parse_import_stmt(pp));
    }

    qv_inita(iopc_attr, &attrs, 16);

    while (!CHECK(pp, 0, ITOK_EOF)) {
        const char *id;
        bool is_abstract = false;
        bool is_local = false;

        if (CHECK(pp, 0, ITOK_VERBATIM_C)) {
            sb_addsb(&pkg->verbatim_c, &TK(pp, 0)->b);
            DROP(pp, 1);
        }

        check_dox_and_attrs(pp, &chunks, &attrs);
        if (!attrs.len && CHECK(pp, 0, ITOK_EOF))
            break;

        if (!CHECK(pp, 0, ITOK_IDENT))
            fatal_loc("expected identifier", TK(pp, 0)->loc);

#define SET_ATTRS_AND_COMMENTS(_o, _t)                       \
        do {                                                 \
            qv_for_each_pos(iopc_attr, pos, &attrs) {        \
                iopc_attr_t *_attr = attrs.tab[pos];         \
                check_attr_type_decl(_attr, _t);             \
                qv_append(iopc_attr, &_o->attrs, _attr);     \
            }                                                \
            read_dox_back(pp, &chunks, 0);                   \
            build_dox(&chunks, _o, _t);                      \
        } while (0)

        for (int i = 0; i < 2; i++) {
            if (SKIP_KW(pp, "abstract")) {
                if (is_abstract) {
                    fatal_loc("repetition of `abstract` keyword",
                             TK(pp, 0)->loc);
                }
                is_abstract = true;
            } else
            if (SKIP_KW(pp, "local")) {
                if (is_local) {
                    fatal_loc("repetition of `local` keyword",
                             TK(pp, 0)->loc);
                }
                is_local = true;
            } else {
                break;
            }
        }

        id = ident(TK(pp, 0));

#define PARSE_STRUCT(_id, _type, _attr)  \
        if (strequal(id, _id)) {                                             \
            iopc_struct_t *st;                                               \
                                                                             \
            SKIP_KW(pp, _id);                                                \
            st = parse_struct_class_union_snmp_stmt(pp, _type, is_abstract,  \
                                                    is_local, is_main_pkg);  \
            SET_ATTRS_AND_COMMENTS(st, _attr);                               \
            if (iopc_is_snmp_tbl(_type)) {                                   \
                check_snmp_brief(st->comments, st->loc, st->name, _id);      \
            }                                                                \
                                                                             \
            qv_append(iopc_struct, &pkg->structs, st);                       \
            if (qm_add(struct, &things, st->name, st)) {                     \
                fatal_loc("something named `%s` already exists",             \
                          st->loc, st->name);                                \
            }                                                                \
            continue;                                                        \
        }

        PARSE_STRUCT("struct", STRUCT_TYPE_STRUCT, IOPC_ATTR_T_STRUCT);
        PARSE_STRUCT("class",  STRUCT_TYPE_CLASS,  IOPC_ATTR_T_STRUCT);
        PARSE_STRUCT("snmpObj", STRUCT_TYPE_SNMP_OBJ, IOPC_ATTR_T_SNMP_OBJ);
        PARSE_STRUCT("snmpTbl", STRUCT_TYPE_SNMP_TBL, IOPC_ATTR_T_SNMP_TBL);
        PARSE_STRUCT("union",  STRUCT_TYPE_UNION,  IOPC_ATTR_T_UNION);
#undef PARSE_STRUCT


        if (strequal(id, "enum")) {
            iopc_enum_t *en = parse_enum_stmt(pp, &attrs);

            SET_ATTRS_AND_COMMENTS(en, IOPC_ATTR_T_ENUM);

            qv_append(iopc_enum, &pkg->enums, en);
            if (qm_add(struct, &things, en->name, (iopc_struct_t *)en)) {
                fatal_loc("something named `%s` already exists",
                          en->loc, en->name);
            }
            continue;
        }

        if (strequal(id, "interface")) {
            iopc_iface_t *iface;
            const char *obj = "interface";

            iface = parse_iface_stmt(pp, IFACE_TYPE_IFACE, obj,
                                     is_main_pkg);

            SET_ATTRS_AND_COMMENTS(iface, IOPC_ATTR_T_IFACE);
            add_iface(pkg, iface, &mod_inter, obj);
            continue;
        }

        if (strequal(id, "snmpIface")) {
            iopc_iface_t *iface;
            const char *obj = "snmpIface";

            iface = parse_iface_stmt(pp, IFACE_TYPE_SNMP_IFACE, obj,
                                     is_main_pkg);

            SET_ATTRS_AND_COMMENTS(iface, IOPC_ATTR_T_SNMP_IFACE);
            add_iface(pkg, iface, &mod_inter, obj);
            continue;
        }

        if (strequal(id, "module")) {
            iopc_struct_t *mod = parse_module_stmt(pp);

            SET_ATTRS_AND_COMMENTS(mod, IOPC_ATTR_T_MOD);

            qv_append(iopc_struct, &pkg->modules, mod);
            if (qm_add(struct, &mod_inter, mod->name, (iopc_struct_t *)mod)) {
                fatal_loc("something named `%s` already exists",
                          mod->loc, mod->name);
            }
            continue;
        }

        if (strequal(id, "typedef")) {
            iopc_field_t *tdef = parse_typedef_stmt(pp);

            qv_for_each_entry(iopc_attr, attr, &attrs) {
                check_attr_type_field(attr, tdef, true);
                qv_append(iopc_attr, &tdef->attrs, attr);
            }
            qv_append(iopc_field, &pkg->typedefs, tdef);
            if (qm_add(struct, &things, tdef->name, (iopc_struct_t *)tdef)) {
                fatal_loc("something named `%s` already exists",
                          tdef->loc, tdef->name);
            }
            continue;
        }

        fatal_loc("unexpected keyword `%s`", TK(pp, 0)->loc, id);
#undef SET_ATTRS_AND_COMMENTS
    }
    qv_wipe(iopc_attr, &attrs);
    qv_deep_wipe(dox_chunk, &chunks, dox_chunk_wipe);
    qm_wipe(struct, &things);
    qm_wipe(struct, &mod_inter);

    EAT(pp, ITOK_EOF);
    return pkg;
}

/*-}}}-*/

iopc_loc_t iopc_loc_merge2(iopc_loc_t l1, iopc_loc_t l2)
{
    assert (l1.file == l2.file);
    return (iopc_loc_t){
        .file = l1.file,
        .lmin = MIN(l1.lmin, l2.lmin),
        .cmin = MIN(l1.cmin, l2.cmin),
        .lmax = MAX(l1.lmax, l2.lmax),
        .cmax = MAX(l1.cmax, l2.cmax),
    };
}

void iopc_loc_merge(iopc_loc_t *l1, iopc_loc_t l2)
{
    *l1 = iopc_loc_merge2(*l1, l2);
}

iopc_pkg_t *iopc_parse_file(const qv_t(cstr) *includes, const qm_t(env) *env,
                            const char *file, const char *data,
                            bool is_main_pkg)
{
    char tmp[PATH_MAX];
    iopc_pkg_t *pkg = NULL;
    iopc_file_t type;

    if (data) {
        type = IOPC_FILE_BUFFER;
    } else
    if (strequal(file, "-")) {
        type = IOPC_FILE_STDIN;
    } else {
        type = IOPC_FILE_FD;
    }

    if (type != IOPC_FILE_STDIN) {
        pstrcpy(tmp, sizeof(tmp), file);
        path_simplify(tmp);
        file = tmp;
        pkg = qm_get_def(pkg, &_G.mods, file, NULL);
    }

    if (!pkg) {
        char *path;
        iopc_parser_t pp = {
            .includes = includes,
            .env      = env,
            .cfolder  = iop_cfolder_new(),
        };

        if (type == IOPC_FILE_STDIN) {
            path = p_strdup("<stdin>");
        } else {
            path = p_strdup(file);
        }
        pp.ld = iopc_lexer_new(path, data, type);
        pkg = parse_package(&pp, path, type, is_main_pkg);
        qv_deep_wipe(iopc_token, &pp.tokens, iopc_token_delete);
        iopc_lexer_delete(&pp.ld);
        iop_cfolder_delete(&pp.cfolder);
    }

    return pkg;
}

void iopc_parser_initialize(void)
{
    qm_init_cached(pkg, &_G.mods);
    qm_init_cached(enums, &_G.enums);
    qm_init_cached(enums, &_G.enums_forbidden);
    qm_init_cached(attr_desc, &_G.attrs);
    init_attributes();
}

void iopc_parser_shutdown(void)
{
    qm_deep_wipe(pkg, &_G.mods, IGNORE, iopc_pkg_delete);
    qm_deep_wipe(enums, &_G.enums, p_delete, IGNORE);
    qm_deep_wipe(enums, &_G.enums_forbidden, p_delete, IGNORE);
    qm_deep_wipe(attr_desc, &_G.attrs, IGNORE, iopc_attr_desc_wipe);
}
