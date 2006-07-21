/**************************************************************************/
/*                                                                        */
/*  Copyright (C) 2004-2006 INTERSEC                                      */
/*                                                                        */
/*  Should you receive a copy of this source code, you must check you     */
/*  have a proper, written authorization of INTERSEC to hold it. If you   */
/*  don't have such an authorization, you must DELETE all source code     */
/*  files in your possession, and inform INTERSEC of the fact you obtain  */
/*  these files. Should you not comply to these terms, you can be         */
/*  prosecuted in the extent permitted by applicable law.                 */
/*                                                                        */
/**************************************************************************/

#ifndef IS_MSG_TEMPLATE_H
#define IS_MSG_TEMPLATE_H

#include "blob.h"
#include "macros.h"

typedef struct msg_template msg_template;

msg_template *msg_template_new(const char *templatefile, const char *datafile);
/* The element returned should be an iovec structure. That would save
 * us a lot of memcpy !
 */
int msg_template_getnext(msg_template *tpl, blob_t *output);
void msg_template_delete(msg_template **tpl);

#ifdef CHECK
#include <check.h>

Suite *check_msg_template_suite(void);

#endif
#endif
