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

#include <dlfcn.h>
#include "iop.h"

static ALWAYS_INLINE
void iopdso_register_struct(iop_dso_t *dso, iop_struct_t const *st)
{
    qm_add(iop_struct, &dso->struct_h, &st->fullname, st);
}

static lstr_t iop_pkgname_from_fullname(lstr_t fullname)
{
    const void *p;
    pstream_t pkgname;
    pstream_t ps = ps_initlstr(&fullname);

    p = memrchr(ps.s, '.', ps_len(&ps));
    if (!p) {
        /* XXX: This case case may happen with the special 'Void' package */
        return LSTR_NULL_V;
    }
    pkgname = __ps_get_ps_upto(&ps, p);
    return LSTR_PS_V(&pkgname);
}

static const iop_struct_t *
iop_get_struct(const iop_pkg_t *pkg, lstr_t fullname)
{
    for (const iop_struct_t * const *st = pkg->structs; *st; st++) {
        if (lstr_equal2(fullname, (*st)->fullname)) {
            return *st;
        }
    }
    for (const iop_iface_t * const *iface = pkg->ifaces; *iface; iface++) {
        for (int i = 0; i < (*iface)->funs_len; i++) {
            const iop_rpc_t *rpc = &(*iface)->funs[i];

            if (lstr_equal2(fullname, rpc->args->fullname)) {
                return rpc->args;
            }
            if (lstr_equal2(fullname, rpc->result->fullname)) {
                return rpc->result;
            }
            if (lstr_equal2(fullname, rpc->exn->fullname)) {
                return rpc->exn;
            }
        }
    }

    return NULL;
}

static void iopdso_fix_struct_ref(const iop_struct_t **st)
{
    const iop_struct_t *fix;
    lstr_t pkgname = iop_pkgname_from_fullname((*st)->fullname);
    const iop_pkg_t *pkg;

    pkg = iop_get_pkg(pkgname);
    if (!pkg) {
        pkgname = iop_pkgname_from_fullname(pkgname);
        pkg = iop_get_pkg(pkgname);
        if (!pkg) {
            e_trace(3, "no package `%*pM`", LSTR_FMT_ARG(pkgname));
            return;
        }
    }
    fix = iop_get_struct(pkg, (*st)->fullname);
    if (!fix) {
        e_error("IOP DSO: did not find struct %s in memory",
                (*st)->fullname.s);
        return;
    }
    if (fix != *st) {
        e_trace(3, "fixup `%*pM`, %p => %p", LSTR_FMT_ARG((*st)->fullname),
                *st, fix);
        *st = fix;
    }
}

static void iopdso_fix_class_parent(const iop_struct_t *desc)
{
    iop_class_attrs_t *class_attrs;

    if (!iop_struct_is_class(desc)) {
        return;
    }
    class_attrs = (iop_class_attrs_t *)desc->class_attrs;
    while (class_attrs->parent) {
        iopdso_fix_struct_ref(&class_attrs->parent);
        desc = class_attrs->parent;
        class_attrs = (iop_class_attrs_t *)desc->class_attrs;
    }
}

static void iopdso_fix_pkg(const iop_pkg_t *pkg)
{
    for (const iop_struct_t *const *it = pkg->structs; *it; it++) {
        const iop_struct_t *desc = *it;

        iopdso_fix_struct_ref(&desc);
        iopdso_fix_class_parent(desc);

        for (int i = 0; i < desc->fields_len; i++) {
            iop_field_t *f = (iop_field_t *)&desc->fields[i];

            if (f->type == IOP_T_STRUCT || f->type == IOP_T_UNION) {
                iopdso_fix_struct_ref(&f->u1.st_desc);
            }
        }
    }
    for (const iop_iface_t *const *it = pkg->ifaces; *it; it++) {
        for (int i = 0; i < (*it)->funs_len; i++) {
            iop_rpc_t *rpc = (iop_rpc_t *)&(*it)->funs[i];

            iopdso_fix_struct_ref(&rpc->args);
            iopdso_fix_struct_ref(&rpc->result);
            iopdso_fix_struct_ref(&rpc->exn);
        }
    }
}

