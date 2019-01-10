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

#ifndef IS_LIB_COMMON_SORT_H
#define IS_LIB_COMMON_SORT_H

#include "core.h"

void    dsort32(uint32_t base[], size_t n);
void    dsort64(uint64_t base[], size_t n);
size_t  uniq32(uint32_t data[], size_t len);
size_t  uniq64(uint64_t data[], size_t len);
size_t  bisect32(uint32_t what, const uint32_t data[], size_t len);
size_t  bisect64(uint64_t what, const uint64_t data[], size_t len);
bool    contains32(uint32_t what, const uint32_t data[], size_t len);
bool    contains64(uint64_t what, const uint64_t data[], size_t len);

#endif
