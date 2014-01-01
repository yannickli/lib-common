/**************************************************************************/
/*                                                                        */
/*  Copyright (C) 2004-2014 INTERSEC SA                                   */
/*                                                                        */
/*  Should you receive a copy of this source code, you must check you     */
/*  have a proper, written authorization of INTERSEC to hold it. If you   */
/*  don't have such an authorization, you must DELETE all source code     */
/*  files in your possession, and inform INTERSEC of the fact you obtain  */
/*  these files. Should you not comply to these terms, you can be         */
/*  prosecuted in the extent permitted by applicable law.                 */
/*                                                                        */
/**************************************************************************/

#include "core.h"

/** \addtogroup lc_obj
 * \{
 */

/** \file lib-common/core-obj.c
 * \brief Objects and Virtual Tables in C (module)
 */

bool cls_inherits(const void *_cls, const void *vptr)
{
    const object_class_t *cls = _cls;
    while (cls) {
        if (cls == vptr)
            return true;
        cls = cls->super;
    }
    return false;
}

static void obj_init_real_aux(object_t *o, const object_class_t *cls)
{
    object_t *(*init)(object_t *) = cls->init;

    if (!init)
        return;

    while (cls->super && init == cls->super->init) {
        cls = cls->super;
    }
    if (cls->super)
        obj_init_real_aux(o, cls->super);
    (*init)(o);
}

void *obj_init_real(const void *_cls, void *_o, ssize_t refcnt)
{
    const object_class_t *cls = _cls;
    object_t *o = _o;

    o->refcnt = refcnt;
    o->v.ptr  = cls;
    obj_init_real_aux(o, cls);
    return o;
}

void obj_wipe_real(object_t *o)
{
    const object_class_t *cls = o->v.ptr;
    void (*wipe)(object_t *);

    /* a crash here means obj_wipe was called on a reachable object.
     * It's likely the caller should have used obj_release() instead.
     */
    assert (o->refcnt == OBJECT_REFCNT_STATIC || o->refcnt == 1);

    while ((wipe = cls->wipe)) {
        (*wipe)(o);
        do {
            cls = cls->super;
            if (!cls)
                return;
        } while (wipe == cls->wipe);
    }
    o->refcnt = 0;
}

static object_t *obj_retain_(object_t *obj)
{
    if (likely(obj->refcnt > 0)) {
        if (likely(++obj->refcnt > 0))
            return obj;
        e_panic("too many refcounts");
    }

    switch (obj->refcnt) {
      case 0:
        e_panic("probably acting on a deleted object");
      case OBJECT_REFCNT_STATIC:
        e_panic("forbidden to call retain on a statically allocated object");
      default:
        /* WTF?! probably a memory corruption */
        e_panic("should not happen");
    }
}

static void obj_release_(object_t *obj)
{
    if (obj->refcnt > 1) {
        --obj->refcnt;
        return;
    }
    if (obj->refcnt == 1) {
        if (obj_vmethod(obj, can_wipe) && !obj_vcall(obj, can_wipe))
            return;
        obj_wipe_real(obj);
        p_delete(&obj);
        return;
    }

    switch (obj->refcnt) {
      case 0:
        e_panic("object refcounting issue");
      case OBJECT_REFCNT_STATIC:
        /* you should call obj_wipe instead */
        e_panic("forbidden to call release on a statically allocated object");
      default:
        /* WTF?! probably a memory corruption we should have hit 0 first */
        e_panic("should not happen");
    }
}

static uint32_t obj_hash_(const object_t *o)
{
    uintptr_t p = (uintptr_t)o;

    if (sizeof(o) == 4)
        return p;
    return (uint32_t)((uint64_t)p >> 32) ^ (uint32_t)p;
}

static bool obj_equal_(const object_t *o1, const object_t *o2)
{
    return o1 == o2;
}

const object_class_t *object_class(void)
{
    static object_class_t const klass = {
        .type_size = sizeof(object_t),
        .type_name = "object",

        .retain    = obj_retain_,
        .release   = obj_release_,
        .hash      = obj_hash_,
        .equal     = obj_equal_,
    };
    return &klass;
}

/**\}*/
