/**************************************************************************/
/*                                                                        */
/*  Copyright (C) 2004-2011 INTERSEC SAS                                  */
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

static void iopdso_register_pkg(iop_dso_t *dso, iop_pkg_t const *pkg)
{
    if (qm_add(iop_pkg, &dso->pkg_h, &pkg->name, pkg) < 0)
        return;
    for (const iop_enum_t *const *it = pkg->enums; *it; it++) {
        qm_add(iop_enum, &dso->enum_h, &(*it)->fullname, *it);
    }
    for (const iop_struct_t *const *it = pkg->structs; *it; it++) {
        qm_add(iop_struct, &dso->struct_h, &(*it)->fullname, *it);
    }
    for (const iop_iface_t *const *it = pkg->ifaces; *it; it++) {
        qm_add(iop_iface, &dso->iface_h, &(*it)->fullname, *it);
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
    iop_pkg_t **pkgp;
    void       *handle;
    iop_dso_t  *dso;

    handle = dlopen(path, RTLD_LAZY | RTLD_LOCAL);
    if (handle == NULL) {
        e_error("IOP DSO: unable to dlopen(%s): %s", path, dlerror());
        return NULL;
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
