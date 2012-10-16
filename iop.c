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

#include "arith.h"
#include "iop.h"
#include "iop-helpers.inl.c"

static void
iop_init_fields(void *value, const iop_field_t *fdesc, const iop_field_t *end)
{
    for (; fdesc < end; fdesc++) {
        void *ptr = (char *)value + fdesc->data_offs;

        if (fdesc->repeat == IOP_R_REQUIRED && fdesc->type == IOP_T_STRUCT) {
            /* We can't handle the unions here since we don't know which field
             * has been selected */
            const iop_struct_t *desc = fdesc->u1.st_desc;

            iop_init_fields(ptr, desc->fields, desc->fields +
                            desc->fields_len);
        } else
        if (fdesc->repeat == IOP_R_DEFVAL) {
            switch (fdesc->type) {
              case IOP_T_I8: case IOP_T_U8:
                *(uint8_t *)ptr  = fdesc->u1.defval_u64;
                break;
              case IOP_T_I16: case IOP_T_U16:
                *(uint16_t *)ptr = fdesc->u1.defval_u64;
                break;
              case IOP_T_ENUM:
                *(uint32_t *)ptr = fdesc->u0.defval_enum;
                break;
              case IOP_T_I32: case IOP_T_U32:
                *(uint32_t *)ptr = fdesc->u1.defval_u64;
                break;
              case IOP_T_I64: case IOP_T_U64:
                *(uint64_t *)ptr = fdesc->u1.defval_u64;
                break;
              case IOP_T_BOOL:
                *(bool *)ptr     = !!fdesc->u1.defval_u64; /* Map to 0/1 */
                break;
              case IOP_T_DOUBLE:
                *(double *)ptr   = fdesc->u1.defval_d;
                break;
              case IOP_T_STRING:
              case IOP_T_XML:
                *(lstr_t *)ptr = LSTR_INIT_V(fdesc->u1.defval_data, fdesc->u0.defval_len);
                break;
              case IOP_T_DATA:
                ((iop_data_t *)ptr)->len  = fdesc->u0.defval_len;
                ((iop_data_t *)ptr)->data = (void *)fdesc->u1.defval_data;
                ((iop_data_t *)ptr)->flags = 0;
                break;
              default:
                e_panic("unsupported");
                continue;
            }
        }
    }
}
void iop_init(const iop_struct_t *desc, void *value)
{
    memset(value, 0, desc->size);
    iop_init_fields(value, desc->fields, desc->fields + desc->fields_len);
}

/*----- duplicating values -{{{-*/

static size_t iop_dup_size(const iop_struct_t *desc, const void *val)
{
    const iop_field_t *fdesc;
    const iop_field_t *end;
    size_t len = 0;

    if (desc->is_union) {
        fdesc = get_union_field(desc, val);
        end   = fdesc + 1;
    } else {
        fdesc = desc->fields;
        end   = fdesc + desc->fields_len;
    }

    for (; fdesc < end; fdesc++) {
        const void *ptr = (char *)val + fdesc->data_offs;
        int n = 1;

        if (fdesc->repeat == IOP_R_REPEATED) {
            n    = ((iop_data_t *)ptr)->len;
            ptr  = ((iop_data_t *)ptr)->data;
            len += ROUND_UP(n * fdesc->size, 8);
        }

        if (!((1 << fdesc->type) & IOP_BLK_OK)) /* DATA,STRING,STRUCT,UNION */
            continue;

        if (fdesc->repeat == IOP_R_OPTIONAL) {
            if (!iop_value_has(fdesc, ptr))
                continue;
            if ((1 << fdesc->type) & IOP_STRUCTS_OK) {
                ptr  = *(void **)ptr;
                len += ROUND_UP(fdesc->size, 8);
            }
        }

        if ((1 << fdesc->type) & IOP_STRUCTS_OK) {
            for (int j = 0; j < n; j++) {
                const void *v = &IOP_FIELD(const char, ptr, j * fdesc->size);
                len += iop_dup_size(fdesc->u1.st_desc, v);
            }
        } else {
            for (int j = 0; j < n; j++) {
                len += ROUND_UP(IOP_FIELD(iop_data_t, ptr, j).len + 1, 8);
            }
        }
    }

    return len;
}

static uint8_t *realign(uint8_t *ptr)
{
    return (uint8_t *)ROUND_UP((uintptr_t)ptr, 8);
}

static uint8_t *
__iop_copy(const iop_struct_t *st, uint8_t *dst, void *wval, const void *rval)
{
    const iop_field_t *fdesc;
    const iop_field_t *end;

    if (st->is_union) {
        fdesc = get_union_field(st, rval);
        end   = fdesc + 1;
    } else {
        fdesc = st->fields;
        end   = fdesc + st->fields_len;
    }

    for (; fdesc < end; fdesc++) {
        const void *rp = (char *)rval + fdesc->data_offs;
        const void *wp = (char *)wval + fdesc->data_offs;
        int n = 1;

        if (fdesc->repeat == IOP_R_REPEATED) {
            n   = ((iop_data_t *)rp)->len;
            rp  = ((iop_data_t *)rp)->data;
            wp  = ((iop_data_t *)wp)->data = dst;
            dst = realign(mempcpy(dst, rp, n * fdesc->size));
        }

        if (!((1 << fdesc->type) & IOP_BLK_OK)) /* DATA,STRING,STRUCT,UNION */
            continue;

        if (fdesc->repeat == IOP_R_OPTIONAL) {
            if (!iop_value_has(fdesc, rp))
                continue;
            if ((1 << fdesc->type) & IOP_STRUCTS_OK) {
                rp  = *(void **)rp;
                wp  = *(void **)wp = dst;
                dst = realign(mempcpy(dst, rp, fdesc->size));
            }
        }

        if ((1 << fdesc->type) & IOP_STRUCTS_OK) {
            for (int j = 0; j < n; j++) {
                const void *rv = &IOP_FIELD(const char, rp, j * fdesc->size);
                void *wv = &IOP_FIELD(char, wp, j * fdesc->size);

                dst = __iop_copy(fdesc->u1.st_desc, dst, wv, rv);
            }
        } else
        if (fdesc->type == IOP_T_STRING || fdesc->type == IOP_T_XML) {
            for (int j = 0; j < n; j++) {
                lstr_t *orig = &IOP_FIELD(lstr_t, rp, j);

                /* We have to fix the lstr_t mem_pool manually because some
                 * naughty programmers could have played with it */
                IOP_FIELD(lstr_t, wp, j).s = (const char *)dst;
                IOP_FIELD(lstr_t, wp, j).mem_pool = MEM_STATIC;
                dst = realign(mempcpyz(dst, orig->s, orig->len));
            }
        } else {
            for (int j = 0; j < n; j++) {
                iop_data_t *orig = &IOP_FIELD(iop_data_t, rp, j);

                IOP_FIELD(iop_data_t, wp, j).data = dst;
                dst = realign(mempcpyz(dst, orig->data, orig->len));
            }
        }
    }

    return dst;
}

