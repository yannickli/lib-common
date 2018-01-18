/***** THIS FILE IS AUTOGENERATED DO NOT MODIFY DIRECTLY ! *****/
#if !defined(linux) && !defined(__APPLE__)
#  warning "iop.iop assumed linux alignments"
#endif
#ifndef __x86_64__
#  warning "iop.iop assumed x86_64 alignments"
#endif

#include "iop.iop.h"

/* Enum iop.IntSize {{{ */

static int const iop__int_size__values[] = {
 0, 1, 2, 3,
};
static int const iop__ranges__1[] = {
    0, 0,
    4,
};
static const lstr_t iop__int_size__names[] = {
    LSTR_IMMED("S8"),
    LSTR_IMMED("S16"),
    LSTR_IMMED("S32"),
    LSTR_IMMED("S64"),
};
iop_enum_t const iop__int_size__e = {
    .name         = LSTR_IMMED("IntSize"),
    .fullname     = LSTR_IMMED("iop.IntSize"),
    .names        = iop__int_size__names,
    .values       = iop__int_size__values,
    .ranges       = iop__ranges__1,
    .ranges_len   = countof(iop__ranges__1) / 2,
    .enum_len     = 4,
};
iop_enum_t const * const iop__int_size__ep = &iop__int_size__e;

/* }}} */
/* Enum iop.StringType {{{ */

static int const iop__string_type__values[] = {
 0, 1, 2,
};
static int const iop__ranges__2[] = {
    0, 0,
    3,
};
static const lstr_t iop__string_type__names[] = {
    LSTR_IMMED("STRING"),
    LSTR_IMMED("BYTES"),
    LSTR_IMMED("XML"),
};
iop_enum_t const iop__string_type__e = {
    .name         = LSTR_IMMED("StringType"),
    .fullname     = LSTR_IMMED("iop.StringType"),
    .names        = iop__string_type__names,
    .values       = iop__string_type__values,
    .ranges       = iop__ranges__2,
    .ranges_len   = countof(iop__ranges__2) / 2,
    .enum_len     = 3,
};
iop_enum_t const * const iop__string_type__ep = &iop__string_type__e;

/* }}} */
/* Structure iop.IntType {{{ */

static iop_field_t const iop__int_type__desc_fields[] = {
    {
        .name      = LSTR_IMMED("isSigned"),
        .tag       = 1,
        .tag_len   = 0,
        .repeat    = IOP_R_REQUIRED,
        .type      = IOP_T_BOOL,
        .data_offs = offsetof(iop__int_type__t, is_signed),
        .size      = fieldsizeof(iop__int_type__t, is_signed),
    },
    {
        .name      = LSTR_IMMED("size"),
        .tag       = 2,
        .tag_len   = 0,
        .repeat    = IOP_R_REQUIRED,
        .type      = IOP_T_ENUM,
        .data_offs = offsetof(iop__int_type__t, size),
        .size      = fieldsizeof(iop__int_type__t, size),
        .u1        = { .en_desc = &iop__int_size__e },
    },
};
static int const iop__ranges__3[] = {
    0, 1,
    2,
};
const iop_struct_t iop__int_type__s = {
    .fullname   = LSTR_IMMED("iop.IntType"),
    .fields     = iop__int_type__desc_fields,
    .ranges     = iop__ranges__3,
    .ranges_len = countof(iop__ranges__3) / 2,
    .fields_len = countof(iop__int_type__desc_fields),
    .size       = sizeof(iop__int_type__t),
};
iop_struct_t const * const iop__int_type__sp = &iop__int_type__s;

/* }}} */
/* Union iop.IopType {{{ */

