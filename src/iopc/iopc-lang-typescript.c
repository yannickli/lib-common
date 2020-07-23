/***************************************************************************/
/*                                                                         */
/* Copyright 2020 INTERSEC SA                                              */
/*                                                                         */
/* Licensed under the Apache License, Version 2.0 (the "License");         */
/* you may not use this file except in compliance with the License.        */
/* You may obtain a copy of the License at                                 */
/*                                                                         */
/*     http://www.apache.org/licenses/LICENSE-2.0                          */
/*                                                                         */
/* Unless required by applicable law or agreed to in writing, software     */
/* distributed under the License is distributed on an "AS IS" BASIS,       */
/* WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.*/
/* See the License for the specific language governing permissions and     */
/* limitations under the License.                                          */
/*                                                                         */
/***************************************************************************/

#include "iopc.h"

struct iopc_do_typescript_globs iopc_do_typescript_g;
#define _G  iopc_do_typescript_g

static const char *reserved_model_names_g[] = {
    /* event methods */
    "on", "off", "trigger", "once", "listenTo", "stopListening",
    "listenToOnce",

    /* model methods */
    "get", "set", "escape", "has", "unset", "clear", "toJSON", "sync",
    "fetch", "save", "destroy", "validate", "isValid", "url", "parse",
    "clone", "isNew", "hasChanged", "previous",

    /* out custom methods */
    "getLastAttr", "equal", "icQuery",

    /* underscore methods */
    "keys", "values", "pairs", "invert", "pick", "omit", "chain",
    "isEmpty"
};

static const char *reserved_union_model_names_g[] = {
    "kind", "is", "isSet", "getPair", "unionGet"
};

static qv_t(str) pp_g;

#define RO_WARN \
    "/***** THIS FILE IS AUTOGENERATED DO NOT MODIFY DIRECTLY ! *****/\n"

static const char *pp_under(iopc_path_t *path)
{
    SB_1k(buf);
    char *res;

    tab_for_each_entry(bit, &path->bits) {
        sb_addf(&buf, "%s__", bit);
    }
    sb_shrink(&buf, 2);
    qv_append(&pp_g, res = sb_detach(&buf, NULL));
    return res;
}

static const char *pp_path(iopc_path_t *path)
{
    SB_1k(buf);
    char *res;

    tab_for_each_entry(bit, &path->bits) {
        sb_addf(&buf, "%s/", bit);
    }
    sb_shrink(&buf, 1);
    qv_append(&pp_g, res = sb_detach(&buf, NULL));
    return res;
}

static const char *pp_dot(iopc_path_t *path)
{
    SB_1k(buf);
    char *res;

    tab_for_each_entry(bit, &path->bits) {
        sb_addf(&buf, "%s.", bit);
    }
    sb_shrink(&buf, 1);
    qv_append(&pp_g, res = sb_detach(&buf, NULL));
    return res;
}

static bool is_name_reserved_in_model(const char *field_name, bool is_union)
{
    carray_for_each_entry(name, reserved_model_names_g) {
        if (strequal(field_name, name)) {
            return true;
        }
    }
    if (is_union) {
        carray_for_each_entry(name, reserved_union_model_names_g) {
            if (strequal(field_name, name)) {
                return true;
            }
        }
    }

    return false;
}

static void iopc_dump_import(sb_t *buf, const iopc_pkg_t *dep,
                             qh_t(cstr) *imported)
{
    const char *import_name = pp_under(dep->name);

    if (qh_put(cstr, imported, import_name, 0) & QHASH_COLLISION) {
        return;
    }

    sb_addf(buf, "import * as %s from \"iop/%s.iop\";\n",
            pp_under(dep->name), pp_path(dep->name));
}

static bool iopc_should_dump_coll(const iopc_struct_t *st)
{
    tab_for_each_entry(attr, &st->attrs) {
        if (attr->desc->id == IOPC_ATTR_TS_NO_COLL) {
            return false;
        }
    }
    return true;
}

