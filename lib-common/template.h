#ifndef IS_TEMPLATE_H
#define IS_TEMPLATE_H

#include "blob.h"
#include "macros.h"

/* A template is made of parts.
 * Every part is encoded with a specific encoding
 * Every part is made of parts that are either verbatim, chunked or evaluated
 */
typedef enum part_encoding {
    ENC_NONE = 1,
    ENC_HTML,
} part_encoding;

typedef enum part_type {
    PART_VERBATIM,
    PART_VARIABLE,
    PART_MULTI,
} part_type;

typedef struct part_verbatim {
    byte *data;
    int size;
} part_verbatim;

typedef struct part_variable {
    int index; /* Index of the variable in the data file */
    part_encoding enc;
} part_variable;

typedef struct part_multi part_multi;

typedef struct tpl_part {
    part_type type;
    union {
        part_verbatim *verbatim;
        part_variable *variable;
        part_multi *multi;
    } u;
} tpl_part;

struct part_multi {
    part_encoding enc; /* Apply this encoding to the whole content */
    int nbparts;
    tpl_part parts[];
};

typedef struct template {
    FILE *datafd;
    part_multi *body;
} template;

template *template_new(const char *templatefile, const char *datafile);
/* The element returned should be an iovec structure. That would save
 * us a lot of memcpy !
 */
int template_getnext(template *tpl, blob_t *output);
void template_delete(template **tpl);

void part_multi_delete(part_multi **multi);
void part_multi_wipe(part_multi *multi);

part_variable *part_variable_new(int ind);
void part_variable_delete(part_variable **variable);
void part_variable_wipe(part_variable *variable);

part_verbatim *part_verbatim_new(const char*src, int size);
void part_verbatim_delete(part_verbatim **verbatim);
void part_verbatim_wipe(part_verbatim *verbatim);

#ifdef CHECK
#include <check.h>

Suite *check_template_suite(void);

#endif
#endif
