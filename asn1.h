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

#ifndef IS_LIB_COMMON_ASN1_H
#define IS_LIB_COMMON_ASN1_H

#include <lib-common/arith.h>
#include <lib-common/container.h>
#include "asn1-macros.h"
#include "asn1-writer.h"

#define ASN1_TAG_INVALID   0xFFFFFFFF

enum asn1_tag_class {
    ASN1_TAG_CLASS_UNIVERSAL,
    ASN1_TAG_CLASS_APPLICATION,
    ASN1_TAG_CLASS_CONTEXT_SPECIFIC,
    ASN1_TAG_CLASS_PRIVATE
};

#define ASN1_MK_TAG(class, value)  ((ASN1_TAG_CLASS_##class << 6) | (value))

#define ASN1_TAG_CONSTRUCTED(tag) ((1 << 5) | (tag))

/* Tag for constructed values. */
#define ASN1_MK_TAG_C(class, value) \
    ASN1_TAG_CONSTRUCTED(ASN1_MK_TAG(class, value))

/* Tag list on :
 * http://www.obj-sys.com/asn1tutorial/node124.html */
#define ASN1_TAG_BOOLEAN           ASN1_MK_TAG(UNIVERSAL, 1)
#define ASN1_TAG_INTEGER           ASN1_MK_TAG(UNIVERSAL, 2)
#define ASN1_TAG_BIT_STRING        ASN1_MK_TAG(UNIVERSAL, 3)
#define ASN1_TAG_OCTET_STRING      ASN1_MK_TAG(UNIVERSAL, 4)
#define ASN1_TAG_NULL              ASN1_MK_TAG(UNIVERSAL, 5)
#define ASN1_TAG_OBJECT_ID         ASN1_MK_TAG(UNIVERSAL, 6)
#define ASN1_TAG_OBJECT_DECRIPTOR  ASN1_MK_TAG(UNIVERSAL, 7)
#define ASN1_TAG_EXTERNAL          ASN1_MK_TAG(UNIVERSAL, 8)
#define ASN1_TAG_REAL              ASN1_MK_TAG(UNIVERSAL, 9)
#define ASN1_TAG_ENUMERATED        ASN1_MK_TAG(UNIVERSAL, 10)
#define ASN1_TAG_EMBEDDED_PDV      ASN1_MK_TAG(UNIVERSAL, 11)
#define ASN1_TAG_UTF8_STRING       ASN1_MK_TAG(UNIVERSAL, 12)
#define ASN1_TAG_RELATIVE_OID      ASN1_MK_TAG(UNIVERSAL, 13)

#define ASN1_TAG_SEQUENCE          ASN1_MK_TAG(UNIVERSAL, 16)
#define ASN1_TAG_SET               ASN1_MK_TAG(UNIVERSAL, 17)
#define ASN1_TAG_NUMERIC_STRING    ASN1_MK_TAG(UNIVERSAL, 18)
#define ASN1_TAG_PRINTABLE_STRING  ASN1_MK_TAG(UNIVERSAL, 19)
#define ASN1_TAG_TELETEX_STRING    ASN1_MK_TAG(UNIVERSAL, 20)
#define ASN1_TAG_VIDEOTEX_STRING   ASN1_MK_TAG(UNIVERSAL, 21)
#define ASN1_TAG_IA5STRING         ASN1_MK_TAG(UNIVERSAL, 22)
#define ASN1_TAG_UTC_TIME          ASN1_MK_TAG(UNIVERSAL, 23)
#define ASN1_TAG_GENERALIZED_TIME  ASN1_MK_TAG(UNIVERSAL, 24)
#define ASN1_TAG_GRAPHIC_STRING    ASN1_MK_TAG(UNIVERSAL, 25)
#define ASN1_TAG_VISIBLE_STRING    ASN1_MK_TAG(UNIVERSAL, 26)
#define ASN1_TAG_GENERAL_STRING    ASN1_MK_TAG(UNIVERSAL, 27)
#define ASN1_TAG_UNIVERSAL_STRING  ASN1_MK_TAG(UNIVERSAL, 28)
#define ASN1_TAG_CHARACTER_STRING  ASN1_MK_TAG(UNIVERSAL, 29)
#define ASN1_TAG_BMP_STRING        ASN1_MK_TAG(UNIVERSAL, 30)

#define ASN1_TAG_EXTERNAL_C        ASN1_MK_TAG_C(UNIVERSAL, 8)
#define ASN1_TAG_SEQUENCE_C        ASN1_MK_TAG_C(UNIVERSAL, 16)
#define ASN1_TAG_SET_C             ASN1_MK_TAG_C(UNIVERSAL, 17)

int ber_decode_len32(pstream_t *ps, uint32_t *_len);

int ber_decode_int16(pstream_t *ps, int16_t *_val);
int ber_decode_int32(pstream_t *ps, int32_t *_val);
int ber_decode_int64(pstream_t *ps, int64_t *_val);
int ber_decode_uint16(pstream_t *ps, uint16_t *_val);
int ber_decode_uint32(pstream_t *ps, uint32_t *_val);
int ber_decode_uint64(pstream_t *ps, uint64_t *_val);

/* Return number of bytes read, or -1 on error */
int ber_decode_oid(const byte *p, int size, int *oid, int size_oid);

int ber_decode_bit_string_len(pstream_t *ps);

#endif
