/***** THIS FILE IS AUTOGENERATED DO NOT MODIFY DIRECTLY ! *****/
#if !defined(linux) && !defined(__APPLE__)
#  warning "attrs_multi_valid.iop assumed linux alignments"
#endif
#ifndef __x86_64__
#  warning "attrs_multi_valid.iop assumed x86_64 alignments"
#endif

#include "attrs_multi_valid.iop.h"

/* Enum attrs_multi_valid.MyEnum {{{ */

static int const attrs_multi_valid__my_enum__values[] = {
 0, 1, 2,
};
static int const iop__ranges__1[] = {
    0, 0,
    3,
};
static const lstr_t attrs_multi_valid__my_enum__names[] = {
    LSTR_IMMED("E"),
    LSTR_IMMED("F"),
    LSTR_IMMED("G"),
};
iop_enum_t const attrs_multi_valid__my_enum__e = {
    .name         = LSTR_IMMED("MyEnum"),
    .fullname     = LSTR_IMMED("attrs_multi_valid.MyEnum"),
    .names        = attrs_multi_valid__my_enum__names,
    .values       = attrs_multi_valid__my_enum__values,
    .ranges       = iop__ranges__1,
    .ranges_len   = countof(iop__ranges__1) / 2,
    .enum_len     = 3,
};
iop_enum_t const * const attrs_multi_valid__my_enum__ep = &attrs_multi_valid__my_enum__e;

/* }}} */
/* Union attrs_multi_valid.MyUnion {{{ */

static iop_field_t const attrs_multi_valid__my_union__desc_fields[] = {
    {
        .name      = LSTR_IMMED("a"),
        .tag       = 1,
        .tag_len   = 0,
        .repeat    = IOP_R_REQUIRED,
        .type      = IOP_T_I32,
        .data_offs = offsetof(attrs_multi_valid__my_union__t, a),
        .size      = fieldsizeof(attrs_multi_valid__my_union__t, a),
    },
    {
        .name      = LSTR_IMMED("b"),
        .tag       = 2,
        .tag_len   = 0,
        .repeat    = IOP_R_REQUIRED,
        .type      = IOP_T_I64,
        .data_offs = offsetof(attrs_multi_valid__my_union__t, b),
        .size      = fieldsizeof(attrs_multi_valid__my_union__t, b),
    },
    {
        .name      = LSTR_IMMED("c"),
        .tag       = 3,
        .tag_len   = 0,
        .repeat    = IOP_R_REQUIRED,
        .type      = IOP_T_STRING,
        .data_offs = offsetof(attrs_multi_valid__my_union__t, c),
        .size      = fieldsizeof(attrs_multi_valid__my_union__t, c),
    },
    {
        .name      = LSTR_IMMED("d"),
        .tag       = 4,
        .tag_len   = 0,
        .repeat    = IOP_R_REQUIRED,
        .type      = IOP_T_DOUBLE,
        .data_offs = offsetof(attrs_multi_valid__my_union__t, d),
        .size      = fieldsizeof(attrs_multi_valid__my_union__t, d),
    },
};
static int const iop__ranges__2[] = {
    0, 1,
    4,
};
const iop_struct_t attrs_multi_valid__my_union__s = {
    .fullname   = LSTR_IMMED("attrs_multi_valid.MyUnion"),
    .fields     = attrs_multi_valid__my_union__desc_fields,
    .ranges     = iop__ranges__2,
    .ranges_len = countof(iop__ranges__2) / 2,
    .fields_len = countof(attrs_multi_valid__my_union__desc_fields),
    .size       = sizeof(attrs_multi_valid__my_union__t),
    .is_union   = true,
};
iop_struct_t const * const attrs_multi_valid__my_union__sp = &attrs_multi_valid__my_union__s;

/* }}} */
/* Structure attrs_multi_valid.Toto {{{ */

