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

#include "iop.h"
#include "ic.iop.h"
#include "zchk-iop-ressources.h"

IOP_EXPORT_PACKAGES_COMMON;

IOP_EXPORT_PACKAGES(&ic__pkg);

IOP_DSO_EXPORT_RESSOURCES(str, &z_ressource_str_a, &z_ressource_str_b);
IOP_DSO_EXPORT_RESSOURCES(int, &z_ressources_int_1, &z_ressources_int_2);