void *iop_dup(mem_pool_t *mp, const iop_struct_t *st, const void *v)
{
    size_t sz = ROUND_UP(st->size, 8) + iop_dup_size(st, v);
    uint8_t *dst, *res;

    res = mp ? mp->malloc(mp, sz, MEM_RAW) : imalloc(sz, MEM_LIBC | MEM_RAW);
    dst = realign(mempcpy(res, v, st->size));
    dst = __iop_copy(st, dst, res, v);
    assert (dst == res + sz);
    return res;
}

void iop_copy(mem_pool_t *mp, const iop_struct_t *st, void **outp, const void *v)
{
    size_t sz = ROUND_UP(st->size, 8) + iop_dup_size(st, v);
    uint8_t *dst, *res = *outp;

    if (mp) {
        res = mp->realloc(mp, res, 0, sz, MEM_RAW);
    } else {
        res = irealloc(res, 0, sz, MEM_LIBC | MEM_RAW);
    }
    dst = realign(mempcpy(res, v, st->size));
    dst = __iop_copy(st, dst, res, v);
    assert (dst == res + sz);
    *outp = res;
}

/*----- duplicating values -}}}-*/
/*----- comparing values -{{{-*/

/** Compare two optional scalars fields, assuming the field is present */
static bool iop_opt_equals(const iop_field_t *f, const void *v1, const void *v2)
{
#define OPT_EQU(t, v1, v2)  (((t *)(v1))->v == ((t *)(v2))->v)

    switch (f->type) {
      case IOP_T_I8:  case IOP_T_U8:
        return OPT_EQU(iop_opt_u8_t, v1, v2);
      case IOP_T_I16: case IOP_T_U16:
        return OPT_EQU(iop_opt_u16_t, v1, v2);
      case IOP_T_I32: case IOP_T_U32: case IOP_T_ENUM:
        return OPT_EQU(iop_opt_u32_t, v1, v2);
      case IOP_T_I64: case IOP_T_U64:
        return OPT_EQU(iop_opt_u64_t, v1, v2);
      case IOP_T_BOOL:
        return OPT_EQU(iop_opt_bool_t, v1, v2);
      case IOP_T_DOUBLE:
        return OPT_EQU(iop_opt_double_t, v1, v2);
      default:
        e_panic("should not happen");
    }
#undef OPT_EQU
}

static bool
__iop_equals(const iop_struct_t *st, const uint8_t *v1, const uint8_t *v2)
{
    const iop_field_t *fdesc;
    const iop_field_t *end;

    if (st->is_union) {
        if (*(uint16_t *)v1 != *(uint16_t *)v2)
            return false;
        fdesc = get_union_field(st, v1);
        end   = fdesc + 1;
    } else {
        fdesc = st->fields;
        end   = fdesc + st->fields_len;
    }

    for (; fdesc < end; fdesc++) {
        const void *r1 = v1 + fdesc->data_offs;
        const void *r2 = v2 + fdesc->data_offs;
        int n = 1;

        if (fdesc->repeat == IOP_R_REPEATED) {
            n   = ((iop_data_t *)r1)->len;
            if (((iop_data_t *)r2)->len != n)
                return false;
            r1  = ((iop_data_t *)r1)->data;
            r2  = ((iop_data_t *)r2)->data;
        }

        if (fdesc->repeat == IOP_R_OPTIONAL) {
            bool has = iop_value_has(fdesc, r1);

            if (has != iop_value_has(fdesc, r2))
                return false;
            if (!has)
                continue;
            if (!((1 << fdesc->type) & IOP_BLK_OK)) {
                if (!iop_opt_equals(fdesc, r1, r2))
                    return false;
                continue;
            }
            if ((1 << fdesc->type) & IOP_STRUCTS_OK) {
                r1  = *(void **)r1;
                r2  = *(void **)r2;
            }
        }
        if ((1 << fdesc->type) & IOP_STRUCTS_OK) {
            for (int i = 0; i < n; i++) {
                const void *t1 = &IOP_FIELD(const uint8_t, r1, i * fdesc->size);
                const void *t2 = &IOP_FIELD(const uint8_t, r2, i * fdesc->size);

                if (!__iop_equals(fdesc->u1.st_desc, t1, t2))
                    return false;
            }
        } else
        if ((1 << fdesc->type) & IOP_BLK_OK) {
            for (int i = 0; i < n; i++) {
                const iop_data_t *t1 = &IOP_FIELD(const iop_data_t, r1, i);
                const iop_data_t *t2 = &IOP_FIELD(const iop_data_t, r2, i);

                if (t1->len != t2->len || memcmp(t1->data, t2->data, t1->len))
                    return false;
            }
        } else {
            if (memcmp(r1, r2, fdesc->size * n))
                return false;
        }
    }

    return true;
}

bool iop_equals(const iop_struct_t *st, const void *v1, const void *v2)
{
    return __iop_equals(st, v1, v2);
}

static inline bool
iop_field_is_defval(const iop_field_t *fdesc, const void *ptr)
{
    assert (fdesc->repeat == IOP_R_DEFVAL);

    switch (fdesc->type) {
      case IOP_T_I8: case IOP_T_U8:
        return *(uint8_t *)ptr == (uint8_t)fdesc->u1.defval_u64;
      case IOP_T_I16: case IOP_T_U16:
        return *(uint16_t *)ptr == (uint16_t)fdesc->u1.defval_u64;
      case IOP_T_ENUM:
        return *(int *)ptr == fdesc->u0.defval_enum;
      case IOP_T_I32: case IOP_T_U32:
        return *(uint32_t *)ptr == (uint32_t)fdesc->u1.defval_u64;
      case IOP_T_I64: case IOP_T_U64:
      case IOP_T_DOUBLE:
        /* XXX double is handled like U64 because we want to compare them as
         * bit to bit */
        return *(uint64_t *)ptr == fdesc->u1.defval_u64;
      case IOP_T_BOOL:
        return fdesc->u1.defval_u64 ? *(bool *)ptr : !*(bool *)ptr;
      case IOP_T_STRING:
      case IOP_T_XML:
      case IOP_T_DATA:
        if (!fdesc->u0.defval_len) {
            /* In this case we don't care about the string pointer. An empty
             * string is an empty string whatever its pointer is. */
            return !((iop_data_t *)ptr)->len;
        } else {
            /* We consider a NULL string as “take the default value please”
             * and otherwise we check for the pointer equality. */
            return !((iop_data_t *)ptr)->data
                || (((iop_data_t *)ptr)->data == fdesc->u1.defval_data
                 && ((iop_data_t *)ptr)->len  == fdesc->u0.defval_len);
        }
      default:
        e_panic("unsupported");
    }
}

