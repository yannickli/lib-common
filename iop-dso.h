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

#if !defined(IS_LIB_COMMON_IOP_H) || defined(IS_LIB_COMMON_IOP_DSO_H)
#  error "you must include <lib-common/iop.h> instead"
#else
#define IS_LIB_COMMON_IOP_DSO_H

#include "farch.h"

qm_kvec_t(iop_struct, lstr_t, const iop_struct_t *,
          qhash_lstr_hash, qhash_lstr_equal);
qm_kvec_t(iop_iface, lstr_t, const iop_iface_t *,
          qhash_lstr_hash, qhash_lstr_equal);
qm_kvec_t(iop_mod, lstr_t, const iop_mod_t *,
          qhash_lstr_hash, qhash_lstr_equal);

typedef struct iop_dso_t {
    int              refcnt;
    void            *handle;
    lstr_t           path;

    qm_t(iop_pkg)    pkg_h;
    qm_t(iop_enum)   enum_h;
    qm_t(iop_struct) struct_h;
    qm_t(iop_iface)  iface_h;
    qm_t(iop_mod)    mod_h;

    /* Hash table of other iop_dso_t used by this one (in case of fixups). */
    qh_t(ptr) depends_on;
    /* Hash table of other iop_dso_t which need this one (in case of
     * fixups). */
    qh_t(ptr) needed_by;

    flag_t use_external_packages : 1;
    flag_t is_registered         : 1;
} iop_dso_t;

/** Load a DSO from a file, and register its packages. */
iop_dso_t *iop_dso_open(const char *path, sb_t *err);

static ALWAYS_INLINE iop_dso_t *iop_dso_dup(iop_dso_t *dso)
{
    dso->refcnt++;
    return dso;
}

/** Close a DSO and unregister its packages. */
void iop_dso_close(iop_dso_t **dsop);

/** Register the packages contained in a DSO.
 *
 * Packages registration is mandatory in order to pack/unpack classes
 * they contain.
 * \ref iop_dso_open already registers the DSO packages, so calling this
 * function only makes sense if you've called \ref iop_dso_unregister before.
 */
void iop_dso_register(iop_dso_t *dso);

/** Unregister the packages contained in a DSO. */
void iop_dso_unregister(iop_dso_t *dso);

iop_struct_t const *iop_dso_find_type(iop_dso_t const *dso, lstr_t name);
iop_enum_t const *iop_dso_find_enum(iop_dso_t const *dso, lstr_t name);

const void *const *iop_dso_get_ressources(const iop_dso_t *, lstr_t category);

#define IOP_DSO_GET_RESSOURCES(dso, category)                 \
    ((const iop_dso_ressource_t(category) *const *)           \
        iop_dso_get_ressources(dso, LSTR(#category)))

#define iop_dso_ressources_for_each_entry(category, ressource, ressources) \
    for (const iop_dso_ressource_t(category) *ressource,                   \
         *const *_ressource_ptr = (ressources);                            \
         (ressource = _ressource_ptr ? *_ressource_ptr : NULL);            \
         _ressource_ptr++)

#define iop_dso_for_each_ressource(dso, category, ressource)                 \
    iop_dso_ressources_for_each_entry(category, ressource,                   \
                                      IOP_DSO_GET_RESSOURCES(dso, category))

IOP_DSO_DECLARE_RESSOURCE_CATEGORY(iopy_on_register, struct farch_entry_t);

#endif
