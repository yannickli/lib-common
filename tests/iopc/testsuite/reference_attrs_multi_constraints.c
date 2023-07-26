/***** THIS FILE IS AUTOGENERATED DO NOT MODIFY DIRECTLY ! *****/
#ifndef linux
#  warning "attrs_multi_constraints.iop assumed linux alignments"
#endif
#ifndef __x86_64__
#  warning "attrs_multi_constraints.iop assumed x86_64 alignments"
#endif

#include "attrs_multi_constraints.iop.h"

/* Structure attrs_multi_constraints.Test {{{ */

static int attrs_multi_constraints__test__a__check(const void *ptr, int n)
{
    for (int j = 0; j < n; j++) {
        uint32_t  val = IOP_FIELD(uint32_t , ptr, j);

        if (val > 21ULL) {
            if (n > 1) {
                iop_set_err("violation of constraint %s (%ju) on field %s[%d]: val=%ju",
                            "max", (uint64_t)21ULL, "a", j, (uint64_t)val);
            } else {
                iop_set_err("violation of constraint %s (%ju) on field %s: val=%ju",
                            "max", (uint64_t)21ULL, "a", (uint64_t)val);
            }
            return -1;
        }
        if (val < 7ULL) {
            if (n > 1) {
                iop_set_err("violation of constraint %s (%ju) on field %s[%d]: val=%ju",
                            "min", (uint64_t)7ULL, "a", j, (uint64_t)val);
            } else {
                iop_set_err("violation of constraint %s (%ju) on field %s: val=%ju",
                            "min", (uint64_t)7ULL, "a", (uint64_t)val);
            }
            return -1;
        }
    }
    return 0;
}
static iop_field_attr_t const attrs_multi_constraints__test__a__attrs[] = {
    {
        .type = 4,
        .args = (iop_field_attr_arg_t[]){ { .v.i64 = 21ULL } },
    },
    {
        .type = 3,
        .args = (iop_field_attr_arg_t[]){ { .v.i64 = 7ULL } },
    },
};
static int attrs_multi_constraints__test__b__check(const void *ptr, int n)
{
    for (int j = 0; j < n; j++) {
        uint32_t  val = IOP_FIELD(uint32_t , ptr, j);

        if (val > 42ULL) {
            if (n > 1) {
                iop_set_err("violation of constraint %s (%ju) on field %s[%d]: val=%ju",
                            "max", (uint64_t)42ULL, "b", j, (uint64_t)val);
            } else {
                iop_set_err("violation of constraint %s (%ju) on field %s: val=%ju",
                            "max", (uint64_t)42ULL, "b", (uint64_t)val);
            }
            return -1;
        }
        if (val < 5ULL) {
            if (n > 1) {
                iop_set_err("violation of constraint %s (%ju) on field %s[%d]: val=%ju",
                            "min", (uint64_t)5ULL, "b", j, (uint64_t)val);
            } else {
                iop_set_err("violation of constraint %s (%ju) on field %s: val=%ju",
                            "min", (uint64_t)5ULL, "b", (uint64_t)val);
            }
            return -1;
        }
    }
    return 0;
}
static iop_field_attr_t const attrs_multi_constraints__test__b__attrs[] = {
    {
        .type = 4,
        .args = (iop_field_attr_arg_t[]){ { .v.i64 = 42ULL } },
    },
    {
        .type = 3,
        .args = (iop_field_attr_arg_t[]){ { .v.i64 = 5ULL } },
    },
};
static int attrs_multi_constraints__test__c__check(const void *ptr, int n)
{
    for (int j = 0; j < n; j++) {
        uint64_t  val = IOP_FIELD(uint64_t , ptr, j);

        if (val < 5ULL) {
            if (n > 1) {
                iop_set_err("violation of constraint %s (%ju) on field %s[%d]: val=%ju",
                            "min", (uint64_t)5ULL, "c", j, (uint64_t)val);
            } else {
                iop_set_err("violation of constraint %s (%ju) on field %s: val=%ju",
                            "min", (uint64_t)5ULL, "c", (uint64_t)val);
            }
            return -1;
        }
    }
    return 0;
}
static iop_field_attr_t const attrs_multi_constraints__test__c__attrs[] = {
    {
        .type = 3,
        .args = (iop_field_attr_arg_t[]){ { .v.i64 = 5ULL } },
    },
};
static iop_field_attrs_t const attrs_multi_constraints__test__desc_fields_attrs[] = {
    {
        .flags             = 24,
        .attrs_len         = 2,
        .check_constraints = &attrs_multi_constraints__test__a__check,
        .attrs             = attrs_multi_constraints__test__a__attrs,
    },
    {
        .flags             = 24,
        .attrs_len         = 2,
        .check_constraints = &attrs_multi_constraints__test__b__check,
        .attrs             = attrs_multi_constraints__test__b__attrs,
    },
    {
        .flags             = 8,
        .attrs_len         = 1,
        .check_constraints = &attrs_multi_constraints__test__c__check,
        .attrs             = attrs_multi_constraints__test__c__attrs,
    },
};
static iop_field_t const attrs_multi_constraints__test__desc_fields[] = {
    {
        .name      = LSTR_IMMED("a"),
        .tag       = 1,
        .tag_len   = 0,
        .repeat    = IOP_R_REQUIRED,
        .type      = IOP_T_U32,
        .data_offs = offsetof(attrs_multi_constraints__test__t, a),
        .flags     = 1,
        .size      = fieldsizeof(attrs_multi_constraints__test__t, a),
    },
    {
        .name      = LSTR_IMMED("b"),
        .tag       = 2,
        .tag_len   = 0,
        .repeat    = IOP_R_REQUIRED,
        .type      = IOP_T_U32,
        .data_offs = offsetof(attrs_multi_constraints__test__t, b),
        .flags     = 1,
        .size      = fieldsizeof(attrs_multi_constraints__test__t, b),
    },
    {
        .name      = LSTR_IMMED("c"),
        .tag       = 3,
        .tag_len   = 0,
        .repeat    = IOP_R_REQUIRED,
        .type      = IOP_T_U64,
        .data_offs = offsetof(attrs_multi_constraints__test__t, c),
        .flags     = 1,
        .size      = fieldsizeof(attrs_multi_constraints__test__t, c),
    },
};
static int const iop__ranges__1[] = {
    0, 1,
    3,
};
const iop_struct_t attrs_multi_constraints__test__s = {
    .fullname   = LSTR_IMMED("attrs_multi_constraints.Test"),
    .fields     = attrs_multi_constraints__test__desc_fields,
    .ranges     = iop__ranges__1,
    .ranges_len = countof(iop__ranges__1) / 2,
    .fields_len = countof(attrs_multi_constraints__test__desc_fields),
    .size       = sizeof(attrs_multi_constraints__test__t),
    .flags      = 3,
    .fields_attrs = attrs_multi_constraints__test__desc_fields_attrs,
};
iop_struct_t const * const attrs_multi_constraints__test__sp = &attrs_multi_constraints__test__s;

