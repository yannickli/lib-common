/***** THIS FILE IS AUTOGENERATED DO NOT MODIFY DIRECTLY ! *****/
#if !defined(linux) && !defined(__APPLE__)
#  warning "%s assumed linux alignments"
#endif
#ifndef __x86_64__
#  warning "ic.iop assumed x86_64 alignments"
#endif

#include "ic.iop.h"

/* Enum ic.IcPriority {{{ */

static int const ic__ic_priority__values[] = {
 0, 1, 2,
};
static int const iop__ranges__1[] = {
    0, 0,
    3,
};
static const lstr_t ic__ic_priority__names[] = {
    LSTR_IMMED("LOW"),
    LSTR_IMMED("NORMAL"),
    LSTR_IMMED("HIGH"),
};
iop_enum_t const ic__ic_priority__e = {
    .name         = LSTR_IMMED("IcPriority"),
    .fullname     = LSTR_IMMED("ic.IcPriority"),
    .names        = ic__ic_priority__names,
    .values       = ic__ic_priority__values,
    .ranges       = iop__ranges__1,
    .ranges_len   = countof(iop__ranges__1) / 2,
    .enum_len     = 3,
};

/* }}} */
/* Structure ic.SimpleHdr {{{ */

static iop_field_t const ic__simple_hdr__desc_fields[] = {
    {
        .name      = LSTR_IMMED("login"),
        .tag       = 1,
        .tag_len   = 0,
        .repeat    = IOP_R_OPTIONAL,
        .type      = IOP_T_STRING,
        .data_offs = offsetof(ic__simple_hdr__t, login),
        .size      = fieldsizeof(ic__simple_hdr__t, login),
    },
    {
        .name      = LSTR_IMMED("password"),
        .tag       = 2,
        .tag_len   = 0,
        .repeat    = IOP_R_OPTIONAL,
        .type      = IOP_T_STRING,
        .data_offs = offsetof(ic__simple_hdr__t, password),
        .size      = fieldsizeof(ic__simple_hdr__t, password),
    },
    {
        .name      = LSTR_IMMED("kind"),
        .tag       = 3,
        .tag_len   = 0,
        .repeat    = IOP_R_OPTIONAL,
        .type      = IOP_T_STRING,
        .data_offs = offsetof(ic__simple_hdr__t, kind),
        .size      = fieldsizeof(ic__simple_hdr__t, kind),
    },
    {
        .name      = LSTR_IMMED("payload"),
        .tag       = 4,
        .tag_len   = 0,
        .repeat    = IOP_R_DEFVAL,
        .type      = IOP_T_I32,
        .data_offs = offsetof(ic__simple_hdr__t, payload),
        .u1        = { .defval_u64 = 0xffffffffffffffff },
        .size      = fieldsizeof(ic__simple_hdr__t, payload),
    },
};
static int const iop__ranges__2[] = {
    0, 1,
    4,
};
const iop_struct_t ic__simple_hdr__s = {
    .fullname   = LSTR_IMMED("ic.SimpleHdr"),
    .fields     = ic__simple_hdr__desc_fields,
    .ranges     = iop__ranges__2,
    .fields_len = countof(ic__simple_hdr__desc_fields),
    .ranges_len = countof(iop__ranges__2) / 2,
    .size       = sizeof(ic__simple_hdr__t),
};

/* }}} */
/* Union ic.Hdr {{{ */

static iop_field_t const ic__hdr__desc_fields[] = {
    {
        .name      = LSTR_IMMED("simple"),
        .tag       = 1,
        .tag_len   = 0,
        .repeat    = IOP_R_REQUIRED,
        .type      = IOP_T_STRUCT,
        .data_offs = offsetof(ic__hdr__t, simple),
        .size      = sizeof(ic__simple_hdr__t),
        .u1        = { .st_desc = &ic__simple_hdr__s },
    },
};
static int const iop__ranges__3[] = {
    0, 1,
    1,
};
const iop_struct_t ic__hdr__s = {
    .fullname   = LSTR_IMMED("ic.Hdr"),
    .fields     = ic__hdr__desc_fields,
    .ranges     = iop__ranges__3,
    .fields_len = countof(ic__hdr__desc_fields),
    .ranges_len = countof(iop__ranges__3) / 2,
    .size       = sizeof(ic__hdr__t),
    .is_union   = true,
};

/* }}} */
/* Package ic {{{ */

static const iop_pkg_t *const ic__deps[] = {
    NULL,
};

static const iop_enum_t *const ic__enums[] = {
    &ic__ic_priority__e,
    NULL,
};

static const iop_struct_t *const ic__structs[] = {
    &ic__simple_hdr__s,
    &ic__hdr__s,
    NULL,
};

static const iop_iface_t *const ic__ifaces[] = {
    NULL,
};

static const iop_mod_t *const ic__mods[] = {
    NULL,
};

iop_pkg_t const ic__pkg = {
    .name    = LSTR_IMMED("ic"),
    .deps    = ic__deps,
    .enums   = ic__enums,
    .structs = ic__structs,
    .ifaces  = ic__ifaces,
    .mods    = ic__mods,
};

/* }}} */

