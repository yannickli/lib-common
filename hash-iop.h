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

#if !defined(IS_LIB_COMMON_HASH_H) || defined(IS_LIB_COMMON_HASH_IOP_H)
#  error "you must include <lib-common/hash.h> instead"
#else
#define IS_LIB_COMMON_HASH_IOP_H

struct iop_struct_t;

typedef void (iop_hash_f)(void * nonnull ctx, const void * nonnull input,
                          int ilen);

enum {
    IOP_HASH_SKIP_MISSING = 1 << 0, /* Skip missing optional fields         */
    IOP_HASH_SKIP_DEFAULT = 1 << 1, /* Skip fields having the default value */
    IOP_HASH_SHALLOW_DEFAULT = 1 << 2, /* Compare pointers, not content of
                                          string to detect default values */
    IOP_HASH_DONT_INCLUDE_CLASS_ID = 1 << 3, /* Do not take the class id into
                                                account when hashing a class
                                              */
};

#define ATTRS
#define F(x)  x
#include "hash-iop.in.h"
#undef F
#undef ATTRS

#endif