/*----- duplicating values -}}}-*/
/*----- hashing values -{{{-*/

struct iop_hash_ctx {
    size_t   pos;
    uint8_t  buf[1024];
    void   (*hfun)(void *ctx, const void *input, int len);
    void    *ctx;
};

static ALWAYS_INLINE
void iop_hash_update(struct iop_hash_ctx *ctx, const void *d, size_t len)
{
    size_t pos = ctx->pos;

    if (pos + len > sizeof(ctx->buf)) {
        ctx->pos = 0;
        ctx->hfun(ctx->ctx, ctx->buf, pos);
        ctx->hfun(ctx->ctx, d, len);
    } else {
        memcpy(ctx->buf + pos, d, len);
        if ((pos += len) > sizeof(ctx->buf) / 2) {
            ctx->pos = 0;
            ctx->hfun(ctx->ctx, ctx->buf, pos);
        } else {
            ctx->pos = pos;
        }
    }
}

static ALWAYS_INLINE
void iop_hash_update_u16(struct iop_hash_ctx *ctx, uint16_t i)
{
    size_t pos = ctx->pos;

    assert (pos + 2 < sizeof(ctx->buf));
    put_unaligned_le16(ctx->buf + pos, i);
    if ((pos += 2) > sizeof(ctx->buf) / 2) {
        ctx->pos = 0;
        ctx->hfun(ctx->ctx, ctx->buf, pos);
    } else {
        ctx->pos = pos;
    }
}

static ALWAYS_INLINE
void iop_hash_update_u32(struct iop_hash_ctx *ctx, uint32_t i)
{
    size_t pos = ctx->pos;

    assert (pos + 4 < sizeof(ctx->buf));
    put_unaligned_le32(ctx->buf + pos, i);
    if ((pos += 4) > sizeof(ctx->buf) / 2) {
        ctx->pos = 0;
        ctx->hfun(ctx->ctx, ctx->buf, pos);
    } else {
        ctx->pos = pos;
    }
}

static ALWAYS_INLINE
void iop_hash_update_i64(struct iop_hash_ctx *ctx, int64_t i)
{
    size_t pos = ctx->pos;

    assert (pos + 8 < sizeof(ctx->buf));
    put_unaligned_le64(ctx->buf + pos, i);
    if ((pos += 8) > sizeof(ctx->buf) / 2) {
        ctx->pos = 0;
        ctx->hfun(ctx->ctx, ctx->buf, pos);
    } else {
        ctx->pos = pos;
    }
}
#define iop_hash_update_dbl(ctx, d) \
    iop_hash_update_i64(ctx, double_bits_cpu(d))

static void
iop_opt_hash(struct iop_hash_ctx *ctx, const iop_field_t *f, const void *v)
{
    uint8_t b;

    switch (f->type) {
      case IOP_T_BOOL:
        b = !!((iop_opt_bool_t *)v)->v;
        iop_hash_update(ctx, &b, 1);
        break;
      case IOP_T_I8:
        iop_hash_update_i64(ctx, ((iop_opt_i8_t *)v)->v);
        break;
      case IOP_T_U8:
        iop_hash_update_i64(ctx, ((iop_opt_u8_t *)v)->v);
        break;
      case IOP_T_I16:
        iop_hash_update_i64(ctx, ((iop_opt_i16_t *)v)->v);
        break;
      case IOP_T_U16:
        iop_hash_update_i64(ctx, ((iop_opt_u16_t *)v)->v);
        break;
      case IOP_T_I32: case IOP_T_ENUM:
        iop_hash_update_i64(ctx, ((iop_opt_i32_t *)v)->v);
        break;
      case IOP_T_U32:
        iop_hash_update_i64(ctx, ((iop_opt_u32_t *)v)->v);
        break;
      case IOP_T_I64:
        iop_hash_update_i64(ctx, ((iop_opt_i64_t *)v)->v);
        break;
      case IOP_T_U64:
        iop_hash_update_i64(ctx, ((iop_opt_u64_t *)v)->v);
        break;
      case IOP_T_DOUBLE:
        iop_hash_update_dbl(ctx, ((iop_opt_double_t *)v)->v);
        break;
      default:
        e_panic("should not happen");
    }
}

static void
__iop_hash(struct iop_hash_ctx *ctx, const iop_struct_t *st, const uint8_t *v)
{
    const iop_field_t *fdesc;
    const iop_field_t *end;

    if (st->is_union) {
        fdesc = get_union_field(st, v);
        end   = fdesc + 1;
    } else {
        fdesc = st->fields;
        end   = fdesc + st->fields_len;
    }

    for (; fdesc < end; fdesc++) {
        const void *r = v + fdesc->data_offs;
        int n = 1;

        iop_hash_update_u16(ctx, fdesc->tag);
        iop_hash_update(ctx, fdesc->name.s, fdesc->name.len);

        if (fdesc->repeat == IOP_R_REPEATED) {
            n  = ((iop_data_t *)r)->len;
            r  = ((iop_data_t *)r)->data;
            iop_hash_update_u32(ctx, n);
        }

        if (fdesc->repeat == IOP_R_OPTIONAL) {
            if (!iop_value_has(fdesc, r))
                continue;

            if ((1 << fdesc->type) & IOP_BLK_OK) {
                if ((1 << fdesc->type) & IOP_STRUCTS_OK) {
                    r = *(void **)r;
                }
            } else {
                iop_opt_hash(ctx, fdesc, r);
                continue;
            }
        }
        switch (fdesc->type) {
          case IOP_T_I8:
            for (int i = 0; i < n; i++) {
                iop_hash_update_i64(ctx, ((int8_t *)r)[i]);
            }
            break;
          case IOP_T_BOOL:
            for (int i = 0; i < n; i++) {
                iop_hash_update_i64(ctx, !!((bool *)r)[i]);
            }
            break;
          case IOP_T_U8:
            for (int i = 0; i < n; i++) {
                iop_hash_update_i64(ctx, ((uint8_t *)r)[i]);
            }
            break;
          case IOP_T_I16:
            for (int i = 0; i < n; i++) {
                iop_hash_update_i64(ctx, ((int16_t *)r)[i]);
            }
            break;
          case IOP_T_U16:
            for (int i = 0; i < n; i++) {
                iop_hash_update_i64(ctx, ((uint16_t *)r)[i]);
            }
            break;
          case IOP_T_I32: case IOP_T_ENUM:
            for (int i = 0; i < n; i++) {
                iop_hash_update_i64(ctx, ((int32_t *)r)[i]);
            }
            break;
          case IOP_T_U32:
            for (int i = 0; i < n; i++) {
                iop_hash_update_i64(ctx, ((uint32_t *)r)[i]);
            }
            break;
          case IOP_T_I64:
            for (int i = 0; i < n; i++) {
                iop_hash_update_i64(ctx, ((int64_t *)r)[i]);
            }
            break;
          case IOP_T_U64:
            for (int i = 0; i < n; i++) {
                iop_hash_update_i64(ctx, ((uint64_t *)r)[i]);
            }
            break;
          case IOP_T_DOUBLE:
            for (int i = 0; i < n; i++) {
                iop_hash_update_dbl(ctx, ((double *)r)[i]);
            }
            break;
          case IOP_T_UNION:
          case IOP_T_STRUCT:
            for (int i = 0; i < n; i++) {
                __iop_hash(ctx, fdesc->u1.st_desc,
                           &IOP_FIELD(const uint8_t, r, i * fdesc->size));
            }
            break;
          case IOP_T_XML:
          case IOP_T_STRING:
          case IOP_T_DATA:
            for (int i = 0; i < n; i++) {
                const iop_data_t *s = &IOP_FIELD(const iop_data_t, r, i);

                iop_hash_update_u32(ctx, s->len);
                iop_hash_update(ctx, s->data, s->len);
            }
            break;
          default:
            e_panic("should not happen");
        }
    }
}