static int iop__iop_type__type_name__check(const void *ptr, int n)
{
    for (int j = 0; j < n; j++) {
        lstr_t    val = IOP_FIELD(lstr_t   , ptr, j);

        for (int c = 0; c < val.len; c++) {
            switch (val.s[c]) {
                case 'a' ... 'z':
                case 'A' ... 'Z':
                case '0' ... '9':
                case '.':
                    break;
                default:
                    iop_set_err("violation of constraint %s (%s) on field %s: %*pM",
                                "pattern", "[a-zA-Z0-9\\.]*", "typeName", LSTR_FMT_ARG(val));
                    return -1;
            }
        }
        if (val.len < 1) {
            iop_set_err("violation of constraint %s (%d) on field %s: length=%d",
                        "minLength", 1, "typeName", val.len);
            return -1;
        }
    }
    return 0;
}
static iop_field_attr_t const iop__iop_type__type_name__attrs[] = {
    {
        .type = 9,
        .args = (iop_field_attr_arg_t[]){ { .v.s = LSTR_IMMED("[a-zA-Z0-9\\.]*") } },
    },
    {
        .type = 7,
        .args = (iop_field_attr_arg_t[]){ { .v.i64 = 1LL } },
    },
};
static iop_field_attrs_t const iop__iop_type__desc_fields_attrs[] = {
    {
        .flags             = 0,
        .attrs_len         = 0,
    },
    {
        .flags             = 0,
        .attrs_len         = 0,
    },
    {
        .flags             = 0,
        .attrs_len         = 0,
    },
    {
        .flags             = 0,
        .attrs_len         = 0,
    },
    {
        .flags             = 0,
        .attrs_len         = 0,
    },
    {
        .flags             = 640,
        .attrs_len         = 2,
        .check_constraints = &iop__iop_type__type_name__check,
        .attrs             = iop__iop_type__type_name__attrs,
    },
    {
        .flags             = 0,
        .attrs_len         = 0,
    },
};
static iop_field_t const iop__iop_type__desc_fields[] = {
    {
        .name      = LSTR_IMMED("i"),
        .tag       = 1,
        .tag_len   = 0,
        .repeat    = IOP_R_REQUIRED,
        .type      = IOP_T_STRUCT,
        .data_offs = offsetof(iop__iop_type__t, i),
        .size      = sizeof(iop__int_type__t),
        .u1        = { .st_desc = &iop__int_type__s },
    },
    {
        .name      = LSTR_IMMED("b"),
        .tag       = 2,
        .tag_len   = 0,
        .repeat    = IOP_R_REQUIRED,
        .type      = IOP_T_VOID,
        .data_offs = offsetof(iop__iop_type__t, i),
    },
    {
        .name      = LSTR_IMMED("d"),
        .tag       = 3,
        .tag_len   = 0,
        .repeat    = IOP_R_REQUIRED,
        .type      = IOP_T_VOID,
        .data_offs = offsetof(iop__iop_type__t, i),
    },
    {
        .name      = LSTR_IMMED("s"),
        .tag       = 4,
        .tag_len   = 0,
        .repeat    = IOP_R_REQUIRED,
        .type      = IOP_T_ENUM,
        .data_offs = offsetof(iop__iop_type__t, s),
        .size      = fieldsizeof(iop__iop_type__t, s),
        .u1        = { .en_desc = &iop__string_type__e },
    },
    {
        .name      = LSTR_IMMED("v"),
        .tag       = 5,
        .tag_len   = 0,
        .repeat    = IOP_R_REQUIRED,
        .type      = IOP_T_VOID,
        .data_offs = offsetof(iop__iop_type__t, i),
    },
    {
        .name      = LSTR_IMMED("typeName"),
        .tag       = 6,
        .tag_len   = 0,
        .repeat    = IOP_R_REQUIRED,
        .type      = IOP_T_STRING,
        .data_offs = offsetof(iop__iop_type__t, type_name),
        .flags     = 1,
        .size      = fieldsizeof(iop__iop_type__t, type_name),
    },
    {
        .name      = LSTR_IMMED("array"),
        .tag       = 7,
        .tag_len   = 0,
        .repeat    = IOP_R_REQUIRED,
        .type      = IOP_T_UNION,
        .data_offs = offsetof(iop__iop_type__t, array),
        .flags     = 4,
        .size      = sizeof(iop__iop_type__t),
        .u1        = { .st_desc = &iop__iop_type__s },
    },
};
static int const iop__ranges__4[] = {
    0, 1,
    7,
};
const iop_struct_t iop__iop_type__s = {
    .fullname   = LSTR_IMMED("iop.IopType"),
    .fields     = iop__iop_type__desc_fields,
    .ranges     = iop__ranges__4,
    .ranges_len = countof(iop__ranges__4) / 2,
    .fields_len = countof(iop__iop_type__desc_fields),
    .size       = sizeof(iop__iop_type__t),
    .flags      = 3,
    .is_union   = true,
    .fields_attrs = iop__iop_type__desc_fields_attrs,
};
iop_struct_t const * const iop__iop_type__sp = &iop__iop_type__s;

