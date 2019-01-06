/**************************************************************************/
/*                                                                        */
/*  Copyright (C) 2004-2019 INTERSEC SA                                   */
/*                                                                        */
/*  Should you receive a copy of this source code, you must check you     */
/*  have a proper, written authorization of INTERSEC to hold it. If you   */
/*  don't have such an authorization, you must DELETE all source code     */
/*  files in your possession, and inform INTERSEC of the fact you obtain  */
/*  these files. Should you not comply to these terms, you can be         */
/*  prosecuted in the extent permitted by applicable law.                 */
/*                                                                        */
/**************************************************************************/

#include "iopc-iopsq.h"
#include "iopc-priv.h"

#include <lib-common/iop-priv.h>

/* {{{ IOP-described package to iopc_pkg_t */
/* {{{ Helpers */

static iopc_path_t *parse_path(lstr_t name, bool is_type, sb_t *err)
{
    pstream_t ps = ps_initlstr(&name);
    iopc_path_t *path = iopc_path_new();

    while (!ps_done(&ps)) {
        pstream_t bit;

        if (ps_get_ps_chr_and_skip(&ps, '.', &bit) < 0) {
            bit = ps;
            __ps_skip_upto(&ps, ps.s_end);
        } else
        if (ps_done(&ps)) {
            sb_sets(err, "trailing dot in package name");
            goto error;
        }

        if (ps_done(&bit)) {
            sb_setf(err, "empty package or sub-package name");
            goto error;
        }
        qv_append(&path->bits, p_dupz(bit.s, ps_len(&bit)));
    }

    return path;

  error:
    iopc_path_delete(&path);
    return NULL;
}

static int iopc_check_type_name(lstr_t name, sb_t *err)
{
    RETHROW(iopc_check_name(name, NULL, err));

    /* XXX Checked by iopc_check_name(). */
    assert (name.len);

    if (!isupper(name.s[0])) {
        sb_sets(err, "first field name character should be uppercase");
        return -1;
    }

    return 0;
}

/* }}} */
/* {{{ IOP struct/union */

static iop_type_t iop_type_from_iop(const iop__type__t *iop_type)
{
    IOP_UNION_SWITCH(iop_type) {
      IOP_UNION_CASE_P(iop__type, iop_type, i, i) {
        switch (i->size) {
#define CASE(_sz)                                                            \
          case INT_SIZE_S##_sz:                                              \
            return i->is_signed ? IOP_T_I##_sz : IOP_T_U##_sz

            CASE(8);
            CASE(16);
            CASE(32);
            CASE(64);

#undef CASE
        }
      }
      IOP_UNION_CASE_V(iop__type, iop_type, b) {
        return IOP_T_BOOL;
      }
      IOP_UNION_CASE_V(iop__type, iop_type, d) {
        return IOP_T_DOUBLE;
      }
      IOP_UNION_CASE(iop__type, iop_type, s, s) {
        switch (s) {
          case STRING_TYPE_STRING:
            return IOP_T_STRING;

          case STRING_TYPE_BYTES:
            return IOP_T_DATA;

          case STRING_TYPE_XML:
            return IOP_T_XML;
        }
      }
      IOP_UNION_CASE_V(iop__type, iop_type, v) {
        return IOP_T_VOID;
      }
      IOP_UNION_CASE_V(iop__type, iop_type, type_name) {
        /* This case should be handled at higher level. */
        e_panic("should not happen");
      }
      IOP_UNION_CASE_V(iop__type, iop_type, array) {
        /* This case should be handled at higher level. */
        e_panic("should not happen");
      }
      IOP_UNION_DEFAULT() {
        e_panic("unhandled type %*pU",
                IOP_UNION_FMT_ARG(iop__type, iop_type));
      }
    }

    return 0;
}

