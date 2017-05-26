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

#ifndef IS_LIBCOMMON_SWIFTC
#define IS_LIBCOMMON_SWIFTC

#include "core.h"

MODULE_DECLARE(c_from_swift);
MODULE_DECLARE(swift_from_c);

#define ZSWIFTC_FIELDS(pfx) \
    OBJECT_FIELDS(pfx);     \
    int my_id

#define ZSWIFTC_METHODS(type_t) \
    OBJECT_METHODS(type_t);    \
    void (* nonnull set_id)(type_t * nonnull)

OBJ_CLASS(zswiftc, object, ZSWIFTC_FIELDS, ZSWIFTC_METHODS)

SWIFT_OBJ_DECLARE_SET_INIT_FUNC(zswiftc);

#endif
