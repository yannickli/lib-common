/***** THIS FILE IS AUTOGENERATED DO NOT MODIFY DIRECTLY ! *****/
#ifndef IOP_HEADER_GUARD_tstgen_TYPES_H
#define IOP_HEADER_GUARD_tstgen_TYPES_H

#include "tstgen-tdef.iop.h"

#include "pkg_a-tdef.iop.h"
#if __has_feature(nullability)
#pragma GCC diagnostic push
#pragma GCC diagnostic error "-Wnullability-completeness"
#endif


EXPORT iop_enum_t const tstgen__my_enum_a__e;
EXPORT iop_enum_t const * const nonnull tstgen__my_enum_a__ep;

typedef tstgen__my_enum_a__t my_enum_a__t;
typedef tstgen__my_enum_a__array_t my_enum_a__array_t;
typedef tstgen__my_enum_a__opt_t my_enum_a__opt_t;
#define my_enum_a__e  tstgen__my_enum_a__e

struct tstgen__my_struct_a__t {
    int32_t  i;
    int32_t  j;
    double   d1;
    struct pkg_a__a__t *nullable weak_ref1;
    struct pkg_a__a__t *nullable weak_ref2;
};
EXPORT iop_struct_t const tstgen__my_struct_a__s;
EXPORT iop_struct_t const * const nonnull tstgen__my_struct_a__sp;
typedef tstgen__my_struct_a__t my_struct_a__t;
typedef tstgen__my_struct_a__array_t my_struct_a__array_t;
#define my_struct_a__s  tstgen__my_struct_a__s

/*----- XXX private data, do not use directly -{{{-*/
typedef enum tstgen__my_union_a__tag_t {
    tstgen__my_union_a__f1__ft = 1,
    tstgen__my_union_a__f4__ft = 4,
} tstgen__my_union_a__tag_t;

#define tstgen__my_union_a__f1__fdesc  tstgen__my_union_a__s.fields[0]
#define tstgen__my_union_a__f4__fdesc  tstgen__my_union_a__s.fields[1]
/*-}}}-*/
struct tstgen__my_union_a__t {
    tstgen__my_union_a__tag_t iop_tag;
    union {
        bool     f1;
        int64_t  f4;
    };
};
EXPORT iop_struct_t const tstgen__my_union_a__s;
EXPORT iop_struct_t const * const nonnull tstgen__my_union_a__sp;
#define tstgen__my_union_a__get(u, field)       IOP_UNION_GET(tstgen__my_union_a, u, field)
struct tstgen__optimized__t {
    bool     f1;
    bool     f3;
    int64_t  f2;
    int64_t  f4;
};
EXPORT iop_struct_t const tstgen__optimized__s;
EXPORT iop_struct_t const * const nonnull tstgen__optimized__sp;
struct tstgen__my_class_a__t {
    const iop_struct_t *nonnull __vptr;
};
EXPORT iop_struct_t const tstgen__my_class_a__s;
EXPORT iop_struct_t const * const nonnull tstgen__my_class_a__sp;
#define tstgen__my_class_a__class_id  0

typedef tstgen__my_class_a__t my_class_a__t;
typedef tstgen__my_class_a__array_t my_class_a__array_t;
#define my_class_a__s  tstgen__my_class_a__s
#define my_class_a__class_id  0

#if __has_feature(nullability)
#pragma GCC diagnostic pop
#endif

#endif
