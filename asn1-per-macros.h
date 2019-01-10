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

#ifndef IS_LIB_INET_ASN1_PER_MACROS_H
#define IS_LIB_INET_ASN1_PER_MACROS_H

#include "asn1.h"

static inline asn1_field_t
*asn1_desc_get_last_field(asn1_desc_t *desc)
{
    int pos = desc->vec.len - 1;

    if (pos < 0) {
        return NULL;
    }

    return &desc->vec.tab[pos];
}

static inline asn1_field_t
*asn1_desc_get_int_field(asn1_desc_t *desc)
{
    asn1_field_t *field = asn1_desc_get_last_field(desc);

    if (field == NULL) {
        e_panic("no field to set min/max");
    }

    if (field->type < ASN1_OBJ_TYPE(int8_t)
    ||  field->type > ASN1_OBJ_TYPE(uint64_t))
    {
        e_panic("field `%s:%s' is not an number field",
                field->name, field->oc_t_name);
    }

    return field;
}

static inline void
asn1_set_int_min(asn1_desc_t *desc, int64_t min)
{
    asn1_field_t *field = asn1_desc_get_int_field(desc);

    field->int_info.min = min;
    asn1_int_info_update(&field->int_info);
}

static inline void
asn1_set_int_max(asn1_desc_t *desc, int64_t max)
{
    asn1_field_t *field = asn1_desc_get_int_field(desc);

    field->int_info.max = max;
    asn1_int_info_update(&field->int_info);
}

static inline void
asn1_set_int_min_max(asn1_desc_t *desc, int64_t min, int64_t max)
{
    asn1_set_int_min(desc, min);
    asn1_set_int_max(desc, max);
}

static inline void
asn1_int_set_extended(asn1_desc_t *desc)
{
    asn1_field_t *field = asn1_desc_get_int_field(desc);

    field->int_info.extended = true;
}

static inline asn1_field_t
*asn1_desc_get_str_field(asn1_desc_t *desc)
{
    asn1_field_t *field = asn1_desc_get_last_field(desc);

    if (field == NULL) {
        e_panic("no field to set min/max");
    }

    if (field->type != ASN1_OBJ_TYPE(lstr_t)
    &&  field->type != ASN1_OBJ_TYPE(asn1_bit_string_t))
    {
        e_panic("field `%s:%s' is not an string field",
                field->name, field->oc_t_name);
    }

    return field;
}

static inline void
asn1_set_str_min(asn1_desc_t *desc, size_t min)
{
    asn1_field_t *field = asn1_desc_get_str_field(desc);

    field->str_info.min = min;
}

static inline void
asn1_set_str_max(asn1_desc_t *desc, size_t max)
{
    asn1_field_t *field = asn1_desc_get_str_field(desc);

    field->str_info.max = max;
}

static inline void
asn1_set_str_min_max(asn1_desc_t *desc, size_t min, size_t max)
{
    asn1_set_str_min(desc, min);
    asn1_set_str_max(desc, max);
}

static inline void
asn1_str_set_extended(asn1_desc_t *desc)
{
    asn1_field_t *field = asn1_desc_get_str_field(desc);

    field->str_info.extended = true;
}

static inline asn1_field_t
*asn1_desc_get_seq_of_field(asn1_desc_t *desc)
{
    asn1_field_t *field = asn1_desc_get_last_field(desc);

    if (field == NULL)
        e_panic("no field to set min/max");

    if (field->type != ASN1_OBJ_TYPE(SEQUENCE)) {
        e_panic("field `%s:%s' is not a SEQUENCE field",
                field->name, field->oc_t_name);
    }

    if (!field->u.comp->is_seq_of) {
        e_panic("field into `%s:%s' is not a SEQUENCE OF field",
                field->name, field->oc_t_name);
    }

    return field;
}

static inline void
asn1_set_seq_of_min(asn1_desc_t *desc, size_t min)
{
    asn1_field_t *field = asn1_desc_get_seq_of_field(desc);

    field->seq_of_info.min = min;
}

static inline void
asn1_set_seq_of_max(asn1_desc_t *desc, size_t max)
{
    asn1_field_t *field = asn1_desc_get_seq_of_field(desc);

    field->seq_of_info.max = max;
}

static inline void
asn1_set_seq_of_min_max(asn1_desc_t *desc, size_t min, size_t max)
{
    asn1_set_seq_of_min(desc, min);
    asn1_set_seq_of_max(desc, max);
}

static inline void
asn1_seq_of_set_extended(asn1_desc_t *desc)
{
    asn1_field_t *field = asn1_desc_get_seq_of_field(desc);

    field->seq_of_info.extended = true;
}

#define ASN1_ENUM(pfx)  asn1_##pfx##_enum
#define ASN1_GET_ENUM(pfx)  ASN1_ENUM(pfx)()

#define ASN1_ENUM_BEGIN(pfx)  \
    const asn1_enum_info_t *ASN1_ENUM(pfx)(void)                          \
    {                                                                     \
        static asn1_enum_info_t *info = NULL;                             \
                                                                          \
        if (unlikely(!info)) {                                            \
            info = asn1_enum_info_new();

#define ASN1_ENUM_END()  \
        }                                                                 \
                                                                          \
        return info;                                                      \
    }
#endif

#define asn1_enum_reg_val(val)  \
            asn1_enum_append(info, val);

#define asn1_enum_reg_extension()  \
            info->extended = true;

#define asn1_reg_extension(desc)  \
            desc->extended = true;

static inline void
asn1_set_enum_info(asn1_field_t *field, const asn1_enum_info_t *info)
{
    if (!field) {
        e_panic("no field into desc");
    }

    if (field->type != ASN1_OBJ_TYPE(enum)) {
        e_panic("%s:%s is not an enum field",
                field->name, field->oc_t_name);
    }

    if (field->enum_info) {
        e_panic("cannot set enum info for %s:%s - info already set",
                field->name, field->oc_t_name);
    }

    field->enum_info = info;
}

#define asn1_set_enum_info(desc, pfx)                                     \
{                                                                         \
    asn1_field_t *field = asn1_desc_get_last_field(desc);                 \
                                                                          \
    asn1_set_enum_info(field, ASN1_GET_ENUM(pfx));                        \
}

#define ASN1_SEQ_OF_DESC_BEGIN(desc, pfx)                                 \
    ASN1_DESC_BEGIN(desc, pfx);                                           \
    desc->is_seq_of = true;

#define ASN1_SEQ_OF_DESC_END(desc)                                        \
    assert (desc->is_seq_of);                                             \
    assert (desc->vec.len == 1);                                          \
    assert (desc->vec.tab[0].mode == ASN1_OBJ_MODE(SEQ_OF));              \
    ASN1_DESC_END(desc)

#define asn1_reg_seq_of(...)         asn1_reg_sequence(__VA_ARGS__)
#define asn1_reg_opt_seq_of(...)     asn1_reg_opt_sequence(__VA_ARGS__)
#define asn1_reg_seq_of_seq_of(...)  asn1_reg_seq_of_sequence(__VA_ARGS__)

static inline void
asn1_set_open_type(asn1_desc_t *desc, size_t buf_len)
{
    asn1_field_t *field = asn1_desc_get_last_field(desc);

    if (!buf_len) {
        e_panic("buffer length must be > 0");
    }

    if (!field) {
        e_panic("no field into desc");
    }

    if (field->is_open_type) {
        e_panic("cannot set open type for %s:%s - already set",
                field->name, field->oc_t_name);
    }

    field->is_open_type = true;
    field->open_type_buf_len = buf_len;
}