void iop_hash(const iop_struct_t *st, const void *v,
              void (*hfun)(void *ctx, const void *input, int ilen),
              void *hctx)
{
    struct iop_hash_ctx ctx = {
        .hfun = hfun,
        .ctx  = hctx,
    };
    __iop_hash(&ctx, st, v);
    if (ctx.pos)
        hfun(hctx, ctx.buf, ctx.pos);
}

/*----- duplicating values -}}}-*/
/*----- get value encoding size -{{{-*/

int iop_bpack_size(const iop_struct_t *desc, const void *val, qv_t(i32) *szs)
{
    const iop_field_t *fdesc;
    const iop_field_t *end;
    int len = 0;

    if (desc->is_union) {
        fdesc = get_union_field(desc, val);
        end   = fdesc + 1;
    } else {
        fdesc = desc->fields;
        end   = fdesc + desc->fields_len;
    }

    for (; fdesc < end; fdesc++) {
        const void *ptr = (char *)val + fdesc->data_offs;
        int n = 1;

        if (fdesc->repeat == IOP_R_OPTIONAL) {
            if (!iop_value_has(fdesc, ptr))
                continue;
            if ((1 << fdesc->type) & IOP_STRUCTS_OK)
                ptr = *(void **)ptr;
        } else
        if (fdesc->repeat == IOP_R_REPEATED) {
            n   = ((iop_data_t *)ptr)->len;
            ptr = ((iop_data_t *)ptr)->data;
            if (n == 0)
                continue;
            if (n > 1) {
                if ((1 << fdesc->type) & IOP_REPEATED_OPTIMIZE_OK) {
                    int32_t i32 = n * fdesc->size;

                    len += 1 + fdesc->tag_len;
                    len += get_len_len(i32) + i32;
                    continue;
                }
                len += 4 + n;
            }
        } else
        if (fdesc->repeat == IOP_R_DEFVAL) {
            /* Skip the field is it's still equal to its default value */
            if (iop_field_is_defval(fdesc, ptr))
                continue;
        }

        len += 1 + fdesc->tag_len;
        switch (fdesc->type) {
          case IOP_T_I8:
            len += n;
            break;
          case IOP_T_U8:
            len += n;
            for (int j = 0; j < n; j++)
                len += IOP_FIELD(uint8_t, ptr, j) >> 7;
            break;
          case IOP_T_I16:
            for (int j = 0; j < n; j++)
                len += get_vint32_len(IOP_FIELD(int16_t, ptr, j));
            break;
          case IOP_T_U16:
            for (int j = 0; j < n; j++)
                len += get_vint32_len(IOP_FIELD(uint16_t, ptr, j));
            break;
          case IOP_T_I32:
          case IOP_T_ENUM:
            for (int j = 0; j < n; j++)
                len += get_vint32_len(IOP_FIELD(int32_t, ptr, j));
            break;
          case IOP_T_U32:
            for (int j = 0; j < n; j++)
                len += get_vint64_len(IOP_FIELD(uint32_t, ptr, j));
            break;
          case IOP_T_I64:
          case IOP_T_U64:
            for (int j = 0; j < n; j++)
                len += get_vint64_len(IOP_FIELD(int64_t, ptr, j));
            break;
          case IOP_T_BOOL:
            len += n;
            break;
          case IOP_T_DOUBLE:
            len += n * 8;
            break;
          case IOP_T_STRING:
          case IOP_T_DATA:
          case IOP_T_XML:
            for (int j = 0; j < n; j++) {
                int32_t i32 = IOP_FIELD(iop_data_t, ptr, j).len;
                len += get_len_len(i32 + 1) + i32 + 1;
            }
            break;
          case IOP_T_UNION:
          case IOP_T_STRUCT:
          default:
            for (int j = 0; j < n; j++) {
                const void *v = &IOP_FIELD(const char, ptr, j * fdesc->size);
                int32_t offs = szs->len, i32;

                qv_growlen(i32, szs, 1);
                i32  = iop_bpack_size(fdesc->u1.st_desc, v, szs);
                szs->tab[offs] = i32;
                len += get_len_len(i32) + i32;
            }
            break;
        }
    }

    return len;
}

/*-}}}-*/
/*----- packing -{{{-*/

static uint8_t *
pack_struct(void *dst, const iop_struct_t *, const void *, const int **);
static uint8_t *
pack_union(void *dst, const iop_struct_t *, const void *, const int **);

static uint8_t *
pack_value(uint8_t *dst, const iop_field_t *f, const void *v, const int **szsp)
{
    uint32_t len;

    switch (f->type) {
      case IOP_T_I8:
        dst    = pack_tag(dst, f->tag, f->tag_len, IOP_WIRE_MASK(INT1));
        *dst++ = *(int8_t *)v;
        return dst;
      case IOP_T_U8:
        return pack_int32(dst, f->tag, f->tag_len, *(uint8_t *)v);
      case IOP_T_I16:
        return pack_int32(dst, f->tag, f->tag_len, *(int16_t *)v);
      case IOP_T_U16:
        return pack_int32(dst, f->tag, f->tag_len, *(uint16_t *)v);
      case IOP_T_I32:
      case IOP_T_ENUM:
        return pack_int32(dst, f->tag, f->tag_len, *(int32_t *)v);
      case IOP_T_U32:
        return pack_int64(dst, f->tag, f->tag_len, *(uint32_t *)v);
      case IOP_T_I64:
      case IOP_T_U64:
        return pack_int64(dst, f->tag, f->tag_len, *(int64_t *)v);
      case IOP_T_BOOL:
        /* bool are mapped to 0 or 1 */
        dst    = pack_tag(dst, f->tag, f->tag_len, IOP_WIRE_MASK(INT1));
        *dst++ = !!*(bool *)v;
        return dst;
      case IOP_T_DOUBLE:
        dst = pack_tag(dst, f->tag, f->tag_len, IOP_WIRE_MASK(QUAD));
        return put_unaligned_double_le(dst, *(double *)v);
      case IOP_T_STRING:
      case IOP_T_DATA:
      case IOP_T_XML:
        len = ((iop_data_t *)v)->len;
        dst = pack_len(dst, f->tag, f->tag_len, len + 1);
        dst = mempcpyz(dst, ((iop_data_t *)v)->data, len);
        return dst;
      case IOP_T_UNION:
        dst = pack_len(dst, f->tag, f->tag_len, *(*szsp)++);
        return pack_union(dst, f->u1.st_desc, v, szsp);
      case IOP_T_STRUCT:
      default:
        dst = pack_len(dst, f->tag, f->tag_len, *(*szsp)++);
        return pack_struct(dst, f->u1.st_desc, v, szsp);
    }
}

