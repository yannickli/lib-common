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

#include "ztst-swiftc.h"

MODULE_SWIFT(swift_from_c, 12, swiftc, 6)

static int c_from_swift_initialize(void *arg)
{
    e_trace(0, "blih init");
    return 0;
}

static int c_from_swift_shutdown(void)
{
    e_trace(0, "blih shutdown");
    return 0;
}

MODULE_BEGIN(c_from_swift)
MODULE_END()

int main(void)
{
    MODULE_REQUIRE(swift_from_c);
    MODULE_RELEASE(swift_from_c);
    return 0;
}