static void iopc_dump_imports(sb_t *buf, iopc_pkg_t *pkg)
{
    qh_t(cstr) imported;
    qv_t(iopc_pkg) t_deps;
    qv_t(iopc_pkg) t_weak_deps;
    qv_t(iopc_pkg) i_deps;

    qh_init(cstr, &imported);
    qv_inita(&t_deps, 1024);
    qv_inita(&t_weak_deps, 1024);
    qv_inita(&i_deps, 1024);

    iopc_get_depends(pkg, &t_deps, &t_weak_deps, &i_deps, 0);

    if (_G.enable_iop_backbone) {
        bool import_struct = false;
        bool import_base_class = false;
        bool import_class = false;
        bool import_union = false;
        bool import_coll = false;

        tab_for_each_entry(st, &pkg->structs) {
            switch (st->type) {
              case STRUCT_TYPE_STRUCT:
                import_struct = true;
                if (iopc_should_dump_coll(st)) {
                    import_coll = true;
                }
                break;
              case STRUCT_TYPE_CLASS:
                import_class = true;
                if (!st->extends.len) {
                    import_base_class = true;
                    if (iopc_should_dump_coll(st)) {
                        import_coll = true;
                    }
                }
                break;
              case STRUCT_TYPE_UNION:
                import_union = true;
                if (iopc_should_dump_coll(st)) {
                    import_coll = true;
                }
                break;
              default:
                break;
            }
        }
        tab_for_each_entry(iface, &pkg->ifaces) {
            if (iface->type == IFACE_TYPE_IFACE) {
                tab_for_each_entry(rpc, &iface->funs) {
                    if ((rpc->arg && rpc->arg_is_anonymous)
                    ||  (rpc->res && rpc->res_is_anonymous)
                    ||  (rpc->exn && rpc->exn_is_anonymous))
                    {
                        import_struct = true;
                        import_coll = true;
                    }
                }
            }
        }
        if (import_struct || import_base_class || import_union) {
            sb_adds(buf, "import { ");
            if (import_struct) {
                sb_adds(buf, "StructModel");
            }
            if (import_base_class) {
                if (import_struct) {
                    sb_adds(buf, ", ");
                }
                sb_adds(buf, "ClassModel");
            }
            if (import_union) {
                if (import_struct || import_base_class) {
                    sb_adds(buf, ", ");
                }
                sb_adds(buf, "UnionModel");
            }
            sb_adds(buf, " } from 'iop/backbone/model';\n");
        }
        if (import_coll) {
            sb_adds(buf, "import { IopCollection } from 'iop/backbone/collection';\n");
        }

        sb_adds(buf, "import * as iop from 'iop/backbone';\n");
        if (pkg->enums.len) {
            sb_adds(buf, "import { Enumeration } from 'iop/enumeration';\n");
            sb_adds(buf, "const enumeration = iop.enumeration;\n");
        }


        if (import_struct || import_union || import_class) {
            sb_adds(buf, "const registerModel = iop.backbone.registerModel;\n");
            sb_adds(buf, "const registerCollection = iop.backbone.registerCollection;\n");
        }
        sb_addf(buf, "import { Package as _IopCorePackage } from 'iop/core';\n");
        sb_addf(buf, "import JSON from 'json/%s.iop.json';\n",
                pp_path(pkg->name));

        sb_addf(buf, "iop.load(JSON as _IopCorePackage);\n\n");
    }

    tab_for_each_entry(dep, &t_deps) {
        iopc_dump_import(buf, dep, &imported);
    }
    tab_for_each_entry(dep, &t_weak_deps) {
        iopc_dump_import(buf, dep, &imported);
    }
    tab_for_each_entry(dep, &i_deps) {
        iopc_dump_import(buf, dep, &imported);
    }

    qv_wipe(&t_deps);
    qv_wipe(&t_weak_deps);
    qv_wipe(&i_deps);
    qh_wipe(cstr, &imported);
}

static void iopc_dump_package_member(sb_t *buf, const iopc_pkg_t *pkg,
                                     const iopc_pkg_t *member_pkg,
                                     iopc_path_t *member_path,
                                     const char *member_name)
{
    if (pkg != member_pkg) {
        assert (member_path->bits.len);
        sb_adds(buf, pp_under(member_path));
        sb_addc(buf, '.');
    }
    sb_adds(buf, member_name);
}

