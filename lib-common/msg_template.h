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

typedef enum part_encoding {
    ENC_NONE = 1,
    ENC_HTML,
    ENC_BASE64,
    ENC_QUOTED_PRINTABLE,
} part_encoding;

typedef struct msg_template msg_template;

msg_template *msg_template_new(void);
void msg_template_delete(msg_template **tpl);
void msg_template_dump(const msg_template *tpl);
int msg_template_add_byte(msg_template *tpl, part_encoding enc,
                          const byte b);
int msg_template_add_cstr(msg_template *tpl, part_encoding enc,
                          const char *str);
int msg_template_add_data(msg_template *tpl, part_encoding enc,
                          const byte *data, int len);
int msg_template_add_blob(msg_template *tpl, part_encoding enc,
                          const blob_t *data);
int msg_template_add_variable(msg_template *tpl, part_encoding enc, 
                              char ** const vars, int nbvars,
                              const char *name);
void msg_template_optimize(msg_template *tpl);
int msg_template_apply(msg_template *tpl, char ** const vars, int nbvars,
                       blob_t **vector, byte *allocated, int count);
int msg_template_nbparts(const msg_template *tpl);

#ifdef CHECK
#include <check.h>

Suite *check_msg_template_suite(void);

#endif
#endif
