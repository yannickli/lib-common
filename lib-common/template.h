#ifndef IS_TEMPLATE_H
#define IS_TEMPLATE_H

#include "blob.h"
#include "macros.h"

typedef struct template template;

template *template_new(const char *templatefile, const char *datafile);
/* The element returned should be an iovec structure. That would save
 * us a lot of memcpy !
 */
int template_getnext(template *tpl, blob_t *output);
void template_delete(template **tpl);

#ifdef CHECK
#include <check.h>

Suite *check_template_suite(void);

#endif
#endif