/* }}} */
/* Structure attrs_multi_constraints.Test2 {{{ */

static int attrs_multi_constraints__test2__a__check(const void *ptr, int n)
{
    for (int j = 0; j < n; j++) {
        double    val = IOP_FIELD(double   , ptr, j);

        if (val < -1.00000000000000000e+00) {
            if (n > 1) {
                iop_set_err("violation of constraint %s (%.17e) on field %s[%d]: val=%.17e",
                            "min", (double)-1.00000000000000000e+00, "a", j, (double)val);
            } else {
                iop_set_err("violation of constraint %s (%.17e) on field %s: val=%.17e",
                            "min", (double)-1.00000000000000000e+00, "a", (double)val);
            }
            return -1;
        }
    }
    return 0;
}
static iop_field_attr_t const attrs_multi_constraints__test2__a__attrs[] = {
    {
        .type = 3,
        .args = (iop_field_attr_arg_t[]){ { .v.d = -1.00000000000000000e+00 } },
    },
};
static int attrs_multi_constraints__test2__b__check(const void *ptr, int n)
{
    for (int j = 0; j < n; j++) {
        double    val = IOP_FIELD(double   , ptr, j);

        if (val < 0.00000000000000000e+00) {
            if (n > 1) {
                iop_set_err("violation of constraint %s (%.17e) on field %s[%d]: val=%.17e",
                            "min", (double)0.00000000000000000e+00, "b", j, (double)val);
            } else {
                iop_set_err("violation of constraint %s (%.17e) on field %s: val=%.17e",
                            "min", (double)0.00000000000000000e+00, "b", (double)val);
            }
            return -1;
        }
    }
    return 0;
}
static iop_field_attr_t const attrs_multi_constraints__test2__b__attrs[] = {
    {
        .type = 3,
        .args = (iop_field_attr_arg_t[]){ { .v.d = 0.00000000000000000e+00 } },
    },
};
static iop_field_attrs_t const attrs_multi_constraints__test2__desc_fields_attrs[] = {
    {
        .flags             = 8,
        .attrs_len         = 1,
        .check_constraints = &attrs_multi_constraints__test2__a__check,
        .attrs             = attrs_multi_constraints__test2__a__attrs,
    },
    {
        .flags             = 8,
        .attrs_len         = 1,
        .check_constraints = &attrs_multi_constraints__test2__b__check,
        .attrs             = attrs_multi_constraints__test2__b__attrs,
    },
};
static iop_field_t const attrs_multi_constraints__test2__desc_fields[] = {
    {
        .name      = LSTR_IMMED("a"),
        .tag       = 1,
        .tag_len   = 0,
        .repeat    = IOP_R_REQUIRED,
        .type      = IOP_T_DOUBLE,
        .data_offs = offsetof(attrs_multi_constraints__test2__t, a),
        .flags     = 1,
        .size      = fieldsizeof(attrs_multi_constraints__test2__t, a),
    },
    {
        .name      = LSTR_IMMED("b"),
        .tag       = 2,
        .tag_len   = 0,
        .repeat    = IOP_R_REQUIRED,
        .type      = IOP_T_DOUBLE,
        .data_offs = offsetof(attrs_multi_constraints__test2__t, b),
        .flags     = 1,
        .size      = fieldsizeof(attrs_multi_constraints__test2__t, b),
    },
};
static int const iop__ranges__2[] = {
    0, 1,
    2,
};
const iop_struct_t attrs_multi_constraints__test2__s = {
    .fullname   = LSTR_IMMED("attrs_multi_constraints.Test2"),
    .fields     = attrs_multi_constraints__test2__desc_fields,
    .ranges     = iop__ranges__2,
    .ranges_len = countof(iop__ranges__2) / 2,
    .fields_len = countof(attrs_multi_constraints__test2__desc_fields),
    .size       = sizeof(attrs_multi_constraints__test2__t),
    .flags      = 3,
    .fields_attrs = attrs_multi_constraints__test2__desc_fields_attrs,
};
iop_struct_t const * const attrs_multi_constraints__test2__sp = &attrs_multi_constraints__test2__s;