static void iopc_dump_enum(sb_t *buf, const char *indent,
                           const iopc_pkg_t *pkg, const iopc_enum_t *en)
{
    bool is_strict = false;
    bool first = false;

    tab_for_each_entry(attr, &en->attrs) {
        if (attr->desc->id == IOPC_ATTR_STRICT) {
            is_strict = true;
            break;
        }
    }

    sb_addf(buf, "\n%sexport type %s_Int = ", indent, en->name);
    first = true;
    tab_for_each_entry(field, &en->values) {
        if (!first) {
            sb_addf(buf, "\n%s    | ", indent);
        }
        sb_addf(buf, "%d", field->value);
        first = false;
    }
    sb_adds(buf, ";\n");

    sb_addf(buf, "%sexport type %s_Str = ", indent, en->name);
    first = true;
    tab_for_each_entry(field, &en->values) {
        if (!first) {
            sb_addf(buf, "\n%s    | ", indent);
        }
        sb_addf(buf, "'%s'", field->name);
        first = false;
    }
    sb_adds(buf, ";\n");

    if (is_strict) {
        sb_addf(buf, "%sexport type %s = %s_Str;\n",
                indent, en->name, en->name);
    } else {
        sb_addf(buf, "%sexport type %s = %s_Str;\n",
                indent, en->name, en->name);
    }

    if (_G.enable_iop_backbone) {
        sb_addf(buf, "%sexport interface %s_ModelIf extends "
                "Enumeration<%s_Str, %s_Int> {\n", indent, en->name, en->name,
                en->name);
        tab_for_each_entry(field, &en->values) {
            sb_addf(buf, "%s    %s: '%s';\n", indent, field->name,
                    field->name);
        }
        sb_addf(buf, "%s}\n", indent);

        sb_addf(buf, "%sexport const %s_Model: %s_ModelIf "
                "= enumeration<%s_Str, %s_Int>('%s.%s') as any;\n",
                indent, en->name, en->name, en->name,  en->name,
                pp_dot(pkg->name), en->name);
    }
}

static void iopc_dump_enums(sb_t *buf, const iopc_pkg_t *pkg)
{
    tab_for_each_entry(en, &pkg->enums) {
        iopc_dump_enum(buf, "", pkg, en);
    }
}

static void iopc_dump_field_basetype(sb_t *buf, const iopc_pkg_t *pkg,
                                     const iopc_field_t *field,
                                     const char * nullable suffix)
{
    switch (field->kind) {
      case IOP_T_I8: sb_adds(buf, "number"); break;
      case IOP_T_U8: sb_adds(buf, "number"); break;
      case IOP_T_I16: sb_adds(buf, "number"); break;
      case IOP_T_U16: sb_adds(buf, "number"); break;
      case IOP_T_I32: sb_adds(buf, "number"); break;
      case IOP_T_U32: sb_adds(buf, "number"); break;
      case IOP_T_I64: sb_adds(buf, "number | string"); break;
      case IOP_T_U64: sb_adds(buf, "number | string"); break;
      case IOP_T_BOOL: sb_adds(buf, "boolean"); break;
      case IOP_T_DOUBLE: sb_adds(buf, "number"); break;
      case IOP_T_VOID: sb_adds(buf, "null"); break;

      case IOP_T_STRING: case IOP_T_XML:
      case IOP_T_DATA:
        sb_adds(buf, "string");
        break;

      case IOP_T_STRUCT:
        iopc_dump_package_member(buf, pkg, field->type_pkg, field->type_path,
                                 field->type_name);
        if (suffix) {
            sb_adds(buf, suffix);
        }
        break;

      case IOP_T_UNION: case IOP_T_ENUM:
        iopc_dump_package_member(buf, pkg, field->type_pkg, field->type_path,
                                 field->type_name);
        if (field->kind == IOP_T_UNION && suffix) {
            sb_adds(buf, suffix);
        } else
        if (suffix) {
            sb_adds(buf, "_Str");
        }
        break;
    }
}

enum {
    OBJECT_FIELD = 1 << 0,
    DEFVAL_AS_OPT = 1 << 1,
    USE_MODEL = 1 << 2,
    USE_PARAM = 1 << 3,
    IN_UNION = 1 << 4,
};