static int attrs_multi_valid__toto__b__check(const void *ptr, int n)
{
    for (int j = 0; j < n; j++) {
        attrs_multi_valid__my_union__t *val = &IOP_FIELD(attrs_multi_valid__my_union__t, ptr, j);

        switch (val->iop_tag) {
          case attrs_multi_valid__my_union__a__ft:
            break;
          case attrs_multi_valid__my_union__b__ft:
            break;
          case attrs_multi_valid__my_union__c__ft:
            iop_set_err("violation of constraint allow (c) on field b");
            return -1;
          case attrs_multi_valid__my_union__d__ft:
            iop_set_err("violation of constraint allow (d) on field b");
            return -1;
        }
    }
    return 0;
}
static int attrs_multi_valid__toto__c__check(const void *ptr, int n)
{
    for (int j = 0; j < n; j++) {
        attrs_multi_valid__my_union__t *val = &IOP_FIELD(attrs_multi_valid__my_union__t, ptr, j);

        switch (val->iop_tag) {
          case attrs_multi_valid__my_union__a__ft:
            break;
          case attrs_multi_valid__my_union__b__ft:
            iop_set_err("violation of constraint allow (b) on field c");
            return -1;
          case attrs_multi_valid__my_union__c__ft:
            break;
          case attrs_multi_valid__my_union__d__ft:
            iop_set_err("violation of constraint allow (d) on field c");
            return -1;
        }
    }
    return 0;
}
static int attrs_multi_valid__toto__f__check(const void *ptr, int n)
{
    for (int j = 0; j < n; j++) {
        attrs_multi_valid__my_enum__t val = IOP_FIELD(int32_t, ptr, j);

        switch (val) {
          case MY_ENUM_E:
            break;
          case MY_ENUM_F:
            break;
          case MY_ENUM_G:
            iop_set_err("violation of constraint allow (G) on field f");
            return -1;
        }
    }
    return 0;
}
static int attrs_multi_valid__toto__g__check(const void *ptr, int n)
{
    for (int j = 0; j < n; j++) {
        attrs_multi_valid__my_enum__t val = IOP_FIELD(int32_t, ptr, j);

        switch (val) {
          case MY_ENUM_E:
            break;
          case MY_ENUM_F:
            iop_set_err("violation of constraint allow (F) on field g");
            return -1;
          case MY_ENUM_G:
            break;
        }
    }
    return 0;
}
static iop_field_attrs_t const attrs_multi_valid__toto__desc_fields_attrs[] = {
    {
        .flags             = 0,
        .attrs_len         = 0,
        .check_constraints = &attrs_multi_valid__toto__b__check,
    },
    {
        .flags             = 0,
        .attrs_len         = 0,
        .check_constraints = &attrs_multi_valid__toto__c__check,
    },
    {
        .flags             = 0,
        .attrs_len         = 0,
        .check_constraints = &attrs_multi_valid__toto__f__check,
    },
    {
        .flags             = 0,
        .attrs_len         = 0,
        .check_constraints = &attrs_multi_valid__toto__g__check,
    },
};
static iop_field_t const attrs_multi_valid__toto__desc_fields[] = {
    {
        .name      = LSTR_IMMED("b"),
        .tag       = 1,
        .tag_len   = 0,
        .repeat    = IOP_R_REQUIRED,
        .type      = IOP_T_UNION,
        .data_offs = offsetof(attrs_multi_valid__toto__t, b),
        .flags     = 1,
        .size      = sizeof(attrs_multi_valid__my_union__t),
        .u1        = { .st_desc = &attrs_multi_valid__my_union__s },
    },
    {
        .name      = LSTR_IMMED("c"),
        .tag       = 2,
        .tag_len   = 0,
        .repeat    = IOP_R_REQUIRED,
        .type      = IOP_T_UNION,
        .data_offs = offsetof(attrs_multi_valid__toto__t, c),
        .flags     = 1,
        .size      = sizeof(attrs_multi_valid__my_union__t),
        .u1        = { .st_desc = &attrs_multi_valid__my_union__s },
    },
    {
        .name      = LSTR_IMMED("f"),
        .tag       = 3,
        .tag_len   = 0,
        .repeat    = IOP_R_REQUIRED,
        .type      = IOP_T_ENUM,
        .data_offs = offsetof(attrs_multi_valid__toto__t, f),
        .flags     = 1,
        .size      = fieldsizeof(attrs_multi_valid__toto__t, f),
        .u1        = { .en_desc = &attrs_multi_valid__my_enum__e },
    },
    {
        .name      = LSTR_IMMED("g"),
        .tag       = 4,
        .tag_len   = 0,
        .repeat    = IOP_R_DEFVAL,
        .type      = IOP_T_ENUM,
        .data_offs = offsetof(attrs_multi_valid__toto__t, g),
        .flags     = 1,
        .u0        = { .defval_enum = 0 },
        .size      = fieldsizeof(attrs_multi_valid__toto__t, g),
        .u1        = { .en_desc = &attrs_multi_valid__my_enum__e },
    },
};
const iop_struct_t attrs_multi_valid__toto__s = {
    .fullname   = LSTR_IMMED("attrs_multi_valid.Toto"),
    .fields     = attrs_multi_valid__toto__desc_fields,
    .ranges     = iop__ranges__2,
    .ranges_len = countof(iop__ranges__2) / 2,
    .fields_len = countof(attrs_multi_valid__toto__desc_fields),
    .size       = sizeof(attrs_multi_valid__toto__t),
    .flags      = 3,
    .fields_attrs = attrs_multi_valid__toto__desc_fields_attrs,
};
iop_struct_t const * const attrs_multi_valid__toto__sp = &attrs_multi_valid__toto__s;

/* }}} */
/* Package attrs_multi_valid {{{ */

static const iop_pkg_t *const attrs_multi_valid__deps[] = {
    NULL,
};

static const iop_enum_t *const attrs_multi_valid__enums[] = {
    &attrs_multi_valid__my_enum__e,
    NULL,
};

static const iop_struct_t *const attrs_multi_valid__structs[] = {
    &attrs_multi_valid__my_union__s,
    &attrs_multi_valid__toto__s,
    NULL,
};

static const iop_iface_t *const attrs_multi_valid__ifaces[] = {
    NULL,
};

static const iop_mod_t *const attrs_multi_valid__mods[] = {
    NULL,
};

iop_pkg_t const attrs_multi_valid__pkg = {
    .name    = LSTR_IMMED("attrs_multi_valid"),
    .deps    = attrs_multi_valid__deps,
    .enums   = attrs_multi_valid__enums,
    .structs = attrs_multi_valid__structs,
    .ifaces  = attrs_multi_valid__ifaces,
    .mods    = attrs_multi_valid__mods,
};
iop_pkg_t const * const attrs_multi_valid__pkgp = &attrs_multi_valid__pkg;

/* }}} */