static uint8_t *pack_value_vec(uint8_t *dst, const iop_field_t *f,
                               const void *v, uint32_t n, const int **szsp)
{
    const iop_data_t *d;
    uint32_t len;

    switch (f->type) {
      case IOP_T_I32:
      case IOP_T_ENUM:
        do {
            dst = pack_int32(dst, 0, 0, *(int32_t *)v);
            v   = (char *)v + 4;
        } while (--n > 0);
        return dst;
      case IOP_T_U32:
        do {
            dst = pack_int64(dst, 0, 0, *(uint32_t *)v);
            v   = (char *)v + 4;
        } while (--n > 0);
        return dst;
      case IOP_T_I64:
      case IOP_T_U64:
        do {
            dst = pack_int64(dst, 0, 0, *(int64_t *)v);
            v   = (char *)v + 8;
        } while (--n > 0);
        return dst;
      case IOP_T_DOUBLE:
        do {
            dst = pack_tag(dst, 0, 0, IOP_WIRE_MASK(QUAD));
            dst = put_unaligned_double_le(dst, *(double *)v);
            v   = (char *)v + 8;
        } while (--n > 0);
        return dst;
      case IOP_T_STRING:
      case IOP_T_DATA:
      case IOP_T_XML:
        do {
            d = v;
            len = d->len;
            dst = pack_len(dst, 0, 0, len + 1);
            dst = mempcpyz(dst, d->data, len);
            v   = (char *)v + f->size;
        } while (--n > 0);
        return dst;
      case IOP_T_UNION:
        do {
            dst = pack_len(dst, 0, 0, *(*szsp)++);
            dst = pack_union(dst, f->u1.st_desc, v, szsp);
            v   = (char *)v + f->size;
        } while (--n > 0);
        return dst;
      case IOP_T_STRUCT:
        do {
            dst = pack_len(dst, 0, 0, *(*szsp)++);
            dst = pack_struct(dst, f->u1.st_desc, v, szsp);
            v   = (char *)v + f->size;
        } while (--n > 0);
        return dst;

      default:
        e_panic("should not happen");
    }
}

static uint8_t *
pack_struct(void *dst, const iop_struct_t *desc, const void *v, const int **szsp)
{
    assert(!desc->is_union); /* We don't want a union here */

    for (int i = 0; i < desc->fields_len; i++) {
        const iop_field_t *f = desc->fields + i;
        const void *ptr = (char *)v + f->data_offs;

        if (f->repeat == IOP_R_OPTIONAL) {
            if (!iop_value_has(f, ptr))
                continue;
            if ((1 << f->type) & IOP_STRUCTS_OK)
                ptr = *(void **)ptr;
        } else
        if (f->repeat == IOP_R_REPEATED) {
            const iop_data_t *data = ptr;

            if (data->len == 0)
                continue;
            ptr = data->data;
            if (data->len > 1) {
                if ((1 << f->type) & IOP_REPEATED_OPTIMIZE_OK) {
                    /* When data unit is really small (byte, bool, …) we
                     * prefer to pack them in one big block */
                    uint32_t sz = data->len * f->size;

                    assert (f->size <= 2);
                    dst = pack_len(dst, f->tag, f->tag_len, sz);
                    dst = mempcpy(dst, data->data, sz);
                } else {
                    dst = pack_tag(dst, f->tag, f->tag_len,
                                   IOP_WIRE_MASK(REPEAT));
                    dst = put_unaligned_le32(dst, data->len);
                    dst = pack_value_vec(dst, f, ptr, data->len, szsp);
                }
                continue;
            }
        } else
        if (f->repeat == IOP_R_DEFVAL) {
            /* Skip the field is it's still equal to its default value */
            if (iop_field_is_defval(f, ptr))
                continue;
        }

        dst = pack_value(dst, f, ptr, szsp);
    }
    return dst;
}

static uint8_t *
pack_union(void *dst, const iop_struct_t *desc, const void *v,
           const int **szsp)
{
    const iop_field_t *f = get_union_field(desc, v);
    assert(f->repeat == IOP_R_REQUIRED);

    return pack_value(dst, f, (char *)v + f->data_offs, szsp);
}

void
iop_bpack(void *dst, const iop_struct_t *desc, const void *v, const int *szs)
{
    if (desc->is_union) {
        pack_union(dst, desc, v, &szs);
    } else {
        pack_struct(dst, desc, v, &szs);
    }
}

lstr_t t_iop_bpack_struct(const iop_struct_t *st, const void *v)
{
    qv_t(i32) sizes;
    void *data;
    int len;

    if (!v)
        return LSTR_NULL_V;

    qv_inita(i32, &sizes, 1024);

    len  = iop_bpack_size(st, v, &sizes);
    data = t_new_raw(char, len);

    iop_bpack(data, st, v, sizes.tab);
    qv_wipe(i32, &sizes);
    return lstr_init_(data, len, MEM_STACK);
}

/*-}}}-*/
/*----- unpacking -{{{-*/

static int get_uint32(pstream_t *ps, int ilen, uint32_t *u32)
{
    PS_WANT(ps_has(ps, ilen));
    *u32 = 0;
    memcpy(u32, ps->p, ilen);
#if __BYTE_ORDER == __BIG_ENDIAN
    *u32 = __builtin_bswap32(*u32);
#endif
    return __ps_skip(ps, ilen);
}

/*
 * XXX: an iop_range helps doing run-length encoded binary search. We know
 * that IOPs tags are mostly contiguous, hence we encode "full" runs of tags
 * this way:
 *   [ offset0, start_tag0, offset1, ..., start_tag_n, offset_n]
 *
 * This means that the offset0-th up to the offset1-th values described by
 * this iop_range take values contiguously from the range:
 *   [ start_tag0 .. start_tag0 + offset1 - offset0 [
 *
 * Of course, offset0 is always equal to 0, and offset_n should be equal to
 * ranges_len.
 *
 * Example: the iop_range for "10 11 12 13 100 101 102" is "0 10 4 100 7"
 *  - positions [0 .. 4[ have values in [10  .. 10 + 4 - 0[
 *  - positions [4 .. 7[ have values in [100 .. 100 + 7 - 4[
 *
 */
