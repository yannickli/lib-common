/**************************************************************************/
/*                                                                        */
/*  Copyright (C) INTERSEC SA                                             */
/*                                                                        */
/*  Should you receive a copy of this source code, you must check you     */
/*  have a proper, written authorization of INTERSEC to hold it. If you   */
/*  don't have such an authorization, you must DELETE all source code     */
/*  files in your possession, and inform INTERSEC of the fact you obtain  */
/*  these files. Should you not comply to these terms, you can be         */
/*  prosecuted in the extent permitted by applicable law.                 */
/*                                                                        */
/**************************************************************************/

#ifndef IS_LIB_COMMON_IOP_INTERNALS_H
#define IS_LIB_COMMON_IOP_INTERNALS_H

typedef enum iop_repeat_t {
    IOP_R_REQUIRED,
    IOP_R_DEFVAL,
    IOP_R_OPTIONAL,
    IOP_R_REPEATED,
} iop_repeat_t;

typedef enum iop_type_t {
    IOP_T_I8,     IOP_T_U8,
    IOP_T_I16,    IOP_T_U16,
    IOP_T_I32,    IOP_T_U32,
    IOP_T_I64,    IOP_T_U64,
    IOP_T_BOOL,
    IOP_T_ENUM,
    IOP_T_DOUBLE,
    IOP_T_STRING,
    IOP_T_DATA,
    IOP_T_UNION,
    IOP_T_STRUCT,
    IOP_T_XML,
} iop_type_t;

/*{{{ iop_field_t */

typedef struct iop_struct_t iop_struct_t;
typedef struct iop_enum_t   iop_enum_t;

enum iop_field_flags_t {
    IOP_FIELD_CHECK_CONSTRAINTS,    /**< check_constraints function exists  */
    IOP_FIELD_NO_EMPTY_ARRAY,       /**< indicates presence of @minOccurs   */
    IOP_FIELD_IS_REFERENCE,         /**< field points to the value          */
};

typedef struct iop_field_t {
    lstr_t       name;
    uint16_t     tag;
    unsigned     tag_len:  2; /**< 0 to 2                                   */
    unsigned     flags  : 14; /**< bitfield of iop_field_flags_t            */
    uint16_t     repeat;      /**< iop_repeat_t                             */
    uint16_t     type;        /**< iop_type_t                               */
    uint16_t     size;        /**< sizeof(type);                            */
    uint16_t     data_offs;   /**< offset to the data                       */
    /**
     *   unused for IOP_T_{U,I}{8,16,32,64}, IOP_T_DOUBLE
     *   unused for IOP_T_{UNION,STRUCT}
     *   defval_enum holds the default value for IOP_T_ENUM
     *   defval_len  holds the default value length for IOP_T_{XML,STRING,DATA}
     */
    union {
        int      defval_enum;
        int      defval_len;
    } u0;
    /**
     *   defval_u64  holds the default value for IOP_T_{U,I}{8,16,32,64}
     *   defval_d    holds the default value for IOP_T_DOUBLE
     *   defval_data holds the default value data for IOP_T_{XML,STRING,DATA}
     *   st_desc     holds a pointer to the struct desc for IOP_T_{STRUCT,UNION}
     *   en_desc     holds a pointer to the enum desc for IOP_T_ENUM
     */
    union {
        const void         *defval_data;
        uint64_t            defval_u64;
        double              defval_d;
        const iop_struct_t *st_desc;
        const iop_enum_t   *en_desc;
    } u1;
} iop_field_t;

#define IOP_FIELD(type_t, v, i)  (((type_t *)v)[i])

/*}}}*/
/*{{{ Generic attributes */

typedef struct iop_help_t {
    lstr_t brief;
    lstr_t details;
    lstr_t warning;
} iop_help_t;

typedef union iop_value_t {
    int64_t     i;
    int64_t     i64;
    int32_t     i32;
    uint64_t    u;
    uint64_t    u64;
    uint32_t    u32;
    double      d;
    lstr_t      s;
    bool        b;
    const void *p;
    void       *v;
} iop_value_t;

/* For each iop object, an enum *_attr_type_t is declared which contains the
 * supported types for a generic attribute:
 * _GEN_ATTR_S for strings (v.s)
 * _GEN_ATTR_I for integers (v.i64)
 * _GEN_ATTR_D for doubles (v.d)
 * _GEN_ATTR_O for json objects (v.s)
 */
typedef struct iop_generic_attr_arg_t {
    iop_value_t v;
} iop_generic_attr_arg_t;

