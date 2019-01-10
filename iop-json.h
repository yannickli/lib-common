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

#if !defined(IS_LIB_COMMON_IOP_H) || defined(IS_LIB_COMMON_IOP_JSON_H)
#  error "you must include <lib-common/iop.h> instead"
#else
#define IS_LIB_COMMON_IOP_JSON_H

/* {{{ Private API and definitions */

typedef enum iop_json_error {
    IOP_JERR_EOF                             =   0,
    IOP_JERR_UNKNOWN                         =  -1,

    /* unterminated things */
    IOP_JERR_UNCLOSED_COMMENT                =  -2,
    IOP_JERR_UNCLOSED_STRING                 =  -3,

    /* numerical conversion */
    IOP_JERR_TOO_BIG_INT                     =  -4,
    IOP_JERR_OUT_OF_RANGE                    =  -5,
    IOP_JERR_PARSE_NUM                       =  -6,
    IOP_JERR_BAD_INT_EXT                     =  -7,

    /* (un)expected things */
    IOP_JERR_EXP_SMTH                        =  -8,
    IOP_JERR_EXP_VAL                         =  -9,
    IOP_JERR_BAD_TOKEN                       = -10,
    IOP_JERR_INVALID_FILE                    = -11,

    /* unreadable values */
    IOP_JERR_BAD_IDENT                       = -12,
    IOP_JERR_BAD_VALUE                       = -13,
    IOP_JERR_ENUM_VALUE                      = -14,

    /* structure checking */
    IOP_JERR_DUPLICATED_MEMBER               = -15,
    IOP_JERR_MISSING_MEMBER                  = -16,
    IOP_JERR_UNION_ARR                       = -17,
    IOP_JERR_UNION_RESERVED                  = -18,
    IOP_JERR_NOTHING_TO_READ                 = -19,

    IOP_JERR_CONSTRAINT                      = -20,

} iop_json_error;

typedef struct iop_json_lex_ctx_t {
    int line;
    int col;

    sb_t b;

    union {
        int64_t i;
        double  d;
    } u;
    bool is_signed;
} iop_json_lex_ctx_t;

typedef struct iop_json_lex_t {
    int peek;

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

    iop_json_lex_ctx_t  cur_ctx;
    iop_json_lex_ctx_t  peeked_ctx;
    iop_json_lex_ctx_t *ctx;
} iop_json_lex_t;

/* }}} */
/* {{{ Parsing JSon */

/** Initialize a JSon parser.
 *
 * \param[in] mp  Memory pool to use for memory allocations.
 * \param[in] ll  JSon parser to initialize.
 */
iop_json_lex_t *iop_jlex_init(mem_pool_t *mp, iop_json_lex_t *ll);

/** New JSon parser.
 *
 * \param[in] mp  Memory pool to use for memory allocations (including the
 *                JSon parser).
 */
static inline iop_json_lex_t *iop_jlex_new(mem_pool_t *mp) {
    return iop_jlex_init(mp, mp_new(mp, iop_json_lex_t, 1));
}

/** Wipe a JSon parser */
void iop_jlex_wipe(iop_json_lex_t *ll);

/** Delete a JSon parser */
static inline void iop_jlex_delete(iop_json_lex_t **ll) {
    if (*ll) {
        mem_pool_t *mp = (*ll)->mp;
        iop_jlex_wipe(*ll);
        mp_delete(mp, ll);
    }
}

/** Attach the JSon parser on a pstream_t.
 *
 * This function setups the JSon parser on the given pstream_t. You must use
 * this function before to use iop_junpack() & co.
 *
 * \param[in] ll  JSon parser.
 * \param[in] ps  pstream_t containing the JSon to parse.
 */
void iop_jlex_attach(iop_json_lex_t *ll, pstream_t *ps);

/** Detach the JSon parser.
 *
 * When calling this function the JSon parser forgets its current data stream.
 * This function is useless in most usages.
 */
static inline void iop_jlex_detach(iop_json_lex_t *ll) {
    ll->ps = NULL;
}

/** Change the unpacker flags.
 *
 * The JSon unpacker supports the following flags: IOP_UNPACK_IGNORE_UNKNOWN.
 *
 * \param[in] ll     The JSon parser.
 * \param[in] flags  Bitfield of flags to use (see iop_unpack_flags in iop.h)
 */
static inline void iop_jlex_set_flags(iop_json_lex_t *ll, int flags)
{
    ll->flags = flags;
}

/** Convert IOP-JSon to an IOP C structure.
 *
 * This function unpacks an IOP structure encoded in JSon format. You have to
 * initialize an iop_json_lex_t structure and iop_jlex_attach() it on the data
 * you want to unpack before calling this function.
 *
 * If you want to set some special flags, you have to set it using
 * iop_jlex_set_flags() before calling this this function.
 *
 * Prefer the generated version instead of this low-level API (see IOP_GENERIC
 * in iop-macros.h).
 *
 * \param[in]  ll            The JSon parser.
 * \param[in]  st            The IOP structure description.
 * \param[out] out           Pointer on the IOP structure to write.
 * \param[in]  single_value  Whether there is one or more structures to unpack
 *                           in the attached pstream_t.
 *  \return
 *    If `single_value` is true, the function returns 0 upon success and
 *    something < 0 in case of errors (see iop_json_error). Note that it
 *    returns IOP_JERR_NOTHING_TO_READ if EOF is reached without finding
 *    a structure to unpack.
 *
 *    If `single_value` is false, the function returns the number of bytes
 *    read successfully, or 0 if it reaches EOF. An empty buffer will not
 *    raise an error.
 */