int iop_ranges_search(int const * ranges, int ranges_len, int tag)
{
    int l = 0, r = ranges_len;

    while (l < r) {
        int i = (l + r) / 2;
        int offs  = ranges[i * 2];
        int start = ranges[i * 2 + 1];

        if (tag < start) {
            r = i;
            continue;
        }
        if (tag + offs >= start + ranges[i * 2 + 2]) {
            l = i + 1;
            continue;
        }
        return ranges[i * 2] + (tag - start);
    }
    return -1;
}

int iop_enum_from_str2(const iop_enum_t *e, const char *s, int len, bool *found)
{
    if (len < 0)
        len = strlen(s);
    *found = false;
    for (int i = 0; i < e->enum_len; i++) {
        if (len == e->names[i].len && !strncasecmp(e->names[i].s, s, len)) {
            *found = true;
            return e->values[i];
        }
    }
    return -1;
}

int iop_enum_from_str(const iop_enum_t *e, const char *s, int len, int err)
{
    bool found;
    int val = iop_enum_from_str2(e, s, len, &found);

    return (found) ? val : err;
}

int iop_enum_from_lstr(const iop_enum_t *e, const lstr_t s, bool *found)
{
    *found = false;
    for (int i = 0; i < e->enum_len; i++) {
        if (s.len == e->names[i].len
        &&  strncasecmp(e->names[i].s, s.s, s.len) == 0)
        {
            *found = true;
            return e->values[i];
        }
    }
    return -1;
}


static int iop_skip_field(pstream_t *ps, iop_wire_type_t wt)
{
    uint32_t u32;

    switch (wt) {
      case IOP_WIRE_BLK1: PS_CHECK(get_uint32(ps, 1, &u32)); break;
      case IOP_WIRE_BLK2: PS_CHECK(get_uint32(ps, 2, &u32)); break;
      case IOP_WIRE_BLK4: PS_CHECK(get_uint32(ps, 4, &u32)); break;

      case IOP_WIRE_INT1:
      case IOP_WIRE_INT2:
      case IOP_WIRE_INT4:
        u32 = 1 << (wt - IOP_WIRE_INT1);
        break;
      case IOP_WIRE_QUAD:
        u32 = 8;
        break;
      default:
        PS_CHECK(-1);
    }

    return ps_skip(ps, u32);
}

static ALWAYS_INLINE
int iop_patch_int(const iop_field_t *fdesc, void *ptr, int64_t i64)
{
    switch (fdesc->type) {
      case IOP_T_I8: case IOP_T_U8:
        *(int8_t *)ptr  = i64;
        break;
      case IOP_T_I16: case IOP_T_U16:
        *(int16_t *)ptr = i64;
        break;
      case IOP_T_ENUM:
      case IOP_T_I32: case IOP_T_U32:
        *(int32_t *)ptr = i64;
        break;
      case IOP_T_I64: case IOP_T_U64:
        *(int64_t *)ptr = i64;
        break;
      case IOP_T_BOOL:
        *(bool *)ptr    = i64;
        break;
    }
    return 0;
}

static ALWAYS_INLINE int
__get_tag_wt(pstream_t *ps, uint32_t *tag, iop_wire_type_t *wt)
{
    *wt  = IOP_WIRE_FMT(ps->b[0]);
    *tag = IOP_TAG(__ps_getc(ps));
    if (likely(*tag < IOP_LONG_TAG(1)))
        return 0;
    if (likely(*tag == IOP_LONG_TAG(1)))
        return get_uint32(ps, 1, tag);
    return get_uint32(ps, 2, tag);
}

int __iop_skip_absent_field_desc(void *value, const iop_field_t *fdesc)
{
    void *ptr = (char *)value + fdesc->data_offs;

    if (fdesc->repeat == IOP_R_REQUIRED) {
        const iop_struct_t *desc = fdesc->u1.st_desc;

        /* For a required field, only structs can be absents, be careful that
         * union must be presents */
        PS_WANT(fdesc->type == IOP_T_STRUCT);
        for (int i = 0; i < desc->fields_len; i++)
            PS_CHECK(__iop_skip_absent_field_desc(ptr, desc->fields + i));
        return 0;
    } else
    if (fdesc->repeat == IOP_R_DEFVAL) {
        switch (fdesc->type) {
          case IOP_T_I8: case IOP_T_U8:
            *(uint8_t *)ptr  = fdesc->u1.defval_u64;
            break;
          case IOP_T_I16: case IOP_T_U16:
            *(uint16_t *)ptr = fdesc->u1.defval_u64;
            break;
          case IOP_T_ENUM:
            *(uint32_t *)ptr = fdesc->u0.defval_enum;
            break;
          case IOP_T_I32: case IOP_T_U32:
            *(uint32_t *)ptr = fdesc->u1.defval_u64;
            break;
          case IOP_T_I64: case IOP_T_U64:
            *(uint64_t *)ptr = fdesc->u1.defval_u64;
            break;
          case IOP_T_BOOL:
            *(bool *)ptr     = !!fdesc->u1.defval_u64; /* Map to 0/1 */
            break;
          case IOP_T_DOUBLE:
            *(double *)ptr   = fdesc->u1.defval_d;
            break;
          case IOP_T_STRING:
          case IOP_T_XML:
            *(lstr_t *)ptr = LSTR_INIT_V(fdesc->u1.defval_data, fdesc->u0.defval_len);
            break;
          case IOP_T_DATA:
            ((iop_data_t *)ptr)->len  = fdesc->u0.defval_len;
            ((iop_data_t *)ptr)->data = (void *)fdesc->u1.defval_data;
            ((iop_data_t *)ptr)->flags = 0;
            break;
          default:
            return -1;
        }
    } else
    if (fdesc->repeat == IOP_R_REPEATED) {
        p_clear((iop_data_t *)ptr, 1);
    } else {
        iop_value_set_absent(fdesc, ptr);
    }
    return 0;
}

static int unpack_struct(mem_pool_t *mp, const iop_struct_t *desc, void *value,
                         pstream_t *ps, bool copy);
static int unpack_union(mem_pool_t *mp, const iop_struct_t *desc, void *value,
                        pstream_t *ps, bool copy);

