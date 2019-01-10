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

static void iopdso_register_pkg(iop_dso_t *dso, iop_pkg_t const *pkg)
{
    if (qm_add(iop_pkg, &dso->pkg_h, &pkg->name, pkg) < 0)
        return;
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
        iopdso_register_pkg(dso, *it);
    }
}

static iop_dso_t *iop_dso_init(iop_dso_t *dso)
{
    p_clear(dso, 1);
    qm_init(iop_pkg,    &dso->pkg_h,    true);
    qm_init(iop_enum,   &dso->enum_h,   true);
    qm_init(iop_struct, &dso->struct_h, true);
    qm_init(iop_iface,  &dso->iface_h,  true);
    qm_init(iop_mod,    &dso->mod_h,    true);

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
    if (dso->handle)
        dlclose(dso->handle);
}
REFCNT_NEW(iop_dso_t, iop_dso);
REFCNT_DELETE(iop_dso_t, iop_dso);

iop_dso_t *iop_dso_open(const char *path)
{
    iop_pkg_t       **pkgp;
    void             *handle;
    iop_dso_vt_t     *dso_vt;
    iop_dso_t        *dso;

    handle = dlopen(path, RTLD_LAZY | RTLD_LOCAL | RTLD_DEEPBIND);
    if (handle == NULL) {
        e_error("IOP DSO: unable to dlopen(%s): %s", path, dlerror());
        return NULL;
    }

    dso_vt = (iop_dso_vt_t *) dlsym(handle, "iop_vtable");
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

    dso = iop_dso_new();
    dso->path   = lstr_dups(path, strlen(path));
    dso->handle = handle;
    while (*pkgp)
        iopdso_register_pkg(dso, *pkgp++);
    return dso;
}

void iop_dso_close(iop_dso_t **dsop)
{
    iop_dso_delete(dsop);
}

static iop_rpc_t const *
find_rpc_in_iface(iop_iface_t const *iface, lstr_t fname)
{
    for (int i = 0; i < iface->funs_len; i++) {
        iop_rpc_t const *rpc = &iface->funs[i];

        if (lstr_equal2(rpc->name, fname))
            return rpc;
    }
    return NULL;
}

static iop_rpc_t const *
find_rpc_in_mod(iop_mod_t const *mod, lstr_t iface, lstr_t fname)
{
    for (int i = 0; i < mod->ifaces_len; i++) {
        const iop_iface_alias_t *alias = &mod->ifaces[i];

        if (lstr_equal2(alias->name, iface))
            return find_rpc_in_iface(alias->iface, fname);
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
    if (pos >= 0)
        return dso->struct_h.values[pos];

    if (lstr_endswith(name, LSTR_IMMED_V("Args"))) {
        name.len -= strlen("Args");
        what = 0;
    } else
    if (lstr_endswith(name, LSTR_IMMED_V("Res"))) {
        name.len -= strlen("Res");
        what = 1;
    } else
    if (lstr_endswith(name, LSTR_IMMED_V("Exn"))) {
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
