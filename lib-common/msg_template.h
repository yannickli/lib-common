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