/* }}} */
/* Class iop.PackageElem {{{ */

static int iop__package_elem__name__check(const void *ptr, int n)
{
    for (int j = 0; j < n; j++) {
        lstr_t    val = IOP_FIELD(lstr_t   , ptr, j);

        for (int c = 0; c < val.len; c++) {
            switch (val.s[c]) {
                case 'a' ... 'z':
                case 'A' ... 'Z':
                case '0' ... '9':
                    break;
                default:
                    iop_set_err("violation of constraint %s (%s) on field %s: %*pM",
                                "pattern", "[a-zA-Z0-9]*", "name", LSTR_FMT_ARG(val));
                    return -1;
            }
        }
        if (val.len < 1) {
            iop_set_err("violation of constraint %s (%d) on field %s: length=%d",
                        "minLength", 1, "name", val.len);
            return -1;
        }
    }
    return 0;
}
static iop_field_attr_t const iop__package_elem__name__attrs[] = {
    {
        .type = 9,
        .args = (iop_field_attr_arg_t[]){ { .v.s = LSTR_IMMED("[a-zA-Z0-9]*") } },
    },
    {
        .type = 7,
        .args = (iop_field_attr_arg_t[]){ { .v.i64 = 1LL } },
    },
};
static iop_field_attrs_t const iop__package_elem__desc_fields_attrs[] = {
    {
        .flags             = 640,
        .attrs_len         = 2,
        .check_constraints = &iop__package_elem__name__check,
        .attrs             = iop__package_elem__name__attrs,
    },
};
static iop_field_t const iop__package_elem__desc_fields[] = {
    {
        .name      = LSTR_IMMED("name"),
        .tag       = 1,
        .tag_len   = 0,
        .repeat    = IOP_R_REQUIRED,
        .type      = IOP_T_STRING,
        .data_offs = offsetof(iop__package_elem__t, name),
        .flags     = 1,
        .size      = fieldsizeof(iop__package_elem__t, name),
    },
};
static int const iop__ranges__5[] = {
    0, 1,
    1,
};
static const iop_class_attrs_t iop__package_elem__class_s = {
    .is_abstract       = true,
    .class_id          = 0,
};
const iop_struct_t iop__package_elem__s = {
    .fullname   = LSTR_IMMED("iop.PackageElem"),
    .fields     = iop__package_elem__desc_fields,
    .ranges     = iop__ranges__5,
    .ranges_len = countof(iop__ranges__5) / 2,
    .fields_len = countof(iop__package_elem__desc_fields),
    .size       = sizeof(iop__package_elem__t),
    .flags      = 15,
    .is_union   = false,
    .st_attrs   = NULL,
    .fields_attrs = iop__package_elem__desc_fields_attrs,
    {
        .class_attrs  = &iop__package_elem__class_s,
    }
};
iop_struct_t const * const iop__package_elem__sp = &iop__package_elem__s;

/* }}} */
/* Union iop.Value {{{ */

