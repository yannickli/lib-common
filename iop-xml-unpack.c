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

#include "xmlr.h"
#include "iop.h"
#include "iop-helpers.inl.c"

static __thread qm_t(part) *parts_g;

static int
xunpack_struct(xml_reader_t, mem_pool_t *, const iop_struct_t *, void *,
               int flags);
static int
xunpack_union(xml_reader_t, mem_pool_t *, const iop_struct_t *, void *,
              int flags);

static int parse_int(xml_reader_t xr, const char *s, int64_t *i64p)
{
    errno = 0;
    *i64p = strtoll(s, &s, 0);
    if (unlikely(skipspaces(s)[0] || errno))
        return xmlr_fail(xr, "node value is not a valid integer");
    return 0;
}

/* parse the href attribute (attr) which contains a Content-Id and return the
 * associated message part found in parts_g qm.
 * Example: href="cid:12345"
 */
static int get_part_from_href(xml_reader_t xr, xmlAttrPtr *attr, lstr_t *part)
{
    t_scope;

    lstr_t href, cid;
    pstream_t ps;
    int pos;

    if (!parts_g)
        return xmlr_fail(xr, "found href attribute with no message parts");

    if (t_xmlr_getattr_str(xr, *attr, false, &href) < 0)
        return xmlr_fail(xr, "failed to read href");

    ps = ps_initlstr(&href);
    if (ps_skipstr(&ps, "cid:") < 0)
        return xmlr_fail(xr, "failed to parse href");

    cid = LSTR_INIT_V(ps.s, ps_len(&ps));
    pos = qm_find(part, parts_g, &cid);
    if (pos < 0)
        return xmlr_fail(xr, "unknown cid in href");

    *part = parts_g->values[pos];
    return 0;
}

/* unpack a string value, supporting references to message parts */
static int get_text(xml_reader_t xr, mem_pool_t *mp, bool b64, lstr_t *str)
{
    xmlAttrPtr attr;
    lstr_t part;

    if (RETHROW(xmlr_node_is_empty(xr))) {
        /* empty element -> check for a href attribute (SOAP package) */
        if ((attr = xmlr_find_attr_s(xr, "href", false))) {
            RETHROW(get_part_from_href(xr, &attr, &part));
            *str = lstr_dupc(part);
        } else {
            *str = LSTR_EMPTY_V;
        }
        RETHROW(xmlr_next_node(xr));
        return 0;
    }

    RETHROW(xmlr_get_cstr_start(xr, true, str));
    if (str->len == 0) {
        /* no text -> check for an empty Include element with a href
         * attribute (XOP package) */
        if (!RETHROW(xmlr_node_is_closing(xr))) {
            RETHROW(xmlr_node_want_s(xr, "Include"));
            if (!RETHROW(xmlr_node_is_empty(xr))) {
                return xmlr_fail(xr, "Include element must be empty");
            }
            RETHROW_PN((attr = xmlr_find_attr_s(xr, "href", true)));
            RETHROW(get_part_from_href(xr, &attr, &part));
            *str = lstr_dupc(part);
            RETHROW(xmlr_node_close(xr));
        } else {
            *str = LSTR_EMPTY_V;
        }
    } else {
        /* common case: text not empty */
        if (!b64) {
            *str = mp_lstr_dup(mp, *str);
        } else {
            sb_t  sb;
            int   blen = DIV_ROUND_UP(str->len * 3, 4);
            char *buf  = mp_new_raw(mp, char, blen + 1);

            sb_init_full(&sb, buf, 0, blen + 1, MEM_STATIC);
            if (sb_add_unb64(&sb, str->s, str->len)) {
                mp_delete(mp, &buf);
                return xmlr_fail(xr, "value isn't valid base64");
            }
            str->s   = buf;
            str->len = sb.len;
        }
    }
    RETHROW(xmlr_get_cstr_done(xr));
    return 0;
}

static int get_enum_value(xml_reader_t xr, const iop_enum_t *en_desc,
                          int64_t *intval)
{
    lstr_t xval;
    bool found = false;

    RETHROW(xmlr_get_cstr_start(xr, false, &xval));

    /* Try to unpack the string value */
    *intval = iop_enum_from_lstr(en_desc, xval, &found);
    if (!found) {
        RETHROW(parse_int(xr, xval.s, intval));
    }
    RETHROW(xmlr_get_cstr_done(xr));
    return 0;
}