static void iopc_dump_field_type(sb_t *buf, const iopc_pkg_t *pkg,
                                 const iopc_field_t *field,
                                 unsigned flags)
{
    const char *suffix = (flags & USE_PARAM) ? "_ModelParam" :
                         (flags & USE_MODEL) ? "_Model" : NULL;

    if (field->kind != IOP_T_STRUCT && field->kind != IOP_T_UNION) {
        flags &= ~USE_MODEL;
    }
    switch (field->repeat) {
      case IOP_R_REPEATED:
        if (!(flags & USE_MODEL)) {
            sb_adds(buf, "Array<");
        } else {
            suffix = "_Collection";
        }
        break;

      default:
        break;
    }

    iopc_dump_field_basetype(buf, pkg, field, suffix);

    switch (field->repeat) {
      case IOP_R_REPEATED:
        if (!(flags & USE_MODEL)) {
            if ((flags & USE_PARAM)) {
                sb_adds(buf, " | ");
                iopc_dump_field_basetype(buf, pkg, field, "_Model");
            }
            sb_addc(buf, '>');
        }
        break;

      case IOP_R_DEFVAL:
        if (!(flags & DEFVAL_AS_OPT)) {
            break;
        }
        /* FALLTHROUGH */

      case IOP_R_OPTIONAL:
        if (!(flags & OBJECT_FIELD)) {
            sb_adds(buf, " | undefined");
        }
        break;

      default:
        break;
    }
}

static void iopc_dump_field(sb_t *buf, const iopc_pkg_t *pkg,
                            const iopc_field_t *field,
                            unsigned flags)
{
    sb_adds(buf, field->name);
    if (flags & OBJECT_FIELD) {
        switch (field->repeat) {
          case IOP_R_DEFVAL:
            if (!(flags & DEFVAL_AS_OPT)) {
                break;
            }
            /* FALLTHROUGH */

          case IOP_R_OPTIONAL:
            sb_addc(buf, '?');
            break;

          case IOP_R_REPEATED:
            if (flags & USE_PARAM) {
                sb_addc(buf, '?');
            }
            break;

          case IOP_R_REQUIRED:
            /* Make a void field in struct optional, so that it does not have
             * to be specified when building the type.
             * However, do not make it optional if it is an element of a
             * union, as the field is a discriminant. */
            if (unlikely(field->kind == IOP_T_VOID && !(flags & IN_UNION))) {
                sb_addc(buf, '?');
            }
            break;

          default:
            break;
        }
    }
    sb_adds(buf, ": ");
    iopc_dump_field_type(buf, pkg, field, flags);
}