static int
iopc_field_set_typename(iopc_field_t *nonnull f, lstr_t typename,
                        sb_t *nonnull err)
{
    f->kind = iop_get_type(typename);

    if (f->kind == IOP_T_STRUCT) {
        if (lstr_contains(typename, LSTR("."))) {
            /* TODO Could parse and check that the type name looks like a
             * proper type name. */
            const iop_obj_t *obj;

            obj = iop_get_obj(typename);
            if (obj) {
                switch (obj->type) {
                  case IOP_OBJ_TYPE_PKG:
                    /* Not expected to happen if we properly check the name.
                     */
                    sb_sets(err, "is a package name");
                    return -1;

                  case IOP_OBJ_TYPE_ST:
                    f->external_st = obj->desc.st;
                    f->kind = obj->desc.st->is_union ? IOP_T_UNION
                                                     : IOP_T_STRUCT;
                    break;

                  case IOP_OBJ_TYPE_ENUM:
                    f->external_en = obj->desc.en;
                    f->kind = IOP_T_ENUM;
                    break;
                }

                f->has_external_type = true;
            }
        } else {
            if (iopc_check_type_name(typename, err) < 0) {
                sb_prepends(err, "invalid type name: ");
                return -1;
            }
        }
    }
    f->type_name = p_dupz(typename.s, typename.len);
    return 0;
}

static int
iopc_field_set_type(iopc_field_t *nonnull f,
                    const iop__type__t *nonnull type, sb_t *nonnull err)
{
    if_assign (array_type, IOP_UNION_GET(iop__type, type, array)) {
        type = *array_type;

        if (IOP_UNION_GET(iop__type, type, array)) {
            sb_setf(err, "multi-dimension arrays are not supported");
            return -1;
        }

        f->repeat = IOP_R_REPEATED;
    }
    if_assign (typename, IOP_UNION_GET(iop__type, type, type_name)) {
        if (iopc_field_set_typename(f, *typename, err) < 0) {
            sb_prependf(err, "type name `%pL': ", typename);
            return -1;
        }
    } else {
        f->kind = iop_type_from_iop(type);
    }

    RETHROW(iopc_check_field_type(f, err));

    return 0;
}

static void
iopc_field_set_defval(iopc_field_t *f, const iop__value__t *defval)
{
    IOP_UNION_SWITCH(defval) {
      IOP_UNION_CASE(iop__value, defval, i, i) {
        f->defval.u64 = i;
        f->defval_is_signed = (i < 0);
        f->defval_type = IOPC_DEFVAL_INTEGER;
      }
      IOP_UNION_CASE(iop__value, defval, u, u) {
        f->defval.u64 = u;
        f->defval_type = IOPC_DEFVAL_INTEGER;
      }
      IOP_UNION_CASE(iop__value, defval, d, d) {
        f->defval.d = d;
        f->defval_type = IOPC_DEFVAL_DOUBLE;
      }
      IOP_UNION_CASE(iop__value, defval, s, s) {
        f->defval.ptr = p_dupz(s.s, s.len);
        f->defval_type = IOPC_DEFVAL_STRING;
      }
      IOP_UNION_CASE(iop__value, defval, b, b) {
        f->defval.u64 = b;
        f->defval_type = IOPC_DEFVAL_INTEGER;
      }
    }
}

static void
iopc_field_set_opt_info(iopc_field_t *nonnull f,
                        const iop__opt_info__t *nullable opt_info)
{
    if (!opt_info) {
        f->repeat = IOP_R_REQUIRED;
    } else
    if (opt_info->def_val) {
        f->repeat = IOP_R_DEFVAL;
        iopc_field_set_defval(f, opt_info->def_val);
    } else {
        f->repeat = IOP_R_OPTIONAL;
    }
}

static iopc_field_t *
iopc_field_load(const iop__field__t *nonnull field_desc,
                const qv_t(iopc_field) *fields, sb_t *nonnull err)
{
    iopc_field_t *f = NULL;

    if (iopc_check_name(field_desc->name, NULL, err) < 0) {
        goto error;
    }
    if (!islower(field_desc->name.s[0])) {
        sb_sets(err, "first field name character should be lowercase");
        goto error;
    }

    f = iopc_field_new();
    f->name = p_dupz(field_desc->name.s, field_desc->name.len);
    f->pos = fields->len;

    if (OPT_ISSET(field_desc->tag)) {
        f->tag = OPT_VAL(field_desc->tag);
    } else {
        f->tag = fields->len ? (*tab_last(fields))->tag + 1 : 1;
    }
    if (iopc_check_tag_value(f->tag, err) < 0) {
        goto error;
    }
    tab_for_each_entry(other_field, fields) {
        if (strequal(other_field->name, f->name)) {
            sb_sets(err, "name already used by another field");
            goto error;
        }
        if (other_field->tag == f->tag) {
            sb_setf(err, "tag `%d' is already used by field `%s'", f->tag,
                    other_field->name);
            goto error;
        }
    }

    if (iopc_field_set_type(f, &field_desc->type, err) < 0) {
        goto error;
    }
    if (f->repeat == IOP_R_REPEATED) {
        if (field_desc->optional) {
            sb_setf(err, "repeated field cannot be optional "
                    "or have a default value");
            goto error;
        }
    } else {
        iopc_field_set_opt_info(f, field_desc->optional);
    }

    if (field_desc->is_reference) {
        if (f->repeat == IOP_R_OPTIONAL) {
            sb_setf(err, "optional references are not supported");
            goto error;
        }
        if (f->repeat == IOP_R_REPEATED) {
            sb_setf(err, "arrays of references are not supported");
            goto error;
        }
        f->is_ref = true;
    }

    return f;

  error:
    sb_prependf(err, "field `%pL': ", &field_desc->name);
    iopc_field_delete(&f);
    return NULL;
}