static iop_field_t const iop__value__desc_fields[] = {
    {
        .name      = LSTR_IMMED("i"),
        .tag       = 1,
        .tag_len   = 0,
        .repeat    = IOP_R_REQUIRED,
        .type      = IOP_T_I64,
        .data_offs = offsetof(iop__value__t, i),
        .size      = fieldsizeof(iop__value__t, i),
    },
    {
        .name      = LSTR_IMMED("u"),
        .tag       = 2,
        .tag_len   = 0,
        .repeat    = IOP_R_REQUIRED,
        .type      = IOP_T_U64,
        .data_offs = offsetof(iop__value__t, u),
        .size      = fieldsizeof(iop__value__t, u),
    },
    {
        .name      = LSTR_IMMED("d"),
        .tag       = 3,
        .tag_len   = 0,
        .repeat    = IOP_R_REQUIRED,
        .type      = IOP_T_DOUBLE,
        .data_offs = offsetof(iop__value__t, d),
        .size      = fieldsizeof(iop__value__t, d),
    },
    {
        .name      = LSTR_IMMED("s"),
        .tag       = 4,
        .tag_len   = 0,
        .repeat    = IOP_R_REQUIRED,
        .type      = IOP_T_STRING,
        .data_offs = offsetof(iop__value__t, s),
        .size      = fieldsizeof(iop__value__t, s),
    },
    {
        .name      = LSTR_IMMED("b"),
        .tag       = 5,
        .tag_len   = 0,
        .repeat    = IOP_R_REQUIRED,
        .type      = IOP_T_BOOL,
        .data_offs = offsetof(iop__value__t, b),
        .size      = fieldsizeof(iop__value__t, b),
    },
};
static int const iop__ranges__6[] = {
    0, 1,
    5,
};
const iop_struct_t iop__value__s = {
    .fullname   = LSTR_IMMED("iop.Value"),
    .fields     = iop__value__desc_fields,
    .ranges     = iop__ranges__6,
    .ranges_len = countof(iop__ranges__6) / 2,
    .fields_len = countof(iop__value__desc_fields),
    .size       = sizeof(iop__value__t),
    .is_union   = true,
};
iop_struct_t const * const iop__value__sp = &iop__value__s;

/* }}} */
/* Structure iop.OptInfo {{{ */

static iop_field_t const iop__opt_info__desc_fields[] = {
    {
        .name      = LSTR_IMMED("defVal"),
        .tag       = 1,
        .tag_len   = 0,
        .repeat    = IOP_R_OPTIONAL,
        .type      = IOP_T_UNION,
        .data_offs = offsetof(iop__opt_info__t, def_val),
        .size      = sizeof(iop__value__t),
        .u1        = { .st_desc = &iop__value__s },
    },
};
const iop_struct_t iop__opt_info__s = {
    .fullname   = LSTR_IMMED("iop.OptInfo"),
    .fields     = iop__opt_info__desc_fields,
    .ranges     = iop__ranges__5,
    .ranges_len = countof(iop__ranges__5) / 2,
    .fields_len = countof(iop__opt_info__desc_fields),
    .size       = sizeof(iop__opt_info__t),
};
iop_struct_t const * const iop__opt_info__sp = &iop__opt_info__s;

/* }}} */
/* Structure iop.Field {{{ */