static int xunpack_value(xml_reader_t xr, mem_pool_t *mp,
                         const iop_field_t *fdesc, void *v, int flags)
{
    int64_t intval = 0;
    uint64_t uintval = 0;
    lstr_t *str = NULL;

    switch (fdesc->type) {
#define CHECK_RANGE(_min, _max)  \
        THROW_ERR_IF(intval < _min || intval > _max)

      case IOP_T_I8:
        RETHROW(xmlr_get_i64_base(xr, 0, &intval));
        CHECK_RANGE(INT8_MIN, INT8_MAX);
        *(int8_t *)v = intval;
        break;
      case IOP_T_U8:
        RETHROW(xmlr_get_i64_base(xr, 0, &intval));
        CHECK_RANGE(0, UINT8_MAX);
        *(uint8_t *)v = intval;
        break;
      case IOP_T_I16:
        RETHROW(xmlr_get_i64_base(xr, 0, &intval));
        CHECK_RANGE(INT16_MIN, INT16_MAX);
        *(int16_t *)v = intval;
        break;
      case IOP_T_U16:
        RETHROW(xmlr_get_i64_base(xr, 0, &intval));
        CHECK_RANGE(0, UINT16_MAX);
        *(uint16_t *)v = intval;
        break;
      case IOP_T_I32:
        RETHROW(xmlr_get_i64_base(xr, 0, &intval));
        CHECK_RANGE(INT32_MIN, INT32_MAX);
        *(int32_t *)v = intval;
        break;
      case IOP_T_U32:
        RETHROW(xmlr_get_i64_base(xr, 0, &intval));
        CHECK_RANGE(0, UINT32_MAX);
        *(uint32_t *)v = intval;
        break;
      case IOP_T_I64:
        RETHROW(xmlr_get_i64_base(xr, 0, &intval));
        *(int64_t *)v = intval;
        break;
      case IOP_T_U64:
        RETHROW(xmlr_get_u64_base(xr, 0, &uintval));
        *(uint64_t *)v = uintval;
        break;
      case IOP_T_ENUM:
        RETHROW(get_enum_value(xr, fdesc->u1.en_desc, &intval));
        CHECK_RANGE(INT32_MIN, INT32_MAX);
        *(int32_t *)v = intval;
        break;
      case IOP_T_BOOL:
        RETHROW(xmlr_get_bool(xr, (bool *)v));
        break;
      case IOP_T_DOUBLE:
        RETHROW(xmlr_get_dbl(xr, (double *)v));
        break;
      case IOP_T_STRING:
        str = v;
        RETHROW(get_text(xr, mp, false, str));
        break;
      case IOP_T_DATA:
        str = v;
        RETHROW(get_text(xr, mp, true, str));
        break;
      case IOP_T_XML:
        str = v;
        RETHROW(mp_xmlr_get_inner_xml(mp, xr, str));
        break;
      case IOP_T_UNION:
        return xunpack_union(xr, mp, fdesc->u1.st_desc, v, flags);
      case IOP_T_STRUCT:
        return xunpack_struct(xr, mp, fdesc->u1.st_desc, v, flags);

#undef CHECK_RANGE
      default:
        e_panic("should not happen");
    }

    return 0;
}

/* Unpack a vector of scalar values. Because a scalar value do not recurse in
 * this function we can safely use realloc. */