static void
iop_structure_get_type_and_fields(const iop__structure__t *desc,
                                  iopc_struct_type_t *type,
                                  iop__field__array_t *fields)
{
    IOP_OBJ_EXACT_SWITCH(desc) {
      IOP_OBJ_CASE_CONST(iop__struct, desc, st) {
        *fields = st->fields;
        *type = STRUCT_TYPE_STRUCT;
      }

      IOP_OBJ_CASE_CONST(iop__union, desc, un) {
        *fields = un->fields;
        *type = STRUCT_TYPE_UNION;
      }

      IOP_OBJ_EXACT_DEFAULT() {
        assert (false);
      }
    }
}

static iopc_struct_t *
iopc_struct_load(const iop__structure__t *nonnull st_desc,
                 sb_t *nonnull err)
{
    iopc_struct_t *st;
    iop__field__array_t fields = IOP_ARRAY_EMPTY;

    st = iopc_struct_new();
    st->name = p_dupz(st_desc->name.s, st_desc->name.len);
    iop_structure_get_type_and_fields(st_desc, &st->type, &fields);

    tab_for_each_ptr(field_desc, &fields) {
        iopc_field_t *f;

        if (!(f = iopc_field_load(field_desc, &st->fields, err))) {
            iopc_struct_delete(&st);
            return NULL;
        }

        qv_append(&st->fields, f);
    }

    return st;
}

/* }}} */
/* {{{ IOP enum */

static iopc_enum_t *iopc_enum_load(const iop__enum__t *en_desc, sb_t *err)
{
    t_scope;
    iopc_enum_t *en;
    int next_val = 0;
    qh_t(lstr) keys;
    qh_t(u32) values;

    t_qh_init(lstr, &keys, en_desc->values.len);
    t_qh_init(u32, &values, en_desc->values.len);
    tab_for_each_ptr(enum_val, &en_desc->values) {
        int32_t val = OPT_DEFVAL(enum_val->val, next_val);

        if (qh_add(u32, &values, val) < 0) {
            sb_setf(err, "key `%pL': the value `%d' is already used",
                    &enum_val->name, val);
            return NULL;
        }
        if (qh_add(lstr, &keys, &enum_val->name) < 0) {
            sb_setf(err, "the key `%pL' is duplicated", &enum_val->name);
            return NULL;
        }
        next_val = val + 1;
    }

    en = iopc_enum_new();
    en->name = p_dupz(en_desc->name.s, en_desc->name.len);
    next_val = 0;
    tab_for_each_ptr(enum_val, &en_desc->values) {
        iopc_enum_field_t *field = iopc_enum_field_new();

        field->name = p_dupz(enum_val->name.s, enum_val->name.len);
        field->value = OPT_DEFVAL(enum_val->val, next_val);
        next_val = field->value + 1;

        qv_append(&en->values, field);
    }

    return en;
}

/* }}} */
/* {{{ IOP package */