static void iopc_dump_struct(sb_t *buf, const char *indent,
                             const iopc_pkg_t *pkg, iopc_struct_t *st,
                             const char *st_name)
{
    if (!st_name) {
        st_name = st->name;
    }

    sb_addf(buf, "%sconst %s_fullname = '%s%s%s.%s';\n",
            indent, st_name, pp_dot(pkg->name), st->iface ? "." : "",
            st->iface ? st->iface->name : "", st_name);

    iopc_struct_sort_fields(st, BY_POS);

    sb_addf(buf, "%sexport interface %s", indent, st_name);
    if (iopc_is_class(st->type) && st->extends.len) {
        const iopc_pkg_t *parent_pkg = st->extends.tab[0]->pkg;
        const iopc_struct_t *parent = st->extends.tab[0]->st;

        sb_adds(buf, " extends ");
        iopc_dump_package_member(buf, pkg, parent_pkg, parent_pkg->name,
                                 parent->name);
    }

    sb_adds(buf, " {\n");

    if (iopc_is_class(st->type) && !st->extends.len) {
        sb_addf(buf, "%s    _class: string;\n", indent);
    }

    tab_for_each_entry(field, &st->fields) {
        sb_addf(buf, "%s    ", indent);
        iopc_dump_field(buf, pkg, field, OBJECT_FIELD);
        sb_adds(buf, ";\n");
    }

    sb_addf(buf, "%s}\n", indent);

    if (_G.enable_iop_backbone) {
        if (iopc_is_class(st->type)) {
            sb_addf(buf, "%sexport interface %s_ModelParam", indent, st_name);

            if (st->extends.len) {
                const iopc_pkg_t *parent_pkg = st->extends.tab[0]->pkg;
                const iopc_struct_t *parent = st->extends.tab[0]->st;

                sb_adds(buf, " extends ");
                iopc_dump_package_member(buf, pkg, parent_pkg, parent_pkg->name,
                                         parent->name);
                sb_adds(buf, "_ModelParam");
            }
        } else {
            sb_addf(buf, "%sexport interface %s_ModelParam", indent, st_name);
        }
        sb_adds(buf, " {\n");
        tab_for_each_entry(field, &st->fields) {
            sb_addf(buf, "%s    ", indent);
            iopc_dump_field(buf, pkg, field,
                            OBJECT_FIELD | DEFVAL_AS_OPT | USE_PARAM);
            if (field->kind == IOP_T_UNION || field->kind == IOP_T_STRUCT) {
                sb_adds(buf, " | ");
                iopc_dump_field_type(buf, pkg, field, OBJECT_FIELD | USE_MODEL);
            }
            sb_adds(buf, ";\n");
        }
        sb_addf(buf, "%s}\n", indent);

        sb_addf(buf, "%sexport class %s_Model", indent, st_name);
        if (iopc_is_class(st->type)) {
            sb_addf(buf, "<Param extends %s_ModelParam = %s_ModelParam>",
                    st_name, st_name);
        }
        sb_adds(buf, " extends ");

        if (iopc_is_class(st->type)) {
            if (st->extends.len) {
                const iopc_pkg_t *parent_pkg = st->extends.tab[0]->pkg;
                const iopc_struct_t *parent = st->extends.tab[0]->st;

                iopc_dump_package_member(buf, pkg, parent_pkg, parent_pkg->name,
                                         parent->name);
                sb_adds(buf, "_Model<Param>");
            } else {
                sb_adds(buf, "ClassModel<Param>");
            }
        } else {
            sb_addf(buf, "StructModel<%s_ModelParam>", st_name);
        }
        sb_adds(buf, " {\n");

        tab_for_each_entry(field, &st->fields) {
            if (!is_name_reserved_in_model(field->name, false)) {
                sb_addf(buf, "%s    public ", indent);
                iopc_dump_field(buf, pkg, field, OBJECT_FIELD | USE_MODEL);
                sb_adds(buf, ";\n");
            }
        }

        sb_addf(buf, "%s};\n", indent);
        sb_addf(buf, "%sregisterModel(%s_Model, %s_fullname);\n",
                indent, st_name, st_name);
        if (iopc_should_dump_coll(st)) {
            if (iopc_is_class(st->type)) {
                sb_addf(buf, "%sexport class %s_Collection<Model extends "
                        "%s_Model = %s_Model> extends ", indent, st_name,
                        st_name, st_name);

                if (st->extends.len) {
                    const iopc_pkg_t *parent_pkg = st->extends.tab[0]->pkg;
                    const iopc_struct_t *parent = st->extends.tab[0]->st;

                    iopc_dump_package_member(buf, pkg, parent_pkg, parent_pkg->name,
                                             parent->name);
                    sb_adds(buf, "_Collection<Model> { };\n");
                } else {
                    sb_adds(buf, "IopCollection<Model> { };\n");
                }
            } else {
                sb_addf(buf, "%sexport class %s_Collection extends IopCollection<%s_Model> { };\n",
                        indent, st_name, st_name);
            }
            sb_addf(buf, "%sregisterCollection<%s_Model>(%s_Collection, %s_fullname);\n",
                    indent, st_name, st_name, st_name);
        }
    }
}