static int xunpack_scalar_vec(xml_reader_t xr, mem_pool_t *mp,
                              const iop_field_t *fdesc, void *v)
{
    lstr_t *data = v;
    int bufsize = 0, datasize = fdesc->size;

    p_clear(data, 1);

    do {
        int64_t intval = 0;
        uint64_t uintval = 0;

        if (datasize >= bufsize) {
            int size   = p_alloc_nr(bufsize);
            data->data = mp->realloc(mp, data->data, bufsize, size, MEM_RAW);
            bufsize    = size;
        }

        switch (fdesc->type) {
#define CHECK_RANGE(_min, _max)  \
            THROW_ERR_IF(intval < _min || intval > _max)

          case IOP_T_I8:
            RETHROW(xmlr_get_i64_base(xr, 0, &intval));
            CHECK_RANGE(INT8_MIN, INT8_MAX);
            ((int8_t *)data->data)[data->len] = intval;
            break;
          case IOP_T_U8:
            RETHROW(xmlr_get_i64_base(xr, 0, &intval));
            CHECK_RANGE(0, UINT8_MAX);
            ((uint8_t *)data->data)[data->len] = intval;
            break;
          case IOP_T_I16:
            RETHROW(xmlr_get_i64_base(xr, 0, &intval));
            CHECK_RANGE(INT16_MIN, INT16_MAX);
            ((int16_t *)data->data)[data->len] = intval;
            break;
          case IOP_T_U16:
            RETHROW(xmlr_get_i64_base(xr, 0, &intval));
            CHECK_RANGE(0, UINT16_MAX);
            ((uint16_t *)data->data)[data->len] = intval;
            break;
          case IOP_T_I32:
            RETHROW(xmlr_get_i64_base(xr, 0, &intval));
            CHECK_RANGE(INT32_MIN, INT32_MAX);
            ((int32_t *)data->data)[data->len] = intval;
            break;
          case IOP_T_U32:
            RETHROW(xmlr_get_i64_base(xr, 0, &intval));
            CHECK_RANGE(0, UINT32_MAX);
            ((uint32_t *)data->data)[data->len] = intval;
            break;
          case IOP_T_I64:
            RETHROW(xmlr_get_i64_base(xr, 0, &intval));
            ((int64_t *)data->data)[data->len] = intval;
            break;
          case IOP_T_U64:
            RETHROW(xmlr_get_u64_base(xr, 0, &uintval));
            ((uint64_t *)data->data)[data->len] = uintval;
            break;
          case IOP_T_ENUM:
            RETHROW(get_enum_value(xr, fdesc->u1.en_desc, &intval));
            CHECK_RANGE(INT32_MIN, INT32_MAX);
            ((int32_t *)data->data)[data->len] = intval;
            break;
          case IOP_T_BOOL:
            RETHROW(xmlr_get_bool(xr, (bool *)data->data + data->len));
            break;
          case IOP_T_DOUBLE:
            RETHROW(xmlr_get_dbl(xr, (double *)data->data + data->len));
            break;

#undef CHECK_RANGE
          default:
            assert(false);
        }

        data->len++;
        datasize += fdesc->size;

        /* Check for another repeated element */
    } while (RETHROW(xmlr_node_is(xr, fdesc->name.s, fdesc->name.len)));
    return 0;
}

/* Unpack a vector of "block" values (structure | union | data | string).
 * We can't safely use realloc because a block has an unkown length.
 * We use a hack to chain each unpacked value and rebuild a continuous array.
 *
 *  n = 0
 *  foreach value
 *      the value is unpacked on the stack
 *      two pointers are added on the stack (the chain)
 *          [0] point on the last chain
 *          [1] point on the data we've just added
 *      n++
 *  next
 *
 *  then allocates a new buffer of (fdesc->size * n) and read back the chain
 *  to fill the new continous array.
 */
static int xunpack_block_vec(xml_reader_t xr, mem_pool_t *mp,
                             const iop_field_t *fdesc, void *v, int flags)
{
    lstr_t *data = v;
    void **prev = NULL, **chain, *ptr;
    int n = 0;

    do {
        ptr = mp_new(mp, char, fdesc->size);

        RETHROW(xunpack_value(xr, mp, fdesc, ptr, flags));
        n++;

        chain    = mp_new(mp, void *, 2);
        chain[0] = prev;
        chain[1] = ptr;
        prev     = chain;
    } while (RETHROW(xmlr_node_is(xr, fdesc->name.s, fdesc->name.len)));

    /* Now we can rebuild the array of value */
    data->len  = n;
    data->data = mp->malloc(mp, fdesc->size * n, MEM_RAW);
    ptr        = (char *)data->data + (n - 1) * fdesc->size;
    while (n--) {
        memcpy(ptr, chain[1], fdesc->size);
        ptr   = (char *)ptr - fdesc->size;
        chain = chain[0];
    }

    return 0;
}