__must_check__
int iop_junpack(iop_json_lex_t *ll, const iop_struct_t *st, void *out,
                bool single_value);

/** Convert IOP-JSon to an IOP C structure using the t_pool().
 *
 * This function allow to unpack an IOP structure encoded in JSon format in
 * one call. This is equivalent to:
 *
 * <code>
 * iop_jlex_init(t_pool(), &jll);
 * iop_jlex_attach(&jll, ps);
 * jll.flags = <...>;
 * iop_junpack(&jll, ..., true);
 * </code>
 *
 * Note that the parameter `single_value` is set to true.
 *
 * Prefer the generated version instead of this low-level API (see IOP_GENERIC
 * in iop-macros.h).
 *
 * \param[in]  ps    The pstream_t to parse.
 * \param[in]  st    The IOP structure description.
 * \param[out] out   Pointer on the IOP structure to write.
 * \param[in]  flags Unpacker flags to use (see iop_jlex_set_flags).
 * \param[out] errb  NULL or the buffer to use to write textual error.
 *
 * \return
 *   The iop_junpack() result.
 */
__must_check__
int t_iop_junpack_ps(pstream_t *ps, const iop_struct_t *st, void *out,
                     int flags, sb_t *errb);

/** Convert an IOP-JSon structure contained in a file to an IOP C structure.
 *
 * This function read a file containing an IOP-JSon structure and set the
 * fields in an IOP C structure.
 *
 * Prefer the generated version instead of this low-level API (see IOP_GENERIC
 * in iop-macros.h).
 *
 * \param[in]  filename The file name to read and parse.
 * \param[in]  st       The IOP structure description.
 * \param[out] out      Pointer on the IOP structure to write.
 * \param[in]  flags    Unpacker flags to use (see iop_jlex_set_flags).
 * \param[out] errb     NULL or the buffer to use to write textual error.
 *
 * \return
 *   The t_iop_junpack_ps() result,
 *   or IOP_JERR_INVALID_FILE in case of invalid file.
 */
__must_check__
int t_iop_junpack_file(const char *filename, const iop_struct_t *st,
                       void *out, int flags, sb_t *errb);

/** Print a textual error after iop_junpack() failure.
 *
 * When iop_junpack() fails, you can print the error textual description in
 * a sb_t with this function.
 */
void iop_jlex_write_error(iop_json_lex_t *ll, sb_t *sb);

/** Print a textual error after iop_junpack() failure.
 *
 * When iop_junpack() fails, you can print the error textual description in
 * a buffer with this function.
 */
int  iop_jlex_write_error_buf(iop_json_lex_t *ll, char *buf, int len);


/* }}} */
/* {{{ Generating JSon */

/** JSon packer custom flags */
enum iop_jpack_flags {
    /** obsolete, kept for backward compatibility. */
    IOP_JPACK_STRICT        = (1U << 0),
    /** generate compact JSon (no indentation, no spaces, …) */
    IOP_JPACK_COMPACT       = (1U << 1),
    /** skip PRIVATE fields */
    IOP_JPACK_SKIP_PRIVATE  = (1U << 2),
};

/** Callback to use for writing JSon into a sb_t. */
static inline int iop_sb_write(void *_b, const void *buf, int len) {
    sb_add((sb_t *)_b, buf, len);
    return len;
}

/** Convert an IOP C structure to IOP-JSon.
 *
 * This function packs an IOP structure into (strict) JSon format.
 *
 * Prefer the generated version instead of this low-level API (see IOP_GENERIC
 * in iop-macros.h).
 *
 * \param[in] st       IOP structure description.
 * \param[in] value    Pointer on the IOP structure to pack.
 * \param[in] writecb  Callback to call when writing (like iop_sb_write).
 * \param[in] priv     Private data to give to the callback.
 * \param[in] flags    Packer flags bitfield (see iop_jpack_flags).
 */
int iop_jpack(const iop_struct_t *st, const void *value,
              int (*writecb)(void *, const void *buf, int len), void *priv,
              unsigned flags);

/** Pack an IOP C structure to IOP-JSon in a sb_t.
 *
 * See iop_jpack().
 *
 * Prefer the generated version instead of this low-level API (see IOP_GENERIC
 * in iop-macros.h).
 */
static inline int
iop_sb_jpack(sb_t *sb, const iop_struct_t *st, const void *value,
             unsigned flags)
{
    return iop_jpack(st, value, &iop_sb_write, sb, flags);
}

/** Dump IOP structures in JSon format using e_trace */
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

/* }}} */

#endif
