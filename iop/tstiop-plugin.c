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

#include <lib-common/iop.h>
#include "tstiop.iop.h"

iop_struct_t const iop__void__s = {
    .fullname   = LSTR_IMMED("Void"),
    .fields_len = 0,
    .size       = 0,
};

IOP_EXPORT_PACKAGES(&tstiop__pkg);
