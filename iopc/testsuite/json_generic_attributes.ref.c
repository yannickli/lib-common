/***** THIS FILE IS AUTOGENERATED DO NOT MODIFY DIRECTLY ! *****/
#if !defined(linux) && !defined(__APPLE__)
#  warning "json_generic_attributes.iop assumed linux alignments"
#endif
#ifndef __x86_64__
#  warning "json_generic_attributes.iop assumed x86_64 alignments"
#endif

#include "json_generic_attributes.iop.h"

/* Class json_generic_attributes.VoiceEvent {{{ */

static iop_field_attr_t const json_generic_attributes__voice_event__length__attrs[] = {
    {
        .type = 15,
        .args = (iop_field_attr_arg_t[]){ { .v.s = LSTR_IMMED("scenario:label") }, {.v.s = LSTR_IMMED("{\"en\":\"Duration\",\"fr\":\"Durée\"}") } },
    },
    {
        .type = 15,
        .args = (iop_field_attr_arg_t[]){ { .v.s = LSTR_IMMED("scenario:type") }, {.v.s = LSTR_IMMED("{\"integer\":{\"size\":\"INT32\",\"format\":\"TIME\"}}") } },
    },
};
static iop_field_attr_t const json_generic_attributes__voice_event__price__attrs[] = {
    {
        .type = 15,
        .args = (iop_field_attr_arg_t[]){ { .v.s = LSTR_IMMED("scenario:label") }, {.v.s = LSTR_IMMED("{\"en\":\"Price\",\"fr\":\"\\\"Gougou\\\" d'ooo'\\n\"}") } },
    },
    {
        .type = 15,
        .args = (iop_field_attr_arg_t[]){ { .v.s = LSTR_IMMED("scenario:type") }, {.v.s = LSTR_IMMED("{\"dbl\":{\"format\":\"CURRENCY\"}}") } },
    },
};
static iop_field_attrs_t const json_generic_attributes__voice_event__desc_fields_attrs[] = {
    {
        .flags             = 32768,
        .attrs_len         = 2,
        .attrs             = json_generic_attributes__voice_event__length__attrs,
    },
    {
        .flags             = 32768,
        .attrs_len         = 2,
        .attrs             = json_generic_attributes__voice_event__price__attrs,
    },
};
static iop_field_t const json_generic_attributes__voice_event__desc_fields[] = {
    {
        .name      = LSTR_IMMED("length"),
        .tag       = 1,
        .tag_len   = 0,
        .repeat    = IOP_R_REQUIRED,
        .type      = IOP_T_I32,
        .data_offs = offsetof(json_generic_attributes__voice_event__t, length),
        .size      = fieldsizeof(json_generic_attributes__voice_event__t, length),
    },
    {
        .name      = LSTR_IMMED("price"),
        .tag       = 2,
        .tag_len   = 0,
        .repeat    = IOP_R_REQUIRED,
        .type      = IOP_T_DOUBLE,
        .data_offs = offsetof(json_generic_attributes__voice_event__t, price),
        .size      = fieldsizeof(json_generic_attributes__voice_event__t, price),
    },
};
static int const iop__ranges__1[] = {
    0, 1,
    2,
};
static const iop_struct_attr_t json_generic_attributes__voice_event__s_attrs[] = {
    {
        .type = 4,
        .args = (iop_field_attr_arg_t[]){ { .v.s = LSTR_IMMED("scenario:label") }, {.v.s = LSTR_IMMED("{\"en\":\"Voice call\",\"fr\":\"Appel téléphonique\"}") } },
    },
    {
        .type = 4,
        .args = (iop_field_attr_arg_t[]){ { .v.s = LSTR_IMMED("empty:attr") }, {.v.s = LSTR_IMMED("{}") } },
    },
};
static const iop_struct_attrs_t json_generic_attributes__voice_event__s_desc_attrs = {
    .flags     = 0,
    .attrs_len = 2,
    .attrs     = json_generic_attributes__voice_event__s_attrs,
};
static const iop_class_attrs_t json_generic_attributes__voice_event__class_s = {
    .class_id          = 0,
};
const iop_struct_t json_generic_attributes__voice_event__s = {
    .fullname   = LSTR_IMMED("json_generic_attributes.VoiceEvent"),
    .fields     = json_generic_attributes__voice_event__desc_fields,
    .ranges     = iop__ranges__1,
    .ranges_len = countof(iop__ranges__1) / 2,
    .fields_len = countof(json_generic_attributes__voice_event__desc_fields),
    .size       = sizeof(json_generic_attributes__voice_event__t),
    .flags      = 13,
    .is_union   = false,
    .st_attrs   = &json_generic_attributes__voice_event__s_desc_attrs,
    .fields_attrs = json_generic_attributes__voice_event__desc_fields_attrs,
    {
        .class_attrs  = &json_generic_attributes__voice_event__class_s,
    }
};
iop_struct_t const * const json_generic_attributes__voice_event__sp = &json_generic_attributes__voice_event__s;

