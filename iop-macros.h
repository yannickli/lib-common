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

#if !defined(IS_LIB_COMMON_IOP_H) || defined(IS_LIB_COMMON_IOP_MACROS_H)
#  error "you must include <lib-common/iop.h> instead"
#else
#define IS_LIB_COMMON_IOP_MACROS_H

/* {{{ iop_data_t initializers (bytes type) (DEPRECATED: use lstr_t) */

/** Initialize a byte field */
#define IOP_DATA(ddata, dlen)  LSTR_INIT_V((const char *)(ddata), dlen)

/** Initialize an optional byte field to “absent” */
#define IOP_DATA_NULL          LSTR_NULL_V

/** Initialize a byte field to the empty value */
#define IOP_DATA_EMPTY         LSTR_EMPTY_V

/** Initialize a byte field from an sb_t */
#define IOP_SBDATA(sb)         LSTR_SB_V(sb)

/* }}} */
/* {{{ IOP array initializers (repeated fields) */

/** Initialize a repeated field */
#define IOP_ARRAY(_data, _len)  { { .tab = (_data) }, .len = (_len) }

/** Initialize an empty repeated field */
#define IOP_ARRAY_EMPTY         IOP_ARRAY(NULL, 0)

/** Initialize a repeated field from a qvector */
#define IOP_ARRAY_TAB(vec)      IOP_ARRAY((vec)->tab, (vec)->len)

/* }}} */
/* {{{ IOP union helpers */

/** Get the tag value of a union field. */
#define IOP_UNION_TAG(pfx, field) pfx##__##field##__ft

/** Allow to make a switch on a union depending on the field chosen.
 *
 * Example:
 *
 * IOP_UNION_SWITCH(my_union) {
 *  IOP_UNION_CASE(pkg__my_union, my_union, a, a_val) {
 *      printf("a value is: %d", a);
 *  }
 *  IOP_UNION_CASE_P(pkg__my_union, my_union, b, b_val_ptr) {
 *      printf("a value is: %u", *b_val_ptr);
 *  }
 * }
 *
 * XXX never use _continue_ inside an IOP_UNION_SWITCH.
 *
 * \param[in] u The union to switch on.
 */
#define IOP_UNION_SWITCH(u) \
    switch ((u)->iop_tag)

/** Case giving the value by pointer.
 *
 * \param[in] pfx   The union prefix (pkg__name).
 * \param[in] u     The union given to IOP_UNION_SWITCH().
 * \param[in] field The union field to select.
 * \param[out] val  Pointer on the field value.
 */
