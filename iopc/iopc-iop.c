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

static iopc_path_t *parse_path(lstr_t name, bool is_type)
{
    pstream_t ps = ps_initlstr(&name);
    iopc_path_t *path = iopc_path_new();

    for (;;) {
        pstream_t bit;

        bit = ps_get_span(&ps, &ctype_isalpha);
        /* TODO error in case of empty "bit". */
        qv_append(&path->bits, p_dupz(bit.s, ps_len(&bit)));
        if (ps_done(&ps)) {
            break;
        }
        if (__ps_getc(&ps) != '.') {
            /* TODO error */
        }
    }

    /* TODO fill loc/pretty_dot/pretty_slash/... */

    return path;
}

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
        /* Set the type as "STRUCT", the typer will sort it out. */
        f->kind = IOP_T_STRUCT;

        if (lstr_contains(*type_name, LSTR("."))) {
            sb_setf(err, "cannot use type `%pL': "
                    "external type names are not supported yet", type_name);
            return -1;
        }
        /* TODO Support and fill paths. */
        if (iopc_check_type_name(*type_name, err) < 0) {
            sb_prependf(err, "invalid type name: `%pL': ", type_name);
            return -1;
        }
        f->type_name = p_dupz(type_name->s, type_name->len);
    } else {
        f->kind = iop_type_from_iop(type);
    }

    /* TODO import checks from "parse_field_type" */

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
    iopc_field_t *f = iopc_field_new();

    f->name = p_dupz(field_desc->name.s, field_desc->name.len);
    if (iopc_check_name(f->name, NULL, err) < 0) {
        goto error;
    }

    f->tag = OPT_DEFVAL(field_desc->tag, *next_tag);
    if (iopc_check_tag_value(f->tag, err) < 0) {
        goto error;
    }
    *next_tag = f->tag + 1;
    /* TODO Check tag unicity ? */

    if (iopc_field_set_type(f, &field_desc->type, err) < 0) {
        goto error;
    }
    if (f->repeat == IOP_R_REPEATED && field_desc->optional) {
        sb_setf(err, "repeated field cannot be optional "
                "or have a default value");
        goto error;
    }

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

static iopc_struct_t *iopc_struct_load(const iop__struct__t *st_desc,
                                       sb_t *err)
{
    iopc_struct_t *st;
    int next_tag = 1;

    RETHROW_NP(iopc_check_upper(st_desc->name, err));

    st = iopc_struct_new();
    st->name = p_dupz(st_desc->name.s, st_desc->name.len);

    tab_for_each_ptr(field_desc, &st_desc->fields) {
        iopc_field_t *f;

        if (!(f = iopc_field_load(field_desc, &next_tag, err))) {
            iopc_struct_delete(&st);
            return NULL;
        }

        qv_append(&st->fields, f);
    }

    return st;
}

static iopc_pkg_t *
iopc_pkg_load_from_iop(const iop__package__t *pkg_desc, sb_t *err)
{
    t_scope;
    iopc_pkg_t *pkg = iopc_pkg_new();
    qh_t(lstr) things;

    pkg->file = p_strdup("<none>");
    pkg->name = parse_path(pkg_desc->name, false);
    /* XXX Nothing to do for attribute "base" (related to package file path).
     */

    t_qh_init(lstr, &things, pkg_desc->elems.len);
    tab_for_each_entry(elem, &pkg_desc->elems) {
        if (qh_add(lstr, &things, &elem->name) < 0) {
            sb_setf(err, "already got a thing named `%pL'", &elem->name);
            goto error;
        }

        IOP_OBJ_EXACT_SWITCH(elem) {
          IOP_OBJ_CASE(iop__struct, elem, st_desc) {
              iopc_struct_t *st;

              if (!(st = iopc_struct_load(st_desc, err))) {
                  goto error;
              }

              qv_append(&pkg->structs, st);
          }
          /* TODO Handle other kinds. */
        }
    }

    return pkg;

  error:
    iopc_pkg_delete(&pkg);
    return NULL;
}

/* }}} */
/* {{{ iopc_pkg_t to iop_pkg_t */

static void mp_iopc_field_to_desc(mem_pool_t *mp, const iopc_field_t *f,
                                  const iopc_struct_t *st, uint16_t *offset,
                                  iop_field_t *fdesc)
{
    *offset = ROUND_UP(*offset, f->align);

    *fdesc = (iop_field_t){
        .name = mp_lstr_dups(mp, f->name, -1),
        .tag = f->tag,
        .tag_len = iopc_tag_len(f->tag),
        .repeat = f->repeat,
        .type = f->kind,
        .data_offs = *offset,
        .flags = iopc_field_build_flags(f, st, NULL), /* TODO attrs */
        /* TODO default value */
        .size = f->size, /* TODO handle pointed values */
        /* TODO enum/st */
    };

    if (st->type != STRUCT_TYPE_UNION) {
        *offset += f->size;
    }
}

static iop_struct_t *mp_iopc_struct_to_desc(mem_pool_t *mp,
                                            const iopc_struct_t *st,
                                            const iopc_pkg_t *pkg)
{
    iop_struct_t *st_desc;
    iop_field_t *fields;
    iop_field_t *wf;
    uint16_t offset = 0;
    iop_array_i32_t ranges;

    /* TODO handle the case of offsets for union and classes */
    if (mp != t_pool()) {
        t_scope;

        ranges = t_iopc_struct_build_ranges(st);
        ranges.tab = mp_dup(mp, ranges.tab, ranges.len);
    } else {
        ranges = t_iopc_struct_build_ranges(st);
    }

    fields = mp_new_raw(mp, iop_field_t, st->fields.len);
    wf = fields;
    tab_for_each_entry(f, &st->fields) {
        mp_iopc_field_to_desc(mp, f, st, &offset, wf++);
    }

    {
        iop_struct_t _st_desc = {
            .fullname = mp_lstr_fmt(mp, "%s.%s", pretty_path_dot(pkg->name),
                                    st->name),
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
    };

    return st_desc;
}

/* TODO maybe we can avoid the mem_pool and keep all the references somehow
 */
static iop_pkg_t *mp_iopc_pkg_to_desc(mem_pool_t *mp, iopc_pkg_t *pkg)
{
    iop_pkg_t *pkg_desc;
    const iop_struct_t **structs;
    const iop_struct_t **wst;

    structs = mp_new_raw(mp, const iop_struct_t *, pkg->structs.len + 1);
    wst = structs;
    tab_for_each_entry(st, &pkg->structs) {
        *wst++ = mp_iopc_struct_to_desc(mp, st, pkg);
    }
    *wst = NULL;

    {
        iop_pkg_t _pkg_desc = {
            .name = mp_lstr_dups(mp, pretty_path_dot(pkg->name), -1),
            /* TODO Enums */
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
        sb_prepends(err, "invalid package description: ");
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
