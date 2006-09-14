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

#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <errno.h>

#include "blob.h"
#include "mem.h"
#include "msg_template.h"

#define VAR_START      "${"
#define VAR_START_LEN  2
#define VAR_END        "}"
#define VAR_END_LEN    1

/* A template is made of parts.
 * Every part is encoded with a specific encoding
 * Every part is made of parts that are either verbatim or evaluated
 * (simple variable substitution, qs, or another part)
 */
typedef enum part_type {
    PART_VERBATIM,
    PART_VARIABLE,
    PART_MULTI,
    PART_QS,
} part_type;

typedef struct part_multi part_multi;

struct msg_template {
    part_multi *body;
};


typedef struct part_verbatim {
    blob_t data;
} part_verbatim;

typedef struct part_qs {
    /* FIXME : data should not be a blob but a QS struct */
    blob_t data;
} part_qs;

typedef struct part_variable {
    int index; /* Index of the variable in the data file */
} part_variable;

typedef struct tpl_part {
    part_type type;
    union {
        part_verbatim *verbatim;
        part_variable *variable;
        part_multi *multi;
        part_qs *qs;
    } u;
    part_encoding enc;
    /* FIXME : Handle conditional part here */
} tpl_part;

struct part_multi {
    int nbparts;
    int nbparts_allocated;
    tpl_part parts[];
};

part_multi *part_multi_new(void);
void part_multi_delete(part_multi **multi);
void part_multi_wipe(part_multi *multi);

void part_multi_addpart(part_multi **multi, tpl_part *part);
static const tpl_part *part_multi_get(const part_multi *multi, int i);
int part_multi_nbparts(const part_multi *multi);
static inline void part_multi_dump(const part_multi *multi);

part_variable *part_variable_new(int ind);
void part_variable_delete(part_variable **variable);
void part_variable_wipe(part_variable *variable);

part_verbatim *part_verbatim_new(const byte *src, int size);
void part_verbatim_delete(part_verbatim **verbatim);
void part_verbatim_wipe(part_verbatim *verbatim);

part_qs *part_qs_new(const char*src, int size);
void part_qs_delete(part_qs **qs);
void part_qs_wipe(part_qs *qs);

void msg_template_dump(const msg_template *tpl)
{
    if (tpl->body) {
        part_multi_dump(tpl->body);
    } else {
        e_debug(1, "empty tpl\n");
    }
}

void part_multi_addpart(part_multi **multi_p, tpl_part *part)
{
    part_multi *multi = *multi_p;

    if (multi->nbparts + 1 > multi->nbparts_allocated) {
        multi->nbparts_allocated += 16;
        e_debug(1, "realloc:%d\n", multi->nbparts_allocated);
        multi = mem_realloc(multi,
                            sizeof(part_multi)
                            + multi->nbparts_allocated * sizeof(tpl_part));
    }
    memcpy(&multi->parts[multi->nbparts], part, sizeof(tpl_part));
    multi->nbparts++;
    *multi_p = multi;
}

static const tpl_part *part_multi_get(const part_multi *multi, int i)
{
    return &multi->parts[i];
}

int part_multi_nbparts(const part_multi *multi)
{
    return multi->nbparts;
}

static inline void part_multi_dump(const part_multi *multi)
{
    int i;
    const tpl_part *curpart;

    e_debug(1, "nbparts:%d\n", multi->nbparts);
    for (i = 0; i < multi->nbparts; i++) {
        curpart = &multi->parts[i];
        e_debug(1, "[%02d]: ", i);
        switch (curpart->type) {
          case PART_VERBATIM:
              e_debug(1, "VERBATIM: (%zd) '%s'\n",
                     curpart->u.verbatim->data.len,
                     blob_get_cstr(&curpart->u.verbatim->data));
            break;
          case PART_VARIABLE:
            e_debug(1, "VARIABLE: %d\n", curpart->u.variable->index);
            break;
          case PART_QS:
              e_debug(1, "QS: (%zd) '%s'\n",
                     curpart->u.qs->data.len,
                     blob_get_cstr(&curpart->u.qs->data));
            break;
          case PART_MULTI:
            e_debug(1, "MULTI: [skipped]\n");
            break;
        }
    }
}

