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

#include "core.h"
#include "z.h"

#define MY_BASE_OBJECT_FIELDS(pfx)                                           \
    OBJECT_FIELDS(pfx);                                                      \
    int a

#define MY_BASE_OBJECT_METHODS(type_t)                                       \
    OBJECT_METHODS(type_t);                                                  \
    lstr_t (*t_get_desc)(type_t *self);                                      \
    int (*get_a)(type_t *self)

OBJ_CLASS(my_base_object, object,
          MY_BASE_OBJECT_FIELDS, MY_BASE_OBJECT_METHODS)

static my_base_object_t *my_base_object_init(my_base_object_t *self)
{
    self->a = 42;
    return self;
}

static lstr_t t_my_base_object_get_desc(my_base_object_t *self)
{
    return t_lstr_fmt("a: %d", obj_vcall(self, get_a));
}

static int my_base_object_get_a(my_base_object_t *self)
{
    return self->a;
}

OBJ_VTABLE(my_base_object)
    my_base_object.init = my_base_object_init;
    my_base_object.t_get_desc = t_my_base_object_get_desc;
    my_base_object.get_a = my_base_object_get_a;
OBJ_VTABLE_END()

#define MY_CHILD_OBJECT_FIELDS(pfx, b_type_t)                                \
    MY_BASE_OBJECT_FIELDS(pfx);                                              \
    b_type_t b

#define MY_CHILD_OBJECT_METHODS(type_t, b_type_t)                            \
    MY_BASE_OBJECT_METHODS(type_t);                                          \
    b_type_t (*get_b)(type_t *self);                                         \
    int (*get_extended_a)(type_t *self);                                     \
    b_type_t (*get_extended_b)(type_t *self)

OBJ_CLASS(my_child_object, my_base_object,
          MY_CHILD_OBJECT_FIELDS, MY_CHILD_OBJECT_METHODS, bool)

static my_child_object_t *my_child_object_init(my_child_object_t *self)
{
    self->b = true;
    return self;
}

static lstr_t t_my_child_object_get_desc(my_child_object_t *self)
{
    lstr_t base_desc = super_call(my_child_object, self, t_get_desc);

    return t_lstr_fmt("%pL, b: %d", &base_desc,  obj_vcall(self, get_b));
}

static bool my_child_object_get_b(my_child_object_t *self)
{
    return self->b;
}

static int my_child_object_get_extended_a_0(my_child_object_t *self)
{
    return -1;
}

OBJ_VTABLE(my_child_object)
    my_child_object.init = my_child_object_init;
    my_child_object.t_get_desc = t_my_child_object_get_desc;
    my_child_object.get_b = my_child_object_get_b;
    my_child_object.get_extended_a = my_child_object_get_extended_a_0;
OBJ_VTABLE_END()

static int my_child_object_get_extended_a_1(my_child_object_t *self)
{
    return -self->a;
}

static bool my_child_object_get_extended_b(my_child_object_t *self)
{
    return !self->b;
}

OBJ_EXT_VTABLE(my_child_object)
    my_child_object.get_extended_a = my_child_object_get_extended_a_1;
    my_child_object.get_extended_b = my_child_object_get_extended_b;
OBJ_EXT_VTABLE_END()

static int my_child_object_get_extended_a_2(my_child_object_t *self)
{
    return self->a;
}

OBJ_EXT_VTABLE_WITH_PRIO(my_child_object, OBJ_EXT_VTABLE_DEF_PRIO + 1)
    my_child_object.get_extended_a = my_child_object_get_extended_a_2;
OBJ_EXT_VTABLE_END()

static lstr_t t_get_obj_desc_indirect(my_base_object_t *obj)
{
    return obj_vcall(obj, t_get_desc);
}

Z_GROUP_EXPORT(core_obj)
{
    Z_TEST(basic, "test basic use of core obj") {
        t_scope;
        my_base_object_t *base_obj = obj_new(my_base_object);
        my_child_object_t *child_obj = obj_new(my_child_object);
        lstr_t desc;

        /* Test with base obj. */
        desc = t_get_obj_desc_indirect(base_obj);
        Z_ASSERT_LSTREQUAL(desc, LSTR("a: 42"));

        /* Test with child obj */
        child_obj->a = 7;
        desc = t_get_obj_desc_indirect(&child_obj->super);
        Z_ASSERT_LSTREQUAL(desc, LSTR("a: 7, b: 1"));

        obj_release(child_obj);
        obj_release(base_obj);
    } Z_TEST_END;

    Z_TEST(extended, "test extended vtable") {
        my_child_object_t *child_obj = obj_new(my_child_object);

        child_obj->a = 5;

        /* Test get_extented_a */
        Z_ASSERT_EQ(child_obj->a, obj_vcall(child_obj, get_extended_a));

        /* Test get_extented_b */
        Z_ASSERT_EQ(!child_obj->b, obj_vcall(child_obj, get_extended_b));

        obj_release(child_obj);
    } Z_TEST_END;
} Z_GROUP_END
