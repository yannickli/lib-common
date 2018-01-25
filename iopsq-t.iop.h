/***** THIS FILE IS AUTOGENERATED DO NOT MODIFY DIRECTLY ! *****/
#ifndef IOP_HEADER_GUARD_iopsq_TYPES_H
#define IOP_HEADER_GUARD_iopsq_TYPES_H

#include "iopsq-tdef.iop.h"

#if __has_feature(nullability)
#pragma GCC diagnostic push
#pragma GCC diagnostic error "-Wnullability-completeness"
#endif

EXPORT iop_enum_t const iopsq__int_size__e;
EXPORT iop_enum_t const * const nonnull iopsq__int_size__ep;

typedef iopsq__int_size__t iop__int_size__t;
typedef iopsq__int_size__array_t iop__int_size__array_t;
typedef iopsq__int_size__opt_t iop__int_size__opt_t;
#define iop__int_size__e  iopsq__int_size__e

EXPORT iop_enum_t const iopsq__string_type__e;
EXPORT iop_enum_t const * const nonnull iopsq__string_type__ep;

typedef iopsq__string_type__t iop__string_type__t;
typedef iopsq__string_type__array_t iop__string_type__array_t;
typedef iopsq__string_type__opt_t iop__string_type__opt_t;
#define iop__string_type__e  iopsq__string_type__e

struct iopsq__int_type__t {
    bool     is_signed;
    iopsq__int_size__t size;
};
EXPORT iop_struct_t const iopsq__int_type__s;
EXPORT iop_struct_t const * const nonnull iopsq__int_type__sp;
typedef iopsq__int_type__t iop__int_type__t;
typedef iopsq__int_type__array_t iop__int_type__array_t;
#define iop__int_type__s  iopsq__int_type__s

/*----- XXX private data, do not use directly -{{{-*/
typedef enum iopsq__iop_type__tag_t {
    iopsq__iop_type__i__ft = 1,
    iopsq__iop_type__b__ft = 2,
#define iopsq__iop_type__b__empty_ft  iopsq__iop_type__b__ft
    iopsq__iop_type__d__ft = 3,
#define iopsq__iop_type__d__empty_ft  iopsq__iop_type__d__ft
    iopsq__iop_type__s__ft = 4,
    iopsq__iop_type__v__ft = 5,
#define iopsq__iop_type__v__empty_ft  iopsq__iop_type__v__ft
    iopsq__iop_type__type_name__ft = 6,
    iopsq__iop_type__array__ft = 7,
} iopsq__iop_type__tag_t;
typedef iopsq__iop_type__tag_t iop__type__tag_t;
#define iop__type__i__ft iopsq__iop_type__i__ft
#define iop__type__b__ft iopsq__iop_type__b__ft
#define iop__type__b__empty_ft  iop__type__b__ft
#define iop__type__d__ft iopsq__iop_type__d__ft
#define iop__type__d__empty_ft  iop__type__d__ft
#define iop__type__s__ft iopsq__iop_type__s__ft
#define iop__type__v__ft iopsq__iop_type__v__ft
#define iop__type__v__empty_ft  iop__type__v__ft
#define iop__type__type_name__ft iopsq__iop_type__type_name__ft
#define iop__type__array__ft iopsq__iop_type__array__ft
/*-}}}-*/
struct iopsq__iop_type__t {
    iopsq__iop_type__tag_t iop_tag;
    union {
        struct iopsq__int_type__t i;
        iopsq__string_type__t s;
        lstr_t   type_name;
        struct iopsq__iop_type__t *nonnull array;
    };
};
EXPORT iop_struct_t const iopsq__iop_type__s;
EXPORT iop_struct_t const * const nonnull iopsq__iop_type__sp;
#define iopsq__iop_type__get(u, field)       IOP_UNION_GET(iopsq__iop_type, u, field)
typedef iopsq__iop_type__t iop__type__t;
typedef iopsq__iop_type__array_t iop__type__array_t;
#define iop__type__s  iopsq__iop_type__s

struct iopsq__package_elem__t {
    const iop_struct_t *nonnull __vptr;
    lstr_t   name;
};
EXPORT iop_struct_t const iopsq__package_elem__s;
EXPORT iop_struct_t const * const nonnull iopsq__package_elem__sp;
#define iopsq__package_elem__class_id  0

typedef iopsq__package_elem__t iop__package_elem__t;
typedef iopsq__package_elem__array_t iop__package_elem__array_t;
#define iop__package_elem__s  iopsq__package_elem__s
#define iop__package_elem__class_id  0

/*----- XXX private data, do not use directly -{{{-*/
typedef enum iopsq__value__tag_t {
    iopsq__value__i__ft = 1,
    iopsq__value__u__ft = 2,
    iopsq__value__d__ft = 3,
    iopsq__value__s__ft = 4,
    iopsq__value__b__ft = 5,
} iopsq__value__tag_t;
typedef iopsq__value__tag_t iop__value__tag_t;
#define iop__value__i__ft iopsq__value__i__ft
#define iop__value__u__ft iopsq__value__u__ft
#define iop__value__d__ft iopsq__value__d__ft
#define iop__value__s__ft iopsq__value__s__ft
#define iop__value__b__ft iopsq__value__b__ft
/*-}}}-*/
struct iopsq__value__t {
    iopsq__value__tag_t iop_tag;
    union {
        int64_t  i;
        uint64_t u;
        double   d;
        lstr_t   s;
        bool     b;
    };
};
EXPORT iop_struct_t const iopsq__value__s;
EXPORT iop_struct_t const * const nonnull iopsq__value__sp;
#define iopsq__value__get(u, field)       IOP_UNION_GET(iopsq__value, u, field)
typedef iopsq__value__t iop__value__t;
typedef iopsq__value__array_t iop__value__array_t;
#define iop__value__s  iopsq__value__s

