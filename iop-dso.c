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

#include <dlfcn.h>
#include "iop.h"
#include "iop-priv.h"

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
        if (lstr_equal(fullname, (*st)->fullname)) {
            return *st;
        }
    }
    for (const iop_iface_t * const *iface = pkg->ifaces; *iface; iface++) {
        for (int i = 0; i < (*iface)->funs_len; i++) {
            const iop_rpc_t *rpc = &(*iface)->funs[i];

            if (lstr_equal(fullname, rpc->args->fullname)) {
                return rpc->args;
            }
            if (lstr_equal(fullname, rpc->result->fullname)) {
                return rpc->result;
            }
            if (lstr_equal(fullname, rpc->exn->fullname)) {
                return rpc->exn;
            }
        }
    }

    return NULL;
}

static void iopdso_fix_struct_ref(iop_dso_t *dso, const iop_struct_t **st)
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
        iop_dso_t *dep = iop_dso_get_from_pkg(pkg);

        e_trace(3, "fixup `%*pM`, %p => %p", LSTR_FMT_ARG((*st)->fullname),
                *st, fix);
        *st = fix;
        if (dep) {
            qh_add(ptr, &dso->depends_on, dep);
            qh_add(ptr, &dep->needed_by,  dso);
        }
    }
}

static void iopdso_fix_class_parent(iop_dso_t *dso, const iop_struct_t *desc)
{
    iop_class_attrs_t *class_attrs;

    if (!iop_struct_is_class(desc)) {
        return;
    }
    class_attrs = (iop_class_attrs_t *)desc->class_attrs;
    while (class_attrs->parent) {
        iopdso_fix_struct_ref(dso, &class_attrs->parent);
        desc = class_attrs->parent;
        class_attrs = (iop_class_attrs_t *)desc->class_attrs;
    }
}

static void iopdso_fix_pkg(iop_dso_t *dso, const iop_pkg_t *pkg)
{
    for (const iop_struct_t *const *it = pkg->structs; *it; it++) {
        const iop_struct_t *desc = *it;

        iopdso_fix_struct_ref(dso, &desc);
        iopdso_fix_class_parent(dso, desc);

        for (int i = 0; i < desc->fields_len; i++) {
            iop_field_t *f = (iop_field_t *)&desc->fields[i];

            if (f->type == IOP_T_STRUCT || f->type == IOP_T_UNION) {
                iopdso_fix_struct_ref(dso, &f->u1.st_desc);
            }
        }
    }
    for (const iop_iface_t *const *it = pkg->ifaces; *it; it++) {
        for (int i = 0; i < (*it)->funs_len; i++) {
            iop_rpc_t *rpc = (iop_rpc_t *)&(*it)->funs[i];

            iopdso_fix_struct_ref(dso, &rpc->args);
            iopdso_fix_struct_ref(dso, &rpc->result);
            iopdso_fix_struct_ref(dso, &rpc->exn);
        }
    }
}

static int iopdso_register_pkg(iop_dso_t *dso, iop_pkg_t const *pkg,
                               iop_env_t *env, sb_t *err)
{
    if (qm_add(iop_pkg, &dso->pkg_h, &pkg->name, pkg) < 0) {
        return 0;
    }
    if (dso->use_external_packages) {
        e_trace(1, "fixup package `%*pM`", LSTR_FMT_ARG(pkg->name));
        iopdso_fix_pkg(dso, pkg);
    }
    RETHROW(iop_register_packages_env(&pkg, 1, dso, env, IOP_REGPKG_FROM_DSO,
                                      err));
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
        if (dso->use_external_packages && iop_get_pkg_env((*it)->name, env)) {
            continue;
        }
        RETHROW(iopdso_register_pkg(dso, *it, env, err));
    }
    return 0;
}

static int iop_dso_reopen(iop_dso_t *dso, sb_t *err);

static iop_dso_t *iop_dso_init(iop_dso_t *dso)
{
    p_clear(dso, 1);
    qm_init_cached(iop_pkg,    &dso->pkg_h);
    qm_init_cached(iop_enum,   &dso->enum_h);
    qm_init_cached(iop_struct, &dso->struct_h);
    qm_init_cached(iop_iface,  &dso->iface_h);
    qm_init_cached(iop_mod,    &dso->mod_h);
    qh_init(ptr,               &dso->depends_on);
    qh_init(ptr,               &dso->needed_by);

    return dso;
}
static void iop_dso_wipe(iop_dso_t *dso)
{
    SB_1k(err);

    /* Delete references of this DSO in depends_on. */
    qh_for_each_pos(ptr, pos, &dso->depends_on) {
        iop_dso_t *depends_on = dso->depends_on.keys[pos];

        qh_del_key(ptr, &depends_on->needed_by, dso);
    }

    /* Unregister needed_by (otherwise they'll have orphan classes when
     * unregistering this one). */
    qh_for_each_pos(ptr, pos, &dso->needed_by) {
        iop_dso_t *needed_by = dso->needed_by.keys[pos];

        iop_dso_unregister(needed_by);
    }

    /* Unregister DSO. */
    iop_dso_unregister(dso);

    /* Reload needed_by. */
    qh_for_each_pos(ptr, pos, &dso->needed_by) {
        iop_dso_t *needed_by = dso->needed_by.keys[pos];

        if (iop_dso_reopen(needed_by, &err) < 0) {
            e_panic("IOP DSO: unable to reload plugin `%*pM` when unloading "
                    "plugin `%*pM`: %*pM",
                    LSTR_FMT_ARG(needed_by->path), LSTR_FMT_ARG(dso->path),
                    SB_FMT_ARG(&err));
        }
    }

    qm_wipe(iop_pkg,    &dso->pkg_h);
    qm_wipe(iop_enum,   &dso->enum_h);
    qm_wipe(iop_struct, &dso->struct_h);
    qm_wipe(iop_iface,  &dso->iface_h);
    qm_wipe(iop_mod,    &dso->mod_h);
    qh_wipe(ptr,        &dso->depends_on);
    qh_wipe(ptr,        &dso->needed_by);
    lstr_wipe(&dso->path);
    if (dso->handle) {
        dlclose(dso->handle);
    }
}
REFCNT_NEW(iop_dso_t, iop_dso);
REFCNT_DELETE(iop_dso_t, iop_dso);

