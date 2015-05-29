/**************************************************************************/
/*                                                                        */
/*  Copyright (C) 2004-2015 INTERSEC SA                                   */
/*                                                                        */
/*  Should you receive a copy of this source code, you must check you     */
/*  have a proper, written authorization of INTERSEC to hold it. If you   */
/*  don't have such an authorization, you must DELETE all source code     */
/*  files in your possession, and inform INTERSEC of the fact you obtain  */
/*  these files. Should you not comply to these terms, you can be         */
/*  prosecuted in the extent permitted by applicable law.                 */
/*                                                                        */
/**************************************************************************/

#include "iop-mib.h"

#define LVL1 "    "
#define LVL2 LVL1 LVL1
#define LVL3 LVL2 LVL1
#define LVL4 LVL2 LVL2
#define LVL5 LVL3 LVL2

/* {{{ Header/Footer */

static void mib_open_banner(sb_t *buf, lstr_t name)
{
    t_scope;
    lstr_t up_name = t_lstr_dup(name);

    lstr_ascii_toupper(&up_name);

    sb_addf(buf,
            "INTERSEC-%*pM-MIB DEFINITIONS ::= BEGIN\n\n"
            "IMPORTS\n"
            LVL1 "MODULE-COMPLIANCE, OBJECT-GROUP, "
            "NOTIFICATION-GROUP FROM SNMPv2-CONF\n"
            LVL1 "MODULE-IDENTITY, OBJECT-TYPE, NOTIFICATION-TYPE, "
            "Integer32 FROM SNMPv2-SMI\n"
            LVL1 "%*pM FROM INTERSEC-MIB;\n\n",
            LSTR_FMT_ARG(up_name), LSTR_FMT_ARG(name));
}

static void mib_close_banner(sb_t *buf)
{
            sb_adds(buf, "\n\n-- vim:syntax=mib\n");
}

/* }}} */

void iop_mib(sb_t *sb, lstr_t name)
{
    mib_open_banner(sb, name);
    mib_close_banner(sb);
}

#undef LVL1
#undef LVL2
#undef LVL3
#undef LVL4
#undef LVL5