static void iopc_dump_union(sb_t *buf, const char *indent,
                            const iopc_pkg_t *pkg, iopc_struct_t *st,
                            const char *st_name)
{
    bool first;

    if (!st_name) {
        st_name = st->name;
    }

    iopc_struct_sort_fields(st, BY_POS);

    sb_addc(buf, '\n');

    first = true;
    sb_addf(buf, "%sexport type %s = ", indent, st_name);
    tab_for_each_entry(field, &st->fields) {
        if (!first) {
            sb_addf(buf, "\n%s    | ", indent);
        }
        sb_adds(buf, "{ ");
        iopc_dump_field(buf, pkg, field, OBJECT_FIELD | IN_UNION);
        sb_adds(buf, " }");
        first = false;
    }
    sb_adds(buf, ";\n");

    first = true;
    sb_addf(buf, "%sexport type %s_Pairs = ", indent, st_name);
    tab_for_each_entry(field, &st->fields) {
        if (!first) {
            sb_addf(buf, "\n%s    | ", indent);
        }
        sb_addf(buf, "{ kind: '%s', value: ", field->name);
        iopc_dump_field_type(buf, pkg, field, OBJECT_FIELD);
        sb_adds(buf, " }");
        first = false;
    }
    sb_adds(buf, ";\n");

    sb_addf(buf, "%sexport type %s_Keys = ", indent, st_name);
    tab_for_each_entry(field, &st->fields) {
        sb_addf(buf, "'%s' | ", field->name);
    }
    sb_shrink(buf, 3);
    sb_adds(buf, ";\n");


    if (_G.enable_iop_backbone) {
        sb_addf(buf, "%sconst %s_fullname = '%s.%s';\n",
                indent, st_name, pp_dot(pkg->name), st_name);

        first = true;
        sb_addf(buf, "%sexport type %s_ModelPairs = ", indent, st_name);
        tab_for_each_entry(field, &st->fields) {
            if (!first) {
                sb_addf(buf, "\n%s    | ", indent);
            }
            sb_addf(buf, "{ kind: '%s', value: ", field->name);
            iopc_dump_field_type(buf, pkg, field, OBJECT_FIELD | USE_MODEL);
            sb_adds(buf, " }");
            first = false;
        }
        sb_adds(buf, ";\n");

        first = true;
        sb_addf(buf, "%sexport type %s_ModelParam = ", indent, st_name);
        tab_for_each_entry(field, &st->fields) {
            if (!first) {
                sb_addf(buf, "\n%s    | ", indent);
            }
            sb_adds(buf, "{ ");
            iopc_dump_field(buf, pkg, field,
                            OBJECT_FIELD | USE_PARAM | IN_UNION);
            sb_adds(buf, " }");

            if (field->kind == IOP_T_UNION || field->kind == IOP_T_STRUCT) {
                sb_addf(buf, "\n%s    | { ", indent);
                iopc_dump_field(buf, pkg, field,
                                OBJECT_FIELD | USE_MODEL | IN_UNION);
                sb_adds(buf, " }");
            }

            first = false;
        }
        sb_adds(buf, ";\n");

        sb_addf(buf, "%sexport class %s_Model extends UnionModel<%s_Keys, %s_ModelPairs, %s_ModelParam> {\n",
                indent, st_name, st_name, st_name, st_name);

        tab_for_each_entry(field, &st->fields) {
            if (!is_name_reserved_in_model(field->name, true)) {
                sb_addf(buf, "%s    public ", indent);
                iopc_dump_field(buf, pkg, field,
                                OBJECT_FIELD | USE_MODEL | IN_UNION);
                sb_adds(buf, " | undefined;\n");
            }
        }

        sb_addf(buf, "%s};\n", indent);
        sb_addf(buf, "%sregisterModel(%s_Model, %s_fullname);\n",
                indent, st_name, st_name);
        sb_addf(buf, "%sexport class %s_Collection extends IopCollection<%s_Model> { };\n",
                indent, st_name, st_name);
        sb_addf(buf, "%sregisterCollection(%s_Collection, %s_fullname);\n",
                indent, st_name, st_name);
    }
}

static void iopc_dump_structs(sb_t *buf, iopc_pkg_t *pkg)
{
    tab_for_each_entry(st, &pkg->structs) {
        switch (st->type) {
          case STRUCT_TYPE_STRUCT:
          case STRUCT_TYPE_CLASS:
            iopc_dump_struct(buf, "", pkg, st, NULL);
            sb_addc(buf, '\n');
            break;

          case STRUCT_TYPE_UNION:
            iopc_dump_union(buf, "", pkg, st, NULL);
            sb_addc(buf, '\n');
            break;

          default:
            break;
        }
    }
}

