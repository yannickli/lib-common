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

#include "core.h"

static struct {
    void (*init[32])(void);
    void (*exit[32])(void);
    int initlen;
    int exitlen;
} core_thread_g;
#define _G  core_thread_g

void intersec_phtread_init(void (*fn)(void))
{
    assert (_G.initlen < countof(_G.init));
    _G.init[_G.initlen++] = fn;
}

void intersec_phtread_exit(void (*fn)(void))
{
    assert (_G.exitlen < countof(_G.exit));
    _G.exit[_G.exitlen++] = fn;
}

void intersec_thread_on_init(void)
{
    for (int i = 0; i < _G.initlen; i++)
        _G.init[i]();
}

void intersec_thread_on_exit(void *unused)
{
    for (int i = 0; i < _G.exitlen; i++)
        _G.exit[i]();
}

