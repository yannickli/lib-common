/**************************************************************************/
/*                                                                        */
/*  Copyright (C) 2004-2015 INTERSEC SA                                   */
/*                                                                        */
/*  Should you receive a copy of this source code, you must check you     */
/*  have a proper, written authorization of INTERSEC to hold it. If you   */
/*  don't have such an authorization, you must DELETE all source code     */
/*  files in your possession, and inform INTERSEC of the fact you obtain  */
/*  these files. Should you not comply to these terms, you can be         */
/*  prosecuted in the extent permitted by applicable law.                 */
/*                                                                        */
/**************************************************************************/

#ifndef IS_IOP_IOPC_IOPC_H
#define IS_IOP_IOPC_IOPC_H

#define IOPC_MAJOR   4
#define IOPC_MINOR   0
#define IOPC_PATCH   4

#define SNMP_OBJ_OID_MIN 1
#define SNMP_OBJ_OID_MAX 0xFFFF
#define SNMP_IFACE_OID_MIN 0
#define SNMP_IFACE_OID_MAX 0xFFFF

#include <lib-common/container.h>
#include <lib-common/iop.h>
#include <lib-common/log.h>

typedef enum iopc_tok_type_t {
    ITOK_EOF       = -1,
    ITOK_DOT       = '.',
    ITOK_SEMI      = ';',
    ITOK_EQUAL     = '=',
    ITOK_MINUS     = '-',
    ITOK_COLON     = ':',
    ITOK_COMMA     = ',',
    ITOK_LPAREN    = '(',
    ITOK_RPAREN    = ')',
    ITOK_LBRACKET  = '[',
    ITOK_RBRACKET  = ']',
    ITOK_LBRACE    = '{',
    ITOK_RBRACE    = '}',
    ITOK_QUESTION  = '?',
    ITOK_STAR      = '*',
    ITOK_UNDER     = '_',
    ITOK_SLASH     = '/',
    ITOK_TILDE     = '~',
    ITOK_CARET     = '^',
    ITOK_PLUS      = '+',
    ITOK_PERCENT   = '%',
    ITOK_AMP       = '&',
    ITOK_VBAR      = '|',
    ITOK_LT        = '<',
    ITOK_GT        = '>',

    ITOK_IDENT     = 128,
    ITOK_LSHIFT,
    ITOK_RSHIFT,
    ITOK_EXP,
    ITOK_INTEGER,
    ITOK_DOUBLE,
    ITOK_BOOL,
    ITOK_STRING,
    ITOK_COMMENT,
    ITOK_DOX_COMMENT,
    ITOK_VERBATIM_C,
    ITOK_ATTR,
    ITOK_GEN_ATTR_NAME,
} iopc_tok_type_t;

/*----- locations -----*/

typedef struct iopc_loc_t {
    const char *file;
    int lmin, lmax;
    int cmin, cmax;
    const char *comment;
} iopc_loc_t;
qvector_t(iopc_loc, iopc_loc_t);

extern struct {
    logger_t logger;

    qv_t(iopc_loc) loc_stack;
    int            print_info;
    int            v2;
    int            v3;
    int            v4;

    int class_id_min;
    int class_id_max;
} iopc_g;

void iopc_loc_merge(iopc_loc_t *l1, iopc_loc_t l2);
iopc_loc_t iopc_loc_merge2(iopc_loc_t l1, iopc_loc_t l2);