static int iop__field__name__check(const void *ptr, int n)
{
    for (int j = 0; j < n; j++) {
        lstr_t    val = IOP_FIELD(lstr_t   , ptr, j);

        for (int c = 0; c < val.len; c++) {
            switch (val.s[c]) {
                case 'a' ... 'z':
                case 'A' ... 'Z':
                case '0' ... '9':
                    break;
                default:
                    iop_set_err("violation of constraint %s (%s) on field %s: %*pM",
                                "pattern", "[a-zA-Z0-9]*", "name", LSTR_FMT_ARG(val));
                    return -1;
            }
        }
        if (val.len < 1) {
            iop_set_err("violation of constraint %s (%d) on field %s: length=%d",
                        "minLength", 1, "name", val.len);
            return -1;
        }
    }
    return 0;
}
static iop_field_attr_t const iop__field__name__attrs[] = {
    {
        .type = 9,
        .args = (iop_field_attr_arg_t[]){ { .v.s = LSTR_IMMED("[a-zA-Z0-9]*") } },
    },
    {
        .type = 7,
        .args = (iop_field_attr_arg_t[]){ { .v.i64 = 1LL } },
    },
};
static iop_field_attrs_t const iop__field__desc_fields_attrs[] = {
    {
        .flags             = 640,
        .attrs_len         = 2,
        .check_constraints = &iop__field__name__check,
        .attrs             = iop__field__name__attrs,
    },
    {
        .flags             = 0,
        .attrs_len         = 0,
    },
    {
        .flags             = 0,
        .attrs_len         = 0,
    },
    {
        .flags             = 0,
        .attrs_len         = 0,
    },
    {
        .flags             = 0,
        .attrs_len         = 0,
    },
};
static iop_field_t const iop__field__desc_fields[] = {
    {
        .name      = LSTR_IMMED("name"),
        .tag       = 1,
        .tag_len   = 0,
        .repeat    = IOP_R_REQUIRED,
        .type      = IOP_T_STRING,
        .data_offs = offsetof(iop__field__t, name),
        .flags     = 1,
        .size      = fieldsizeof(iop__field__t, name),
    },
    {
        .name      = LSTR_IMMED("type"),
        .tag       = 2,
        .tag_len   = 0,
        .repeat    = IOP_R_REQUIRED,
        .type      = IOP_T_UNION,
        .data_offs = offsetof(iop__field__t, type),
        .size      = sizeof(iop__iop_type__t),
        .u1        = { .st_desc = &iop__iop_type__s },
    },
    {
        .name      = LSTR_IMMED("optional"),
        .tag       = 3,
        .tag_len   = 0,
        .repeat    = IOP_R_OPTIONAL,
        .type      = IOP_T_STRUCT,
        .data_offs = offsetof(iop__field__t, optional),
        .size      = sizeof(iop__opt_info__t),
        .u1        = { .st_desc = &iop__opt_info__s },
    },
    {
        .name      = LSTR_IMMED("tag"),
        .tag       = 4,
        .tag_len   = 0,
        .repeat    = IOP_R_OPTIONAL,
        .type      = IOP_T_U16,
        .data_offs = offsetof(iop__field__t, tag),
        .size      = fieldsizeof(iop__field__t, tag),
    },
    {
        .name      = LSTR_IMMED("isReference"),
        .tag       = 5,
        .tag_len   = 0,
        .repeat    = IOP_R_OPTIONAL,
        .type      = IOP_T_VOID,
        .data_offs = offsetof(iop__field__t, is_reference),
        .size      = fieldsizeof(iop__field__t, is_reference),
    },
};
const iop_struct_t iop__field__s = {
    .fullname   = LSTR_IMMED("iop.Field"),
    .fields     = iop__field__desc_fields,
    .ranges     = iop__ranges__6,
    .ranges_len = countof(iop__ranges__6) / 2,
    .fields_len = countof(iop__field__desc_fields),
    .size       = sizeof(iop__field__t),
    .flags      = 3,
    .fields_attrs = iop__field__desc_fields_attrs,
};
iop_struct_t const * const iop__field__sp = &iop__field__s;

/* }}} */
/* Class iop.Structure {{{ */

static iop_field_t const iop__structure__desc_fields[] = {
};
static int const iop__ranges__7[] = {
    0,
};
static const iop_class_attrs_t iop__structure__class_s = {
    .parent            = &iop__package_elem__s,
    .is_abstract       = true,
    .class_id          = 1,
};
const iop_struct_t iop__structure__s = {
    .fullname   = LSTR_IMMED("iop.Structure"),
    .fields     = iop__structure__desc_fields,
    .ranges     = iop__ranges__7,
    .ranges_len = countof(iop__ranges__7) / 2,
    .fields_len = countof(iop__structure__desc_fields),
    .size       = sizeof(iop__structure__t),
    .flags      = 13,
    .is_union   = false,
    .st_attrs   = NULL,
    .fields_attrs = NULL,
    {
        .class_attrs  = &iop__structure__class_s,
    }
};
iop_struct_t const * const iop__structure__sp = &iop__structure__s;

