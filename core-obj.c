/**************************************************************************/
/*                                                                        */
/*  Copyright (C) 2004-2008 INTERSEC SAS                                  */
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

void *obj_init_real(const void *_cls, void *_o)
{
    const object_class_t *cls = _cls;
    object_t *o = _o;

    o->v.ptr = cls;
    obj_init_real_aux(o, cls);
    return o;
}

void obj_wipe_real(object_t *o)
{
    const object_class_t *cls = o->v.ptr;
    void (*wipe)(object_t *);

    while ((wipe = cls->wipe)) {
        (*wipe)(o);
        do {
            cls = cls->super;
            if (!cls)
                return;
        } while (wipe == cls->wipe);
    }
}

const object_class_t *object_class(void)
{
    static object_class_t const klass = {
        .type_size = sizeof(object_t),
        .type_name = "object",
    };
    return &klass;
}

/**\}*/