/* }}} */
/* Structure attrs_multi_constraints.StrTest {{{ */

static int attrs_multi_constraints__str_test__a__check(const void *ptr, int n)
{
    for (int j = 0; j < n; j++) {
        lstr_t    val = IOP_FIELD(lstr_t   , ptr, j);

        if (val.len > 21) {
            iop_set_err("violation of constraint %s (%d) on field %s: length=%d",
                        "maxLength", 21, "a", val.len);
            return -1;
        }
        if (val.len < 7) {
            iop_set_err("violation of constraint %s (%d) on field %s: length=%d",
                        "minLength", 7, "a", val.len);
            return -1;
        }
    }
    return 0;
}
static iop_field_attr_t const attrs_multi_constraints__str_test__a__attrs[] = {
    {
        .type = 8,
        .args = (iop_field_attr_arg_t[]){ { .v.i64 = 21LL } },
    },
    {
        .type = 7,
        .args = (iop_field_attr_arg_t[]){ { .v.i64 = 7LL } },
    },
};
static int attrs_multi_constraints__str_test__b__check(const void *ptr, int n)
{
    for (int j = 0; j < n; j++) {
        lstr_t    val = IOP_FIELD(lstr_t   , ptr, j);

        if (val.len != 42) {
            iop_set_err("violation of constraint %s (%d) on field %s: length=%d",
                        "length", 42, "b", val.len);
            return -1;
        }
    }
    return 0;
}
static iop_field_attr_t const attrs_multi_constraints__str_test__b__attrs[] = {
    {
        .type = 7,
        .args = (iop_field_attr_arg_t[]){ { .v.i64 = 42LL } },
    },
    {
        .type = 8,
        .args = (iop_field_attr_arg_t[]){ { .v.i64 = 42LL } },
    },
};
static iop_field_attrs_t const attrs_multi_constraints__str_test__desc_fields_attrs[] = {
    {
        .flags             = 384,
        .attrs_len         = 2,
        .check_constraints = &attrs_multi_constraints__str_test__a__check,
        .attrs             = attrs_multi_constraints__str_test__a__attrs,
    },
    {
        .flags             = 384,
        .attrs_len         = 2,
        .check_constraints = &attrs_multi_constraints__str_test__b__check,
        .attrs             = attrs_multi_constraints__str_test__b__attrs,
    },
};
static iop_field_t const attrs_multi_constraints__str_test__desc_fields[] = {
    {
        .name      = LSTR_IMMED("a"),
        .tag       = 1,
        .tag_len   = 0,
        .repeat    = IOP_R_REQUIRED,
        .type      = IOP_T_STRING,
        .data_offs = offsetof(attrs_multi_constraints__str_test__t, a),
        .flags     = 1,
        .size      = fieldsizeof(attrs_multi_constraints__str_test__t, a),
    },
    {
        .name      = LSTR_IMMED("b"),
        .tag       = 2,
        .tag_len   = 0,
        .repeat    = IOP_R_REQUIRED,
        .type      = IOP_T_STRING,
        .data_offs = offsetof(attrs_multi_constraints__str_test__t, b),
        .flags     = 1,
        .size      = fieldsizeof(attrs_multi_constraints__str_test__t, b),
    },
};
const iop_struct_t attrs_multi_constraints__str_test__s = {
    .fullname   = LSTR_IMMED("attrs_multi_constraints.StrTest"),
    .fields     = attrs_multi_constraints__str_test__desc_fields,
    .ranges     = iop__ranges__2,
    .ranges_len = countof(iop__ranges__2) / 2,
    .fields_len = countof(attrs_multi_constraints__str_test__desc_fields),
    .size       = sizeof(attrs_multi_constraints__str_test__t),
    .flags      = 3,
    .fields_attrs = attrs_multi_constraints__str_test__desc_fields_attrs,
};
iop_struct_t const * const attrs_multi_constraints__str_test__sp = &attrs_multi_constraints__str_test__s;