#define print_warning(fmt, ...)  \
    logger_warning(&iopc_g.logger, fmt, ##__VA_ARGS__)

#define fatal(fmt, ...)  \
    do {                                    \
        print_warning(fmt, ##__VA_ARGS__);  \
        fflush(stderr);                     \
        _exit(-1);                          \
    } while (0)

static inline const char *cwd(void)
{
    static char buf[BUFSIZ];

    if (getcwd(buf, sizeof(buf))) {
        int len = strlen(buf);

        if (len == sizeof(buf) - 1) {
            return ".../";
        }
        if (buf[len - 1] != '/') {
            buf[len++] = '/';
            buf[len] = '\0';
        }
        return buf;
    }
    return ".../";
}

#define do_loc_(fmt, level, t, loc, ...) \
    logger_log(&iopc_g.logger, level, "%s%s:%d:%d: %s: "fmt, cwd(),          \
               (loc).file, (loc).lmin, (loc).cmin, (t), ##__VA_ARGS__)

#define do_loc(fmt, level, t, loc, ...)  \
    do {                                                                     \
        do_loc_(fmt, level, t, loc, ##__VA_ARGS__);                          \
        for (int i_ = 0; i_ < iopc_g.loc_stack.len; i_++) {                  \
            iopc_loc_t loc_ = iopc_g.loc_stack.tab[i_];                      \
            do_loc_("%s", level, " from", loc_, loc_.comment);               \
        }                                                                    \
    } while (0)

#define t_push_loc(loc, ...)  \
    do {                                                                     \
        qv_append(iopc_loc, &iopc_g.loc_stack, loc);                         \
        qv_last(iopc_loc, &iopc_g.loc_stack)->comment = t_fmt(__VA_ARGS__);  \
    } while (0)

#define pop_loc()          qv_shrink(iopc_loc, &iopc_g.loc_stack, 1)
#define clear_loc()        qv_clear(iopc_loc,  &iopc_g.loc_stack)

#define info_loc(fmt, loc, ...)   \
    do {                                                                     \
        if (iopc_g.print_info) {                                             \
            do_loc(fmt, LOG_INFO, "info", loc, ##__VA_ARGS__);               \
        }                                                                    \
    } while (0)

#define warn_loc(fmt, loc, ...)   \
    do_loc(fmt, LOG_WARNING, "warning", loc, ##__VA_ARGS__)

#define fatal_loc(fmt, loc, ...)  \
    do {                                                                     \
        do_loc(fmt, LOG_ERR, "error", loc, ##__VA_ARGS__);                   \
        fflush(stderr);                                                      \
        _exit(-1);                                                           \
    } while (0)

/*----- doxygen lexer part -----*/

qvector_t(sb, sb_t);

typedef struct dox_chunk_t {
    lstr_t     keyword;
    qv_t(lstr) params;      /* params inside [] following the keyword */
    qv_t(lstr) params_args; /* IDs starting the first paragraph */
    qv_t(sb)   paragraphs;
    int        paragraph0_args_len;
    int        first_sentence_len; /* used for auto-brief */
    iopc_loc_t loc;
} dox_chunk_t;
GENERIC_INIT(dox_chunk_t, dox_chunk);
static inline void dox_chunk_wipe(dox_chunk_t *p)
{
    lstr_wipe(&p->keyword);
    qv_deep_wipe(lstr, &p->params, lstr_wipe);
    qv_deep_wipe(lstr, &p->params_args, lstr_wipe);
    qv_deep_wipe(sb, &p->paragraphs, sb_wipe);
}

qvector_t(dox_chunk, dox_chunk_t);

typedef struct dox_tok_t {
    qv_t(dox_chunk) chunks;
    flag_t is_back : 1;
} dox_tok_t;
static inline void dox_tok_wipe(dox_tok_t *p) {
    /* XXX: don't deep_wipe chunks when wiping dox_tok
     *      elements of chunks will be used after deletion of the token
     */
    qv_wipe(dox_chunk, &p->chunks);
}
GENERIC_NEW_INIT(dox_tok_t, dox_tok);
GENERIC_DELETE(dox_tok_t, dox_tok);

/*----- doxygen data -----*/

typedef enum iopc_dox_type_t {
    IOPC_DOX_TYPE_BRIEF,
    IOPC_DOX_TYPE_DETAILS,
    IOPC_DOX_TYPE_WARNING,

    IOPC_DOX_TYPE_count,
} iopc_dox_type_t;

lstr_t iopc_dox_type_to_lstr(iopc_dox_type_t);

typedef struct iopc_dox_t {
    iopc_dox_type_t type;
    lstr_t          desc;
} iopc_dox_t;
GENERIC_INIT(iopc_dox_t, iopc_dox);
static inline void iopc_dox_wipe(iopc_dox_t *dox)
{
    lstr_wipe(&dox->desc);
}

qvector_t(iopc_dox, iopc_dox_t);

iopc_dox_t *iopc_dox_find_type(const qv_t(iopc_dox) *, iopc_dox_type_t);

/*----- lexer and tokens -----*/

typedef struct iopc_token_t {
    iopc_loc_t      loc;
    iopc_tok_type_t token;

    int refcnt;
    union {
        uint64_t i;
        double   d;
    };
    bool i_is_signed;
    bool b_is_char;
    sb_t b;
    dox_tok_t *dox;
} iopc_token_t;
static inline iopc_token_t *iopc_token_init(iopc_token_t *tk) {
    p_clear(tk, 1);
    sb_init(&tk->b);
    return tk;
}
static inline void iopc_token_wipe(iopc_token_t *tk) {
    sb_wipe(&tk->b);
    dox_tok_delete(&tk->dox);
}
DO_REFCNT(iopc_token_t, iopc_token);
qvector_t(iopc_token, iopc_token_t *);

typedef enum iopc_file_t {
    IOPC_FILE_FD,
    IOPC_FILE_STDIN,
    IOPC_FILE_BUFFER,
} iopc_file_t;

struct lexdata *iopc_lexer_new(const char *file, const char *data,
                               iopc_file_t type);
int iopc_lexer_fd(struct lexdata *);
void iopc_lexer_push_state_attr(struct lexdata *ld);
void iopc_lexer_pop_state(struct lexdata *ld);
void iopc_lexer_delete(struct lexdata **);
iopc_token_t *iopc_next_token(struct lexdata *, bool want_comments);


/*----- ast -----*/

typedef struct iopc_path_t {
    int refcnt;
    iopc_loc_t loc;
    qv_t(str) bits;
    char *pretty_dot;
    char *pretty_slash;
} iopc_path_t;
static inline iopc_path_t *iopc_path_init(iopc_path_t *path) {
    p_clear(path, 1);
    qv_init(str, &path->bits);
    return path;
}
static inline void iopc_path_wipe(iopc_path_t *path) {
    p_delete(&path->pretty_dot);
    p_delete(&path->pretty_slash);
    qv_deep_wipe(str, &path->bits, p_delete);
}
DO_REFCNT(iopc_path_t, iopc_path);
qvector_t(iopc_path, iopc_path_t *);

typedef struct iopc_pkg_t iopc_pkg_t;
static inline void iopc_pkg_delete(iopc_pkg_t **);

typedef struct iopc_import_t {
    iopc_loc_t loc;
    iopc_path_t *path;
    iopc_pkg_t *pkg;
    char *type;
    flag_t used : 1;
} iopc_import_t;
GENERIC_NEW_INIT(iopc_import_t, iopc_import);
static inline void iopc_import_wipe(iopc_import_t *import) {
    iopc_path_delete(&import->path);
    p_delete(&import->type);
}
GENERIC_DELETE(iopc_import_t, iopc_import);
qvector_t(iopc_import, iopc_import_t *);

/*----- attributes -----*/

typedef enum iopc_attr_id_t {
    IOPC_ATTR_CTYPE,
    IOPC_ATTR_NOWARN,
    IOPC_ATTR_PREFIX,
    IOPC_ATTR_STRICT,
    IOPC_ATTR_MIN,
    IOPC_ATTR_MAX,
    IOPC_ATTR_MIN_LENGTH,
    IOPC_ATTR_MAX_LENGTH,
    IOPC_ATTR_LENGTH,
    IOPC_ATTR_MIN_OCCURS,
    IOPC_ATTR_MAX_OCCURS,
    IOPC_ATTR_CDATA,
    IOPC_ATTR_NON_EMPTY,
    IOPC_ATTR_NON_ZERO,
    IOPC_ATTR_PATTERN,
    IOPC_ATTR_PRIVATE,
    IOPC_ATTR_ALIAS,
    IOPC_ATTR_NO_REORDER,
    IOPC_ATTR_ALLOW,
    IOPC_ATTR_DISALLOW,
    IOPC_ATTR_GENERIC,
    IOPC_ATTR_DEPRECATED,
    IOPC_ATTR_SNMP_PARAMS_FROM,
} iopc_attr_id_t;

/* types on which an attribute can apply */
typedef enum iopc_attr_type_t {
    /* fields */
    IOPC_ATTR_T_INT,
    IOPC_ATTR_T_BOOL,
    IOPC_ATTR_T_DOUBLE,
    IOPC_ATTR_T_STRING,
    IOPC_ATTR_T_DATA,
    IOPC_ATTR_T_XML,
    /* fields or declarations */
    IOPC_ATTR_T_ENUM,
    IOPC_ATTR_T_UNION,
    IOPC_ATTR_T_STRUCT,
    /* declarations */
    IOPC_ATTR_T_RPC,
    IOPC_ATTR_T_IFACE,
    IOPC_ATTR_T_MOD,
    /* snmp */
    IOPC_ATTR_T_SNMP_OBJ,
    IOPC_ATTR_T_SNMP_IFACE,
    IOPC_ATTR_T_SNMP_TBL,
} iopc_attr_type_t;

#define IOPC_ATTR_T_ALL_FIELDS  BITMASK_LE(int64_t, IOPC_ATTR_T_STRUCT)
#define IOPC_ATTR_T_ALL         BITMASK_LE(int64_t, IOPC_ATTR_T_MOD)

typedef enum iopc_attr_flag_t {
    /* attribute can apply to required fields */
    IOPC_ATTR_F_FIELD_REQUIRED,
    /* attribute can apply to field with default value */
    IOPC_ATTR_F_FIELD_DEFVAL,
    /* attribute can apply to optional fields */
    IOPC_ATTR_F_FIELD_OPTIONAL,
    /* attribute can apply to repeated fields */
    IOPC_ATTR_F_FIELD_REPEATED,
    /* attribute can apply to declarations */
    IOPC_ATTR_F_DECL,
    /* attribute can be used more than once */
    IOPC_ATTR_F_MULTI,
    /* attribute will generate a check in iop_check_constraints */
    IOPC_ATTR_F_CONSTRAINT,
} iopc_attr_flag_t;

#define IOPC_ATTR_F_FIELD_ALL BITMASK_LE(int64_t, IOPC_ATTR_F_FIELD_REPEATED)
#define IOPC_ATTR_F_FIELD_ALL_BUT_REQUIRED                \
    (IOPC_ATTR_F_FIELD_ALL                                \
     & ~BITMASK_NTH(int64_t, IOPC_ATTR_F_FIELD_REQUIRED))

typedef struct iopc_arg_desc_t {
    lstr_t          name;
    /* ITOK_STRING / ITOK_IDENT / ITOK_DOUBLE / ITOK_INTEGER
     * when set to ITOK_DOUBLE, ITOK_INTEGER is accepted as well */
    iopc_tok_type_t type;
} iopc_arg_desc_t;
static inline iopc_arg_desc_t *iopc_arg_desc_init(iopc_arg_desc_t *arg) {
    p_clear(arg, 1);
    return arg;
}
static inline void iopc_arg_desc_wipe(iopc_arg_desc_t *arg) {
    lstr_wipe(&arg->name);
}
GENERIC_NEW(iopc_arg_desc_t, iopc_arg_desc);
GENERIC_DELETE(iopc_arg_desc_t, iopc_arg_desc);
qvector_t(iopc_arg_desc, iopc_arg_desc_t);

typedef struct iopc_attr_desc_t {
    iopc_attr_id_t      id;
    lstr_t              name;
    qv_t(iopc_arg_desc) args;
    /* bitfield of iopc_attr_type_t */
    int64_t             types;
    /* bitfield of iopc_attr_flag_t */
    int64_t             flags;
} iopc_attr_desc_t;
#define IOPC_ATTR_REPEATED_MONO_ARG(desc)  \
    ({                                                                       \
        const iopc_attr_desc_t *__desc = (desc);                             \
        __desc->args.len == 1 && TST_BIT(&__desc->flags, IOPC_ATTR_F_MULTI); \
    })

static inline iopc_attr_desc_t *iopc_attr_desc_init(iopc_attr_desc_t *d) {
    p_clear(d, 1);
    qv_init(iopc_arg_desc, &d->args);
    return d;
}
static inline void iopc_attr_desc_wipe(iopc_attr_desc_t *d) {
    lstr_wipe(&d->name);
    qv_deep_wipe(iopc_arg_desc, &d->args, iopc_arg_desc_wipe);
}
GENERIC_NEW(iopc_attr_desc_t, iopc_attr_desc);
GENERIC_DELETE(iopc_attr_desc_t, iopc_attr_desc);
qm_kvec_t(attr_desc, lstr_t, iopc_attr_desc_t,
          qhash_lstr_hash, qhash_lstr_equal);


typedef struct iopc_arg_t {
    iopc_loc_t           loc;
    iopc_arg_desc_t     *desc;
    /* ITOK_STRING / ITOK_IDENT / ITOK_DOUBLE / ITOK_INTEGER */
    iopc_tok_type_t      type;
    union {
        int64_t i64;
        double  d;
        lstr_t  s;
    } v;
} iopc_arg_t;

static inline iopc_arg_t iopc_arg_dup(const iopc_arg_t *arg) {
    iopc_arg_t a = *arg;

    if (a.desc) {
        switch (a.desc->type) {
          case ITOK_STRING:
          case ITOK_IDENT:
            a.v.s = lstr_dup(arg->v.s);
            break;
          default:
            break;
        }
    }
    return a;
}
static inline iopc_arg_t *iopc_arg_init(iopc_arg_t *a) {
    p_clear(a, 1);
    return a;
}
static inline void iopc_arg_wipe(iopc_arg_t *a) {
    if (a->desc) {
        switch (a->desc->type) {
          case ITOK_STRING:
          case ITOK_IDENT:
            lstr_wipe(&a->v.s);
            break;
          default:
            break;
        }
    }
}
GENERIC_NEW(iopc_arg_t, iopc_arg);
GENERIC_DELETE(iopc_arg_t, iopc_arg);
qvector_t(iopc_arg, iopc_arg_t);

typedef struct iopc_extends_t {
    flag_t     is_snmp_root : 1;
    iopc_loc_t loc;
    iopc_path_t *path;
    iopc_pkg_t  *pkg;
    char *name;
    struct iopc_struct_t *st;
} iopc_extends_t;
GENERIC_NEW_INIT(iopc_extends_t, iopc_extends);
static inline void iopc_extends_wipe(iopc_extends_t *extends);
GENERIC_DELETE(iopc_extends_t, iopc_extends);
qvector_t(iopc_extends, iopc_extends_t *);

typedef struct iopc_attr_t {
    int                  refcnt;
    iopc_loc_t           loc;
    iopc_attr_desc_t    *desc;
    qv_t(iopc_arg)       args;

    /* Used only for generic attributes */
    lstr_t               real_name;

    /* Used only for snmp_params_from attributes */
    qv_t(iopc_extends)   snmp_params_from;
} iopc_attr_t;

static inline iopc_attr_t *iopc_attr_init(iopc_attr_t *a) {
    p_clear(a, 1);
    qv_init(iopc_arg, &a->args);
    qv_init(iopc_extends, &a->snmp_params_from);
    return a;
}
static inline void iopc_attr_wipe(iopc_attr_t *a) {
    lstr_wipe(&a->real_name);
    qv_deep_wipe(iopc_arg, &a->args, iopc_arg_wipe);
    qv_deep_wipe(iopc_extends, &a->snmp_params_from, iopc_extends_delete);
}
DO_REFCNT(iopc_attr_t, iopc_attr);
qvector_t(iopc_attr, iopc_attr_t *);

int
iopc_attr_check(const qv_t(iopc_attr) *, iopc_attr_id_t,
                const qv_t(iopc_arg) **out);

int t_iopc_attr_check_prefix(const qv_t(iopc_attr) *, lstr_t *out);

typedef enum iopc_struct_type_t {
    STRUCT_TYPE_STRUCT   = 0,
    STRUCT_TYPE_CLASS    = 1,
    STRUCT_TYPE_UNION    = 2,
    STRUCT_TYPE_TYPEDEF  = 3,
    STRUCT_TYPE_SNMP_OBJ = 4,
    STRUCT_TYPE_SNMP_TBL = 5,
} iopc_struct_type_t;

static inline bool iopc_is_class(iopc_struct_type_t type)
{
    return type == STRUCT_TYPE_CLASS;
}
static inline bool iopc_is_snmp_obj(iopc_struct_type_t type)
{
    return type == STRUCT_TYPE_SNMP_OBJ;
}
static inline bool iopc_is_snmp_tbl(iopc_struct_type_t type)
{
    return type == STRUCT_TYPE_SNMP_TBL;
}
static inline bool iopc_is_snmp_st(iopc_struct_type_t type)
{
    return type == STRUCT_TYPE_SNMP_TBL || type == STRUCT_TYPE_SNMP_OBJ;
}
static inline const char *iopc_struct_type_to_str(iopc_struct_type_t type)
{
    switch (type) {
      case STRUCT_TYPE_CLASS:
        return "class";
      case STRUCT_TYPE_SNMP_OBJ:
        return "snmpObj";
      case STRUCT_TYPE_UNION:
        return "union";
      case STRUCT_TYPE_TYPEDEF:
        return "typedef";
      case STRUCT_TYPE_STRUCT:
        return "struct";
      case STRUCT_TYPE_SNMP_TBL:
        return "snmpTbl";
      default:
        e_panic("type not handled");
    }
}

typedef enum iopc_defval_t {
    IOPC_DEFVAL_NONE,
    IOPC_DEFVAL_STRING,
    IOPC_DEFVAL_INTEGER,
    IOPC_DEFVAL_DOUBLE,
} iopc_defval_t;

typedef enum iopc_iface_type_t {
    IFACE_TYPE_IFACE        = 0,
    IFACE_TYPE_SNMP_IFACE   = 1,
} iopc_iface_type_t;

static inline bool iopc_is_snmp_iface(iopc_iface_type_t type)
{
    return type == IFACE_TYPE_SNMP_IFACE;
}

typedef struct iopc_field_t {
    int        refcnt;
    uint16_t   size;
    uint8_t    align;
    iopc_struct_type_t type;

    iopc_loc_t loc;

    int tag;
    int pos;  /* To sort fields by order of appearance in struct/class. */
    int repeat;
    char *name;

    union {
        uint64_t u64;
        double d;
        void *ptr;
    } defval;
    iopc_defval_t defval_type;
    flag_t defval_is_signed : 1;
    flag_t is_visible : 1;
    flag_t resolving  : 1;
    flag_t is_static  : 1;
    flag_t is_ref     : 1;
    /* In case the field is contained by a snmpIface rpc struct', it
     * references another snmpObj field */
    flag_t snmp_is_from_param : 1;

    /** kind of the resolved type */
    iop_type_t kind;
    /** path of the resolved complex type */
    iopc_path_t *type_path;
    /** package of the resolved complex type */
    iopc_pkg_t *type_pkg;
    /** in case of a typedef, package of the typedef, otherwise same as
     * type_pkg */
    iopc_pkg_t *found_pkg;
    /** definition of the resolved complex type */
    union {
        struct iopc_struct_t *struct_def;
        struct iopc_struct_t *union_def;
        struct iopc_enum_t   *enum_def;
    };
    char *type_name;
    char *pp_type;

    qv_t(iopc_attr) attrs;
    qv_t(iopc_dox)  comments;

    /* In case the field is contained by a snmpIface rpc struct' */
    struct iopc_field_t *field_origin; /* the reference field */
    qv_t(iopc_extends) parents;
} iopc_field_t;
static inline iopc_field_t *iopc_field_init(iopc_field_t *field) {
    p_clear(field, 1);
    field->type = STRUCT_TYPE_TYPEDEF;
    qv_init(iopc_attr, &field->attrs);
    qv_init(iopc_dox, &field->comments);
    qv_init(iopc_extends, &field->parents);
    return field;
}
static inline void iopc_field_wipe(iopc_field_t *field) {
    p_delete(&field->name);
    p_delete(&field->type_name);
    p_delete(&field->pp_type);
    iopc_path_delete(&field->type_path);
    qv_deep_wipe(iopc_attr, &field->attrs, iopc_attr_delete);
    qv_deep_wipe(iopc_dox, &field->comments, iopc_dox_wipe);
    if (field->defval_type == IOPC_DEFVAL_STRING) {
        p_delete(&field->defval.ptr);
    }
    qv_deep_wipe(iopc_extends, &field->parents, iopc_extends_delete);
}
DO_REFCNT(iopc_field_t, iopc_field);
qvector_t(iopc_field, iopc_field_t *);
qm_kptr_t(iopc_field, char, iopc_field_t *, qhash_str_hash, qhash_str_equal);

void iopc_check_field_attributes(iopc_field_t *f, bool tdef);
void iopc_field_add_attr(iopc_field_t *f, iopc_attr_t **attrp, bool tdef);

/* used for the code generation of field attributes */
typedef struct iopc_attrs_t {
    unsigned                 flags;  /**< bitfield of iop_field_attr_type_t */
    uint16_t                 attrs_len;
    lstr_t                   attrs_name;
    lstr_t                   checkf_name;
} iopc_attrs_t;
qvector_t(iopc_attrs, iopc_attrs_t);
GENERIC_INIT(iopc_attrs_t, iopc_attrs);
static inline void iopc_attrs_wipe(iopc_attrs_t *attrs)
{
    lstr_wipe(&attrs->attrs_name);
    lstr_wipe(&attrs->checkf_name);
}


/* Used to detect duplicated ids in an inheritance tree */
qm_k32_t(id_class, struct iopc_struct_t *);

typedef struct iopc_struct_t {
    uint16_t   size;
    uint8_t    align;
    iopc_struct_type_t type;
    iopc_loc_t loc;
    flag_t     is_visible : 1;
    flag_t     optimized : 1;
    flag_t     resolving : 1;
    flag_t     resolved  : 1;
    flag_t     resolving_inheritance : 1;
    flag_t     resolved_inheritance  : 1;
    flag_t     checked_constraints  : 1;
    flag_t     has_constraints      : 1;
    flag_t     has_fields_attrs     : 1;    /**< st.fields_attrs existence  */
    flag_t     is_abstract          : 1;
    flag_t     is_local             : 1;
    /* struct has snmpParams attribute */
    flag_t     is_snmp_params       : 1;
    /* struct is a snmpIface rpc' struct */
    flag_t     contains_snmp_info : 1;
    unsigned   flags;                       /**< st.flags                   */

    char      *name;
    lstr_t     sig;
    union {
        int    class_id;
        int    oid;
    };
    struct iopc_struct_t *same_as;
    struct iopc_iface_t  *iface;
    qv_t(iopc_field)   fields;
    qv_t(iopc_field)   static_fields;
    int                nb_real_static_fields; /**< those with a defval */
    qv_t(iopc_extends) extends;
    qv_t(iopc_attr)    attrs;
    qv_t(iopc_dox)     comments;

    /* Used for master classes (ie. not having a parent); indexes all the
     * children classes by their id. */
    qm_t(id_class) children_by_id;
} iopc_struct_t;
static inline iopc_struct_t *iopc_struct_init(iopc_struct_t *st) {
    p_clear(st, 1);
    qv_init(iopc_field, &st->fields);
    qv_init(iopc_field, &st->static_fields);
    qv_init(iopc_extends, &st->extends);
    qv_init(iopc_attr, &st->attrs);
    qv_init(iopc_dox, &st->comments);
    qm_init(id_class, &st->children_by_id);
    return st;
}
static inline void iopc_struct_delete(iopc_struct_t **);
static inline void iopc_iface_delete(struct iopc_iface_t **);
static inline void iopc_struct_wipe(iopc_struct_t *st) {
    qv_deep_wipe(iopc_extends, &st->extends, iopc_extends_delete);
    qv_deep_wipe(iopc_field, &st->fields, iopc_field_delete);
    qv_deep_wipe(iopc_field, &st->static_fields, iopc_field_delete);
    qv_deep_wipe(iopc_attr, &st->attrs, iopc_attr_delete);
    qv_deep_wipe(iopc_dox, &st->comments, iopc_dox_wipe);
    p_delete(&st->name);
    lstr_wipe(&st->sig);
    qm_wipe(id_class, &st->children_by_id);
}
GENERIC_NEW(iopc_struct_t, iopc_struct);
GENERIC_DELETE(iopc_struct_t, iopc_struct);
qvector_t(iopc_struct, iopc_struct_t *);
qm_kptr_t(iopc_struct, char, iopc_struct_t *,
          qhash_str_hash, qhash_str_equal);

static inline void iopc_extends_wipe(iopc_extends_t *extends) {
    iopc_path_delete(&extends->path);
    p_delete(&extends->name);
}

typedef struct iopc_enum_field_t {
    iopc_loc_t loc;
    char *name;
    int value;
    qv_t(iopc_attr) attrs;
    qv_t(iopc_dox) comments;
} iopc_enum_field_t;
static inline iopc_enum_field_t *iopc_enum_field_init(iopc_enum_field_t *e) {
    p_clear(e, 1);
    qv_init(iopc_attr, &e->attrs);
    qv_init(iopc_dox, &e->comments);
    return e;
}
static inline void iopc_enum_field_wipe(iopc_enum_field_t *e) {
    p_delete(&e->name);
    qv_deep_wipe(iopc_attr, &e->attrs, iopc_attr_delete);
    qv_deep_wipe(iopc_dox, &e->comments, iopc_dox_wipe);
}
GENERIC_NEW(iopc_enum_field_t, iopc_enum_field);
GENERIC_DELETE(iopc_enum_field_t, iopc_enum_field);
qvector_t(iopc_enum_field, iopc_enum_field_t *);

typedef struct iopc_enum_t {
    flag_t     is_visible : 1;
    iopc_loc_t loc;
    char *name;
    qv_t(iopc_enum_field) values;
    qv_t(iopc_attr)       attrs;
    qv_t(iopc_dox)        comments;
} iopc_enum_t;
static inline iopc_enum_t *iopc_enum_init(iopc_enum_t *e) {
    p_clear(e, 1);
    qv_init(iopc_enum_field, &e->values);
    qv_init(iopc_attr, &e->attrs);
    qv_init(iopc_dox, &e->comments);
    return e;
}
static inline void iopc_enum_wipe(iopc_enum_t *e) {
    qv_deep_wipe(iopc_enum_field, &e->values, iopc_enum_field_delete);
    qv_deep_wipe(iopc_attr, &e->attrs, iopc_attr_delete);
    qv_deep_wipe(iopc_dox, &e->comments, iopc_dox_wipe);
    p_delete(&e->name);
}
GENERIC_NEW(iopc_enum_t, iopc_enum);
GENERIC_DELETE(iopc_enum_t, iopc_enum);
qvector_t(iopc_enum, iopc_enum_t *);

typedef struct iopc_fun_t {
    iopc_loc_t loc;
    int        tag;
    char      *name;

    flag_t arg_is_anonymous : 1;
    flag_t res_is_anonymous : 1;
    flag_t exn_is_anonymous : 1;
    flag_t fun_is_async     : 1;
    union {
        iopc_struct_t *arg;
        iopc_field_t  *farg; /* When we reference an existing structure */
    };
    union {
        iopc_struct_t *res;
        iopc_field_t  *fres; /* When we reference an existing structure */
    };
    union {
        iopc_struct_t *exn;
        iopc_field_t  *fexn; /* When we reference an existing structure */
    };

    qv_t(iopc_attr) attrs;
    qv_t(iopc_dox)  comments;
} iopc_fun_t;
static inline iopc_fun_t *iopc_fun_init(iopc_fun_t *fun) {
    p_clear(fun, 1);
    qv_init(iopc_attr, &fun->attrs);
    qv_init(iopc_dox, &fun->comments);
    return fun;
}
static inline void iopc_fun_wipe(iopc_fun_t *fun) {
#define DELETE(v) \
    if (fun->v##_is_anonymous) {                \
        iopc_struct_delete(&fun->v);            \
    } else {                                    \
        iopc_field_delete(&fun->f##v);          \
    }
    DELETE(arg);
    DELETE(res);
    DELETE(exn);
#undef DELETE
    p_delete(&fun->name);
    qv_deep_wipe(iopc_attr, &fun->attrs, iopc_attr_delete);
    qv_deep_wipe(iopc_dox, &fun->comments, iopc_dox_wipe);
}
GENERIC_NEW(iopc_fun_t, iopc_fun);
GENERIC_DELETE(iopc_fun_t, iopc_fun);
qvector_t(iopc_fun, iopc_fun_t *);
qm_kptr_t(iopc_fun, char, iopc_fun_t *, qhash_str_hash, qhash_str_equal);

typedef struct iopc_iface_t {
    flag_t     is_visible : 1;
    iopc_loc_t loc;
    unsigned   flags;

    char *name;
    qv_t(iopc_fun)  funs;
    qv_t(iopc_attr) attrs;
    qv_t(iopc_dox)  comments;

    /* Used only for snmpIface*/
    iopc_iface_type_t type;
    int oid;
    qv_t(iopc_extends) extends;
} iopc_iface_t;
static inline iopc_iface_t *iopc_iface_init(iopc_iface_t *iface) {
    p_clear(iface, 1);
    qv_init(iopc_fun, &iface->funs);
    qv_init(iopc_attr, &iface->attrs);
    qv_init(iopc_dox, &iface->comments);
    qv_init(iopc_extends, &iface->extends);
    return iface;
}
static inline void iopc_iface_wipe(iopc_iface_t *iface) {
    qv_deep_wipe(iopc_fun, &iface->funs, iopc_fun_delete);
    qv_deep_wipe(iopc_attr, &iface->attrs, iopc_attr_delete);
    qv_deep_wipe(iopc_dox, &iface->comments, iopc_dox_wipe);
    qv_deep_wipe(iopc_extends, &iface->extends, iopc_extends_delete);
    p_delete(&iface->name);
}
GENERIC_NEW(iopc_iface_t, iopc_iface);
GENERIC_DELETE(iopc_iface_t, iopc_iface);
qvector_t(iopc_iface, iopc_iface_t *);

struct iopc_pkg_t {
    flag_t t_resolving : 1;
    flag_t i_resolving : 1;
    flag_t t_resolved  : 1;
    flag_t i_resolved  : 1;

    char        *file;
    char        *base;
    iopc_path_t *name;
    sb_t         verbatim_c;

    qv_t(iopc_import) imports;
    qv_t(iopc_enum)   enums;
    qv_t(iopc_struct) structs;
    qv_t(iopc_iface)  ifaces;
    qv_t(iopc_struct) modules;
    qv_t(iopc_field)  typedefs;
    qv_t(iopc_dox)    comments;
};
static inline iopc_pkg_t *iopc_pkg_init(iopc_pkg_t *pkg) {
    p_clear(pkg, 1);
    sb_init(&pkg->verbatim_c);
    qv_init(iopc_import, &pkg->imports);
    qv_init(iopc_enum, &pkg->enums);
    qv_init(iopc_struct, &pkg->structs);
    qv_init(iopc_struct, &pkg->modules);
    qv_init(iopc_iface, &pkg->ifaces);
    qv_init(iopc_field, &pkg->typedefs);
    qv_init(iopc_dox, &pkg->comments);
    return pkg;
}
static inline void iopc_pkg_wipe(iopc_pkg_t *pkg) {
    sb_wipe(&pkg->verbatim_c);
    qv_deep_wipe(iopc_iface, &pkg->ifaces, iopc_iface_delete);
    qv_deep_wipe(iopc_struct, &pkg->structs, iopc_struct_delete);
    qv_deep_wipe(iopc_struct, &pkg->modules, iopc_struct_delete);
    qv_deep_wipe(iopc_enum, &pkg->enums, iopc_enum_delete);
    qv_deep_wipe(iopc_import, &pkg->imports, iopc_import_delete);
    qv_deep_wipe(iopc_field, &pkg->typedefs, iopc_field_delete);
    qv_deep_wipe(iopc_dox, &pkg->comments, iopc_dox_wipe);
    iopc_path_delete(&pkg->name);
    p_delete(&pkg->file);
    p_delete(&pkg->base);
}
GENERIC_NEW(iopc_pkg_t, iopc_pkg);
GENERIC_DELETE(iopc_pkg_t, iopc_pkg);
qvector_t(iopc_pkg, iopc_pkg_t *);
qm_kptr_ckey_t(iopc_pkg, char, iopc_pkg_t *, qhash_str_hash, qhash_str_equal);

/*----- pretty printing  -----*/

const char *t_pretty_token(iopc_tok_type_t token);
const char *pretty_path(iopc_path_t *path);
const char *pretty_path_dot(iopc_path_t *path);
static inline const char *pretty_path_base(iopc_path_t *path) {
    return path->bits.tab[path->bits.len - 1];
}


/*----- parser & typer -----*/

qm_kptr_t(iopc_env, char, char *, qhash_str_hash, qhash_str_equal);

void iopc_parser_initialize(void);
void iopc_parser_shutdown(void);
iopc_pkg_t *iopc_parse_file(const qv_t(cstr) *includes,
                            const qm_t(iopc_env) *env, const char *file,
                            const char *data, bool is_main_pkg);
void iopc_resolve(iopc_pkg_t *pkg);
void iopc_resolve_second_pass(iopc_pkg_t *pkg);
void iopc_types_fold(iopc_pkg_t *pkg);
void iopc_depends_uniquify(qv_t(iopc_pkg) *deps);
void iopc_get_depends(iopc_pkg_t *pkg,
                      qv_t(iopc_pkg) *t_deps,
                      qv_t(iopc_pkg) *t_weak_deps,
                      qv_t(iopc_pkg) *i_deps);


/*----- utilities -----*/

bool iopc_field_is_signed(const iopc_field_t *);

/*----- writing output files -----*/

#define DOUBLE_FMT  "%.17e"

#define IOPC_ATTR_GET_ARG_V(_type, _a)  \
    ((_a)->type == ITOK_INTEGER || (_a)->type == ITOK_BOOL ? \
     (_type)(_a)->v.i64 : (_type)(_a)->v.d)

typedef struct iopc_write_buf_t {
    sb_t *buf;
    sb_t *tab; /* for tabulations in start of lines */
} iopc_write_buf_t;

#define IOPC_WRITE_START_LINE(_wbuf, _fct, _fmt, ...)  \
    do {                                               \
        const iopc_write_buf_t *__b = (_wbuf);         \
        sb_addsb(__b->buf, __b->tab);                  \
        _fct(__b->buf, _fmt, ##__VA_ARGS__);           \
    } while (0)

#define IOPC_WRITE_START_LINE_CSTR(_wbuf, _str)  \
    IOPC_WRITE_START_LINE((_wbuf), sb_add_lstr, LSTR(_str))

void iopc_write_buf_init(iopc_write_buf_t *wbuf, sb_t *buf, sb_t *tab);

void iopc_write_buf_tab_inc(const iopc_write_buf_t *);

void iopc_write_buf_tab_dec(const iopc_write_buf_t *);

int
iopc_set_path(const char *outdir, const iopc_pkg_t *pkg,
              const char *ext, int max_len, char *path, bool only_pkg);

void iopc_write_file(const sb_t *buf, const char *path);

/*----- language backends -----*/

extern struct iopc_do_c_globs {
    int resolve_includes;
    const char *data_c_type;
    /** remove const on all objects that may contain a pointer to an
     * iop_struct_t */
    bool no_const;
    /** use iop compat header in memory instead of lib-common/iop.h */
    const char *iop_compat_header;
} iopc_do_c_g;

void iopc_do_c(iopc_pkg_t *pkg, const char *outdir, sb_t *depbuf);
void iopc_do_json(iopc_pkg_t *pkg, const char *outdir, sb_t *depbuf);

/*----- IOPC DSO -----*/

/** Specify the class id range used when building IOP DSO with iopc_dso_load.
 */
void iopc_dso_set_class_id_range(uint16_t class_id_min,
                                 uint16_t class_id_max);

/** Build an IOP DSO.
 *
 * \param[in] iopfile  the source IOP file
 * \param[in] env      a map of buffered IOP files (dependencies)
 * \param[in] outdir   the directory to store the IOP DSO file
 *                     (outdir/pkgname.so)
 *
 * \return             0 if ok, -1 if the build failed
 */
int iopc_dso_build(const char *iopfile, const qm_t(iopc_env) *env,
                   const char *outdir);

#endif