/*}}}*/
/*{{{ iop_enum_t */

typedef enum iop_enum_value_attr_type_t {
    IOP_ENUM_VALUE_ATTR_HELP,
    IOP_ENUM_VALUE_GEN_ATTR_S,
    IOP_ENUM_VALUE_GEN_ATTR_I,
    IOP_ENUM_VALUE_GEN_ATTR_D,
    IOP_ENUM_VALUE_GEN_ATTR_O,
} iop_enum_value_attr_type_t;

typedef iop_generic_attr_arg_t iop_enum_value_attr_arg_t;

typedef struct iop_enum_value_attr_t {
    iop_enum_value_attr_type_t       type;
    const iop_enum_value_attr_arg_t *args;
} iop_enum_value_attr_t;

typedef struct iop_enum_value_attrs_t {
    unsigned                     flags; /**< bitfield of iop_enum_value_attr_type_t */
    uint16_t                     attrs_len;
    uint8_t                      version;   /**< version 0 */
    uint8_t                      padding;
    const iop_enum_value_attr_t *attrs;
} iop_enum_value_attrs_t;

typedef enum iop_enum_attr_type_t {
    IOP_ENUM_ATTR_HELP,
    IOP_ENUM_GEN_ATTR_S,
    IOP_ENUM_GEN_ATTR_I,
    IOP_ENUM_GEN_ATTR_D,
    IOP_ENUM_GEN_ATTR_O,
} iop_enum_attr_type_t;

typedef iop_generic_attr_arg_t iop_enum_attr_arg_t;

typedef struct iop_enum_attr_t {
    iop_enum_attr_type_t       type;
    const iop_enum_attr_arg_t *args;
} iop_enum_attr_t;

typedef struct iop_enum_attrs_t {
    unsigned               flags; /**< bitfield of iop_enum_attr_type_t */
    uint16_t               attrs_len;
    uint8_t                version;   /**< version 0 */
    uint8_t                padding;
    const iop_enum_attr_t *attrs;
} iop_enum_attrs_t;

/*
 * .ranges helps finding tags into .fields.
 * ----------------------------------------
 *
 * it's a suite of [i_1, tag_1, ... i_k, tag_k, ... t_n, i_{n+1}]
 *
 * it says that at .fields[i_k] starts a contiguous range of tags from
 * value tag_k to tag_k + i_{k+1} - i_k.
 * Of course n == .ranges_len and i_{n+1} == .fields_len.
 *
 *
 * Example:
 * -------
 *
 * If you have the tags [1, 2, 3,  7,  9, 10, 11], the ranges will read:
 *                       --v----   v   ----v----
 *                      [0, 1,   3, 7,   4, 9,   7]
 *
 * Looking for a given tag needs a binary search the odd places of the
 * ranges array. Then, if you're tag T on the kth range, the index of your
 * description in .fields is:  (i_k + T - t_k)
 *
 */
struct iop_enum_t {
    const lstr_t                  name;
    const lstr_t                  fullname;
    const lstr_t                 *names;
    const int                    *values;
    const int                    *ranges;
    uint16_t                      enum_len;
    uint16_t                      flags; /**< bitfield of iop_enum_flags_t */
    int                           ranges_len;
    /* XXX do not dereference the following 2 members without checking
     * TST_BIT(this->flags, IOP_ENUM_EXTENDED) first */
    const iop_enum_attrs_t       *en_attrs;
    const iop_enum_value_attrs_t *values_attrs;
};

enum iop_enum_flags_t {
    IOP_ENUM_EXTENDED,      /**< to access en_attrs and values_attrs */
    IOP_ENUM_STRICT,        /**< strict packing/unpacking of enum values */
};

/*}}}*/
/*{{{ iop_struct_t */

typedef int (*check_constraints_f)(const void *ptr, int n);

typedef iop_generic_attr_arg_t iop_field_attr_arg_t;

typedef enum iop_field_attr_type_t {
    IOP_FIELD_MIN_OCCURS,
    IOP_FIELD_MAX_OCCURS,
    IOP_FIELD_CDATA,
    IOP_FIELD_MIN,
    IOP_FIELD_MAX,
    IOP_FIELD_NON_EMPTY,
    IOP_FIELD_NON_ZERO,
    IOP_FIELD_MIN_LENGTH,
    IOP_FIELD_MAX_LENGTH,
    IOP_FIELD_PATTERN,
    IOP_FIELD_PRIVATE,
    IOP_FIELD_ATTR_HELP,
    IOP_FIELD_GEN_ATTR_S,
    IOP_FIELD_GEN_ATTR_I,
    IOP_FIELD_GEN_ATTR_D,
    IOP_FIELD_GEN_ATTR_O,
} iop_field_attr_type_t;