/* }}} */
/* Class json_generic_attributes.DataEvent {{{ */

static iop_field_attr_t const json_generic_attributes__data_event__length__attrs[] = {
    {
        .type = 15,
        .args = (iop_field_attr_arg_t[]){ { .v.s = LSTR_IMMED("scenario:label") }, {.v.s = LSTR_IMMED("{\"en\":\"Bandwidth size\",\"fr\":\"Bande passante\"}") } },
    },
    {
        .type = 15,
        .args = (iop_field_attr_arg_t[]){ { .v.s = LSTR_IMMED("scenario:type") }, {.v.s = LSTR_IMMED("{\"integer\":{\"size\":\"INT32\",\"format\":\"TIME\"}}") } },
    },
    {
        .type = 15,
        .args = (iop_field_attr_arg_t[]){ { .v.s = LSTR_IMMED("empty:attr") }, {.v.s = LSTR_IMMED("{}") } },
    },
};
static iop_field_attr_t const json_generic_attributes__data_event__price__attrs[] = {
    {
        .type = 15,
        .args = (iop_field_attr_arg_t[]){ { .v.s = LSTR_IMMED("scenario:label") }, {.v.s = LSTR_IMMED("{\"en\":\"Price\",\"fr\":\"\\\"Gougou\\\" d'ooo'\\n\"}") } },
    },
    {
        .type = 15,
        .args = (iop_field_attr_arg_t[]){ { .v.s = LSTR_IMMED("scenario:type") }, {.v.s = LSTR_IMMED("{\"dbl\":{\"format\":\"CURRENCY\"}}") } },
    },
};
static iop_field_attrs_t const json_generic_attributes__data_event__desc_fields_attrs[] = {
    {
        .flags             = 32768,
        .attrs_len         = 3,
        .attrs             = json_generic_attributes__data_event__length__attrs,
    },
    {
        .flags             = 32768,
        .attrs_len         = 2,
        .attrs             = json_generic_attributes__data_event__price__attrs,
    },
};
static iop_field_t const json_generic_attributes__data_event__desc_fields[] = {
    {
        .name      = LSTR_IMMED("length"),
        .tag       = 1,
        .tag_len   = 0,
        .repeat    = IOP_R_REQUIRED,
        .type      = IOP_T_I32,
        .data_offs = offsetof(json_generic_attributes__data_event__t, length),
        .size      = fieldsizeof(json_generic_attributes__data_event__t, length),
    },
    {
        .name      = LSTR_IMMED("price"),
        .tag       = 2,
        .tag_len   = 0,
        .repeat    = IOP_R_REQUIRED,
        .type      = IOP_T_DOUBLE,
        .data_offs = offsetof(json_generic_attributes__data_event__t, price),
        .size      = fieldsizeof(json_generic_attributes__data_event__t, price),
    },
};
static const iop_struct_attr_t json_generic_attributes__data_event__s_attrs[] = {
    {
        .type = 4,
        .args = (iop_field_attr_arg_t[]){ { .v.s = LSTR_IMMED("scenario:label") }, {.v.s = LSTR_IMMED("{\"en\":\"Data session\",\"fr\":\"Session de donnée\"}") } },
    },
};
static const iop_struct_attrs_t json_generic_attributes__data_event__s_desc_attrs = {
    .flags     = 0,
    .attrs_len = 1,
    .attrs     = json_generic_attributes__data_event__s_attrs,
};
static const iop_class_attrs_t json_generic_attributes__data_event__class_s = {
    .class_id          = 0,
};
const iop_struct_t json_generic_attributes__data_event__s = {
    .fullname   = LSTR_IMMED("json_generic_attributes.DataEvent"),
    .fields     = json_generic_attributes__data_event__desc_fields,
    .ranges     = iop__ranges__1,
    .ranges_len = countof(iop__ranges__1) / 2,
    .fields_len = countof(json_generic_attributes__data_event__desc_fields),
    .size       = sizeof(json_generic_attributes__data_event__t),
    .flags      = 13,
    .is_union   = false,
    .st_attrs   = &json_generic_attributes__data_event__s_desc_attrs,
    .fields_attrs = json_generic_attributes__data_event__desc_fields_attrs,
    {
        .class_attrs  = &json_generic_attributes__data_event__class_s,
    }
};
iop_struct_t const * const json_generic_attributes__data_event__sp = &json_generic_attributes__data_event__s;

