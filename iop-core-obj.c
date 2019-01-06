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

#include "iop.h"

static void iop_core_obj_wipe(iop_core_obj_t *obj)
{
    p_delete(&obj->desc);
}

OBJ_VTABLE(iop_core_obj)
    iop_core_obj.wipe = &iop_core_obj_wipe;
OBJ_VTABLE_END()

qm_k32_t(iop_core_cls, const object_class_t *nonnull);

struct iop_core_obj_map_t {
    qm_t(iop_core_cls) qm;
};

static iop_core_obj_map_t *iop_core_obj_map_init(iop_core_obj_map_t *map)
{
    p_clear(map, 1);
    qm_init(iop_core_cls, &map->qm);

    return map;
}

DO_NEW(iop_core_obj_map_t, iop_core_obj_map);

static void iop_core_obj_map_wipe(iop_core_obj_map_t *map)
{
    qm_wipe(iop_core_cls, &map->qm);
}

DO_DELETE(iop_core_obj_map_t, iop_core_obj_map);

const object_class_t *nullable
_iop_core_obj_map_get_cls(const iop_core_obj_map_t *nonnull map,
                          const void *nonnull iop_obj)
{
    const iop_struct_t *iop_class;

    iop_class = *(const iop_struct_t **)iop_obj;

    assert (iop_struct_is_class(iop_class));

    return qm_get_def_safe(iop_core_cls, &map->qm,
                           iop_class->class_attrs->class_id, NULL);
}

void *nullable
_iop_core_obj_map_new_obj(const iop_core_obj_map_t *nonnull map,
                          const void *nonnull iop_obj)
{
    const object_class_t *cls;
    const iop_struct_t *iop_class;
    iop_core_obj_t *obj;

    cls = RETHROW_P(_iop_core_obj_map_get_cls(map, iop_obj));
    obj = obj_new_of_class(iop_core_obj, cls);
    iop_class = *(const iop_struct_t **)iop_obj;
    obj->desc = mp_iop_dup_desc_sz(NULL, iop_class, iop_obj, NULL);

    return obj;
}

void _iop_core_obj_map_register_cls(iop_core_obj_map_t *nonnull map,
                                    const iop_struct_t *nonnull iop_cls,
                                    const object_class_t *nonnull cls)
{
    uint32_t pos;

    e_assert(panic, iop_struct_is_class(iop_cls),
             "the IOP struct is not a class");
    pos = qm_put(iop_core_cls, &map->qm,
                 iop_cls->class_attrs->class_id, cls, 0);
    e_assert(panic, !(pos & QHASH_COLLISION),
             "class for `%pL' registered twice", &iop_cls->fullname);
}