static void iopdso_register_pkg(iop_dso_t *dso, iop_pkg_t const *pkg,
                                bool use_external_packages)
{
    if (qm_add(iop_pkg, &dso->pkg_h, &pkg->name, pkg) < 0) {
        return;
    }
    if (use_external_packages) {
        e_trace(1, "fixup package `%*pM`", LSTR_FMT_ARG(pkg->name));
        iopdso_fix_pkg(pkg);
    }
    iop_register_packages(&pkg, 1);
    for (const iop_enum_t *const *it = pkg->enums; *it; it++) {
        qm_add(iop_enum, &dso->enum_h, &(*it)->fullname, *it);
    }
    for (const iop_struct_t *const *it = pkg->structs; *it; it++) {
        iopdso_register_struct(dso, *it);
    }
    for (const iop_iface_t *const *it = pkg->ifaces; *it; it++) {
        qm_add(iop_iface, &dso->iface_h, &(*it)->fullname, *it);
        for (int i = 0; i < (*it)->funs_len; i++) {
            const iop_rpc_t *rpc = &(*it)->funs[i];

            iopdso_register_struct(dso, rpc->args);
            iopdso_register_struct(dso, rpc->result);
            iopdso_register_struct(dso, rpc->exn);
        }
    }
    for (const iop_mod_t *const *it = pkg->mods; *it; it++) {
        qm_add(iop_mod, &dso->mod_h, &(*it)->fullname, *it);
    }
    for (const iop_pkg_t *const *it = pkg->deps; *it; it++) {
        if (use_external_packages && iop_get_pkg((*it)->name)) {
            continue;
        }
        iopdso_register_pkg(dso, *it, use_external_packages);
    }
}

static iop_dso_t *iop_dso_init(iop_dso_t *dso)
{
    p_clear(dso, 1);
    qm_init_cached(iop_pkg,    &dso->pkg_h);
    qm_init_cached(iop_enum,   &dso->enum_h);
    qm_init_cached(iop_struct, &dso->struct_h);
    qm_init_cached(iop_iface,  &dso->iface_h);
    qm_init_cached(iop_mod,    &dso->mod_h);

    return dso;
}
static void iop_dso_wipe(iop_dso_t *dso)
{
    qm_wipe(iop_pkg,    &dso->pkg_h);
    qm_wipe(iop_enum,   &dso->enum_h);
    qm_wipe(iop_struct, &dso->struct_h);
    qm_wipe(iop_iface,  &dso->iface_h);
    qm_wipe(iop_mod,    &dso->mod_h);
    lstr_wipe(&dso->path);
    if (dso->handle) {
        dlclose(dso->handle);
    }
}
REFCNT_NEW(iop_dso_t, iop_dso);
REFCNT_DELETE(iop_dso_t, iop_dso);

iop_dso_t *iop_dso_open(const char *path)
{
    iop_pkg_t       **pkgp;
    bool              use_external_packages;
    void             *handle;
    iop_dso_vt_t     *dso_vt;
    iop_dso_t        *dso;

#ifndef RTLD_DEEPBIND
# define RTLD_DEEPBIND  0
#endif
    handle = dlopen(path, RTLD_LAZY | RTLD_LOCAL | RTLD_DEEPBIND);
    if (handle == NULL) {
        e_error("IOP DSO: unable to dlopen(%s): %s", path, dlerror());
        return NULL;
    }

    dso_vt = dlsym(handle, "iop_vtable");
    if (dso_vt == NULL || dso_vt->vt_size == 0) {
        e_warning("IOP DSO: unable to find valid IOP vtable in plugin (%s), "
                  "no error management allowed: %s", path, dlerror());
    } else {
        dso_vt->iop_set_verr = &iop_set_verr;
    }

    pkgp = dlsym(handle, "iop_packages");
    if (pkgp == NULL) {
        e_error("IOP DSO: unable to find IOP packages in plugin (%s): %s",
                path, dlerror());
        dlclose(handle);
        return NULL;
    }

    use_external_packages = !!dlsym(handle, "iop_use_external_packages");

    dso = iop_dso_new();
    dso->path = lstr_dup(LSTR_OPT(path));
    dso->handle = handle;
    while (*pkgp) {
        iopdso_register_pkg(dso, *pkgp++, use_external_packages);
    }
    return dso;
}

