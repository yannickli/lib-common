/**************************************************************************/
/*                                                                        */
/*  Copyright (C) 2006-2007 INTERSEC SAS                                  */
/*                                                                        */
/*  Should you receive a copy of this source code, you must check you     */
/*  have a proper, written authorization of INTERSEC to hold it. If you   */
/*  don't have such an authorization, you must DELETE all source code     */
/*  files in your possession, and inform INTERSEC of the fact you obtain  */
/*  these files. Should you not comply to these terms, you can be         */
/*  prosecuted in the extent permitted by applicable law.                 */
/*                                                                        */
/**************************************************************************/

#ifndef IS_LIB_COMMON_LICENCE_H
#define IS_LIB_COMMON_LICENCE_H

#include <inttypes.h>
#include "conf.h"
#include "macros.h"

bool is_my_mac_addr(const char *addr);
int list_my_macs(char *dst, size_t size);
int list_my_cpus(char *dst, size_t size);
int read_cpu_signature(uint32_t *dst);
bool licence_check_expiration_ok(const conf_t *conf);
bool licence_check_signature_ok(const conf_t *conf);
bool licence_check_host_ok(const conf_t *conf);
int licence_do_signature(const conf_t *conf, char *dst, size_t size);

/*[ CHECK ]::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::{{{*/
#ifdef CHECK
#include <check.h>

Suite *check_licence_suite(void);

#endif
/*:::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::}}}*/
#endif /* IS_LIB_COMMON_LICENCE_H */