static int unpack_value(mem_pool_t *mp, iop_wire_type_t wt,
                        const iop_field_t *fdesc, void *v,
                        pstream_t *ps, bool copy)
{
    uint32_t u32;
    pstream_t ps_tmp;

    switch (wt) {
      case IOP_WIRE_BLK1:
        PS_CHECK(get_uint32(ps, 1, &u32));
        goto read_blk;

      case IOP_WIRE_BLK2:
        PS_CHECK(get_uint32(ps, 2, &u32));
        goto read_blk;

      case IOP_WIRE_BLK4:
        PS_CHECK(get_uint32(ps, 4, &u32));

      read_blk:
        PS_WANT((1 << fdesc->type) & IOP_BLK_OK);
        PS_WANT(ps_has(ps, u32));
        switch (fdesc->type) {
          case IOP_T_STRING:
          case IOP_T_DATA:
          case IOP_T_XML:
            *(iop_data_t *)v = (iop_data_t){
                .data = copy ? mp_dup(mp, ps->s, u32) : (void *)ps->p,
                .len  = u32 - 1,
            };
            return __ps_skip(ps, u32);
          case IOP_T_UNION:
            ps_tmp = __ps_get_ps(ps, u32);
            return unpack_union(mp, fdesc->u1.st_desc, v, &ps_tmp, copy);
          case IOP_T_STRUCT:
            ps_tmp = __ps_get_ps(ps, u32);
            return unpack_struct(mp, fdesc->u1.st_desc, v, &ps_tmp, copy);
        }
        return -1;

      case IOP_WIRE_INT1:
        PS_WANT(ps_has(ps, 1));
        PS_WANT((1 << fdesc->type) & IOP_INT_OK);
        return iop_patch_int(fdesc, v, (int8_t)__ps_getc(ps));

      case IOP_WIRE_INT2:
        PS_WANT(ps_has(ps, 2));
        PS_WANT((1 << fdesc->type) & IOP_INT_OK);
        return iop_patch_int(fdesc, v, (int16_t)__ps_get_le16(ps));

      case IOP_WIRE_INT4:
        PS_WANT(ps_has(ps, 4));
        PS_WANT((1 << fdesc->type) & IOP_INT_OK);
        return iop_patch_int(fdesc, v, (int32_t)__ps_get_le32(ps));

      case IOP_WIRE_QUAD:
        if ((1 << fdesc->type) & IOP_INT_OK) {
            PS_WANT(ps_has(ps, 8));
            return iop_patch_int(fdesc, v, __ps_get_le64(ps));
        }
#if __FLOAT_WORD_ORDER != __BYTE_ORDER
        if (fdesc->type == IOP_T_DOUBLE)
            return ps_get_double_le(ps, v);
#endif
        PS_WANT((1 << fdesc->type) & IOP_QUAD_OK);
        return ps_get_le64(ps, v);
      default:
        return -1;
    }
}

static int unpack_struct(mem_pool_t *mp, const iop_struct_t *desc, void *value,
                         pstream_t *ps, bool copy)
{
    const iop_field_t *fdesc = desc->fields;
    const iop_field_t *end   = desc->fields + desc->fields_len;

    while (fdesc < end && !ps_done(ps)) {
        iop_wire_type_t wt;
        uint32_t tag, n = 1;
        void *v;

        PS_CHECK(__get_tag_wt(ps, &tag, &wt));
        while (unlikely(tag > fdesc->tag)) {
            e_named_trace(5, "iop/c/unpacker",
                          "unpacking struct %*pM, skipping %*pM field",
                          LSTR_FMT_ARG(desc->fullname),
                          LSTR_FMT_ARG(fdesc->name));
            PS_CHECK(__iop_skip_absent_field_desc(value, fdesc));
            if (++fdesc == end)
                return 0;
        }
        if (unlikely(tag < fdesc->tag)) {
            e_named_trace(5, "iop/c/unpacker",
                          "unpacking struct %*pM, skipping %*pM field",
                          LSTR_FMT_ARG(desc->fullname),
                          LSTR_FMT_ARG(fdesc->name));
            PS_CHECK(iop_skip_field(ps, wt));
            continue;
        }

        e_named_trace(5, "iop/c/unpacker",
                      "unpacking struct %*pM, unpacking %*pM field",
                      LSTR_FMT_ARG(desc->fullname),
                      LSTR_FMT_ARG(fdesc->name));

        if (wt == IOP_WIRE_REPEAT) {
            PS_CHECK(get_uint32(ps, 4, &n));
            PS_WANT(n >= 1);
            PS_WANT(ps_has(ps, 1) && IOP_TAG(ps->b[0]) == 0);
            wt = IOP_WIRE_FMT(__ps_getc(ps));
        }

        v = (char *)value + fdesc->data_offs;
        if (fdesc->repeat == IOP_R_REPEATED) {
            iop_data_t *data = v;

            if (wt != IOP_WIRE_REPEAT
            &&  ((1 << fdesc->type) & IOP_REPEATED_OPTIMIZE_OK))
            {
                /* optimized version of repeated fields are packed in simples
                 * IOP blocks */
                uint32_t len;

                switch (wt) {
                  case IOP_WIRE_BLK1:
                    PS_CHECK(get_uint32(ps, 1, &len));
                    break;
                  case IOP_WIRE_BLK2:
                    PS_CHECK(get_uint32(ps, 2, &len));
                    break;
                  case IOP_WIRE_BLK4:
                    PS_CHECK(get_uint32(ps, 4, &len));
                    break;
                  default:
                    /* Here we expect to have a uniq-value packed as a normal
                     * field (data->len == 1) */
                    goto unpack_array;
                }
                PS_WANT(ps_has(ps, len));

                if (fdesc->size == 1) {
                    data->len = len;
                    data->data = (copy ? mp_dup(mp, ps->s, len)
                                       : (void *)ps->p);
                } else {
                    assert (fdesc->size == 2);
                    PS_WANT(len % 2 == 0);
                    data->len  = len / 2;
                    data->data = mp_dup(mp, ps->s, len);
                }

                __ps_skip(ps, len);
                fdesc++;
                continue;
            }

          unpack_array:
            data->len  = n;
            data->data = v = mp->malloc(mp, n * fdesc->size, MEM_RAW);

            while (n-- > 1) {
                PS_CHECK(unpack_value(mp, wt, fdesc, v, ps, copy));
                PS_WANT(ps_has(ps, 1) && IOP_TAG(ps->b[0]) == 0);
                wt = IOP_WIRE_FMT(__ps_getc(ps));
                v  = (char *)v + fdesc->size;
            }
            PS_CHECK(unpack_value(mp, wt, fdesc, v, ps, copy));
        } else {
            if (fdesc->repeat == IOP_R_OPTIONAL) {
                v = iop_value_set_here(mp, fdesc, v);
            }
            while (n-- > 1) {
                PS_CHECK(iop_skip_field(ps, wt));
                PS_WANT(ps_has(ps, 1) && IOP_TAG(ps->b[0]) == 0);
                wt = IOP_WIRE_FMT(__ps_getc(ps));
            }
            PS_CHECK(unpack_value(mp, wt, fdesc, v, ps, copy));
        }
        fdesc++;
    }

    for (; fdesc < end; fdesc++) {
        e_named_trace(5, "iop/c/unpacker",
                      "unpacking struct %*pM, skipping %*pM field",
                      LSTR_FMT_ARG(desc->fullname),
                      LSTR_FMT_ARG(fdesc->name));
        PS_CHECK(__iop_skip_absent_field_desc(value, fdesc));
    }
    return 0;
}