static void iopc_dump_rpc(sb_t *buf, const iopc_pkg_t *pkg,
                          const iopc_fun_t *rpc)
{
    t_scope;

    if (rpc->arg) {
        if (rpc->arg_is_anonymous) {
            iopc_dump_struct(buf, "        ", pkg, rpc->arg,
                             t_fmt("%sArgs", rpc->name));
        } else {
            sb_addf(buf, "        export type %sArgs = ", rpc->name);
            iopc_dump_field_basetype(buf, pkg, rpc->farg, NULL);
            sb_adds(buf, ";\n");
        }
    } else {
        sb_addf(buf, "        export type %sArgs = void;\n", rpc->name);
    }

    if (rpc->res) {
        if (rpc->res_is_anonymous) {
            iopc_dump_struct(buf, "        ", pkg, rpc->res,
                             t_fmt("%sRes", rpc->name));
        } else {
            sb_addf(buf, "        export type %sRes = ", rpc->name);
            iopc_dump_field_basetype(buf, pkg, rpc->fres, NULL);
            sb_adds(buf, ";\n");
        }
    } else {
        sb_addf(buf, "        export type %sRes = void;\n", rpc->name);
    }

    if (rpc->exn) {
        if (rpc->exn_is_anonymous) {
            iopc_dump_struct(buf, "        ", pkg, rpc->exn,
                             t_fmt("%sExn", rpc->name));
        } else {
            sb_addf(buf, "        export type %sExn = ", rpc->name);
            iopc_dump_field_basetype(buf, pkg, rpc->fexn, NULL);
            sb_adds(buf, ";\n");
        }
    } else {
        sb_addf(buf, "        export type %sExn = void;\n", rpc->name);
    }

    sb_addf(buf, "        export type %s = (", rpc->name);
    if (rpc->arg) {
        if (rpc->arg_is_anonymous) {
            bool first = true;

            tab_for_each_entry(field, &rpc->arg->fields) {
                if (!first) {
                    sb_adds(buf, ", ");
                }
                iopc_dump_field(buf, pkg, field, DEFVAL_AS_OPT);
                first = false;
            }
        } else {
            sb_adds(buf, "arg: ");
            iopc_dump_field_basetype(buf, pkg, rpc->farg, NULL);
        }
    }
    sb_adds(buf, ") => ");

    if (rpc->fun_is_async) {
        sb_adds(buf, "void");
    } else
    if (rpc->res) {
        sb_addf(buf, "Promise<%sRes>", rpc->name);
    } else {
        sb_adds(buf, "Promise<void>");
    }

    sb_adds(buf, ";\n\n");
}

static void iopc_dump_iface(sb_t *buf, const iopc_pkg_t *pkg,
                            iopc_iface_t *iface)
{
    sb_addf(buf, "    export namespace %s {\n", iface->name);

    tab_for_each_entry(rpc, &iface->funs) {
        iopc_dump_rpc(buf, pkg, rpc);
    }

    sb_addf(buf, "    }\n");
}

static void iopc_dump_ifaces(sb_t *buf, iopc_pkg_t *pkg)
{
    sb_adds(buf, "\nexport namespace interfaces {\n");

    tab_for_each_entry(iface, &pkg->ifaces) {
        switch (iface->type) {
          case IFACE_TYPE_IFACE:
            iopc_dump_iface(buf, pkg, iface);
            break;

          default:
            break;
        }
    }

    sb_adds(buf, "}\n");
}

int iopc_do_typescript(iopc_pkg_t *pkg, const char *outdir, sb_t *depbuf)
{
    SB_8k(buf);
    char path[PATH_MAX];

    iopc_set_path(outdir, pkg, ".iop.ts", sizeof(path), path, true);

    sb_adds(&buf, RO_WARN);
    sb_adds(&buf, "/* tslint:disable */\n");
    sb_adds(&buf, "/* eslint-disable */\n");
    iopc_dump_imports(&buf, pkg);

    iopc_dump_enums(&buf, pkg);
    iopc_dump_structs(&buf, pkg);
    iopc_dump_ifaces(&buf, pkg);

    qv_deep_wipe(&pp_g, p_delete);
    return iopc_write_file(&buf, path);
}