typedef struct iop_field_attr_t {
    iop_field_attr_type_t        type;
    const iop_field_attr_arg_t  *args;
} iop_field_attr_t;

typedef struct iop_field_attrs_t {
    check_constraints_f      check_constraints;
    unsigned                 flags;  /**< bitfield of iop_field_attr_type_t */
    uint16_t                 attrs_len;
    uint8_t                  version;   /**< version 0 */
    uint8_t                  padding;
    const iop_field_attr_t  *attrs;
} iop_field_attrs_t;

typedef enum iop_struct_attr_type_t {
    IOP_STRUCT_ATTR_HELP,
    IOP_STRUCT_GEN_ATTR_S,
    IOP_STRUCT_GEN_ATTR_I,
    IOP_STRUCT_GEN_ATTR_D,
    IOP_STRUCT_GEN_ATTR_O,
} iop_struct_attr_type_t;

typedef iop_generic_attr_arg_t iop_struct_attr_arg_t;

typedef struct iop_struct_attr_t {
    iop_struct_attr_type_t       type;
    const iop_struct_attr_arg_t *args;
} iop_struct_attr_t;

typedef struct iop_struct_attrs_t {
    unsigned                 flags; /**< bitfield of iop_struct_attr_type_t */
    uint16_t                 attrs_len;
    uint8_t                  version;   /**< version 0 */
    uint8_t                  padding;
    const iop_struct_attr_t *attrs;
} iop_struct_attrs_t;

typedef struct iop_static_field_t {
    lstr_t                   name;
    iop_value_t              value;
    const iop_field_attrs_t *attrs; /**< NULL if there are none */
    uint16_t                 type;
} iop_static_field_t;

/* Class attributes */
typedef struct iop_class_attrs_t {
    const iop_struct_t        *parent; /**< NULL for "master" classes       */
    const iop_static_field_t **static_fields; /**< NULL if there are none   */
    uint8_t                    static_fields_len;
    uint8_t                    is_abstract : 1;
    uint8_t                    padding     : 7;
    uint16_t                   class_id;
} iop_class_attrs_t;

struct iop_struct_t {
    const lstr_t        fullname;
    const iop_field_t  *fields;
    const int          *ranges;
    uint16_t            ranges_len;
    uint16_t            fields_len;
    uint16_t            size;           /**< sizeof(type);                  */
    unsigned            flags    : 15;  /**< bitfield of iop_struct_flags_t */
    unsigned            is_union :  1;  /**< struct or union ?              */
    /* XXX do not dereference the following members without checking
     * TST_BIT(this->flags, IOP_STRUCT_EXTENDED) first */
    const iop_struct_attrs_t *st_attrs;
    const iop_field_attrs_t  *fields_attrs;
    /* XXX do not dereference the following members without checking
     * TST_BIT(this->flags, IOP_STRUCT_IS_CLASS) first */
    const iop_class_attrs_t  *class_attrs;
};

enum iop_struct_flags_t {
    IOP_STRUCT_EXTENDED,        /**< st_attrs and field_attrs exist */
    IOP_STRUCT_HAS_CONSTRAINTS, /**< will iop_check_constraints do smth? */
    IOP_STRUCT_IS_CLASS,        /**< is it a class? */
    IOP_STRUCT_STATIC_HAS_TYPE, /**< in class mode, does iop_static_field_t
                                 * have a type field? */
};

/*}}}*/
/*{{{ iop_rpc_t */

enum iop_rpc_flags_t {
    IOP_RPC_IS_ALIAS,
    IOP_RPC_HAS_ALIAS,
};

typedef iop_field_attr_arg_t iop_rpc_attr_arg_t;

typedef enum iop_rpc_attr_type_t {
    IOP_RPC_ALIAS,
    IOP_RPC_ATTR_HELP,
    IOP_RPC_ATTR_ARG_HELP,
    IOP_RPC_ATTR_RES_HELP,
    IOP_RPC_ATTR_EXN_HELP,
    IOP_RPC_GEN_ATTR_S,
    IOP_RPC_GEN_ATTR_I,
    IOP_RPC_GEN_ATTR_D,
    IOP_RPC_GEN_ATTR_O,
} iop_rpc_attr_type_t;

typedef struct iop_rpc_attr_t {
    iop_rpc_attr_type_t type;
    const iop_rpc_attr_arg_t *args;
} iop_rpc_attr_t;

