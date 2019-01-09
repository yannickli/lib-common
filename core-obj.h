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

#if !defined(IS_LIB_COMMON_CORE_H) || defined(IS_LIB_COMMON_CORE_OBJ_H)
#  error "you must include <lib-common/core.h> instead"
#else
#define IS_LIB_COMMON_CORE_OBJ_H

#include <lib-common/core.h>

/** \defgroup lc_obj Intersec Object Oriented C
 *
 * \brief Object Oriented Programming with C.
 *
 * \{
 */

/** \file lib-common/core-obj.h
 * \brief Objects and Virtual Tables in C (header)
 */

bool cls_inherits(const void *cls, const void *vptr);

#define obj_is_a(obj, pfx)         \
    (!(obj) || cls_inherits((obj)->v.as_obj_cls, pfx##_class()))

#define obj_vfield(o, field)       ((o)->v.ptr->field)
#define obj_vmethod(o, method)     ((o)->v.ptr->method)
#define obj_vcall(o, method, ...)  obj_vmethod(o, method)(o, ##__VA_ARGS__)

#if !defined(NDEBUG) && defined(__GNUC__)
#  define obj_cast_debug(pfx, o) \
    ({ typeof(o) __##pfx##_o = (o);                                          \
       if (unlikely(!obj_is_a(__##pfx##_o, pfx))) {                          \
           e_panic("%s:%d: cannot cast (%p : %s) into a %s",                 \
                   __FILE__, __LINE__, __##pfx##_o,                          \
                   obj_vfield(__##pfx##_o, type_name),                       \
                   pfx##_class()->type_name);                                \
     }                                                                       \
     __##pfx##_o; })
#  define cls_cast_debug(pfx, c) \
    ({ typeof(c) __##pfx##_c = (c);                                          \
       if (unlikely(!cls_inherits(__##pfx##_c, pfx##_class()))) {            \
           e_panic("%s:%d: bad class cast (%s into %s)", __FILE__, __LINE__, \
                   __##pfx##_c->type_name, pfx##_class()->type_name);        \
       }                                                                     \
       __##pfx##_c; })
#else
#  define obj_cast_debug(pfx, o)  (o)
#  define cls_cast_debug(pfx, c)  (c)
#endif

#define obj_vcast(pfx, o)  ((pfx##_t *)obj_cast_debug(pfx, o))
#define obj_ccast(pfx, o)  ((const pfx##_t *)obj_cast_debug(pfx, o))
#define cls_cast(pfx, c)   ((const pfx##_class_t *)cls_cast_debug(pfx, c))

#define cls_call(pfx, o, f, ...) \
    (pfx##_class()->f(obj_vcast(pfx, o), ##__VA_ARGS__))

#define OBJ_CLASS(pfx, superclass, fields, methods) \
    typedef struct pfx##_t pfx##_t;                                          \
    typedef struct pfx##_class_t pfx##_class_t;                              \
                                                                             \
    struct pfx##_t {                                                         \
        fields(pfx);                                                         \
    };                                                                       \
    struct pfx##_class_t {                                                   \
        const superclass##_class_t *super;                                   \
        const char *type_name;                                               \
        int type_size;                                                       \
        methods(pfx##_t);                                                    \
    };                                                                       \
                                                                             \
    const pfx##_class_t *pfx##_class(void);                                  \
                                                                             \
    __unused__                                                               \
    static inline const superclass##_class_t *pfx##_super(void) {            \
        return superclass##_class();                                         \
    }

#define OBJ_VTABLE(pfx) \
    const pfx##_class_t *pfx##_class(void) {                                 \
        static pfx##_class_t pfx;                                            \
        static pfx##_class_t * const res = &pfx;                             \
        if (unlikely(!pfx.super)) {                                          \
            const void * const cls_super = pfx##_super();                    \
            const char * const type_name = #pfx;                             \
            const int          type_size = sizeof(pfx##_t);                  \
            memcpy(&pfx, pfx##_super(), sizeof(*pfx##_super()));

#define OBJ_VTABLE_END() \
            res->super     = cls_super;                                      \
            res->type_name = type_name;                                      \
            res->type_size = type_size;                                      \
        }                                                                    \
        return res;                                                          \
    }

#define OBJECT_FIELDS(pfx) \
    union {                                                                  \
        const object_class_t *as_obj_cls;                                    \
        const pfx##_class_t  *ptr;                                           \
    } v

#define OBJECT_METHODS(type_t) \
    type_t *(*init)(type_t *);                  \
    bool    (*release)(type_t *);               \
    void    (*wipe)(type_t *)

OBJ_CLASS(object, object, OBJECT_FIELDS, OBJECT_METHODS);


void *obj_init_real(const void *cls, void *o);
void obj_wipe_real(object_t *o);

#define obj_delete_with_expr(op, destruct_expr) \
    do {                                                                     \
        if (*op) {                                                           \
            object_t *o = obj_vcast(object, *(op));                          \
            if (!obj_vmethod(o, release) || obj_vcall(o, release)) {         \
                obj_wipe(o);                                                 \
                destruct_expr;                                               \
            } else {                                                         \
                *(op) = NULL;                                                \
            }                                                                \
        }                                                                    \
    } while (0)

#define obj_new(pfx)      \
    (pfx##_t *)obj_init_real(pfx##_class(), p_new(pfx##_t, 1))
#define obj_mp_new(mp, pfx) \
    (pfx##_t *)obj_init_real(pfx##_class(), mp_new(mp, pfx##_t, 1))
#define obj_init(pfx, v)  \
    (pfx##_t *)obj_init_real(pfx##_class(), memset(v, 0, sizeof(*v)))
#define obj_wipe(o)            obj_wipe_real(obj_vcast(object, o))
#define obj_delete(op)         obj_delete_with_expr(op, p_delete(op))
#define obj_mp_delete(mp, op)  obj_delete_with_expr(op, mp_delete(mp, op))

/**\}*/
#endif
