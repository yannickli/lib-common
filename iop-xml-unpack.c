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

#include "xmlr.h"
#include "iop.h"
#include "iop-helpers.inl.c"

#define XML_CNAME(xr) \
    (const char *)xmlTextReaderConstLocalName(xr)

static int
xunpack_struct(xml_reader_t, mem_pool_t *, const iop_struct_t *, void *);
static int
xunpack_union(xml_reader_t, mem_pool_t *, const iop_struct_t *, void *);

static int parse_int(xml_reader_t xr, const char *s, int64_t *i64p)
{
    errno = 0;
    *i64p = strtoll(s, &s, 10);
    if (unlikely(skipspaces(s)[0] || errno))
        return xmlr_fail(xr, "node value is not a valid integer");
    return 0;
}

static int xunpack_value(xml_reader_t xr, mem_pool_t *mp,
                         const iop_field_t *fdesc, void *v)
{
    int64_t intval = 0;
    uint64_t uintval = 0;
    const char *xval;
    bool found = false;
    int len;

    switch (fdesc->type) {
      case IOP_T_I8: case IOP_T_U8:
        RETHROW(xmlr_get_i64(xr, &intval));
        *(uint8_t *)v = intval;
        break;
      case IOP_T_I16: case IOP_T_U16:
        RETHROW(xmlr_get_i64(xr, &intval));
        *(uint16_t *)v = intval;
        break;
      case IOP_T_ENUM:
        RETHROW(xmlr_get_cstr_start(xr, false, &xval, &len));

        /* Try to unpack the string value */
        intval = iop_enum_from_str2(fdesc->u1.en_desc, xval, len, &found);
        if (!found)
            RETHROW(parse_int(xr, xval, &intval));
        *(uint32_t *)v = intval;
        RETHROW(xmlr_get_cstr_done(xr));
        break;
      case IOP_T_I32: case IOP_T_U32:
        RETHROW(xmlr_get_i64(xr, &intval));
        *(uint32_t *)v = intval;
        break;
      case IOP_T_I64:
        RETHROW(xmlr_get_i64(xr, &intval));
        *(uint64_t *)v = intval;
        break;
      case IOP_T_U64:
        RETHROW(xmlr_get_u64(xr, &uintval));
        *(uint64_t *)v = uintval;
        break;
      case IOP_T_BOOL:
        RETHROW(xmlr_get_bool(xr, (bool *)v));
        break;
      case IOP_T_DOUBLE:
        RETHROW(xmlr_get_dbl(xr, (double *)v));
        break;
      case IOP_T_STRING:
        {
            clstr_t *str = v;

            RETHROW(xmlr_get_cstr_start(xr, true, &str->s, &str->len));
            str->s = mp_dupz(mp, str->s, str->len);
            RETHROW(xmlr_get_cstr_done(xr));
        }
        break;
      case IOP_T_DATA:
        {
            clstr_t *str = v;

            RETHROW(xmlr_get_cstr_start(xr, true, &str->s, &str->len));
            if (str->len == 0) {
                str->s = mp_new(mp, char , 1);
            } else {
                sb_t  sb;
                int   blen = DIV_ROUND_UP(str->len * 3, 4);
                char *buf  = mp_new_raw(mp, char, blen + 1);

                sb_init_full(&sb, buf, 0, blen + 1, MEM_OTHER);
                if (sb_add_unb64(&sb, str->s, str->len))
                    return xmlr_fail(xr, "value isn't valid base64");
                str->s   = buf;
                str->len = sb.len;
            }
            RETHROW(xmlr_get_cstr_done(xr));
        }
        break;
      case IOP_T_XML:
        {
            clstr_t *str = v;
            char *xres;

            /* Get xml block */
            RETHROW(xmlr_get_inner_xml(xr, &xres, &str->len));
            str->s = mp_dupz(mp, xres, str->len);
            free(xres); /* Free the stupid string allocated by the libxml */
            return 0;
        }
        break;
      case IOP_T_UNION:
        return xunpack_union(xr, mp, fdesc->u1.st_desc, v);
      case IOP_T_STRUCT:
        return xunpack_struct(xr, mp, fdesc->u1.st_desc, v);
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
    iop_data_t *data = v;
    int bufsize = 0, datasize = fdesc->size;
    int len;

    do {
        int64_t  intval  = 0;
        uint64_t uintval = 0;
        const char *xval;
        bool found = false;

        if (datasize >= bufsize) {
            int size   = p_alloc_nr(bufsize);
            data->data = mp->realloc(mp, data->data, bufsize, size, MEM_RAW);
            bufsize    = size;
        }

        switch (fdesc->type) {
          case IOP_T_I8: case IOP_T_U8:
            RETHROW(xmlr_get_i64(xr, &intval));
            ((uint8_t *)data->data)[data->len] = intval;
            break;

          case IOP_T_I16: case IOP_T_U16:
            RETHROW(xmlr_get_i64(xr, &intval));
            ((uint16_t *)data->data)[data->len] = intval;
            break;

          case IOP_T_ENUM:
            RETHROW(xmlr_get_cstr_start(xr, false, &xval, &len));

            /* Try to unpack the string value */
            intval = iop_enum_from_str2(fdesc->u1.en_desc, xval, -1, &found);
            if (!found)
                RETHROW(parse_int(xr, xval, &intval));
            ((uint32_t *)data->data)[data->len] = intval;
            RETHROW(xmlr_get_cstr_done(xr));
            break;

          case IOP_T_I32: case IOP_T_U32:
            RETHROW(xmlr_get_i64(xr, &intval));
            ((uint32_t *)data->data)[data->len] = intval;
            break;

          case IOP_T_I64:
            RETHROW(xmlr_get_i64(xr, &intval));
            ((uint64_t *)data->data)[data->len] = intval;
            break;

          case IOP_T_U64:
            RETHROW(xmlr_get_u64(xr, &uintval));
            ((uint64_t *)data->data)[data->len] = uintval;
            break;

          case IOP_T_BOOL:
            RETHROW(xmlr_get_bool(xr, (bool *)data->data + data->len));
            break;

          case IOP_T_DOUBLE:
            RETHROW(xmlr_get_dbl(xr, (double *)data->data + data->len));
            break;

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
                             const iop_field_t *fdesc, void *v)
{
    iop_data_t *data = v;
    void **prev = NULL, **chain, *ptr;
    int n = 0;

    do {
        ptr = mp_new(mp, char, fdesc->size);

        RETHROW(xunpack_value(xr, mp, fdesc, ptr));
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
               void *value)
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
        void *v;

        if (unlikely(fdesc == end))
            return xmlr_fail(xr, "expecting closing tag");

        /* Find the field description by the tag name */
        xfdesc = get_field_by_name(desc, fdesc, XML_CNAME(xr));
        if (unlikely(!xfdesc || xfdesc->tag < fdesc->tag))
            return xmlr_fail(xr, "unknown tag <%s>", XML_CNAME(xr));

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
            if ((1 << fdesc->type) & IOP_BLK_OK) {
                RETHROW(xunpack_block_vec(xr, mp, fdesc, v));
            } else {
                RETHROW(xunpack_scalar_vec(xr, mp, fdesc, v));
            }
            fdesc++;
            continue;
        } else
        if (fdesc->repeat == IOP_R_OPTIONAL) {
            v = iop_value_set_here(mp, fdesc, v);
        }

        RETHROW(xunpack_value(xr, mp, fdesc, v));
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
              void *value)
{
    const iop_field_t *fdesc;

    RETHROW(xmlr_next_child(xr));
    fdesc = get_field_by_name(desc, desc->fields, XML_CNAME(xr));
    if (unlikely(!fdesc))
        return xmlr_fail(xr, "unknown tag <%s>", XML_CNAME(xr));

    /* Write the selected tag */
    *((uint16_t *)value) = fdesc->tag;
    RETHROW(xunpack_value(xr, mp, fdesc, (char *)value + fdesc->data_offs));
    return xmlr_node_close(xr);
}

/* Unpack an iop structure from XML.
 * The next element to be read have to be the first elementof the structure to
 * unpack.
 * This function consume the end element just after what it has to unpack.
 */
int iop_xunpack(void *xr, mem_pool_t *mp, const iop_struct_t *desc,
                void *value)
{
    if (desc->is_union) {
        return xunpack_union(xr, mp, desc, value);
    }
    return xunpack_struct(xr, mp, desc, value);
}
