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

qm_kvec_t(iop_pkg, lstr_t, const iop_pkg_t *,
          qhash_lstr_hash, qhash_lstr_equal);
qm_kvec_t(iop_enum, lstr_t, const iop_enum_t *,
          qhash_lstr_hash, qhash_lstr_equal);
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
} iop_dso_t;

typedef struct iop_dso_vt_t {
    size_t  vt_size;
    void  (*iop_set_verr)(const char *fmt, va_list ap)
        __attribute__((format(printf, 1, 0)));
} iop_dso_vt_t;

iop_dso_t *iop_dso_open(const char *path);
static ALWAYS_INLINE iop_dso_t *iop_dso_dup(iop_dso_t *dso)
{
    dso->refcnt++;
    return dso;
}
void iop_dso_close(iop_dso_t **dsop);

iop_struct_t const *iop_dso_find_type(iop_dso_t const *dso, lstr_t name);

#define IOP_EXPORT_PACKAGES(...) \
    EXPORT iop_pkg_t const *const iop_packages[];   \
    iop_pkg_t const *const iop_packages[] = { __VA_ARGS__, NULL }

#define IOP_EXPORT_PACKAGES_COMMON \
    EXPORT iop_dso_vt_t iop_vtable;                                     \
    iop_dso_vt_t iop_vtable = {                                         \
        .vt_size = sizeof(iop_dso_vt_t),                                \
        .iop_set_verr = NULL,                                           \
    };                                                                  \
    iop_struct_t const iop__void__s = {                                 \
        .fullname   = LSTR_IMMED("Void"),                               \
        .fields_len = 0,                                                \
        .size       = 0,                                                \
    };                                                                  \
                                                                        \
    __attribute__((format(printf, 1, 2)))                               \
    int iop_set_err(const char *fmt, ...) {                             \
        va_list ap;                                                     \
                                                                        \
        va_start(ap, fmt);                                              \
        if (NULL == iop_vtable.iop_set_verr) {                          \
            fputs("iop_vtable.iop_set_verr not defined", stderr);       \
            exit(1);                                                    \
        }                                                               \
        (iop_vtable.iop_set_verr)(fmt, ap);                             \
        va_end(ap);                                                     \
        return -1;                                                      \
    }                                                                   \

#endif