msg_template *msg_template_new(void)
{
    msg_template *tpl;
    tpl = p_new(msg_template, 1);
    tpl->body = part_multi_new();
    return tpl;
}

int msg_template_nbparts(const msg_template *tpl)
{
    if (!tpl || !tpl->body) {
        return 0;
    }
    return part_multi_nbparts(tpl->body);
}

void msg_template_delete(msg_template **tpl)
{
    if (!tpl || !*tpl) {
        return;
    }
    if ((*tpl)->body) {
        part_multi_delete(&(*tpl)->body);
    }
    p_delete(tpl);
}

int msg_template_add_byte(msg_template *tpl, part_encoding enc,
                          byte b)
{
    return msg_template_add_data(tpl, enc, &b, 1);
}

int msg_template_add_cstr(msg_template *tpl, part_encoding enc,
                                   const char *str)
{
    return msg_template_add_data(tpl, enc, (const byte *)str, strlen(str));
}

int msg_template_add_data(msg_template *tpl, part_encoding enc,
                              const byte *data, int len)
{
    part_verbatim *verb;
    tpl_part part;
    if (!tpl || ! data) {
        return -1;
    }
    verb = part_verbatim_new(data, len);
    part.type = PART_VERBATIM;
    part.u.verbatim = verb;
    part.enc = enc;

    part_multi_addpart(&tpl->body, &part);
    return 0;
}

int msg_template_add_blob(msg_template *tpl, part_encoding enc,
                          const blob_t *data)
{
    return msg_template_add_data(tpl, enc,
                                 (const byte *)blob_get_cstr(data),
                                 data->len);
}

int msg_template_add_variable(msg_template *tpl, part_encoding enc, 
                              char ** const vars, int nbvars,
                              const char *name)
{
    part_variable *varpart;
    tpl_part part;
    int i;

    if (!tpl || !name) {
        return -1;
    }
    for (i = 0; i < nbvars; i++) {
        if (!strcmp(name, vars[i])) {
            break;
        }
    }
    if (i == nbvars) {
        return -1;
    }
    varpart = part_variable_new(i);
    part.type = PART_VARIABLE;
    part.u.variable = varpart;
    part.enc = enc;

    part_multi_addpart(&tpl->body, &part);
    return 0;
}

part_multi *part_multi_new(void)
{
    return p_new(part_multi, 1);
}

void part_multi_wipe(part_multi *multi)
{
    tpl_part *curpart;
    int i;

    for (i = 0; i < multi->nbparts; i++) {
        curpart = &multi->parts[i];
        switch (curpart->type) {
          case PART_VERBATIM:
            part_verbatim_delete(&curpart->u.verbatim);
            break;
          case PART_VARIABLE:
            part_variable_delete(&curpart->u.variable);
            break;
          case PART_QS:
            part_qs_delete(&curpart->u.qs);
            break;
          case PART_MULTI:
            part_multi_delete(&curpart->u.multi);
            break;
        }
    }
    multi->nbparts = 0;
}

void part_multi_delete(part_multi **multi)
{
    part_multi_wipe(*multi);
    p_delete(multi);
}

part_verbatim *part_verbatim_new(const byte *src, int size)
{
    part_verbatim *verb;

    verb = p_new(part_verbatim, 1);
    blob_init(&verb->data);
    blob_set_data(&verb->data, src, size);
    return verb;
}

void part_verbatim_wipe(part_verbatim *verb)
{
    blob_wipe(&verb->data);
}

void part_verbatim_delete(part_verbatim **verb)
{
    part_verbatim_wipe(*verb);
    p_delete(verb);
}

part_variable *part_variable_new(int ind)
{
    part_variable *var;

    var = p_new(part_variable, 1);
    var->index = ind;
    return var;
}

void part_variable_wipe(part_variable __unused__ *var)
{
}

void part_variable_delete(part_variable **var)
{
    part_variable_wipe(*var);
    p_delete(var);
}