struct iopsq__opt_info__t {
    struct iopsq__value__t *nullable def_val;
};
EXPORT iop_struct_t const iopsq__opt_info__s;
EXPORT iop_struct_t const * const nonnull iopsq__opt_info__sp;
typedef iopsq__opt_info__t iop__opt_info__t;
typedef iopsq__opt_info__array_t iop__opt_info__array_t;
#define iop__opt_info__s  iopsq__opt_info__s

struct iopsq__field__t {
    lstr_t   name;
    struct iopsq__iop_type__t type;
    struct iopsq__opt_info__t *nullable optional;
    opt_u16_t        tag;
    bool             is_reference;
};
EXPORT iop_struct_t const iopsq__field__s;
EXPORT iop_struct_t const * const nonnull iopsq__field__sp;
typedef iopsq__field__t iop__field__t;
typedef iopsq__field__array_t iop__field__array_t;
#define iop__field__s  iopsq__field__s

struct iopsq__structure__t {
    union {
        struct iopsq__package_elem__t super;
        struct {
            const iop_struct_t *nonnull __vptr;
            /* fields of iopsq__package_elem__t */
            lstr_t   name;
        };
    };
};
EXPORT iop_struct_t const iopsq__structure__s;
EXPORT iop_struct_t const * const nonnull iopsq__structure__sp;
#define iopsq__structure__class_id  1

typedef iopsq__structure__t iop__structure__t;
typedef iopsq__structure__array_t iop__structure__array_t;
#define iop__structure__s  iopsq__structure__s
#define iop__structure__class_id  1

struct iopsq__struct__t {
    union {
        struct iopsq__structure__t super;
        struct {
            struct {
                const iop_struct_t *nonnull __vptr;
                /* fields of iopsq__package_elem__t */
                lstr_t   name;
            };
            /* fields of iopsq__structure__t */
        };
    };
    iopsq__field__array_t fields;
};
EXPORT iop_struct_t const iopsq__struct__s;
EXPORT iop_struct_t const * const nonnull iopsq__struct__sp;
#define iopsq__struct__class_id  2

typedef iopsq__struct__t iop__struct__t;
typedef iopsq__struct__array_t iop__struct__array_t;
#define iop__struct__s  iopsq__struct__s
#define iop__struct__class_id  2

struct iopsq__enum_val__t {
    lstr_t   name;
    opt_i32_t        val;
};
EXPORT iop_struct_t const iopsq__enum_val__s;
EXPORT iop_struct_t const * const nonnull iopsq__enum_val__sp;
typedef iopsq__enum_val__t iop__enum_val__t;
typedef iopsq__enum_val__array_t iop__enum_val__array_t;
#define iop__enum_val__s  iopsq__enum_val__s

struct iopsq__enum__t {
    union {
        struct iopsq__package_elem__t super;
        struct {
            const iop_struct_t *nonnull __vptr;
            /* fields of iopsq__package_elem__t */
            lstr_t   name;
        };
    };
    iopsq__enum_val__array_t values;
};
EXPORT iop_struct_t const iopsq__enum__s;
EXPORT iop_struct_t const * const nonnull iopsq__enum__sp;
#define iopsq__enum__class_id  3

typedef iopsq__enum__t iop__enum__t;
typedef iopsq__enum__array_t iop__enum__array_t;
#define iop__enum__s  iopsq__enum__s
#define iop__enum__class_id  3

struct iopsq__union__t {
    union {
        struct iopsq__structure__t super;
        struct {
            struct {
                const iop_struct_t *nonnull __vptr;
                /* fields of iopsq__package_elem__t */
                lstr_t   name;
            };
            /* fields of iopsq__structure__t */
        };
    };
    iopsq__field__array_t fields;
};
EXPORT iop_struct_t const iopsq__union__s;
EXPORT iop_struct_t const * const nonnull iopsq__union__sp;
#define iopsq__union__class_id  4

typedef iopsq__union__t iop__union__t;
typedef iopsq__union__array_t iop__union__array_t;
#define iop__union__s  iopsq__union__s
#define iop__union__class_id  4

struct iopsq__package__t {
    lstr_t   name;
    iopsq__package_elem__array_t elems;
};
EXPORT iop_struct_t const iopsq__package__s;
EXPORT iop_struct_t const * const nonnull iopsq__package__sp;
typedef iopsq__package__t iop__package__t;
typedef iopsq__package__array_t iop__package__array_t;
#define iop__package__s  iopsq__package__s

#if __has_feature(nullability)
#pragma GCC diagnostic pop
#endif

#endif
