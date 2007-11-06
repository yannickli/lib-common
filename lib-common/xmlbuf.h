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

#ifndef IS_TEASER_XMLBUF_H
#define IS_TEASER_XMLBUF_H

#include <lib-common/str_array.h>
#include <lib-common/blob.h>

typedef struct xmlbuf_t {
    flag_t can_do_attr : 1;
    blob_t buf;
    string_array stack;
} xmlbuf_t;

xmlbuf_t *xmlbuf_init(xmlbuf_t *buf);
void xmlbuf_wipe(xmlbuf_t *buf);
GENERIC_NEW(xmlbuf_t, xmlbuf);
GENERIC_DELETE(xmlbuf_t, xmlbuf);

void xmlbuf_opentag(xmlbuf_t *buf, const char *tag);
void xmlbuf_putattr(xmlbuf_t *buf, const char *key, const char *val);
void xmlbuf_puttext(xmlbuf_t *buf, const char *s, int len);
void xmlbuf_closetag(xmlbuf_t *buf, int noindent);
void xmlbuf_close(xmlbuf_t *buf);

#endif /* IS_TEASER_XMLBUF_H */
