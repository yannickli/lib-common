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

#include <lib-common/iop.h>
#include <lib-common/iop-rpc.h>
#include <lib-common/core.iop.h>

#include "test.iop.h"
#include "tst1.iop.h"
#include "testvoid.iop.h"

IOP_EXPORT_PACKAGES_COMMON;

IOP_EXPORT_PACKAGES(&test__pkg, &tst1__pkg, &ic__pkg, &core__pkg,
                    &testvoid__pkg);