static int iop_dso_register_(iop_dso_t *dso, sb_t *err);

static int iop_dso_open_(iop_dso_t *dso, sb_t *err)
{
    iop_pkg_t       **pkgp;
    void         *handle;
    iop_dso_vt_t *dso_vt;

#ifndef RTLD_DEEPBIND
# define RTLD_DEEPBIND  0
#endif
    handle = dlopen(dso->path.s, RTLD_LAZY | RTLD_LOCAL | RTLD_DEEPBIND);
    if (handle == NULL) {
        sb_setf(err, "unable to dlopen(%*pM): %s",
                LSTR_FMT_ARG(dso->path), dlerror());
        return -1;
    }

    dso_vt = dlsym(handle, "iop_vtable");
    if (dso_vt == NULL || dso_vt->vt_size == 0) {
        e_warning("IOP DSO: unable to find valid IOP vtable in plugin "
                  "(%*pM), no error management allowed: %s",
                  LSTR_FMT_ARG(dso->path), dlerror());
    } else {
        dso_vt->iop_set_verr = &iop_set_verr;
    }

    pkgp = dlsym(handle, "iop_packages");
    if (pkgp == NULL) {
        sb_setf(err, "unable to find IOP packages in plugin (%*pM): %s",
                LSTR_FMT_ARG(dso->path), dlerror());
        dlclose(handle);
        return -1;
    }

    dso->handle = handle;
    dso->use_external_packages = !!dlsym(handle, "iop_use_external_packages");

    return iop_dso_register_(dso, err);
}

iop_dso_t *iop_dso_open(const char *path, sb_t *err)
{
    iop_dso_t *dso = iop_dso_new();

    dso->path = lstr_dups(path, -1);

    if (iop_dso_open_(dso, err) < 0) {
        iop_dso_delete(&dso);
        return NULL;
    }

    return dso;
}

static int iop_dso_reopen(iop_dso_t *dso, sb_t *err)
{
    lstr_t path = LSTR_NULL_V;

    lstr_transfer(&path, &dso->path);

    iop_dso_wipe(dso);
    iop_dso_init(dso);

    dso->path = path;

    return iop_dso_open_(dso, err);
}

void iop_dso_close(iop_dso_t **dsop)
{
    iop_dso_delete(dsop);
}

static int iop_dso_register_(iop_dso_t *dso, sb_t *err)
{
    if (!dso->is_registered) {
        iop_env_t env;
        iop_pkg_t **pkgp = dlsym(dso->handle, "iop_packages");

        if (!pkgp) {
            /* This should not happen because this was checked before. */
            e_panic("IOP DSO: iop_packages not found when registering DSO");
        }

        qm_clear(iop_pkg, &dso->pkg_h);
        iop_env_get(&env);
        while (*pkgp) {
            if (iopdso_register_pkg(dso, *pkgp++, &env, err) < 0) {
                iop_env_wipe(&env);
                return -1;
            }
        }
        RETHROW(iop_check_registered_classes(&env, err));
        iop_env_set(&env);
        dso->is_registered = true;
    }
    return 0;
}

void iop_dso_register(iop_dso_t *dso)
{
    SB_1k(err);

    if (iop_dso_register_(dso, &err) < 0) {
        e_fatal("IOP DSO: %*pM", SB_FMT_ARG(&err));
    }
}

void iop_dso_unregister(iop_dso_t *dso)
{
    if (dso->is_registered) {
        t_scope;
        qv_t(cvoid) vec;

        t_qv_init(cvoid, &vec, qm_len(iop_pkg, &dso->pkg_h));
        qm_for_each_pos(iop_pkg, pos, &dso->pkg_h) {
            qv_append(cvoid, &vec, dso->pkg_h.values[pos]);
        }
        iop_unregister_packages((const iop_pkg_t **)vec.tab, vec.len);
        dso->is_registered = false;
    }
}

static iop_rpc_t const *
find_rpc_in_iface(iop_iface_t const *iface, lstr_t fname)
{
    for (int i = 0; i < iface->funs_len; i++) {
        iop_rpc_t const *rpc = &iface->funs[i];

        if (lstr_equal(rpc->name, fname)) {
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

        if (lstr_equal(alias->name, iface)) {
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