/* }}} */
/* Class iop.Struct {{{ */

static iop_field_t const iop__struct__desc_fields[] = {
    {
        .name      = LSTR_IMMED("fields"),
        .tag       = 1,
        .tag_len   = 0,
        .repeat    = IOP_R_REPEATED,
        .type      = IOP_T_STRUCT,
        .data_offs = offsetof(iop__struct__t, fields),
        .size      = sizeof(iop__field__t),
        .u1        = { .st_desc = &iop__field__s },
    },
};
static const iop_class_attrs_t iop__struct__class_s = {
    .parent            = &iop__structure__s,
    .class_id          = 2,
};
const iop_struct_t iop__struct__s = {
    .fullname   = LSTR_IMMED("iop.Struct"),
    .fields     = iop__struct__desc_fields,
    .ranges     = iop__ranges__5,
    .ranges_len = countof(iop__ranges__5) / 2,
    .fields_len = countof(iop__struct__desc_fields),
    .size       = sizeof(iop__struct__t),
    .flags      = 15,
    .is_union   = false,
    .st_attrs   = NULL,
    .fields_attrs = NULL,
    {
        .class_attrs  = &iop__struct__class_s,
    }
};
iop_struct_t const * const iop__struct__sp = &iop__struct__s;

/* }}} */
/* Structure iop.EnumVal {{{ */

static int iop__enum_val__name__check(const void *ptr, int n)
{
    for (int j = 0; j < n; j++) {
        lstr_t    val = IOP_FIELD(lstr_t   , ptr, j);

        for (int c = 0; c < val.len; c++) {
            switch (val.s[c]) {
                case 'A' ... 'Z':
                case '0' ... '9':
                case '_':
                    break;
                default:
                    iop_set_err("violation of constraint %s (%s) on field %s: %*pM",
                                "pattern", "[A-Z0-9_]*", "name", LSTR_FMT_ARG(val));
                    return -1;
            }
        }
        if (val.len < 1) {
            iop_set_err("violation of constraint %s (%d) on field %s: length=%d",
                        "minLength", 1, "name", val.len);
            return -1;
        }
    }
    return 0;
}
static iop_field_attr_t const iop__enum_val__name__attrs[] = {
    {
        .type = 9,
        .args = (iop_field_attr_arg_t[]){ { .v.s = LSTR_IMMED("[A-Z0-9_]*") } },
    },
    {
        .type = 7,
        .args = (iop_field_attr_arg_t[]){ { .v.i64 = 1LL } },
    },
};
static iop_field_attrs_t const iop__enum_val__desc_fields_attrs[] = {
    {
        .flags             = 640,
        .attrs_len         = 2,
        .check_constraints = &iop__enum_val__name__check,
        .attrs             = iop__enum_val__name__attrs,
    },
    {
        .flags             = 0,
        .attrs_len         = 0,
    },
};
static iop_field_t const iop__enum_val__desc_fields[] = {
    {
        .name      = LSTR_IMMED("name"),
        .tag       = 1,
        .tag_len   = 0,
        .repeat    = IOP_R_REQUIRED,
        .type      = IOP_T_STRING,
        .data_offs = offsetof(iop__enum_val__t, name),
        .flags     = 1,
        .size      = fieldsizeof(iop__enum_val__t, name),
    },
    {
        .name      = LSTR_IMMED("val"),
        .tag       = 2,
        .tag_len   = 0,
        .repeat    = IOP_R_OPTIONAL,
        .type      = IOP_T_I32,
        .data_offs = offsetof(iop__enum_val__t, val),
        .size      = fieldsizeof(iop__enum_val__t, val),
    },
};
const iop_struct_t iop__enum_val__s = {
    .fullname   = LSTR_IMMED("iop.EnumVal"),
    .fields     = iop__enum_val__desc_fields,
    .ranges     = iop__ranges__3,
    .ranges_len = countof(iop__ranges__3) / 2,
    .fields_len = countof(iop__enum_val__desc_fields),
    .size       = sizeof(iop__enum_val__t),
    .flags      = 3,
    .fields_attrs = iop__enum_val__desc_fields_attrs,
};
iop_struct_t const * const iop__enum_val__sp = &iop__enum_val__s;