typedef struct iop_rpc_attrs_t {
    unsigned                 flags;  /**< bitfield of iop_rpc_attr_type_t */
    uint16_t                 attrs_len;
    uint8_t                  version;   /**< version 0 */
    uint8_t                  padding;
    const iop_rpc_attr_t    *attrs;
} iop_rpc_attrs_t;

typedef struct iop_rpc_t {
    const lstr_t        name;
    const iop_struct_t *args;
    const iop_struct_t *result;
    const iop_struct_t *exn;
    uint32_t            tag;
    unsigned            async : 1;
    unsigned            flags : 31; /**< bitfield of iop_rpc_flags_t */
} iop_rpc_t;

/*}}}*/
/*{{{ iop_iface_t */

typedef enum iop_iface_attr_type_t {
    IOP_IFACE_ATTR_HELP,
    IOP_IFACE_GEN_ATTR_S,
    IOP_IFACE_GEN_ATTR_I,
    IOP_IFACE_GEN_ATTR_D,
    IOP_IFACE_GEN_ATTR_O,
} iop_iface_attr_type_t;

typedef iop_generic_attr_arg_t iop_iface_attr_arg_t;

typedef struct iop_iface_attr_t {
    iop_iface_attr_type_t       type;
    const iop_iface_attr_arg_t *args;
} iop_iface_attr_t;

typedef struct iop_iface_attrs_t {
    unsigned                flags; /**< bitfield of iop_iface_attr_type_t */
    uint16_t                attrs_len;
    uint8_t                 version;   /**< version 0 */
    uint8_t                 padding;
    const iop_iface_attr_t *attrs;
} iop_iface_attrs_t;

typedef struct iop_iface_t {
    const lstr_t             fullname;
    const iop_rpc_t         *funs;
    uint16_t                 funs_len;
    uint16_t                 flags; /**< bitfield of iop_iface_flags_t */
    const iop_rpc_attrs_t   *rpc_attrs;
    /** check TST_BIT(flags, IOP_IFACE_HAS_ATTRS)
     *  before accessing iface_attrs */
    const iop_iface_attrs_t *iface_attrs;
} iop_iface_t;

enum iop_iface_flags_t {
    IOP_IFACE_EXTENDED,
    IOP_IFACE_HAS_ATTRS,
};

/*}}}*/
/*{{{ iop_mod_t */

typedef struct iop_iface_alias_t {
    const iop_iface_t  *iface;
    const lstr_t        name;
    uint32_t            tag;
} iop_iface_alias_t;

typedef enum iop_mod_iface_attr_type_t {
    IOP_MOD_IFACE_ATTR_HELP,
} iop_mod_iface_attr_type_t;

typedef iop_generic_attr_arg_t iop_mod_iface_attr_arg_t;

typedef struct iop_mod_iface_attr_t {
    iop_mod_iface_attr_type_t       type;
    const iop_mod_iface_attr_arg_t *args;
} iop_mod_iface_attr_t;

typedef struct iop_mod_iface_attrs_t {
    unsigned                flags; /**< bitfield of iop_mod_iface_attr_type_t */
    uint16_t                attrs_len;
    uint8_t                 version;   /**< version 0 */
    uint8_t                 padding;
    const iop_mod_iface_attr_t *attrs;
} iop_mod_iface_attrs_t;

typedef enum iop_mod_attr_type_t {
    IOP_MOD_ATTR_HELP,
} iop_mod_attr_type_t;

typedef iop_generic_attr_arg_t iop_mod_attr_arg_t;

typedef struct iop_mod_attr_t {
    iop_mod_attr_type_t       type;
    const iop_mod_attr_arg_t *args;
} iop_mod_attr_t;

typedef struct iop_mod_attrs_t {
    unsigned              flags;     /**< bitfield of iop_mod_attr_type_t */
    uint16_t              attrs_len;
    uint8_t               version;   /**< version 0 */
    uint8_t               padding;
    const iop_mod_attr_t *attrs;
} iop_mod_attrs_t;

enum iop_mod_flags_t {
    IOP_MOD_EXTENDED,
};

typedef struct iop_mod_t {
    const lstr_t fullname;
    const iop_iface_alias_t *ifaces;
    uint16_t ifaces_len;
    uint16_t flags; /**< bitfield of iop_mod_flags_t */
    /** check TST_BIT(flags, IOP_MOD_EXTENDED)
     *  before accessing mod_attrs and ifaces_attrs */
    const iop_mod_attrs_t       *mod_attrs;
    const iop_mod_iface_attrs_t *ifaces_attrs;
} iop_mod_t;