/* }}} */
/* Structure attrs_multi_constraints.TabTest {{{ */

static int attrs_multi_constraints__tab_test__a__check(const void *ptr, int n)
{
    if (n > 21) {
        iop_set_err("violation of constraint %s (%d) on field %s: length=%d",
                    "maxOccurs", 21, "a", n);
        return -1;
    }
    if (n < 7) {
        iop_set_err("violation of constraint %s (%d) on field %s: length=%d",
                    "minOccurs", 7, "a", n);
        return -1;
    }
    return 0;
}
static iop_field_attr_t const attrs_multi_constraints__tab_test__a__attrs[] = {
    {
        .type = 1,
        .args = (iop_field_attr_arg_t[]){ { .v.i64 = 21LL } },
    },
    {
        .type = 0,
        .args = (iop_field_attr_arg_t[]){ { .v.i64 = 7LL } },
    },
};
static iop_field_attrs_t const attrs_multi_constraints__tab_test__desc_fields_attrs[] = {
    {
        .flags             = 3,
        .attrs_len         = 2,
        .check_constraints = &attrs_multi_constraints__tab_test__a__check,
        .attrs             = attrs_multi_constraints__tab_test__a__attrs,
    },
};
static iop_field_t const attrs_multi_constraints__tab_test__desc_fields[] = {
    {
        .name      = LSTR_IMMED("a"),
        .tag       = 1,
        .tag_len   = 0,
        .repeat    = IOP_R_REPEATED,
        .type      = IOP_T_I32,
        .data_offs = offsetof(attrs_multi_constraints__tab_test__t, a),
        .flags     = 3,
        .size      = fieldsizeof(attrs_multi_constraints__tab_test__t, a.tab[0]),
    },
};
static int const iop__ranges__3[] = {
    0, 1,
    1,
};
const iop_struct_t attrs_multi_constraints__tab_test__s = {
    .fullname   = LSTR_IMMED("attrs_multi_constraints.TabTest"),
    .fields     = attrs_multi_constraints__tab_test__desc_fields,
    .ranges     = iop__ranges__3,
    .ranges_len = countof(iop__ranges__3) / 2,
    .fields_len = countof(attrs_multi_constraints__tab_test__desc_fields),
    .size       = sizeof(attrs_multi_constraints__tab_test__t),
    .flags      = 3,
    .fields_attrs = attrs_multi_constraints__tab_test__desc_fields_attrs,
};
iop_struct_t const * const attrs_multi_constraints__tab_test__sp = &attrs_multi_constraints__tab_test__s;

/* }}} */
/* Typedef attrs_multi_constraints.Min3 {{{ */