/* }}} */
/* Class iop.Enum {{{ */

static iop_field_t const iop__enum__desc_fields[] = {
    {
        .name      = LSTR_IMMED("values"),
        .tag       = 1,
        .tag_len   = 0,
        .repeat    = IOP_R_REPEATED,
        .type      = IOP_T_STRUCT,
        .data_offs = offsetof(iop__enum__t, values),
        .size      = sizeof(iop__enum_val__t),
        .u1        = { .st_desc = &iop__enum_val__s },
    },
};
static const iop_class_attrs_t iop__enum__class_s = {
    .parent            = &iop__package_elem__s,
    .class_id          = 3,
};
const iop_struct_t iop__enum__s = {
    .fullname   = LSTR_IMMED("iop.Enum"),
    .fields     = iop__enum__desc_fields,
    .ranges     = iop__ranges__5,
    .ranges_len = countof(iop__ranges__5) / 2,
    .fields_len = countof(iop__enum__desc_fields),
    .size       = sizeof(iop__enum__t),
    .flags      = 15,
    .is_union   = false,
    .st_attrs   = NULL,
    .fields_attrs = NULL,
    {
        .class_attrs  = &iop__enum__class_s,
    }
};
iop_struct_t const * const iop__enum__sp = &iop__enum__s;

/* }}} */
/* Class iop.Union {{{ */

static int iop__union__fields__check(const void *ptr, int n)
{
    if (n < 1) {
        iop_set_err("violation of constraint %s (%d) on field %s: length=%d",
                    "minOccurs", 1, "fields", n);
        return -1;
    }
    return 0;
}
static iop_field_attr_t const iop__union__fields__attrs[] = {
    {
        .type = 0,
        .args = (iop_field_attr_arg_t[]){ { .v.i64 = 1LL } },
    },
};
static iop_field_attrs_t const iop__union__desc_fields_attrs[] = {
    {
        .flags             = 1,
        .attrs_len         = 1,
        .check_constraints = &iop__union__fields__check,
        .attrs             = iop__union__fields__attrs,
    },
};
static iop_field_t const iop__union__desc_fields[] = {
    {
        .name      = LSTR_IMMED("fields"),
        .tag       = 1,
        .tag_len   = 0,
        .repeat    = IOP_R_REPEATED,
        .type      = IOP_T_STRUCT,
        .data_offs = offsetof(iop__union__t, fields),
        .flags     = 3,
        .size      = sizeof(iop__field__t),
        .u1        = { .st_desc = &iop__field__s },
    },
};
static const iop_class_attrs_t iop__union__class_s = {
    .parent            = &iop__structure__s,
    .class_id          = 4,
};
const iop_struct_t iop__union__s = {
    .fullname   = LSTR_IMMED("iop.Union"),
    .fields     = iop__union__desc_fields,
    .ranges     = iop__ranges__5,
    .ranges_len = countof(iop__ranges__5) / 2,
    .fields_len = countof(iop__union__desc_fields),
    .size       = sizeof(iop__union__t),
    .flags      = 15,
    .is_union   = false,
    .st_attrs   = NULL,
    .fields_attrs = iop__union__desc_fields_attrs,
    {
        .class_attrs  = &iop__union__class_s,
    }
};
iop_struct_t const * const iop__union__sp = &iop__union__s;

/* }}} */
/* Structure iop.Package {{{ */

