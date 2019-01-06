/**************************************************************************/
/*                                                                        */
/*  Copyright (C) 2004-2019 INTERSEC SA                                   */
/*                                                                        */
/*  Should you receive a copy of this source code, you must check you     */
/*  have a proper, written authorization of INTERSEC to hold it. If you   */
/*  don't have such an authorization, you must DELETE all source code     */
/*  files in your possession, and inform INTERSEC of the fact you obtain  */
/*  these files. Should you not comply to these terms, you can be         */
/*  prosecuted in the extent permitted by applicable law.                 */
/*                                                                        */
/**************************************************************************/

#ifndef IS_COMPAT_SYS_CAPABILITY_H
#define IS_COMPAT_SYS_CAPABILITY_H

/* on older capability.h those are hidden behind a
 * #if !defined(_BSD_SOURCE) guard but we need that and also need the symbols
 * so ignore the symbols from sys/capability.h when those are visible and
 * inconditionnaly redefine them
 */
#define capset       intersec_diverts_symbol_capset
#define capget       intersec_diverts_symbol_capget
#define capgetp      intersec_diverts_symbol_capgetp
#define capsetp      intersec_diverts_symbol_capsetp
#define _cap_names   intersec_diverts_symbol__cap_names
#include_next <sys/capability.h>
#undef capset
#undef capget
#undef capgetp
#undef capsetp
#undef _cap_names

extern int capset(cap_user_header_t header, cap_user_data_t data);
extern int capget(cap_user_header_t header, const cap_user_data_t data);
extern int capgetp(pid_t pid, cap_t cap_d);
extern int capsetp(pid_t pid, cap_t cap_d);
extern char const *_cap_names[];

#endif
