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

#ifndef IS_IOP_IOPC_IOP_H
#define IS_IOP_IOPC_IOP_H

#include "iopc.h"

#include <lib-common/iopsq.iop.h>
#include <lib-common/iop-priv.h>

/** Generates an IOP package description from its IOP version.
 *
 * \warning This function can use elements from current IOP environment
 * (referenced by full type name), so the environment should *not* be updated
 * during the lifetime of an IOP description obtained with this function.
 *
 * \param[in,out] mp        Memory pool to use for any needed allocation
 *                          (should be a frame-based pool).
 *
 * \param[in]     pkg_desc  IOP description of the package.
 *
 * \param[out]    err       Error buffer.
 */
iop_pkg_t *mp_iopsq_build_pkg(mem_pool_t *nonnull mp,
                              const iopsq__package__t *nonnull pkg_desc,
                              sb_t *nonnull err);

/** Generates an IOP struct or union description from its IOP version.
 *
 * \warning Same as for \ref mp_iop_pkg_from_desc.
 *
 * \param[in,out] mp        Memory pool to use for any needed allocation
 *                          (should be a frame-based pool).
 *
 * \param[in]     st_desc   IOP description of the struct/union.
 *
 * \param[out]    err       Error buffer.
 */
const iop_struct_t *
mp_iopsq_build_struct(mem_pool_t *nonnull mp,
                      const iopsq__structure__t *nonnull st_desc,
                      sb_t *nonnull err);

#endif /* IS_IOP_IOPC_IOP_H */
