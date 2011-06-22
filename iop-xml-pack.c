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

#include <math.h>
#include "iop.h"
#include "iop-helpers.inl.c"

static void xpack_struct(sb_t *, const iop_struct_t *, const void *, bool, bool);
static void xpack_union(sb_t *, const iop_struct_t *, const void *, bool, bool);

static void
xpack_value(sb_t *sb, const iop_field_t *f, const void *v, bool verbose,
            bool wenums)
{
    static clstr_t const types[] = {
        [IOP_T_I8]     = CLSTR_IMMED(" xsi:type=\"xsd:byte\">"),
        [IOP_T_U8]     = CLSTR_IMMED(" xsi:type=\"xsd:unsignedByte\">"),
        [IOP_T_I16]    = CLSTR_IMMED(" xsi:type=\"xsd:short\">"),
        [IOP_T_U16]    = CLSTR_IMMED(" xsi:type=\"xsd:unsignedShort\">"),
        [IOP_T_I32]    = CLSTR_IMMED(" xsi:type=\"xsd:int\">"),
        [IOP_T_ENUM]   = CLSTR_IMMED(" xsi:type=\"xsd:int\">"),
        [IOP_T_U32]    = CLSTR_IMMED(" xsi:type=\"xsd:unsignedInt\">"),
        [IOP_T_I64]    = CLSTR_IMMED(" xsi:type=\"xsd:long\">"),
        [IOP_T_U64]    = CLSTR_IMMED(" xsi:type=\"xsd:unsignedLong\">"),
        [IOP_T_BOOL]   = CLSTR_IMMED(" xsi:type=\"xsd:boolean\">"),
        [IOP_T_DOUBLE] = CLSTR_IMMED(" xsi:type=\"xsd:double\">"),
        [IOP_T_STRING] = CLSTR_IMMED(" xsi:type=\"xsd:string\">"),
        [IOP_T_DATA]   = CLSTR_IMMED(" xsi:type=\"xsd:base64Binary\">"),
        [IOP_T_XML]    = CLSTR_IMMED(">"),
        [IOP_T_UNION]  = CLSTR_IMMED(">"),
        [IOP_T_STRUCT] = CLSTR_IMMED(">"),
    };
    const clstr_t *s;

    sb_grow(sb, 64 + f->name.len * 2);
    sb_addc(sb, '<');
    sb_add(sb, f->name.s, f->name.len);
    if (verbose && !(wenums && f->type == IOP_T_ENUM)) {
        sb_add(sb, types[f->type].s, types[f->type].len);
    } else {
        sb_addc(sb, '>');
    }

    switch (f->type) {
        double d;

      case IOP_T_I8:     sb_addf(sb, "%i",      *(  int8_t *)v); break;
      case IOP_T_U8:     sb_addf(sb, "%u",      *( uint8_t *)v); break;
      case IOP_T_I16:    sb_addf(sb, "%i",      *( int16_t *)v); break;
      case IOP_T_U16:    sb_addf(sb, "%u",      *(uint16_t *)v); break;
      case IOP_T_I32:    sb_addf(sb, "%i",      *( int32_t *)v); break;
      case IOP_T_ENUM:
        if (!wenums) {
            sb_addf(sb, "%i",      *( int32_t *)v);
        } else {
            clstr_t str = iop_enum_to_str(f->u1.en_desc, *(int32_t *)v);
            if (!str.s) {
                sb_addf(sb, "%i",      *( int32_t *)v);
            } else {
                sb_add(sb, str.s, str.len);
            }
        }
        break;
      case IOP_T_U32:    sb_addf(sb, "%u",      *(uint32_t *)v); break;
      case IOP_T_I64:    sb_addf(sb, "%jd",     *( int64_t *)v); break;
      case IOP_T_U64:    sb_addf(sb, "%ju",     *(uint64_t *)v); break;
      case IOP_T_DOUBLE:
        d = *(double *)v;
        if (isinf(d)) {
            sb_adds(sb, d < 0 ? "-INF" : "INF");
        } else {
            sb_addf(sb, "%.17e", d);
        }
        break;
      case IOP_T_BOOL:
        if (*(bool *)v) {
            sb_addc(sb, '1');
        } else {
            sb_addc(sb, '0');
        }
        break;
      case IOP_T_STRING:
        s = v;
        sb_add_xmlescape(sb, s->s, s->len);
        break;
      case IOP_T_DATA:
        s = v;
        if (s->len)
            sb_add_b64(sb, s->s, s->len, -1);
        break;
      case IOP_T_XML:
        s = v;
        sb_add(sb, s->s, s->len);
        break;
      case IOP_T_UNION:
        xpack_union(sb, f->u1.st_desc, v, verbose, wenums);
        break;
      case IOP_T_STRUCT:
      default:
        xpack_struct(sb, f->u1.st_desc, v, verbose, wenums);
        break;
    }
    sb_adds(sb, "</");
    sb_add(sb, f->name.s, f->name.len);
    sb_addc(sb, '>');
}

static void
xpack_struct(sb_t *sb, const iop_struct_t *desc, const void *v,
             bool verbose, bool wenums)
{
    for (int i = 0; i < desc->fields_len; i++) {
        const iop_field_t *f = desc->fields + i;
        const void *ptr = (char *)v + f->data_offs;
        int len = 1;

        if (f->repeat == IOP_R_OPTIONAL) {
            if (!iop_value_has(f, ptr))
                continue;
            if ((1 << f->type) & IOP_STRUCTS_OK)
                ptr = *(void **)ptr;
        }
        if (f->repeat == IOP_R_REPEATED) {
            const iop_data_t *data = ptr;
            len = data->len;
            ptr = data->data;
        }

        while (len-- > 0) {
            xpack_value(sb, f, ptr, verbose, wenums);
            ptr = (const char *)ptr + f->size;
        }
    }
}

static void
xpack_union(sb_t *sb, const iop_struct_t *desc, const void *v, bool verbose,
            bool wenums)
{
    const iop_field_t *f = get_union_field(desc, v);

    xpack_value(sb, f, (char *)v + f->data_offs, verbose, wenums);
}

void iop_xpack(sb_t *sb, const iop_struct_t *desc, const void *v, bool verbose,
               bool with_enums)
{
    if (desc->is_union) {
        xpack_union(sb, desc, v, verbose, with_enums);
    } else {
        xpack_struct(sb, desc, v, verbose, with_enums);
    }
}