/* note: returns 0 on success, -1 on error and 1 if the pstream hasn't been
 * fully consumed. */
static int unpack_union(mem_pool_t *mp, const iop_struct_t *desc, void *value,
                        pstream_t *ps, bool copy)
{
    const iop_field_t *fdesc = desc->fields;

    iop_wire_type_t wt;
    uint32_t tag;
    int ifield;

    PS_WANT(!ps_done(ps));
    /* We get the selected tag in the union */
    PS_CHECK(__get_tag_wt(ps, &tag, &wt));
    /* Repeated fields are forbidden in union */
    PS_WANT(wt != IOP_WIRE_REPEAT);

    ifield = iop_ranges_search(desc->ranges, desc->ranges_len, tag);
    PS_CHECK(ifield);
    fdesc += ifield;

    /* Write the selected field */
    *((uint16_t *)value) = fdesc->tag;

    e_named_trace(5, "iop/c/unpacker", "unpacking union %*pM field %*pM",
                  LSTR_FMT_ARG(desc->fullname), LSTR_FMT_ARG(fdesc->name));
    PS_CHECK(unpack_value(mp, wt, fdesc, (char *)value + fdesc->data_offs, ps,
                          copy));
    return ps_done(ps) ? 0 : 1;
}

int iop_bunpack(mem_pool_t *mp, const iop_struct_t *desc, void *value,
                pstream_t ps, bool copy)
{
    e_named_trace(5, "iop/c/unpacker", "unpacking IOP object %*pM",
                  LSTR_FMT_ARG(desc->fullname));
    if (desc->is_union) {
        return unpack_union(mp, desc, value, &ps, copy) ? -1 : 0;
    }
    return unpack_struct(mp, desc, value, &ps, copy);
}

/* This function act like `iop_bunpack` but consume the pstream and doesn't
 * check that the pstream has been fully consumed. This allows to unpack
 * a suite of union.
 *
 * XXX: this function can unpack only union because the struct can't be
 * delimited in a stream.
 * */
int iop_bunpack_multi(mem_pool_t *mp, const iop_struct_t *desc, void *value,
                pstream_t *ps, bool copy)
{
    assert(desc->is_union);

    e_named_trace(5, "iop/c/unpacker", "unpacking IOP union(s) %*pM",
                  LSTR_FMT_ARG(desc->fullname));
    return (unpack_union(mp, desc, value, ps, copy) < 0) ? -1 : 0;
}

/* XXX this function doesn't check the IOP content and trust what it reads
 * XXX: this function can unpack only union because the struct can't be
 * delimited in a stream. */
int iop_bskip(const iop_struct_t *desc, pstream_t *ps)
{
    const iop_field_t *fdesc = desc->fields;
    iop_wire_type_t wt;
    uint32_t tag, u32;
    int ifield;

    assert(desc->is_union);

    PS_WANT(!ps_done(ps));
    /* We get the selected tag in the union */
    PS_CHECK(__get_tag_wt(ps, &tag, &wt));
    /* Repeated fields are forbidden in union */
    PS_WANT(wt != IOP_WIRE_REPEAT);

    ifield = iop_ranges_search(desc->ranges, desc->ranges_len, tag);
    PS_CHECK(ifield);
    fdesc += ifield;

    /* Skip union data */
    switch (wt) {
      case IOP_WIRE_BLK1:
        PS_CHECK(get_uint32(ps, 1, &u32));
        goto read_blk;

      case IOP_WIRE_BLK2:
        PS_CHECK(get_uint32(ps, 2, &u32));
        goto read_blk;

      case IOP_WIRE_BLK4:
        PS_CHECK(get_uint32(ps, 4, &u32));

      read_blk:
        PS_WANT((1 << fdesc->type) & IOP_BLK_OK);
        PS_CHECK(ps_skip(ps, u32)); /* Skip block */
        return 0;

      case IOP_WIRE_INT1:
        PS_WANT((1 << fdesc->type) & IOP_INT_OK);
        PS_CHECK(ps_skip(ps, 1));
        return 0;

      case IOP_WIRE_INT2:
        PS_WANT((1 << fdesc->type) & IOP_INT_OK);
        PS_CHECK(ps_skip(ps, 2));
        return 0;

      case IOP_WIRE_INT4:
        PS_WANT((1 << fdesc->type) & IOP_INT_OK);
        PS_CHECK(ps_skip(ps, 4));
        return 0;

      case IOP_WIRE_QUAD:
        if ((1 << fdesc->type) & IOP_INT_OK) {
            PS_CHECK(ps_skip(ps, 8));
            return 0;
        }
#if __FLOAT_WORD_ORDER != __BYTE_ORDER
        if (fdesc->type == IOP_T_DOUBLE) {
            PS_CHECK(ps_skip(ps, sizeof(double)));
            return 0;
        }
#endif
        PS_WANT((1 << fdesc->type) & IOP_QUAD_OK);
        PS_CHECK(ps_skip(ps, 8));
        return 0;
      default:
        return -1;
    }
}

ssize_t iop_get_field_len(pstream_t ps)
{
    iop_wire_type_t wt;
    uint32_t tag, u32, tag_len, len_len;

    if (ps_done(&ps))
        return 0;
    wt  = IOP_WIRE_FMT(ps.b[0]);
    tag = IOP_TAG(ps.b[0]);
    if (likely(tag < IOP_LONG_TAG(1))) {
        tag_len = 1;
    } else {
        tag_len = 2 + tag - IOP_LONG_TAG(1);
    }
    switch (wt) {
      case IOP_WIRE_BLK1:
        len_len = 1;
        break;
      case IOP_WIRE_BLK2:
        len_len = 2;
        break;
      case IOP_WIRE_BLK4:
        len_len = 4;
        break;
      case IOP_WIRE_REPEAT: /* not supported by this function */
        return -1;
      case IOP_WIRE_INT1:
        return tag_len + 1;
      case IOP_WIRE_INT2:
        return tag_len + 2;
      case IOP_WIRE_INT4:
        return tag_len + 4;
      case IOP_WIRE_QUAD:
        return tag_len + 8;
      default:
        return -1;
    }
    if (ps_skip(&ps, tag_len) < 0)
        return 0;
    if (get_uint32(&ps, len_len, &u32) < 0)
        return 0;
    return tag_len + u32;
}

/*-}}}-*/

iop_struct_t const iop__void__s = {
    .fullname   = LSTR_IMMED("Void"),
    .fields_len = 0,
    .size       = 0,
};
