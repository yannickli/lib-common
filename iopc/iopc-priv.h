/**************************************************************************/
/*                                                                        */
/*  Copyright (C) 2004-2019 INTERSEC SA                                   */
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

/** Check that the name does not contain a character that is already a keyword
 * in some programmation language.
 */
int iopc_check_name(lstr_t name, qv_t(iopc_attr) *nullable attrs,
                    sb_t *nonnull err);

/** Check for type incompatibilities in an IOPC field. */
int iopc_check_field_type(const iopc_field_t *f, sb_t *err);

/* }}} */
/* {{{ Typer. */

bool iopc_field_type_is_class(const iopc_field_t *f);

/* }}} */
/* {{{ C Language */

/** Create an 'iop_pkg_t' from an 'iopc_pkg_t'.
 *
 * \warning The types must be resolved by the typer first.
 *
 * \param[in,out] mp  The memory pool for all needed allocations. Must be a
 *                    by-frame memory pool (flag MEM_BY_FRAME set).
 */
iop_pkg_t *mp_iopc_pkg_to_desc(mem_pool_t *mp, iopc_pkg_t *pkg, sb_t *err);

/* }}} */

#endif /* IS_IOP_IOPC_PRIV_H */
