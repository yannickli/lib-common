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

#ifndef IS_IOP_IOPC_IOP_H
#define IS_IOP_IOPC_IOP_H

#include "iopc.h"

#include <lib-common/iopsq.iop.h>
#include <lib-common/iop-priv.h>

/* IOP² - An IOP-based library for IOP generation.
 *
 * Tools for dynamic generation of usable IOP content.
 *
 *  iopsq.Package ── 1 ──→ iopc_pkg_t ── 2,3 ──→ iop_pkg_t
 *
 * 1. Building of an iopc package from an IOP² package. The code is in
 *    iopc-iopsq.c and it works in a similar way as iopc-parser.
 *    The package can refer to types from IOP environment: builtin IOP types
 *    can be used and have the expected pointer value (&foo__bar__s).
 *
 * 2. Resolution of IOP types with iopc-typer.
 *
 * 3. Generation of an IOP C package description. The code is in iopc-lang-c.c
 *    because it does quite the same as the code generator except that it
 *    directly generates the structures. This is the part that uses the
 *    provided memory pool.
 *
 * Limitations:
 *
 *    - Default values: default values exist in iopsq.iop and are correctly
 *    transformed at step 1., but not at step 3 (yet).
 *
 *    - Not supported yet: classes, attributes, modules, interfaces, typedefs,
 *    RPCs, SNMP objects.
 *
 *    - Typedefs from IOPs loaded to the environment cannot be used by
 *    refering to them with "typeName" like for the other types because
 *    typedefs cease to exist in packge descriptions iop_pkg_t, so they are
 *    missing in the IOP environment too.
 *
 *    - Dynamic transformation of IOP syntax (mypackage.iop) into IOP
 *    description without having to create a DSO: already possible but no
 *    helper is provided to do that. The step 3. should be protected against
 *    unsupported features first, because for now, most of them would be
 *    silently ignored.
 *
 *   - No tool to "extract" an IOP² description of an IOP type for now. This
 *   feature could be useful for versioning and migration of IOP objects.
 *
 *   - Sub-packages and multi-package loading.
 */

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

/** Generates an dumb IOP package from a single package elem description.
 *
 * \note Mainly meant to be used for testing.
 */
iop_pkg_t *
mp_iopsq_build_mono_element_pkg(mem_pool_t *nonnull mp,
                                const iop__package_elem__t *nonnull elem,
                                sb_t *nonnull err);

#endif /* IS_IOP_IOPC_IOP_H */
