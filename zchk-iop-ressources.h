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

#ifndef IS_ZCHK_IOP_RESSOURCES_H
#define IS_ZCHK_IOP_RESSOURCES_H

#include "iop.h"

IOP_DSO_DECLARE_RESSOURCE_CATEGORY(str, const char *);
IOP_DSO_DECLARE_RESSOURCE_CATEGORY(int, int);

extern const char *z_ressource_str_a;
extern const char *z_ressource_str_b;

extern const int z_ressources_int_1;
extern const int z_ressources_int_2;

#endif
