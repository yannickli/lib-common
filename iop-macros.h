/**************************************************************************/
/*                                                                        */
/*  Copyright (C) 2004-2011 INTERSEC SAS                                  */
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

/*----- iop_data_t initializers (bytes) -----*/
#define IOP_DATA(ddata, dlen)  (iop_data_t){ .data = (ddata), .len = (dlen) }
#define IOP_DATA_NULL          (iop_data_t){ .data = NULL, .len = 0 }
#define IOP_DATA_EMPTY         (iop_data_t){ .data = (void *) "", .len = 0 }
#define IOP_SBDATA(sb)         (iop_data_t){ .data = (sb)->data,             \
                                             .len  = (sb)->len }

/*----- IOP array initializers (repetead) -----*/
#define IOP_ARRAY(_data, _len)  { { .tab = (_data) }, .len = (_len) }
#define IOP_ARRAY_EMPTY         IOP_ARRAY(NULL, 0)
#define IOP_ARRAY_TAB(vec)      IOP_ARRAY((vec)->tab, (vec)->len)


/*----- IOP union helpers ----*/

/* get the tag value of a union field */
#define IOP_UNION_TAG(pfx, field) pfx##__##field##__ft

/* To use the union type */
#define IOP_UNION_SWITCH(u) \
    switch ((u)->iop_tag)

/* Case giving the value by a pointer */
#define IOP_UNION_CASE_P(pfx, u, field, val) \
        break;                                                        \
      case IOP_UNION_TAG(pfx, field):                                 \
        { const pfx##__t __attribute__((unused)) *val = u; }          \
        for (typeof((u)->field) *val = &(u)->field; val; val = NULL)

/* Case giving the value by copy */
#define IOP_UNION_CASE(pfx, u, field, val) \
        break;                                                          \
      case IOP_UNION_TAG(pfx, field):                                   \
        { const pfx##__t __attribute__((unused)) *val = u; }            \
        for (typeof((u)->field) *val##_p = &(u)->field,                 \
             __attribute__((unused)) val = *val##_p;                    \
             val##_p; val##_p = NULL)

#define IOP_UNION_DEFAULT() \
        break;              \
      default: (void)0;

/* Extract a value from a union */
#define IOP_UNION_GET(pfx, u, field) \
    ({ const pfx##__t *_tmp0 = u;                                        \
       typeof(u) _tmp = (typeof(u))_tmp0;                                \
       (_tmp->iop_tag == IOP_UNION_TAG(pfx, field)) ? &_tmp->field : NULL; })

#define IOP_UNION_COPY(dst, pfx, u, field) \
    ({ pfx##__t *_tmp = u;                                           \
       bool _copyok  = (_tmp->iop_tag == IOP_UNION_TAG(pfx, field)); \
       if (_copyok)                                                  \
           dst = _tmp->field;                                        \
       _copyok;                                                      \
    })

/* Make a union */
#define IOP_UNION_CST(pfx, field, val) \
    { IOP_UNION_TAG(pfx, field), { .field = val } }

#define IOP_UNION_VA_CST(pfx, field, ...) \
    { IOP_UNION_TAG(pfx, field), { .field = { __VA_ARGS__ } } }

#define IOP_UNION(pfx, field, val) \
    (pfx##__t){ IOP_UNION_TAG(pfx, field), { .field = val } }

#define IOP_UNION_VA(pfx, field, ...) \
    (pfx##__t){ IOP_UNION_TAG(pfx, field), { .field = { __VA_ARGS__ } } }


/*----- Optional scalar values usage -----*/
#define IOP_OPT(val)           { .v = (val), .has_field = true }
#define IOP_OPT_NONE           { .has_field = false }
#define IOP_OPT_IF(cond, val)  { .has_field = (cond), .v = (cond) ? (val) : 0 }

#define IOP_OPT_ISSET(_v)  ((_v).has_field == true)
#define IOP_OPT_VAL(_v)    ((_v).v)

#define IOP_OPT_SET(dst, val)  \
    ({ typeof(dst) *_dst = &(dst); _dst->has_field = true; _dst->v = (val); })
#define IOP_OPT_CLR(dst)   (void)((dst).has_field = false)
#define IOP_OPT_SET_IF(dst, cond, val) \
    ({ if (cond) {                                         \
           IOP_OPT_SET(dst, val);                          \
       } else {                                            \
           IOP_OPT_CLR(dst);                               \
       }                                                   \
    })

/*----- Data packing helpers -----*/
#define t_iop_bpack(_mod, _type, _val)                     \
    ({                                                     \
        const _mod##__##_type##__t *_tval = _val;          \
        t_iop_bpack_struct(&_mod##__##_type##__s, _tval);  \
    })

#define t_iop_bunpack(_str, _mod, _type, _val)                          \
    ({                                                                  \
        _mod##__##_type##__t *_tval = _val;                             \
        typeof(_str) _str2 = _str;                                      \
        pstream_t _ps = ps_init(_str2->s, _str2->len);                  \
        iop_bunpack(t_pool(), &_mod##__##_type##__s, _tval, _ps,        \
                    false);                                             \
    })

#define t_iop_bunpack_dup(_str, _mod, _type, _val)                      \
    ({                                                                  \
        _mod##__##_type##__t *_tval = _val;                             \
        typeof(_str) _str2 = _str;                                      \
        pstream_t _ps = ps_init(_str2->s, _str2->len);                  \
        iop_bunpack(t_pool(), &_mod##__##_type##__s, _tval, _ps,        \
                    true);                                              \
    })


/*----- RPC helpers -----*/
#define IOP_RPC(_mod, _if, _rpc)          _mod##__##_if(_rpc##__rpc)
#define IOP_RPC_T(_mod, _if, _rpc, what)  _mod##__##_if(_rpc##_##what##__t)
#define IOP_RPC_CMD(_mod, _if, _rpc) \
    (_mod##__##_if##__TAG << 16) | _mod##__##_if(_rpc##__rpc__tag)


/*----- generate several typed helpers -----*/
#define IOP_ENUM(pfx) \
    static inline const char *pfx##__to_str(pfx##__t v) {                    \
        return iop_enum_to_str(&pfx##__e, v).s;                              \
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
    }

#define IOP_GENERIC(pfx) \
    static inline bool pfx##__equals(const pfx##__t *v1, const pfx##__t *v2) \
    {                                                                        \
        return iop_equals(&pfx##__s, (void *)v1, (void *)v2);                \
    }                                                                        \
    static inline void pfx##__init(pfx##__t *v) {                            \
        iop_init(&pfx##__s, (void *)v);                                      \
    }                                                                        \
    static inline pfx##__t *pfx##__dup(mem_pool_t *mp, const pfx##__t *v) {  \
        return (pfx##__t *)iop_dup(mp, &pfx##__s, (const void *)v);          \
    }                                                                        \
    static inline void                                                       \
    pfx##__copy(mem_pool_t *mp, pfx##__t **out, const pfx##__t *v) {         \
        iop_copy(mp, &pfx##__s, (void **)out, (const void *)v);              \
    }                                                                        \
    static inline int                                                        \
    pfx##__jpack(const pfx##__t *v,                                          \
                 int (*wcb)(void *, const void *, int), void *priv) {        \
        return iop_jpack(&pfx##__s, (const void *)v, wcb, priv, false);      \
    }                                                                        \
    static inline int pfx##__junpack(iop_json_lex_t *ll, pfx##__t *v,        \
                                     bool single)                            \
    {                                                                        \
        return iop_junpack(ll, &pfx##__s, (void *)v, single);                \
    }

#endif
