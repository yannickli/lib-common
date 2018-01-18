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

#ifndef IS_IOP_IOPC_PRIV_H
#define IS_IOP_IOPC_PRIV_H

#include "iopc.h"

/* {{{ IOP Parser */

/** Checks that an IOP tag has an authorized value. */
int iopc_check_tag_value(int tag, sb_t *err);

/** Check that the first character of the token is uppercase.
 *
 * \param[in] tk  Non-empty token.
 */
int iopc_check_upper(lstr_t tk, sb_t *err);

/** Check the correctness of a type name.
 *
 * \param[in] tk  Non-empty alphanumeric type name.
 */
static inline int iopc_check_type_name(lstr_t type_name, sb_t *err)
{
    return iopc_check_upper(type_name, err);
}

/** Check that the name does not contain a character that is already a keyword
 * in some programmation language.
 */
int iopc_check_name(lstr_t name, qv_t(iopc_attr) *nullable attrs,
                    sb_t *nonnull err);

/* }}} */
/* {{{ C Language */

/** Give the extra length for the encoding of an IOP tag. */
static inline int iopc_tag_len(int32_t tag)
{
    /* Tags 0..29 are encoded inline -> 0 byte,
     * Tags 30..255 are encoded as value 30 plus 1 byte,
     * Tags 256..65535 are encoded as value 31 plus 2 extra bytes */
    return (tag >= 30) + (tag >= 256);
}

/** Process the value of the flags associated to an IOP field. */
unsigned iopc_field_build_flags(const iopc_field_t *nonnull f,
                                const iopc_struct_t *nonnull st,
                                const iopc_attrs_t *nullable attrs);

/** Build the range used to associate an IOP tag to a struct field. */
iop_array_i32_t t_iopc_struct_build_ranges(const iopc_struct_t *st);

/** Build the range used to associate an enum value to the associated element.
 */
iop_array_i32_t t_iopc_enum_build_ranges(const iopc_enum_t *en);

/** Calculate sizes and alignments then and optimize the fields order to
 * minimize the structure size.
 */
void iopc_struct_optimize(iopc_struct_t *st);

/** Get the field size and alignment for the C type associated to an IOP
 * scalar type.
 */
void iop_scalar_type_get_size_and_alignment(iop_type_t type,
                                            uint16_t *nullable size,
                                            uint8_t *nullable align);

/** Check for type incompatibilities in an IOPC field. */
int iopc_check_field_type(const iopc_field_t *f, sb_t *err);

/* }}} */

#endif /* IS_IOP_IOPC_PRIV_H */
