/***** THIS FILE IS AUTOGENERATED DO NOT MODIFY DIRECTLY ! *****/
#ifndef IOP_HEADER_GUARD_ic_TYPES_H
#define IOP_HEADER_GUARD_ic_TYPES_H

#include <lib-common/iop.h>

typedef struct ic__simple_hdr__t {
    lstr_t           login;
    lstr_t           password;
    lstr_t           kind;
    int32_t  payload;
} ic__simple_hdr__t;
extern iop_struct_t const ic__simple_hdr__s;
IOP_GENERIC(ic__simple_hdr);

typedef struct ic__hdr__t {
    uint16_t iop_tag;
    union {
        struct ic__simple_hdr__t simple;
    };
} ic__hdr__t;
extern iop_struct_t const ic__hdr__s;
IOP_GENERIC(ic__hdr);
/*----- XXX private data, do not use directly -{{{-*/
#define ic__hdr__simple__ft 1
/*-}}}-*/
#define ic__hdr__get(u, field)       IOP_UNION_GET(ic__hdr, u, field)
#endif