/*}}}*/
/*{{{ iop_pkg_t */

typedef struct iop_pkg_t iop_pkg_t;
struct iop_pkg_t {
    const lstr_t               name;
    iop_enum_t   const *const *enums;
    iop_struct_t const *const *structs;
    iop_iface_t  const *const *ifaces;
    iop_mod_t    const *const *mods;
    iop_pkg_t    const *const *deps;
};

/*}}}*/
/*{{{ iop_array */

#define IOP_ARRAY_OF(type_t)                     \
    struct {                                     \
        type_t *tab;                             \
        int32_t len;                             \
        unsigned flags;                          \
    }
typedef IOP_ARRAY_OF(int8_t) iop_array_i8_t;
typedef IOP_ARRAY_OF(uint8_t) iop_array_u8_t;
typedef IOP_ARRAY_OF(int16_t) iop_array_i16_t;
typedef IOP_ARRAY_OF(uint16_t) iop_array_u16_t;
typedef IOP_ARRAY_OF(int32_t) iop_array_i32_t;
typedef IOP_ARRAY_OF(uint32_t) iop_array_u32_t;
typedef IOP_ARRAY_OF(int64_t) iop_array_i64_t;
typedef IOP_ARRAY_OF(uint64_t) iop_array_u64_t;
typedef IOP_ARRAY_OF(bool) iop_array_bool_t;
typedef iop_array_bool_t iop_array__Bool_t;
typedef IOP_ARRAY_OF(double) iop_array_double_t;
typedef IOP_ARRAY_OF(lstr_t) iop_array_lstr_t;

/*}}}*/
/*{{{ iop__void__t */

typedef struct iop__void__t { } iop__void__t;
extern iop_struct_t const iop__void__s;

/*}}}*/
/*{{{ IOP constraints */

__attr_printf__(1, 2)
int         iop_set_err(const char *fmt, ...) __cold;
__attr_printf__(1, 0)
void        iop_set_verr(const char *fmt, va_list ap) __cold ;
int         iop_set_err2(const lstr_t *s) __cold;
void        iop_clear_err(void);

/*}}}*/
/*{{{ IOP DSO */

typedef struct iop_dso_vt_t {
    size_t  vt_size;
    __attr_printf__(1, 0)
    void  (*iop_set_verr)(const char *fmt, va_list ap);
} iop_dso_vt_t;

#define IOP_EXPORT_PACKAGES(...) \
    EXPORT iop_pkg_t const *const iop_packages[];   \
    iop_pkg_t const *const iop_packages[] = { __VA_ARGS__, NULL }

#define IOP_USE_EXTERNAL_PACKAGES \
    EXPORT bool iop_use_external_packages;  \
    bool iop_use_external_packages = true;

#define IOP_EXPORT_PACKAGES_COMMON \
    EXPORT iop_dso_vt_t iop_vtable;                                     \
    iop_dso_vt_t iop_vtable = {                                         \
        .vt_size = sizeof(iop_dso_vt_t),                                \
        .iop_set_verr = NULL,                                           \
    };                                                                  \
    iop_struct_t const iop__void__s = {                                 \
        .fullname   = LSTR_IMMED("Void"),                               \
        .fields_len = 0,                                                \
        .size       = 0,                                                \
    };                                                                  \
                                                                        \
    __attr_printf__(1, 2)                                               \
    int iop_set_err(const char *fmt, ...) {                             \
        va_list ap;                                                     \
                                                                        \
        va_start(ap, fmt);                                              \
        if (NULL == iop_vtable.iop_set_verr) {                          \
            fputs("iop_vtable.iop_set_verr not defined", stderr);       \
            exit(1);                                                    \
        }                                                               \
        (iop_vtable.iop_set_verr)(fmt, ap);                             \
        va_end(ap);                                                     \
        return -1;                                                      \
    }                                                                   \

#define iop_dso_ressource_t(category)  iop_dso_ressource_##category##_t

#define IOP_DSO_DECLARE_RESSOURCE_CATEGORY(category, type)  \
    typedef type iop_dso_ressource_t(category)

#define IOP_DSO_EXPORT_RESSOURCES(category, ...)                \
    EXPORT const iop_dso_ressource_t(category) *const           \
        iop_dso_ressources_##category[];                        \
    const iop_dso_ressource_t(category) *const                  \
        iop_dso_ressources_##category[] = { __VA_ARGS__, NULL }

/*}}}*/

#endif