/* }}} */
/* Structure json_generic_attributes.Test {{{ */

static iop_field_t const json_generic_attributes__test__desc_fields[] = {
};
static int const iop__ranges__2[] = {
    0,
};
static const iop_struct_attr_t json_generic_attributes__test__s_attrs[] = {
    {
        .type = 4,
        .args = (iop_field_attr_arg_t[]){ { .v.s = LSTR_IMMED("ns:id") }, {.v.s = LSTR_IMMED("{\"a1\":[1,2,3],\"a2\":[],\"s1\":{\"f1\":1},\"s2\":{}}") } },
    },
};
static const iop_struct_attrs_t json_generic_attributes__test__s_desc_attrs = {
    .flags     = 0,
    .attrs_len = 1,
    .attrs     = json_generic_attributes__test__s_attrs,
};
const iop_struct_t json_generic_attributes__test__s = {
    .fullname   = LSTR_IMMED("json_generic_attributes.Test"),
    .fields     = json_generic_attributes__test__desc_fields,
    .ranges     = iop__ranges__2,
    .ranges_len = countof(iop__ranges__2) / 2,
    .fields_len = countof(json_generic_attributes__test__desc_fields),
    .size       = sizeof(json_generic_attributes__test__t),
    .flags      = 1,
    .st_attrs   = &json_generic_attributes__test__s_desc_attrs,
};
iop_struct_t const * const json_generic_attributes__test__sp = &json_generic_attributes__test__s;

/* }}} */
/* Package json_generic_attributes {{{ */

static const iop_pkg_t *const json_generic_attributes__deps[] = {
    NULL,
};

static const iop_enum_t *const json_generic_attributes__enums[] = {
    NULL,
};

static const iop_struct_t *const json_generic_attributes__structs[] = {
    &json_generic_attributes__voice_event__s,
    &json_generic_attributes__data_event__s,
    &json_generic_attributes__test__s,
    NULL,
};

static const iop_iface_t *const json_generic_attributes__ifaces[] = {
    NULL,
};

static const iop_mod_t *const json_generic_attributes__mods[] = {
    NULL,
};

iop_pkg_t const json_generic_attributes__pkg = {
    .name    = LSTR_IMMED("json_generic_attributes"),
    .deps    = json_generic_attributes__deps,
    .enums   = json_generic_attributes__enums,
    .structs = json_generic_attributes__structs,
    .ifaces  = json_generic_attributes__ifaces,
    .mods    = json_generic_attributes__mods,
};
iop_pkg_t const * const json_generic_attributes__pkgp = &json_generic_attributes__pkg;

/* }}} */

