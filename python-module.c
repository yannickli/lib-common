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

#include "python-common.h"
#include <lib-common/core.h>

EXPORT
void initcommon(void);

static PyMethodDef methods[] = {
    {
        NULL,
        NULL,
        0,
        NULL
    }
};

void initcommon(void)
{
    python_common_initialize("common", methods);
}
