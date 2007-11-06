/**************************************************************************/
/*                                                                        */
/*  Copyright (C) 2004-2007 INTERSEC SAS                                  */
/*                                                                        */
/*  Should you receive a copy of this source code, you must check you     */
/*  have a proper, written authorization of INTERSEC to hold it. If you   */
/*  don't have such an authorization, you must DELETE all source code     */
/*  files in your possession, and inform INTERSEC of the fact you obtain  */
/*  these files. Should you not comply to these terms, you can be         */
/*  prosecuted in the extent permitted by applicable law.                 */
/*                                                                        */
/**************************************************************************/

#ifndef IS_LIB_COMMON_XMLPP_H
#define IS_LIB_COMMON_XMLPP_H

#include <lib-common/str_array.h>
#include <lib-common/blob.h>

typedef struct xmlpp_t {
    flag_t can_do_attr : 1;
    blob_t *buf;
    string_array stack;
} xmlpp_t;

void xmlpp_open(xmlpp_t *, blob_t *buf);
void xmlpp_opentag(xmlpp_t *, const char *tag);
void xmlpp_putattr(xmlpp_t *, const char *key, const char *val);
void xmlpp_puttext(xmlpp_t *, const char *s, int len);
void xmlpp_put(xmlpp_t *, const char *fmt, ...) __attr_printf__(2, 3);
void xmlpp_closetag(xmlpp_t *, int noindent);
void xmlpp_close(xmlpp_t *);

#endif /* IS_LIB_COMMON_XMLPP_H */