static int
xunpack_struct(xml_reader_t xr, mem_pool_t *mp, const iop_struct_t *desc,
               void *value, int flags)
{
    const iop_field_t *fdesc = desc->fields;
    const iop_field_t *end   = desc->fields + desc->fields_len;
    int res = xmlr_next_child(xr);

    /* No children */
    if (res == XMLR_NOCHILD)
        goto end;
    if (res < 0)
        return res;
    do {
        const iop_field_t *xfdesc;
        lstr_t name;
        void *v;
        int n = 1;

        if (unlikely(fdesc == end)) {
            if (!(flags & IOP_UNPACK_IGNORE_UNKNOWN))
                return xmlr_fail(xr, "expecting closing tag");
            return xmlr_next_uncle(xr);
        }

        /* Find the field description by the tag name */
        RETHROW(xmlr_node_get_local_name(xr, &name));
        xfdesc = get_field_by_name(desc, fdesc, name.s, name.len);
        if (unlikely(!xfdesc)) {
            if (!(flags & IOP_UNPACK_IGNORE_UNKNOWN))
                return xmlr_fail(xr, "unknown tag <%*pM>", LSTR_FMT_ARG(name));
            do {
                RETHROW(xmlr_next_sibling(xr));
                if (RETHROW(xmlr_node_is_closing(xr)))
                    goto end;
                RETHROW(xmlr_node_get_local_name(xr, &name));
                xfdesc = get_field_by_name(desc, fdesc, name.s, name.len);
            } while (!xfdesc);
        }
        if (unlikely(xfdesc->tag < fdesc->tag)) {
            return xmlr_fail(xr, "unknown tag <%*pM>", LSTR_FMT_ARG(name));
        }

        if (flags & IOP_UNPACK_FORBID_PRIVATE) {
            const iop_field_attrs_t *attrs = iop_field_get_attrs(desc, xfdesc);
            if (attrs && TST_BIT(&attrs->flags, IOP_FIELD_PRIVATE)) {
                return xmlr_fail(xr, "private tag <%*pM>", LSTR_FMT_ARG(name));
            }
        }

        /* Handle optional fields */
        while (unlikely(xfdesc != fdesc)) {
            if (__iop_skip_absent_field_desc(value, fdesc) < 0) {
                return xmlr_fail(xr, "missing mandatory tag <%*pM>",
                                 LSTR_FMT_ARG(fdesc->name));
            }
            fdesc++;
        }

        /* Read field value */
        v = (char *)value + fdesc->data_offs;
        if (fdesc->repeat == IOP_R_REPEATED) {
            lstr_t *data = v;

            if ((1 << fdesc->type) & IOP_BLK_OK) {
                RETHROW(xunpack_block_vec(xr, mp, fdesc, v, flags));
            } else {
                RETHROW(xunpack_scalar_vec(xr, mp, fdesc, v));
            }
            v = data->data;
            n = data->len;
            goto next;
        } else
        if (fdesc->repeat == IOP_R_OPTIONAL) {
            v = iop_value_set_here(mp, fdesc, v);
        }

        RETHROW(xunpack_value(xr, mp, fdesc, v, flags));

      next:
        if (unlikely(iop_field_has_constraints(desc, fdesc))) {
            if (iop_field_check_constraints(desc, fdesc, v, n, false) < 0) {
                return xmlr_fail(xr, "%*pM", LSTR_FMT_ARG(iop_get_err_lstr()));
            }
        }
        fdesc++;
    } while (!xmlr_node_is_closing(xr));

    /* Check for absent fields */
  end:
    for (; fdesc < end; fdesc++) {
        if (__iop_skip_absent_field_desc(value, fdesc) < 0) {
            return xmlr_fail(xr, "missing mandatory tag <%*pM>",
                             LSTR_FMT_ARG(fdesc->name));
        }
    }
    return xmlr_node_close(xr);
}

static int
xunpack_union(xml_reader_t xr, mem_pool_t *mp, const iop_struct_t *desc,
              void *value, int flags)
{
    const iop_field_t *fdesc;
    lstr_t name;

    RETHROW(xmlr_next_child(xr));
    RETHROW(xmlr_node_get_local_name(xr, &name));
    fdesc = get_field_by_name(desc, desc->fields, name.s, name.len);
    if (unlikely(!fdesc))
        return xmlr_fail(xr, "unknown tag <%*pM>", LSTR_FMT_ARG(name));

    /* Write the selected tag */
    *((uint16_t *)value) = fdesc->tag;
    value = (char *)value + fdesc->data_offs;
    RETHROW(xunpack_value(xr, mp, fdesc, value, flags));
    if (unlikely(iop_field_has_constraints(desc, fdesc))) {
        if (iop_field_check_constraints(desc, fdesc, value, 1, false) < 0) {
            return xmlr_fail(xr, "%*pM", LSTR_FMT_ARG(iop_get_err_lstr()));
        }
    }
    return xmlr_node_close(xr);
}

int iop_xunpack_flags(void *xr, mem_pool_t *mp, const iop_struct_t *desc,
                      void *value, int flags)
{
    return iop_xunpack_parts(xr, mp, desc, value, flags, NULL);
}

int iop_xunpack_parts(void *xr, mem_pool_t *mp, const iop_struct_t *desc,
                      void *value, int flags, qm_t(part) *parts)
{
    int ret;

    parts_g = parts;
    if (desc->is_union) {
        ret = xunpack_union(xr, mp, desc, value, flags);
    } else {
        ret = xunpack_struct(xr, mp, desc, value, flags);
    }
    parts_g = NULL;
    return ret;
}