part_qs *part_qs_new(const char*src, int size)
{
    part_qs *qs;

    qs = p_new(part_qs, 1);
    /* FIXME : data should not be a blob but a QS struct */
    blob_init(&qs->data);
    blob_set_data(&qs->data, src, size);
    return qs;
}

void part_qs_wipe(part_qs *qs)
{
    blob_wipe(&qs->data);
}

void part_qs_delete(part_qs **qs)
{
    part_qs_wipe(*qs);
    p_delete(qs);
}

/*
 * Fill vector with pointer to blobs
 */
int msg_template_apply(msg_template *tpl, char ** const vars, int nbvars,
                       blob_t ** const vector, byte *allocated, int count)
{
    int i;
    blob_t *curblob;
    const tpl_part *curpart;
    const int nbparts = part_multi_nbparts(tpl->body);

    if (count < nbparts) {
        return -1;
    }
    for (i = 0; i < nbparts; i++) {
        curpart = part_multi_get(tpl->body, i);
        e_debug(2, "[%d] ", i);
        switch (curpart->type) {
          case PART_VERBATIM:
            e_debug(2, "Verbatim:'%s'\n",
                    blob_get_cstr(&curpart->u.verbatim->data));
            vector[i] = &curpart->u.verbatim->data;
            allocated[i] = 0;
            break;
          case PART_VARIABLE:
            if (curpart->u.variable->index > nbvars) {
                /* Ignore non-specified fields */
                break;
            }
            e_debug(2, "Var:%d\n", curpart->u.variable->index);
            curblob = blob_new();
            blob_set_cstr(curblob, vars[curpart->u.variable->index]);
            vector[i] = curblob;
            allocated[i] = 1;
            break;
          case PART_QS:
            e_debug(2, "QS:'%s'\n", blob_get_cstr(&curpart->u.qs->data));
            curblob = blob_new();
            /* FIXME : Do the real job !!!
             */
#if 0
             qs_run(&curpart->u.qs->data, curblob, vars, nbvars);
#else
            blob_set(curblob, &curpart->u.qs->data);
#endif
            vector[i] = curblob;
            allocated[i] = 1;
            break;
          case PART_MULTI:
            /* FIXME */
            curblob = blob_new();
            blob_set_cstr(curblob, "***MULTI***");
            vector[i] = curblob;
            allocated[i] = 1;
            break;
        }
    }
    return 0;
}

void msg_template_optimize(msg_template *tpl)
{
    /* TODO: Concatenate constant parts (after encoding them if applicable) */
    tpl = tpl;
}

#ifdef CHECK
#include <check.h>
#include <stdio.h>

START_TEST(check_msg_template_simple)
{
    blob_t out;
    msg_template *tpl;

    tpl = msg_template_new();
    fail_if(tpl == NULL, "msg_template_new failed");

    msg_template_delete(&tpl);
}
END_TEST

Suite *check_msg_template_suite(void)
{
    Suite *s  = suite_create("Template");
    TCase *tc = tcase_create("Core");

    suite_add_tcase(s, tc);
    tcase_add_test(tc, check_msg_template_simple);
    return s;
}
#endif
#if 1
/*
 gcc -g -o msg_template msg_template.c err_report.c  blob.c string_is.c
 */
