/**************************************************************************/
/*                                                                        */
/*  Copyright (C) 2004-2017 INTERSEC SA                                   */
/*                                                                        */
/*  Should you receive a copy of this source code, you must check you     */
/*  have a proper, written authorization of INTERSEC to hold it. If you   */
/*  don't have such an authorization, you must DELETE all source code     */
/*  files in your possession, and inform INTERSEC of the fact you obtain  */
/*  these files. Should you not comply to these terms, you can be         */
/*  prosecuted in the extent permitted by applicable law.                 */
/*                                                                        */
/**************************************************************************/

#include "iopc-iop.h"
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
        }
        if (ps_done(&bit)) {
            sb_setf(err, "empty package or sub-package name");
            iopc_path_delete(&path);

            return NULL;
        }
        qv_append(&path->bits, p_dupz(bit.s, ps_len(&bit)));
    }

    return path;
}

static lstr_t mp_build_fullname(mem_pool_t *mp, const iopc_pkg_t *pkg,
                                const char *name)
{
    return mp_lstr_fmt(mp, "%s.%s", pretty_path_dot(pkg->name), name);
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
iopc_field_set_type(iopc_field_t *f, const iop__type__t *type, sb_t *err)
{
    if_assign (array_type, IOP_UNION_GET(iop__type, type, array)) {
        type = *array_type;

        if (IOP_UNION_GET(iop__type, type, array)) {
            sb_setf(err, "multi-dimension arrays are not supported");
            return -1;
        }

        f->repeat = IOP_R_REPEATED;
    }
    if_assign (type_name, IOP_UNION_GET(iop__type, type, type_name)) {
        f->kind = iop_get_type(*type_name);

        if (f->kind == IOP_T_STRUCT) {
            if (lstr_contains(*type_name, LSTR("."))) {
                sb_setf(err, "cannot use type `%pL': "
                        "external type names are not supported yet",
                        type_name);
                return -1;
            }
            /* TODO Support and fill paths. */
            if (iopc_check_type_name(*type_name, err) < 0) {
                sb_prependf(err, "invalid type name: `%pL': ", type_name);
                return -1;
            }
        }
        f->type_name = p_dupz(type_name->s, type_name->len);
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

static int
iopc_field_set_opt_info(iopc_field_t *nonnull f,
                        const iop__opt_info__t *nullable opt_info,
                        sb_t *err)
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

    return 0;
}

static iopc_field_t *iopc_field_load(const iop__field__t *field_desc,
                                     int *next_tag, sb_t *err)
{
    iopc_field_t *f;

    RETHROW_NP(iopc_check_name(field_desc->name, NULL, err));

    f = iopc_field_new();
    f->name = p_dupz(field_desc->name.s, field_desc->name.len);
    f->tag = OPT_DEFVAL(field_desc->tag, *next_tag);
    if (iopc_check_tag_value(f->tag, err) < 0) {
        goto error;
    }
    *next_tag = f->tag + 1;
    /* TODO Check tag unicity ? */

    if (iopc_field_set_type(f, &field_desc->type, err) < 0) {
        goto error;
    }
    if (f->repeat == IOP_R_REPEATED) {
        if (field_desc->optional) {
            sb_setf(err, "repeated field cannot be optional "
                    "or have a default value");
            goto error;
        }
    } else
    if (iopc_field_set_opt_info(f, field_desc->optional, err) < 0) {
        goto error;
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

static iopc_struct_t *iopc_struct_load(const iop__structure__t *st_desc,
                                       sb_t *err)
{
    iopc_struct_t *st;
    int next_tag = 1;
    iop__field__array_t fields;

    st = iopc_struct_new();
    st->name = p_dupz(st_desc->name.s, st_desc->name.len);
    iop_structure_get_type_and_fields(st_desc, &st->type, &fields);

    tab_for_each_ptr(field_desc, &fields) {
        iopc_field_t *f;

        if (!(f = iopc_field_load(field_desc, &next_tag, err))) {
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
            sb_setf(err, "value `%pL' is already used", &enum_val->name);
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
iopc_pkg_load_from_iop(const iop__package__t *pkg_desc, sb_t *err)
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
        RETHROW_NP(iopc_check_name(elem->name, NULL, err));
        RETHROW_NP(iopc_check_upper(elem->name, err));

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
/* {{{ iopc_pkg_t to iop_pkg_t */
/* {{{ IOP struct/union */

static void mp_iopc_field_to_desc(mem_pool_t *mp, const iopc_field_t *f,
                                  const iopc_struct_t *st, uint16_t *offset,
                                  iop_field_t *fdesc)
{
    uint16_t size;

    *offset = ROUND_UP(*offset, f->align);

    /* The "size" is not the same between iop_field_t and iopc_field_t:
     *
     *    iopc_field_t: size of the field (as returned by fieldtypeof)
     *    iop_field_t: size of the underlying type except for class
     */
    if (iop_type_is_scalar(f->kind)) {
        iop_scalar_type_get_size_and_alignment(f->kind, &size, NULL);
    } else {
        if (f->struct_def->type == STRUCT_TYPE_CLASS) {
            size = sizeof(void *);
        } else {
            size = f->struct_def->size;
        }
    }

    *fdesc = (iop_field_t){
        .name = mp_lstr_dups(mp, f->name, -1),
        .tag = f->tag,
        .tag_len = iopc_tag_len(f->tag),
        .repeat = f->repeat,
        .type = f->kind,
        .data_offs = *offset,
        .flags = iopc_field_build_flags(f, st, NULL), /* TODO attrs */
        /* TODO default value */
        .size = size,
    };

    if (!iop_type_is_scalar(fdesc->type)) {
        STATIC_ASSERT(offsetof(iopc_field_t, struct_def) ==
                      offsetof(iopc_field_t, union_def));

        /* Should be set in "mp_iopc_struct_to_desc". */
        assert (f->struct_def->desc);

        /* TODO We're going to have to load dependancies first if we want
         * that to work in multi-package contexts. */
        fdesc->u1.st_desc = f->struct_def->desc;
    } else
    if (fdesc->type == IOP_T_ENUM) {
        /* Should be set in "mp_iopc_enum_to_desc". */
        assert (f->enum_def->desc);

        /* TODO Ditto. */
        fdesc->u1.en_desc = f->enum_def->desc;
    }

    if (st->type != STRUCT_TYPE_UNION) {
        *offset += f->size;
    }

}

static iop_struct_t *mp_iopc_struct_to_desc(mem_pool_t *mp,
                                            iopc_struct_t *st,
                                            const iopc_pkg_t *pkg)
{
    iop_struct_t *st_desc;
    iop_field_t *fields;
    uint16_t offset;
    iop_array_i32_t ranges;

    if (mp != t_pool()) {
        t_scope;

        ranges = t_iopc_struct_build_ranges(st);
        ranges.tab = mp_dup(mp, ranges.tab, ranges.len);
    } else {
        ranges = t_iopc_struct_build_ranges(st);
    }

    fields = mp_new_raw(mp, iop_field_t, st->fields.len);
    {
        iop_struct_t _st_desc = {
            .fullname = mp_build_fullname(mp, pkg, st->name),
            .fields = fields,
            .fields_len = st->fields.len,
            .ranges = ranges.tab,
            .ranges_len = ranges.len / 2,
            .size = st->size, /* TODO Check correctness */
            .flags = st->flags,
            .is_union = (st->type == STRUCT_TYPE_UNION),
            /* TODO st_attrs */
            /* TODO field_attrs */
            /* TODO class_attrs */
        };

        st_desc = mp_new_raw(mp, iop_struct_t, 1);
        /* XXX Work-around all the const fields in 'iop_struct_t'. */
        p_copy(st_desc, &_st_desc, 1);
        st->desc = st_desc;
    };

    if (st->type == STRUCT_TYPE_UNION) {
        offset = sizeof(uint16_t);
    } else {
        /* TODO handle the case of offsets for union */
        offset = 0;
    }

    tab_for_each_pos(pos, &st->fields) {
        mp_iopc_field_to_desc(mp, st->fields.tab[pos], st, &offset,
                              &fields[pos]);
    }

    if (st->type == STRUCT_TYPE_UNION) {
        /* The anonymous union field offset depends on its alignment, which
         * depends of the IOP union field with the biggest alignment. This
         * loop realigns all the fields offsets on the biggest offset.
         */
        tab_for_each_pos(pos, &st->fields) {
            fields[pos].data_offs = offset;
        }
    }

    return st_desc;
}

/* }}} */
/* {{{ IOP enum */

static iop_enum_t *mp_iopc_enum_to_desc(mem_pool_t *mp,
                                        iopc_enum_t *en,
                                        const iopc_pkg_t *pkg)
{
    iop_enum_t *en_desc;
    iop_array_i32_t ranges;
    lstr_t *names;
    int32_t *values;
    int pos = 0;

    if (mp != t_pool()) {
        t_scope;

        ranges = t_iopc_enum_build_ranges(en);
        ranges.tab = mp_dup(mp, ranges.tab, ranges.len);
    } else {
        ranges = t_iopc_enum_build_ranges(en);
    }

    names = mp_new_raw(mp, lstr_t, en->values.len);
    values = mp_new_raw(mp, int32_t, en->values.len);
    tab_for_each_entry(f, &en->values) {
        names[pos] = mp_lstr_dups(mp, f->name, -1);
        values[pos] = f->value;
        pos++;
    }

    {
        iop_enum_t _en_desc = {
            .name = mp_lstr_dup(mp, LSTR(en->name)),
            .fullname = mp_build_fullname(mp, pkg, en->name),
            .names = names,
            .values = values,
            .ranges = ranges.tab,
            .ranges_len = ranges.len / 2,
            .enum_len = en->values.len,
            /* TODO attrs */
            /* TODO aliases */
        };

        en_desc = mp_new_raw(mp, iop_enum_t, 1);
        p_copy(en_desc, &_en_desc, 1);
        en->desc = en_desc;
    }

    return en_desc;
}

/* }}} */

/* TODO maybe we can avoid the mem_pool and keep all the references somehow
 */
static iop_pkg_t *mp_iopc_pkg_to_desc(mem_pool_t *mp, iopc_pkg_t *pkg)
{
    iop_pkg_t *pkg_desc;
    const iop_struct_t **structs;
    const iop_struct_t **wst;
    const iop_enum_t **enums;
    const iop_enum_t **wen;

    enums = mp_new_raw(mp, const iop_enum_t *, pkg->enums.len + 1);
    wen = enums;
    tab_for_each_entry(en, &pkg->enums) {
        *wen++ = mp_iopc_enum_to_desc(mp, en, pkg);
    }
    *wen = NULL;

    structs = mp_new_raw(mp, const iop_struct_t *, pkg->structs.len + 1);
    wst = structs;
    tab_for_each_entry(st, &pkg->structs) {
        *wst++ = mp_iopc_struct_to_desc(mp, st, pkg);
    }
    *wst = NULL;

    {
        iop_pkg_t _pkg_desc = {
            .name = mp_lstr_dups(mp, pretty_path_dot(pkg->name), -1),
            .enums = enums,
            .structs = structs,
            /* TODO (later) ifaces, mods */
            /* TODO deps */
        };

        pkg_desc = mp_new_raw(mp, iop_pkg_t, 1);
        /* XXX Work-around all the const fields in 'iop_pkg_t'. */
        p_copy(pkg_desc, &_pkg_desc, 1);
    }

    return pkg_desc;
}

/* }}} */
/* {{{ IOP² API */

iop_pkg_t *mp_iop_pkg_from_desc(mem_pool_t *mp,
                                const iop__package__t *pkg_desc, sb_t *err)
{
    iop_pkg_t *pkg = NULL;
    iopc_pkg_t *iopc_pkg;

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

    tab_for_each_entry(st, &iopc_pkg->structs) {
        iopc_struct_optimize(st);
    }

    pkg = mp_iopc_pkg_to_desc(mp, iopc_pkg);

  end:
    iopc_pkg_delete(&iopc_pkg);
    return pkg;
}

/* }}} */