#define IOP_UNION_CASE_P(pfx, u, field, val) \
        break;                                                        \
      case IOP_UNION_TAG(pfx, field):                                 \
        { const pfx##__t __attribute__((unused)) *val = u; }          \
        for (typeof((u)->field) *val = &(u)->field; val; val = NULL)

/** Case giving the field value by value.
 *
 * \param[in] pfx   The union prefix (pkg__name).
 * \param[in] u     The union given to IOP_UNION_SWITCH().
 * \param[in] field The union field to select.
 * \param[out] val  A copy of the field value.
 */
#define IOP_UNION_CASE(pfx, u, field, val) \
        break;                                                          \
      case IOP_UNION_TAG(pfx, field):                                   \
        { const pfx##__t __attribute__((unused)) *val = u; }            \
        for (typeof((u)->field) *val##_p = &(u)->field,                 \
             __attribute__((unused)) val = *val##_p;                    \
             val##_p; val##_p = NULL)

/** Default case. */
#define IOP_UNION_DEFAULT() \
        break;              \
      default: (void)0;

/** Extract a value from a union.
 *
 * \param[in] pfx   The union prefix (pkg__name).
 * \param[in] u     The union object.
 * \param[in] field The union field to select.
 *
 * \return
 *   A pointer on the wanted field or NULL if this field isn't selected.
 */
#define IOP_UNION_GET(pfx, u, field) \
    ({ const pfx##__t *_tmp0 = u;                                        \
       typeof(u) _tmp = (typeof(u))_tmp0;                                \
       (_tmp->iop_tag == IOP_UNION_TAG(pfx, field)) ? &_tmp->field : NULL; })

/** Select an union field.
 *
 * \param[in]    pfx   The union prefix (pkg__name).
 * \param[inout] u     The union object.
 * \param[in]    field The union field to select.
 *
 * \return
 *   A pointer on the selected field.
 */
#define IOP_UNION_SET(pfx, u, field) \
    ({ pfx##__t *_tmp = u;                                               \
       _tmp->iop_tag = IOP_UNION_TAG(pfx, field);                        \
       &_tmp->field;                                                     \
    })

/** Extract a value from a union by copying it.
 *
 * \param[out] dst   The variable to put the field value into.
 * \param[in]  pfx   The union prefix (pkg__name).
 * \param[in]  u     The union object.
 * \param[in]  field The union field to select.
 *
 * \return
 *   True if the wanted field is selected, false otherwise.
 */
#define IOP_UNION_COPY(dst, pfx, u, field) \
    ({ pfx##__t *_tmp = u;                                           \
       bool _copyok  = (_tmp->iop_tag == IOP_UNION_TAG(pfx, field)); \
       if (_copyok)                                                  \
           dst = _tmp->field;                                        \
       _copyok;                                                      \
    })

/** Make an immediate IOP union.
 *
 * \param[in]  pfx   The union prefix (pkg__name).
 * \param[in]  field The field to select.
 * \param[in]  val   The field value.
 */
#define IOP_UNION_CST(pfx, field, val) \
    { IOP_UNION_TAG(pfx, field), { .field = val } }

/** Make an immediate IOP union.
 *
 * \param[in]  pfx   The union prefix (pkg__name).
 * \param[in]  field The field to select.
 * \param[in]  val   The field values.
 */
#define IOP_UNION_VA_CST(pfx, field, ...) \
    { IOP_UNION_TAG(pfx, field), { .field = { __VA_ARGS__ } } }

/** Make an IOP union.
 *
 * \param[in]  pfx   The union prefix (pkg__name).
 * \param[in]  field The field to select.
 * \param[in]  val   The field value.
 */
#define IOP_UNION(pfx, field, val) \
    (pfx##__t){ IOP_UNION_TAG(pfx, field), { .field = val } }

/** Make an IOP union.
 *
 * \param[in]  pfx   The union prefix (pkg__name).
 * \param[in]  field The field to select.
 * \param[in]  val   The field values.
 */
#define IOP_UNION_VA(pfx, field, ...) \
    (pfx##__t){ IOP_UNION_TAG(pfx, field), { .field = { __VA_ARGS__ } } }

/** Get the selected field name of an IOP union.
 *
 * \param[in] _data      The IOP union.
 * \param[in] _type_desc The IOP union description.
 */
#define IOP_UNION_TYPE_TO_STR(_data, _type_desc)                        \
    ({                                                                  \
        int _res = iop_ranges_search((_type_desc).ranges,               \
                                     (_type_desc).ranges_len,           \
                                     (_data)->iop_tag);                 \
        _res >= 0 ? (_type_desc).fields[_res].name.s : NULL;            \
    })


/* }}} */
/* {{{ Optional scalar fields usage (Deprecated) */

/* XXX Deprecated, please use OPT_XXX macros  */
#define IOP_OPT(...)         OPT(__VA_ARGS__)
#define IOP_OPT_NONE         OPT_NONE
#define IOP_OPT_IF(...)      OPT_IF(__VA_ARGS__)
#define IOP_OPT_ISSET(...)   OPT_ISSET(__VA_ARGS__)
#define IOP_OPT_VAL(...)     OPT_VAL(__VA_ARGS__)
#define IOP_OPT_DEFVAL(...)  OPT_DEFVAL(__VA_ARGS__)
#define IOP_OPT_GET(...)     OPT_GET(__VA_ARGS__)
#define IOP_OPT_SET(...)     OPT_SET(__VA_ARGS__)
#define IOP_OPT_CLR(...)     OPT_CLR(__VA_ARGS__)
#define IOP_OPT_SET_IF(...)  OPT_SET_IF(__VA_ARGS__)
#define IOP_OPT_CLR_IF(...)  OPT_CLR_IF(__VA_ARGS__)

/* }}} */
/* {{{ Data packing helpers */

/** Pack an IOP structure into IOP binary format using the t_pool().
 *
 * This version of `iop_bpack` allows to pack an IOP structure in one
 * operation and uses the t_pool() to allocate the resulting buffer.
 *
 * \param[in] _pkg   The IOP structure package name.
 * \param[in] _type  The IOP structure type name.
 * \param[in] _flags Packer modifiers (see iop_bpack_flags).
 * \return
 *   The buffer containing the packed structure.
 */
#define t_iop_bpack_flags(_pkg, _type, _val, _flags)                     \
    ({                                                                   \
        const _pkg##__##_type##__t *_tval = (_val);                      \
        \
        t_iop_bpack_struct_flags(&_pkg##__##_type##__s, _tval, _flags);  \
    })

/** Pack an IOP structure into IOP binary format using the t_pool().
 *
 * This version of `iop_bpack` allows to pack an IOP structure in one
 * operation and uses the t_pool() to allocate the resulting buffer.
 *
 * \param[in] _pkg   The IOP structure package name.
 * \param[in] _type  The IOP structure type name.
 * \return
 *   The buffer containing the packed structure.
 */
#define t_iop_bpack(_pkg, _type, _val)                     \
    ({                                                     \
        const _pkg##__##_type##__t *_tval = (_val);        \
        \
        t_iop_bpack_struct(&_pkg##__##_type##__s, _tval);  \
    })

/** Unpack an IOP structure from IOP binary format using the t_pool().
 *
 * This version of `iop_bunpack` allows to unpack an IOP structure in one
 * operation and uses the t_pool() to allocate the resulting structure.
 *
 * This function calls `iop_bunpack` with the copy parameter set to false.
 *
 * \param[in]  _str  The lstr_t “compatible” variable containing the packed
 *                   object.
 * \param[in]  _pkg  The IOP structure package name.
 * \param[in]  _type The IOP structure type name.
 * \param[out] _valp Pointer on the structure to use for unpacking.
 */
#define t_iop_bunpack(_str, _pkg, _type, _valp)                          \
    ({                                                                   \
        _pkg##__##_type##__t *_tval = (_valp);                           \
        typeof(_str) _str2 = (_str);                                     \
        pstream_t _ps = ps_init(_str2->s, _str2->len);                   \
        \
        t_iop_bunpack_ps(&_pkg##__##_type##__s, _tval, _ps, false);      \
    })

/** Unpack an IOP structure from IOP binary format using the t_pool().
 *
 * This version of `iop_bunpack` allows to unpack an IOP structure in one
 * operation and uses the t_pool() to allocate the resulting structure.
 *
 * This function calls `iop_bunpack` with the copy parameter set to true.
 *
 * \param[in]  _str  The lstr_t “compatible” variable containing the packed
 *                   object.
 * \param[in]  _pkg  The IOP structure package name.
 * \param[in]  _type The IOP structure type name.
 * \param[out] _valp Pointer on the structure to use for unpacking.
 */
#define t_iop_bunpack_dup(_str, _pkg, _type, _valp)                     \
    ({                                                                  \
        _pkg##__##_type##__t *_tval = (_valp);                          \
        typeof(_str) _str2 = (_str);                                    \
        pstream_t _ps = ps_init(_str2->s, _str2->len);                  \
        \
        t_iop_bunpack_ps(&_pkg##__##_type##__s, _tval, _ps, true);      \
    })

/* }}} */
/* {{{ RPC helpers */

/** Get an RPC structure definition.
 *
 * \param[in] _mod  RPC module name.
 * \param[in] _if   RPC interface name.
 * \param[in] _rpc  RPC name.
 */
#define IOP_RPC(_mod, _if, _rpc)          _mod##__##_if(_rpc##__rpc)

/** Get the type of RPC arguments/response/exception.
 *
 * \param[in] _mod  RPC module name.
 * \param[in] _if   RPC interface name.
 * \param[in] _rpc  RPC name.
 * \param[in] what  `arg`, `res` or `exn`.
 */
#define IOP_RPC_T(_mod, _if, _rpc, what)  _mod##__##_if(_rpc##_##what##__t)

/** Get the command tag of an RPC.
 *
 * \param[in] _mod  RPC module name.
 * \param[in] _if   RPC interface name.
 * \param[in] _rpc  RPC name.
 */
#define IOP_RPC_CMD(_mod, _if, _rpc) \
    (_mod##__##_if##__TAG << 16) | _mod##__##_if(_rpc##__rpc__tag)

/* }}} */
/* {{{ Helpers generated for enums */

#define IOP_ENUM(pfx) \
    static inline const char *pfx##__to_str(pfx##__t v) {                    \
        return iop_enum_to_str(&pfx##__e, v).s;                              \
    }                                                                        \
    static inline lstr_t pfx##__to_lstr(pfx##__t v) {                        \
        return iop_enum_to_str(&pfx##__e, v);                                \
    }                                                                        \
    static inline int pfx##__from_str(const char *s, int len, int err) {     \
        return iop_enum_from_str(&pfx##__e, s, len, err);                    \
    }                                                                        \
    static inline int pfx##__from_str2(const char *s, int len, bool *found)  \
    {                                                                        \
        return iop_enum_from_str2(&pfx##__e, s, len, found);                 \
    }                                                                        \
    static inline int pfx##__from_lstr(const lstr_t s, bool *found)          \
    {                                                                        \
        return iop_enum_from_lstr(&pfx##__e, s, found);                      \
    }                                                                        \
    static inline bool pfx##__exists(pfx##__t v)                             \
    {                                                                        \
        return iop_ranges_search(pfx##__e.ranges, pfx##__e.ranges_len,       \
                                 v) >= 0;                                    \
    }

/* }}} */
/* {{{ Helpers generated for structures and unions */

#define IOP_GENERIC_BASICS(pfx)  \
    static inline bool pfx##__equals(const pfx##__t *v1, const pfx##__t *v2) \
    {                                                                        \
        return iop_equals(&pfx##__s, (void *)v1, (void *)v2);                \
    }                                                                        \
    static inline pfx##__t *pfx##__init(pfx##__t *v) {                       \
        iop_init(&pfx##__s, (void *)v);                                      \
                                                                             \
        return v;                                                            \
    }                                                                        \
    static inline pfx##__t *pfx##__dup(mem_pool_t *mp, const pfx##__t *v) {  \
        return (pfx##__t *)iop_dup(mp, &pfx##__s, (const void *)v);          \
    }                                                                        \
    static inline void                                                       \
    pfx##__copy(mem_pool_t *mp, pfx##__t **out, const pfx##__t *v) {         \
        iop_copy(mp, &pfx##__s, (void **)out, (const void *)v);              \
    }                                                                        \
    static inline int pfx##__check(pfx##__t *v) {                            \
        return iop_check_constraints(&pfx##__s, (void *)v);                  \
    }

#define IOP_GENERIC_BASICS_STRUCT_UNION(pfx)  \
    static inline int pfx##__sort(pfx##__t *vec, int len, lstr_t path,       \
                                  int flags) {                               \
        return iop_sort(&pfx##__s, (void *)vec, len, path, flags);           \
    }

/* {{{ Binary helpers */

#define IOP_GENERIC_BINARY_UNION(pfx)  \
    __must_check__ static inline int                                         \
    t_##pfx##__bunpack_multi(pfx##__t *value, pstream_t *ps, bool copy)      \
    {                                                                        \
        return t_iop_bunpack_multi(&pfx##__s, value, ps, copy);              \
    }

#define IOP_GENERIC_BINARY_PACK(pfx)  \
    __must_check__ static inline int                                         \
    pfx##__bpack_size(const pfx##__t *v, qv_t(i32) *szs)                     \
    {                                                                        \
        return iop_bpack_size(&pfx##__s, v, szs);                            \
    }                                                                        \
    static inline void                                                       \
    pfx##__bpack(void *dst, const pfx##__t *v, const int *szs)               \
    {                                                                        \
        return iop_bpack(dst, &pfx##__s, v, szs);                            \
    }                                                                        \
    static inline lstr_t t_##pfx##__bpack(const pfx##__t *v) {               \
        return t_iop_bpack_struct(&pfx##__s, v);                             \
    }

#define IOP_GENERIC_BINARY_UNPACK(pfx)  \
    __must_check__ static inline int                                         \
    t_##pfx##__bunpack_ptr_ps(pfx##__t **value, pstream_t ps, bool copy)     \
    {                                                                        \
        return iop_bunpack_ptr(t_pool(), &pfx##__s, (void **)value,          \
                                 ps, copy);                                  \
    }                                                                        \
    __must_check__ static inline int                                         \
    t_##pfx##__bunpack_ptr(lstr_t *s, pfx##__t **value)                      \
    {                                                                        \
        pstream_t ps = ps_init(s->s, s->len);                                \
        return iop_bunpack_ptr(t_pool(), &pfx##__s, (void **)value,          \
                               ps, false);                                   \
    }

#define IOP_GENERIC_BINARY_UNPACK_STRUCT_UNION(pfx)  \
    __must_check__ static inline int                                         \
    t_##pfx##__bunpack_ps(pfx##__t *value, pstream_t ps, bool copy)          \
    {                                                                        \
        return t_iop_bunpack_ps(&pfx##__s, value, ps, copy);                 \
    }                                                                        \
    __must_check__ static inline int                                         \
    t_##pfx##__bunpack(lstr_t *s, pfx##__t *value)                           \
    {                                                                        \
        pstream_t ps = ps_init(s->s, s->len);                                \
        return t_iop_bunpack_ps(&pfx##__s, value, ps, false);                \
    }

/* }}} */
/* {{{ Json helpers */

#define IOP_GENERIC_JSON_PACK(pfx)  \
    static inline int                                                        \
    pfx##__jpack(const pfx##__t *v,                                          \
                 int (*wcb)(void *, const void *, int), void *priv,          \
                 unsigned flags)                                             \
    {                                                                        \
        return iop_jpack(&pfx##__s, (const void *)v, wcb, priv, flags);      \
    }                                                                        \
    static inline int                                                        \
    pfx##__sb_jpack(sb_t *sb, const pfx##__t *v,  unsigned flags) {          \
        return iop_sb_jpack(sb, &pfx##__s, v, flags);                        \
    }

#define IOP_GENERIC_JSON_UNPACK(pfx)  \
    __must_check__                                                           \
    static inline int pfx##__junpack_ptr(iop_json_lex_t *ll, pfx##__t **v,   \
                                         bool single)                        \
    {                                                                        \
        return iop_junpack_ptr(ll, &pfx##__s, (void **)v, single);           \
    }                                                                        \
    __must_check__                                                           \
    static inline int t_##pfx##__junpack_ptr_ps(pstream_t *ps, pfx##__t **v, \
                                                int flags, sb_t *errb)       \
    {                                                                        \
        return t_iop_junpack_ptr_ps(ps, &pfx##__s, (void **)v, flags, errb); \
    }                                                                        \
    __must_check__                                                           \
    static inline int t_##pfx##__junpack_ptr_file(const char *filename,      \
                                                  pfx##__t **v, int flags,   \
                                                  sb_t *errb)                \
    {                                                                        \
        return t_iop_junpack_ptr_file(filename, &pfx##__s, (void **)v,       \
                                      flags, errb);                          \
    }

#define IOP_GENERIC_JSON_UNPACK_STRUCT_UNION(pfx)  \
    __must_check__                                                           \
    static inline int pfx##__junpack(iop_json_lex_t *ll, pfx##__t *v,        \
                                     bool single)                            \
    {                                                                        \
        return iop_junpack(ll, &pfx##__s, (void *)v, single);                \
    }                                                                        \
    __must_check__                                                           \
    static inline int t_##pfx##__junpack_ps(pstream_t *ps, pfx##__t *v,      \
                                            int flags, sb_t *errb)           \
    {                                                                        \
        return t_iop_junpack_ps(ps, &pfx##__s, (void *)v, flags, errb);      \
    }                                                                        \
    __must_check__                                                           \
    static inline int t_##pfx##__junpack_file(const char *filename,          \
                                              pfx##__t *v, int flags,        \
                                              sb_t *errb)                    \
    {                                                                        \
        return t_iop_junpack_file(filename, &pfx##__s, v, flags, errb);      \
    }

/* }}} */

#define IOP_GENERIC(pfx) \
    IOP_GENERIC_BASICS(pfx)                                                  \
    IOP_GENERIC_BASICS_STRUCT_UNION(pfx)                                     \
    /* ---- Binary ---- */                                                   \
    IOP_GENERIC_BINARY_UNION(pfx)                                            \
    IOP_GENERIC_BINARY_PACK(pfx)                                             \
    IOP_GENERIC_BINARY_UNPACK(pfx)                                           \
    IOP_GENERIC_BINARY_UNPACK_STRUCT_UNION(pfx)                              \
    /* ---- JSon ---- */                                                     \
    IOP_GENERIC_JSON_PACK(pfx)                                               \
    IOP_GENERIC_JSON_UNPACK(pfx)                                             \
    IOP_GENERIC_JSON_UNPACK_STRUCT_UNION(pfx)

/* }}} */
/* {{{ Helpers generated for classes */

#define IOP_GENERIC_BASICS_CLASS(pfx)  \
    static inline int pfx##__sort(pfx##__t **vec, int len, lstr_t path,      \
                                  int flags) {                               \
        return iop_sort(&pfx##__s, (void *)vec, len, path, flags);           \
    }

#define IOP_CLASS(pfx) \
    IOP_GENERIC_BASICS(pfx)                                                  \
    IOP_GENERIC_BASICS_CLASS(pfx)                                            \
    /* ---- Binary ---- */                                                   \
    IOP_GENERIC_BINARY_PACK(pfx)                                             \
    IOP_GENERIC_BINARY_UNPACK(pfx)                                           \
    /* ---- JSon ---- */                                                     \
    IOP_GENERIC_JSON_PACK(pfx)                                               \
    IOP_GENERIC_JSON_UNPACK(pfx)

/* }}} */
/* {{{ Helpers for classes manipulation */

#ifndef NDEBUG
#  define iop_obj_cast_debug(pfx, o)  \
    ({                                                                       \
        if (!iop_obj_is_a((void *)(o), pfx)) {                               \
            e_panic("cannot cast %p to type " TOSTR(pfx), (o));              \
        }                                                                    \
        (o);                                                                 \
    })
#else
#  define iop_obj_cast_debug(pfx, o)  (o)
#endif

/** Cast an IOP class object to the wanted type.
 *
 * In debug mode, this macro checks if the wanted type is compatible with the
 * actual type of the object (ie. if iop_obj_is_a returns true).
 * In release mode, no check will be made.
 *
 * \param[in]  pfx  Prefix of the destination type.
 * \param[in]  o    Pointer on the class object to cast.
 *
 * \return  a pointer equal to \p o, of the \p pfx type.
 */
#define iop_obj_vcast(pfx, o)  ((pfx##__t *)iop_obj_cast_debug(pfx, o))

/** Cast an IOP class object to the wanted type.
 *
 * Same as iop_obj_vcast, but returns a const pointer.
 */
#define iop_obj_ccast(pfx, o)  ((const pfx##__t *)iop_obj_cast_debug(pfx, o))

/* }}} */
/* {{{ Private helpers */

#define IOP_FIELD(type_t, v, i)  (((type_t *)v)[i])

/* }}} */

#endif
