/**************************************************************************/
/*                                                                        */
/*  Copyright (C) 2004-2017 INTERSEC SA                                   */
/*                                                                        */
/*  Should you receive a copy of this source code, you must check you     */
/*  have a proper, written authorization of INTERSEC to hold it. If you   */
/*  don't have such an authorization, you must DELETE all source code     */
/*  files in your possession, and inform INTERSEC of the fact you obtain  */
/*  these files. Should you not comply to these terms, you can be         */
/*  prosecuted in the extent permitted by applicable law.                 */
/*                                                                        */
/**************************************************************************/

/*
 * Intersec ASN.1 aligned PER encoder/decoder
 *
 * Based on previous ASN.1 library, it uses the same field registration
 * macros. However, some features of the previous ASN.1 library are not
 * supported.
 *
 * New features that were not included into previous library are:
 *     - Constraint support
 *     - Extension support
 *     - Explicit open type support
 *
 *
 *  - Supported native C types:
 *         - int8_t uint8_t int16_t uint16_t int32_t uint32_t int64_t
 *         - enum
 *         - bool
 *         - lstr_t
 *
 *  - Supported ASN.1 types
 *         - INTEGER
 *             - Unconstrained
 *             - Constrained - ex. : (1 .. 42)
 *             - Extended    - ex. : (1 .. 42, ...)
 *                                   (1 .. 42, ... , 0 .. 128)
 *         - BOOLEAN
 *         - OCTET STRING
 *             - Unconstrained
 *             - Constrained - ex. : (SIZE (3))
 *                                   (SIZE (0 .. 10))
 *             - Extended    - ex. : (SIZE (1 .. 4, ...))
 *             - Length >= 16384 NOT supported yet (fragmented form)
 *             - Constraint FROM not supported
 *         - BIT STRING
 *             - Idem
 *         - ENUMERATED
 *             - Full support, extension included
 *             - Needs optimization
 *         - SEQUENCE
 *             - Extensions supproted when encoding and decoding
 *               only if absent.
 *         - CHOICE
 *             - Warning : field order not checked yet (fields must use
 *                         canonical order defined for tag values).
 *                         See [3] 8.6 (p.12).
 *         - SET
 *             - PER encoding of a SET is the same as SEQUENCE encoding
 *             - Make sure you respect canonical ordering when registering
 *               a SET (see [3] 8.6 - p.12).
 *         - OPEN TYPE
 *             - When encountered type is predictible before beginning
 *               decoding, user shall only set the OPEN TYPE flag when
 *               registering the field. If the type is not predicable, then
 *               user shall declare an octet string and then decode its
 *               content later.
 *
 * References :
 *
 *     [1] ITU-T X.691 (02/2002)
 *         data/dev/doc/asn1/per/X.691-0207.pdf
 *     [2] Olivier Dubuisson
 *         ASN.1 Communication entre systèmes hétérogènes
 *         Springer 1999
 *         French version  : Paper book on Seb's desk
 *         English version : data/dev/doc/asn1/ASN1dubuisson.pdf
 *     [3] ITU-T X.680 (07/2002)
 *         data/dev/doc/asn1/X-680-0207.pdf
 *
 * XXX Suspected bug :
 *     We never encode l - 1 (see [2] 20.5). But it seems to be used
 *     only for extension bit-map length.
 */

#ifndef IS_LIB_INET_ASN1_PER_H
#define IS_LIB_INET_ASN1_PER_H

#include "asn1.h"
#include "asn1-per-macros.h"

#define ASN1_MAX_LEN  SIZE_MAX /* FIXME put real ASN1_MAX_LEN instead */

/* TODO optimize */
static inline int asn1_enum_pos(const asn1_enum_info_t *e, int32_t val)
{
    tab_for_each_pos(pos, &e->values) {
        if (e->values.tab[pos] == val) {
            return pos;
        }
    }

    return -1;
}

static inline void asn1_enum_append(asn1_enum_info_t *e, int32_t val)
{
    if (e->values.len) {
        int32_t last = *qv_last(i32, &e->values);

        if (val < last) {
            e_panic("enumeration value `%d` "
                    "should be registered before value `%d`", val, last);
        }

        if (val == last) {
            e_panic("duplicated enumeration value `%d`", val);
        }
    }

    qv_append(i32, &e->values, val);
}

int aper_encode_desc(sb_t *sb, const void *st, const asn1_desc_t *desc);
int t_aper_decode_desc(pstream_t *ps, const asn1_desc_t *desc,
                       flag_t copy, void *st);

#define aper_encode(sb, pfx, st)  \
    ({                                                                       \
        if (!__builtin_types_compatible_p(typeof(st), pfx##_t *)             \
        &&  !__builtin_types_compatible_p(typeof(st), const pfx##_t *))      \
        {                                                                    \
            __error__("ASN.1 PER encoder: `"#st"' type "                     \
                      "is not <"#pfx"_t *>");                                \
        }                                                                    \
        aper_encode_desc(sb, st, ASN1_GET_DESC(pfx));                        \
    })

#endif

#define t_aper_decode(ps, pfx, copy, st)  \
    ({                                                                       \
        if (!__builtin_types_compatible_p(typeof(st), pfx##_t *)) {          \
            __error__("ASN.1 PER decoder: `"#st"' type "                     \
                      "is not <"#pfx"_t *>");                                \
        }                                                                    \
        t_aper_decode_desc(ps, ASN1_GET_DESC(pfx), copy, st);                \
    })

/*
 * The decode_log_level allows to specify the way we want to log (or not)
 * decoding errors:
 *   * < 0 means e_info
 *   * >= 0 means e_trace(level, ...)
 */
void aper_set_decode_log_level(int level);
