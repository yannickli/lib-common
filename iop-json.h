/**************************************************************************/
/*                                                                        */
/*  Copyright (C) 2004-2012 INTERSEC SAS                                  */
/*                                                                        */
/*  Should you receive a copy of this source code, you must check you     */
/*  have a proper, written authorization of INTERSEC to hold it. If you   */
/*  don't have such an authorization, you must DELETE all source code     */
/*  files in your possession, and inform INTERSEC of the fact you obtain  */
/*  these files. Should you not comply to these terms, you can be         */
/*  prosecuted in the extent permitted by applicable law.                 */
/*                                                                        */
/**************************************************************************/

#if !defined(IS_LIB_COMMON_IOP_H) || defined(IS_LIB_COMMON_IOP_JSON_H)
#  error "you must include <lib-common/iop.h> instead"
#else
#define IS_LIB_COMMON_IOP_JSON_H

typedef enum iop_json_error {
    IOP_JERR_EOF                             =   0,
    IOP_JERR_UNKNOWN                         =  -1,

    /* unterminated things */
    IOP_JERR_UNCLOSED_COMMENT                =  -2,
    IOP_JERR_UNCLOSED_STRING                 =  -3,

    /* numerical conversion */
    IOP_JERR_TOO_BIG_INT                     =  -4,
    IOP_JERR_PARSE_NUM                       =  -5,
    IOP_JERR_BAD_INT_EXT                     =  -6,

    /* (un)expected things */
    IOP_JERR_EXP_SMTH                        =  -7,
    IOP_JERR_EXP_VAL                         =  -8,
    IOP_JERR_BAD_TOKEN                       =  -9,

    /* unreadable values */
    IOP_JERR_BAD_IDENT                       = -10,
    IOP_JERR_BAD_VALUE                       = -11,
    IOP_JERR_ENUM_VALUE                      = -12,

    /* structure checking */
    IOP_JERR_DUPLICATED_MEMBER               = -13,
    IOP_JERR_MISSING_MEMBER                  = -14,
    IOP_JERR_UNION_ARR                       = -15,
    IOP_JERR_UNION_RESERVED                  = -16,
    IOP_JERR_NOTHING_TO_READ                 = -17,

    IOP_JERR_CONSTRAINT                      = -18,

} iop_json_error;

typedef struct iop_json_lex_t {
    int peek;
    int line;
    int col;

    /* bitfield of iop_unpack_flags */
    int flags;

    /* Context storage */
    int             s_line;
    int             s_col;
    pstream_t       s_ps;
    iop_cfolder_t  *cfolder;

    iop_json_error err;
    char *         err_str;

    mem_pool_t *mp;
    pstream_t  *ps;
    sb_t b;

    union {
        int64_t i;
        double  d;
    } u;
    bool    is_signed;
} iop_json_lex_t;

/* json lexer */
iop_json_lex_t *iop_jlex_init(mem_pool_t *mp, iop_json_lex_t *ll);
static inline iop_json_lex_t *iop_jlex_new(mem_pool_t *mp) {
    return iop_jlex_init(mp, mp_new(mp, iop_json_lex_t, 1));
}
void iop_jlex_wipe(iop_json_lex_t *ll);
static inline void iop_jlex_delete(iop_json_lex_t **ll) {
    if (*ll) {
        mem_pool_t *mp = (*ll)->mp;
        iop_jlex_wipe(*ll);
        mp_delete(mp, ll);
    }
}

void iop_jlex_attach(iop_json_lex_t *ll, pstream_t *ps);
static inline void iop_jlex_detach(iop_json_lex_t *ll) {
    ll->ps = NULL;
}

/* errors printing */
void iop_jlex_write_error(iop_json_lex_t *ll, sb_t *sb);
int  iop_jlex_write_error_buf(iop_json_lex_t *ll, char *buf, int len);

/* IOP to/from json */

enum iop_jpack_flags {
    IOP_JPACK_STRICT  = (1U << 0), /*< XXX obsolete, kept for backward
                                     compatibility */
    IOP_JPACK_COMPACT = (1U << 1),
};

int iop_jpack(const iop_struct_t *, const void *value,
              int (*)(void *, const void *buf, int len), void *,
              unsigned flags);
int iop_junpack(iop_json_lex_t *, const iop_struct_t *, void *,
                bool single_value);

int t_iop_junpack_ps(pstream_t *ps, const iop_struct_t *desc, void *v,
                     int flags, sb_t *errb);

#ifndef NDEBUG
void iop_jtrace_(int lvl, const char *fname, int lno, const char *func,
                 const char *name, const iop_struct_t *, const void *);
#define iop_jtrace(lvl, st, v) \
    do {                                                     \
        if (e_is_traced(lvl)) {                              \
            const st##__t *__v = (v);                        \
            iop_jtrace_(lvl, __FILE__, __LINE__, __func__,   \
                        NULL, &st##__s, __v);                \
        }                                                    \
    } while (0)
#define iop_named_jtrace(lvl, name, st, v)                   \
    do {                                                     \
        if (e_name_is_traced(lvl, name)) {                   \
            const st##__t *__v = (v);                        \
            iop_jtrace_(lvl, __FILE__, __LINE__, __func__,   \
                        name, &st##__s, __v);                \
        }                                                    \
    } while (0)
#else
#define iop_jtrace_(lvl, ...)               e_trace_ignore(lvl, ##__VA_ARGS__)
#define iop_jtrace(lvl, st, v)              e_trace_ignore(lvl, v)
#define iop_named_jtrace(lvl, name, st, v)  e_trace_ignore(lvl, name, v)
#endif

static inline int iop_sb_write(void *_b, const void *buf, int len) {
    sb_add((sb_t *)_b, buf, len);
    return len;
}

static inline int
iop_sb_jpack(sb_t *sb, const iop_struct_t *desc, const void *value,
             unsigned flags)
{
    return iop_jpack(desc, value, &iop_sb_write, sb, flags);
}

#endif