void iop_dso_close(iop_dso_t **dsop)
{
    const iop_dso_t *dso = *dsop;

    if (dso) {
        qm_for_each_pos(iop_pkg, pos, &dso->pkg_h) {
            iop_unregister_packages(&dso->pkg_h.values[pos], 1);
        }
        iop_dso_delete(dsop);
    }
}

static iop_rpc_t const *
find_rpc_in_iface(iop_iface_t const *iface, lstr_t fname)
{
    for (int i = 0; i < iface->funs_len; i++) {
        iop_rpc_t const *rpc = &iface->funs[i];

        if (lstr_equal2(rpc->name, fname)) {
            return rpc;
        }
    }
    return NULL;
}

static iop_rpc_t const *
find_rpc_in_mod(iop_mod_t const *mod, lstr_t iface, lstr_t fname)
{
    for (int i = 0; i < mod->ifaces_len; i++) {
        const iop_iface_alias_t *alias = &mod->ifaces[i];

        if (lstr_equal2(alias->name, iface)) {
            return find_rpc_in_iface(alias->iface, fname);
        }
    }
    return NULL;
}

iop_struct_t const *iop_dso_find_type(iop_dso_t const *dso, lstr_t name)
{
    const char *s;
    int what, pos;
    lstr_t fname;
    iop_rpc_t const *rpc;

    pos = qm_find_safe(iop_struct, &dso->struct_h, &name);
    if (pos >= 0) {
        return dso->struct_h.values[pos];
    }

    if (lstr_endswith(name, LSTR("Args"))) {
        name.len -= strlen("Args");
        what = 0;
    } else
    if (lstr_endswith(name, LSTR("Res"))) {
        name.len -= strlen("Res");
        what = 1;
    } else
    if (lstr_endswith(name, LSTR("Exn"))) {
        name.len -= strlen("Exn");
        what = 2;
    } else {
        return NULL;
    }

    /* name is some.path.to.pkg.{Module.iface.rpc|Iface.rpc} */
    s        = RETHROW_P(memrchr(name.s, '.', name.len));
    fname    = LSTR_INIT_V(s + 1, name.s + name.len - s - 1);
    name.len = s - name.s;

    s = RETHROW_P(memrchr(name.s, '.', name.len));
    if ('A' <= s[1] && s[1] <= 'Z') {
        /* name is an Iface */
        pos = RETHROW_NP(qm_find_safe(iop_iface, &dso->iface_h, &name));
        rpc = RETHROW_P(find_rpc_in_iface(dso->iface_h.values[pos], fname));
    } else {
        /* name is a Module.iface */
        lstr_t mod = LSTR_INIT(name.s, s - name.s);
        lstr_t ifn = LSTR_INIT(s + 1, name.s + name.len - (s + 1));

        pos = RETHROW_NP(qm_find_safe(iop_mod, &dso->mod_h, &mod));
        rpc = RETHROW_P(find_rpc_in_mod(dso->mod_h.values[pos], ifn, fname));
    }
    switch (what) {
      case 0:  return rpc->args;
      case 1:  return rpc->result;
      default: return rpc->exn;
    }
}

iop_enum_t const *iop_dso_find_enum(iop_dso_t const *dso, lstr_t name)
{
    return qm_get_def_safe(iop_enum, &dso->enum_h, &name, NULL);
}

const void *const *iop_dso_get_ressources(const iop_dso_t *dso, lstr_t category)
{
    t_scope;
    lstr_t name = t_lstr_cat(LSTR("iop_dso_ressources_"), category);

    return dlsym(dso->handle, name.s);
}