iop_typedef_t const attrs_multi_constraints__min3__td = {
    .fullname = LSTR_IMMED("attrs_multi_constraints.Min3"),
    .type = IOP_T_U64,
};
iop_typedef_t const * const attrs_multi_constraints__min3__tdp = &attrs_multi_constraints__min3__td;

/* }}} */
/* Typedef attrs_multi_constraints.Min5 {{{ */

iop_typedef_t const attrs_multi_constraints__min5__td = {
    .fullname = LSTR_IMMED("attrs_multi_constraints.Min5"),
    .type = IOP_T_U64,
};
iop_typedef_t const * const attrs_multi_constraints__min5__tdp = &attrs_multi_constraints__min5__td;

/* }}} */
/* Typedef attrs_multi_constraints.Digit {{{ */

iop_typedef_t const attrs_multi_constraints__digit__td = {
    .fullname = LSTR_IMMED("attrs_multi_constraints.Digit"),
    .type = IOP_T_U32,
};
iop_typedef_t const * const attrs_multi_constraints__digit__tdp = &attrs_multi_constraints__digit__td;

/* }}} */
/* Typedef attrs_multi_constraints.Neg {{{ */

iop_typedef_t const attrs_multi_constraints__neg__td = {
    .fullname = LSTR_IMMED("attrs_multi_constraints.Neg"),
    .type = IOP_T_DOUBLE,
};
iop_typedef_t const * const attrs_multi_constraints__neg__tdp = &attrs_multi_constraints__neg__td;

/* }}} */
/* Typedef attrs_multi_constraints.Zero {{{ */

iop_typedef_t const attrs_multi_constraints__zero__td = {
    .fullname = LSTR_IMMED("attrs_multi_constraints.Zero"),
    .type = IOP_T_DOUBLE,
};
iop_typedef_t const * const attrs_multi_constraints__zero__tdp = &attrs_multi_constraints__zero__td;

/* }}} */
/* Typedef attrs_multi_constraints.ExStr {{{ */

iop_typedef_t const attrs_multi_constraints__ex_str__td = {
    .fullname = LSTR_IMMED("attrs_multi_constraints.ExStr"),
    .type = IOP_T_STRING,
};
iop_typedef_t const * const attrs_multi_constraints__ex_str__tdp = &attrs_multi_constraints__ex_str__td;

/* }}} */
/* Typedef attrs_multi_constraints.ExTab {{{ */

iop_typedef_t const attrs_multi_constraints__ex_tab__td = {
    .fullname = LSTR_IMMED("attrs_multi_constraints.ExTab"),
    .type = IOP_T_I32,
};
iop_typedef_t const * const attrs_multi_constraints__ex_tab__tdp = &attrs_multi_constraints__ex_tab__td;

/* }}} */
/* Package attrs_multi_constraints {{{ */

static const iop_pkg_t *const attrs_multi_constraints__deps[] = {
    NULL,
};

static const iop_enum_t *const attrs_multi_constraints__enums[] = {
    NULL,
};

static const iop_struct_t *const attrs_multi_constraints__structs[] = {
    &attrs_multi_constraints__test__s,
    &attrs_multi_constraints__test2__s,
    &attrs_multi_constraints__str_test__s,
    &attrs_multi_constraints__tab_test__s,
    NULL,
};

static const iop_iface_t *const attrs_multi_constraints__ifaces[] = {
    NULL,
};

static const iop_mod_t *const attrs_multi_constraints__mods[] = {
    NULL,
};

static const iop_typedef_t *const attrs_multi_constraints__td[] = {
    &attrs_multi_constraints__min3__td,
    &attrs_multi_constraints__min5__td,
    &attrs_multi_constraints__digit__td,
    &attrs_multi_constraints__neg__td,
    &attrs_multi_constraints__zero__td,
    &attrs_multi_constraints__ex_str__td,
    &attrs_multi_constraints__ex_tab__td,
    NULL,
};

iop_pkg_t const attrs_multi_constraints__pkg = {
    .name     = LSTR_IMMED("attrs_multi_constraints"),
    .deps     = attrs_multi_constraints__deps,
    .enums    = attrs_multi_constraints__enums,
    .structs  = attrs_multi_constraints__structs,
    .ifaces   = attrs_multi_constraints__ifaces,
    .mods     = attrs_multi_constraints__mods,
    .typedefs = attrs_multi_constraints__td,
};
iop_pkg_t const * const attrs_multi_constraints__pkgp = &attrs_multi_constraints__pkg;

/* }}} */