#include <stdlib.h>
#include <stdio.h>
static inline int split_csv_line(char *line, char **fields[]);
int main(void)
{
    blob_t csv_blob;
    const char *p;
    char *q, *fieldline, *dataline;
    char **fields, **data;
    msg_template *tpl;
    blob_t **out;
    byte *allocated;
    int nbfields, nbparts, nbdata, i;
    blob_t blob;

    e_set_verbosity (1);

    blob_init(&csv_blob);
    blob_append_file_data(&csv_blob, "samples/simple.csv");

    p = blob_get_cstr(&csv_blob);
    q = strchr(blob_get_cstr(&csv_blob), '\n');
    if (!q) {
        return -1;
    }

    fieldline = mem_alloc(q - p + 1);
    pstrcpylen(fieldline, q - p + 1, p, q - p);
    fieldline[q - p] = '\0';
    blob_kill_first(&csv_blob, q - p + 1);

    e_debug(1, blob_get_cstr(&csv_blob));

    nbfields = split_csv_line(fieldline, &fields);
    if (nbfields < 0) {
        return 1;
    }
    
    tpl = msg_template_new();

    /* Add some parts */
    msg_template_add_cstr(tpl, ENC_NONE, "TO:'");
    msg_template_add_variable(tpl, ENC_NONE, fields, nbfields, "telephone");
    msg_template_add_cstr(tpl, ENC_NONE, "'\n");

    msg_template_add_cstr(tpl, ENC_NONE, "Subject:'");
    msg_template_add_cstr(tpl, ENC_NONE, "Bon anniversaire ");
    msg_template_add_variable(tpl, ENC_NONE, fields, nbfields, "prenom");
    msg_template_add_cstr(tpl, ENC_NONE, "!'\n");

    msg_template_add_cstr(tpl, ENC_NONE, "Data:'");
    msg_template_add_cstr(tpl, ENC_NONE, "Nous vous souhaitons un très bon anniversaire !");
    msg_template_add_cstr(tpl, ENC_NONE, "'\n");

    /* TODO: Add some QS stuff, using "fields" */

    /* Add some binary parts */
    blob_init(&blob);
    for (i = 0; i < 3; i++) {
        blob_set_fmt(&blob, "FAKE BINARY CHUNK %d", i);
        msg_template_add_blob(tpl, ENC_NONE, &blob);
    }

    /* We're done with "demo objects". Free them. */
    mem_free(fieldline);
    mem_free(fields);
    blob_wipe(&blob);


    /* Display the resulting template */
    msg_template_dump(tpl);
    msg_template_optimize(tpl);
    msg_template_dump(tpl);

    /* Use csv_blob to produce some output */
    nbparts = msg_template_nbparts(tpl);
    allocated = p_new(byte, nbparts);
    out = p_new(blob_t *, nbparts);

    while (csv_blob.len) {
        p = blob_get_cstr(&csv_blob);
        q = strchr(blob_get_cstr(&csv_blob), '\n');
        if (!q) {
            return -1;
        }
        dataline = p_new(char, q - p + 1);
        pstrcpylen(dataline, q - p + 1, p, q - p);
        dataline[q - p] = '\0';
        blob_kill_first(&csv_blob, q - p + 1);
        nbdata = split_csv_line(dataline, &data);
        if (nbdata < 0) {
            return 1;
        }
        if (nbdata != nbfields) {
            e_debug(1, "Incoherent CSV!\n");
            break;
        }
        
        msg_template_apply(tpl, data, nbdata, (blob_t **const)out, allocated,
                           nbparts);
        for (i = 0; i < nbparts; i++) {
            e_debug(1, "%.*s", (int)out[i]->len, blob_get_cstr(out[i]));
        }
        #if 0 
        writev(out, nbparts);
        #endif
        for (i = 0; i < nbparts; i++) {
            if (allocated[i]) {
                blob_delete(&out[i]);
            } else {
                out[i] = NULL;
            }
        }
        p_delete(&dataline);
    }
    p_delete(&out);
    p_delete(&allocated);

    /* THE END */
    blob_wipe(&csv_blob);
    msg_template_delete(&tpl);
    return 0;
}

/* Put '\0' in line to get fields as strings
 */
static inline int split_csv_line(char *line, char **fields[])
{
    int nbfields;
    char *p, *q;

    if (!line || !fields) {
        return -1;
    }

    nbfields = 0;
    *fields = NULL;
    p = line;
    q = p + strlen(p);
    if (q > p && q[-1] == '\n') {
        q[-1] = '\0';
    }

    while (*p) {
        *fields = realloc(*fields, (nbfields + 1) * sizeof(char*));
        (*fields)[nbfields] = p;
        nbfields++;
        q = strchr(p, ';');
        if (!q) {
            break;
        }
        *q = '\0';
        p = q + 1;
    }
    e_debug(1, "Nbfields:%d\n", nbfields);
    return nbfields;
}
#endif