static int iop__package__name__check(const void *ptr, int n)
{
    for (int j = 0; j < n; j++) {
        lstr_t    val = IOP_FIELD(lstr_t   , ptr, j);

        for (int c = 0; c < val.len; c++) {
            switch (val.s[c]) {
                case 'a' ... 'z':
                case '_':
                case '.':
                    break;
                default:
                    iop_set_err("violation of constraint %s (%s) on field %s: %*pM",
                                "pattern", "[a-z_\\.]*", "name", LSTR_FMT_ARG(val));
                    return -1;
            }
        }
        if (val.len < 1) {
            iop_set_err("violation of constraint %s (%d) on field %s: length=%d",
                        "minLength", 1, "name", val.len);
            return -1;
        }
    }
    return 0;
}
static iop_field_attr_t const iop__package__name__attrs[] = {
    {
        .type = 9,
        .args = (iop_field_attr_arg_t[]){ { .v.s = LSTR_IMMED("[a-z_\\.]*") } },
    },
    {
        .type = 7,
        .args = (iop_field_attr_arg_t[]){ { .v.i64 = 1LL } },
    },
};
static iop_field_attrs_t const iop__package__desc_fields_attrs[] = {
    {
        .flags             = 640,
        .attrs_len         = 2,
        .check_constraints = &iop__package__name__check,
        .attrs             = iop__package__name__attrs,
    },
    {
        .flags             = 0,
        .attrs_len         = 0,
    },
};
static iop_field_t const iop__package__desc_fields[] = {
    {
        .name      = LSTR_IMMED("name"),
        .tag       = 1,
        .tag_len   = 0,
        .repeat    = IOP_R_REQUIRED,
        .type      = IOP_T_STRING,
        .data_offs = offsetof(iop__package__t, name),
        .flags     = 1,
        .size      = fieldsizeof(iop__package__t, name),
    },
    {
        .name      = LSTR_IMMED("elems"),
        .tag       = 2,
        .tag_len   = 0,
        .repeat    = IOP_R_REPEATED,
        .type      = IOP_T_STRUCT,
        .data_offs = offsetof(iop__package__t, elems),
        .size      = sizeof(iop__package_elem__t *),
        .u1        = { .st_desc = &iop__package_elem__s },
    },
};
const iop_struct_t iop__package__s = {
    .fullname   = LSTR_IMMED("iop.Package"),
    .fields     = iop__package__desc_fields,
    .ranges     = iop__ranges__3,
    .ranges_len = countof(iop__ranges__3) / 2,
    .fields_len = countof(iop__package__desc_fields),
    .size       = sizeof(iop__package__t),
    .flags      = 3,
    .fields_attrs = iop__package__desc_fields_attrs,
};
iop_struct_t const * const iop__package__sp = &iop__package__s;

/* }}} */
/* Package iop {{{ */

static const iop_pkg_t *const iop__deps[] = {
    NULL,
};

static const iop_enum_t *const iop__enums[] = {
    &iop__int_size__e,
    &iop__string_type__e,
    NULL,
};

static const iop_struct_t *const iop__structs[] = {
    &iop__int_type__s,
    &iop__iop_type__s,
    &iop__package_elem__s,
    &iop__value__s,
    &iop__opt_info__s,
    &iop__field__s,
    &iop__structure__s,
    &iop__struct__s,
    &iop__enum_val__s,
    &iop__enum__s,
    &iop__union__s,
    &iop__package__s,
    NULL,
};

static const iop_iface_t *const iop__ifaces[] = {
    NULL,
};

static const iop_mod_t *const iop__mods[] = {
    NULL,
};

iop_pkg_t const iop__pkg = {
    .name    = LSTR_IMMED("iop"),
    .deps    = iop__deps,
    .enums   = iop__enums,
    .structs = iop__structs,
    .ifaces  = iop__ifaces,
    .mods    = iop__mods,
};
iop_pkg_t const * const iop__pkgp = &iop__pkg;

/* }}} */

