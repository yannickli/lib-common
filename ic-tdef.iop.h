/***** THIS FILE IS AUTOGENERATED DO NOT MODIFY DIRECTLY ! *****/
#ifndef IOP_HEADER_GUARD_ic_TYPEDEFS_H
#define IOP_HEADER_GUARD_ic_TYPEDEFS_H

#include <lib-common/iop.h>

typedef enum ic__ic_priority__t {
    IC_PRIORITY_LOW,
    IC_PRIORITY_NORMAL,
    IC_PRIORITY_HIGH,
} ic__ic_priority__t;
typedef IOP_ARRAY_OF(enum ic__ic_priority__t) ic__ic_priority__array_t;
#define IC_PRIORITY_count 3
#define IC_PRIORITY_min   0
#define IC_PRIORITY_max   2

typedef struct ic__simple_hdr__t ic__simple_hdr__t;
typedef IOP_ARRAY_OF(ic__simple_hdr__t) ic__simple_hdr__array_t;

typedef struct ic__hdr__t ic__hdr__t;
typedef IOP_ARRAY_OF(ic__hdr__t) ic__hdr__array_t;

#endif