static iopc_pkg_t *
iopc_pkg_load_from_iop(const iop__package__t *nonnull pkg_desc,
                       sb_t *nonnull err)
{
    t_scope;
    iopc_pkg_t *pkg = iopc_pkg_new();
    qh_t(lstr) things;

    pkg->file = p_strdup("<none>");
    pkg->name = parse_path(pkg_desc->name, false, err);
    if (!pkg->name) {
        sb_prepends(err, "invalid name: ");
        goto error;
    }
    /* XXX Nothing to do for attribute "base" (related to package file path).
     */

    t_qh_init(lstr, &things, pkg_desc->elems.len);
    tab_for_each_entry(elem, &pkg_desc->elems) {
        if (iopc_check_type_name(elem->name, err) < 0) {
            goto error;
        }

        if (qh_add(lstr, &things, &elem->name) < 0) {
            sb_setf(err, "already got a thing named `%pL'", &elem->name);
            goto error;
        }

        IOP_OBJ_SWITCH(iop__package_elem, elem) {
          IOP_OBJ_CASE(iop__structure, elem, st_desc) {
              iopc_struct_t *st;

              if (!(st = iopc_struct_load(st_desc, err))) {
                  sb_prependf(err, "cannot load `%pL': ", &elem->name);
                  goto error;
              }

              qv_append(&pkg->structs, st);
          }

          IOP_OBJ_CASE(iop__enum, elem, en_desc) {
              iopc_enum_t *en;

              if (!(en = iopc_enum_load(en_desc, err))) {
                  sb_prependf(err, "cannot load enum `%pL': ", &elem->name);
                  goto error;
              }

              qv_append(&pkg->enums, en);
          }

          /* TODO Classes */
          /* TODO Typedefs */
          /* TODO Interfaces */
          /* TODO Modules */
          /* TODO SNMP stuff */

          IOP_OBJ_DEFAULT(iop__package_elem) {
              sb_setf(err,
                      "package elements of type `%pL' are not supported yet",
                      &elem->__vptr->fullname);
              goto error;
          }
        }
    }

    return pkg;

  error:
    iopc_pkg_delete(&pkg);
    return NULL;
}

/* }}} */
/* }}} */
/* {{{ IOP² API */

iop_pkg_t *mp_iopsq_build_pkg(mem_pool_t *nonnull mp,
                              const iop__package__t *nonnull pkg_desc,
                              sb_t *nonnull err)
{
    iop_pkg_t *pkg = NULL;
    iopc_pkg_t *iopc_pkg;

    if (!expect(mp->mem_pool & MEM_BY_FRAME)) {
        sb_sets(err, "incompatible memory pool type");
        return NULL;
    }

    if (!(iopc_pkg = iopc_pkg_load_from_iop(pkg_desc, err))) {
        sb_prependf(err, "invalid package `%pL': ", &pkg_desc->name);
        return NULL;
    }

    log_start_buffering_filter(false, LOG_ERR);
    if (iopc_resolve(iopc_pkg) < 0 || iopc_resolve_second_pass(iopc_pkg) < 0)
    {
        const qv_t(log_buffer) *logs = log_stop_buffering();

        sb_sets(err, "failed to resolve the package");
        tab_for_each_ptr(log, logs) {
            sb_addf(err, ": %pL", &log->msg);
        }
        goto end;
    }
    IGNORE(log_stop_buffering());

    pkg = mp_iopc_pkg_to_desc(mp, iopc_pkg, err);
    if (!pkg) {
        sb_prependf(err, "failed to generate package `%s': ",
                    pretty_path_dot(iopc_pkg->name));
        goto end;
    }

  end:
    iopc_pkg_delete(&iopc_pkg);
    return pkg;
}

iop_pkg_t *
mp_iopsq_build_mono_element_pkg(mem_pool_t *nonnull mp,
                                const iop__package_elem__t *nonnull elem,
                                sb_t *nonnull err)
{
    iop__package__t pkg_desc;
    iop__package_elem__t *_elem = unconst_cast(iop__package_elem__t, elem);

    iop_init(iop__package, &pkg_desc);
    pkg_desc.name = LSTR("user_package");
    pkg_desc.elems = IOP_TYPED_ARRAY(iop__package_elem, &_elem, 1);

    return mp_iopsq_build_pkg(mp, &pkg_desc, err);
}

const iop_struct_t *
mp_iopsq_build_struct(mem_pool_t *nonnull mp,
                      const iop__structure__t *nonnull iop_desc,
                      sb_t *nonnull err)
{
    iop_pkg_t *pkg;

    pkg = RETHROW_P(mp_iopsq_build_mono_element_pkg(mp, &iop_desc->super,
                                                    err));

    return pkg->structs[0];
}

/* }}} */
