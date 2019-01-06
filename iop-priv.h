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

#ifndef IS_LIB_COMMON_IOP_PRIV_H
#define IS_LIB_COMMON_IOP_PRIV_H

/* {{{ class_id/class_name qm declarations */

typedef struct class_id_key_t {
    const iop_struct_t *master;
    uint16_t            child_id;
} class_id_key_t;

static inline uint32_t
class_id_key_hash(const qhash_t *h, const class_id_key_t *key)
{
    return mem_hash32(key, offsetof(class_id_key_t, child_id) + 2);
}
static inline bool
class_id_key_equal(const qhash_t *h, const class_id_key_t *k1,
                   const class_id_key_t *k2)
{
    return (k1->master == k2->master) && (k1->child_id == k2->child_id);
}

qm_kvec_t(class_id, class_id_key_t, const iop_struct_t *,
          class_id_key_hash, class_id_key_equal);

typedef struct class_name_key_t {
    const iop_struct_t *master;
    const lstr_t       *child_fullname;
} class_name_key_t;

static inline uint32_t
class_name_key_hash(const qhash_t *h, const class_name_key_t *key)
{
    return qhash_hash_ptr(h, key->master)
         ^ qhash_lstr_hash(h, key->child_fullname);
}
static inline bool
class_name_key_equal(const qhash_t *h, const class_name_key_t *k1,
                   const class_name_key_t *k2)
{
    return (k1->master == k2->master)
         && lstr_equal(*k1->child_fullname, *k2->child_fullname);
}

qm_kvec_t(class_name, class_name_key_t, const iop_struct_t *,
          class_name_key_hash, class_name_key_equal);

/* }}} */
/* {{{ IOP context */

qm_kvec_t(iop_objs, lstr_t, qv_t(cvoid), qhash_lstr_hash, qhash_lstr_equal);

static inline uint32_t qhash_pkg_hash(const qhash_t *qh, const iop_pkg_t *pkg)
{
    return qhash_hash_ptr(qh, (const void *)pkg);
}

static inline bool
qhash_pkg_equal(const qhash_t *qh,
                const iop_pkg_t *pkg1, const iop_pkg_t *pkg2)
{
    return pkg1 == pkg2;
}

qm_kptr_ckey_t(iop_dsos, iop_pkg_t, iop_dso_t *,
               qhash_pkg_hash, qhash_pkg_equal);

typedef struct iop_env_t {
    qm_t(class_id)   classes_by_id;
    qm_t(class_name) classes_by_name;
    qm_t(iop_dsos)   dsos_by_pkg;
    qm_t(iop_objs)   pkgs_by_name;
    qm_t(iop_objs)   enums_by_fullname;
} iop_env_t;

iop_env_t *iop_env_init(iop_env_t *env);
void iop_env_wipe(iop_env_t *env);
void iop_env_set(iop_env_t *env);
void iop_env_get(iop_env_t *env);

const iop_pkg_t *iop_get_pkg_env(lstr_t pkgname, const iop_env_t *env);
int iop_check_registered_classes(const iop_env_t *env, sb_t *err);

iop_dso_t *iop_dso_get_from_pkg(const iop_pkg_t *pkg);

int iop_register_packages_env(const iop_pkg_t **pkgs, int len,
                              iop_dso_t * nullable dso,
                              iop_env_t *env, unsigned flags, sb_t *err);

/* }}} */

#endif
