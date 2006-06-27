#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <stdlib.h>
#include <stdio.h>
#include <errno.h>

#include "blob.h"
#include "mem.h"
#include "msg_template.h"

#define VAR_START "${"
#define VAR_END "}"

/* A template is made of parts.
 * Every part is encoded with a specific encoding
 * Every part is made of parts that are either verbatim, chunked or evaluated
 */
typedef struct part_multi part_multi;

struct msg_template {
    FILE *datafd;
    part_multi *body;
};

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

void part_multi_delete(part_multi **multi);
void part_multi_wipe(part_multi *multi);

part_variable *part_variable_new(int ind);
void part_variable_delete(part_variable **variable);
void part_variable_wipe(part_variable *variable);

part_verbatim *part_verbatim_new(const char*src, int size);
void part_verbatim_delete(part_verbatim **verbatim);
void part_verbatim_wipe(part_verbatim *verbatim);

msg_template *msg_template_new(const char *templatefile, const char *datafile)
{
    int tplfd;
    msg_template *tpl;
    char *buf, *p, *q;
    struct stat st;
    int toread, nb, size;

    tpl = p_new(msg_template, 1);

    tplfd = open(templatefile, O_RDONLY);
    if (tplfd < 0) {
        e_error("open template '%s' failed\n", templatefile);
        goto error;
    }
    tpl->datafd = fopen(datafile, "rb");
    if (!tpl->datafd) {
        e_error("open datafile '%s' failed\n", datafile);
        goto error;
    }

    /* FIXME: 10 is fixed and never checked. This is a bad idea */
    tpl->body = calloc(1, sizeof(part_multi) + 10 * sizeof(tpl_part));

    if (stat(templatefile, &st) != 0) {
        e_error("Unable to stat '%s'\n", templatefile);
        goto error;
    }

    toread = st.st_size;
    size = st.st_size;
    buf = p_new(char, toread);
    p = buf;

    while (toread) {
        nb = read(tplfd, p, toread);
        if (nb < 0) {
            if (errno == EINTR || errno == EAGAIN) {
                continue;
            }
            goto error;
        } else if (nb == 0) {
            goto error;
        } else {
            p += nb;
            toread -= nb;
        }
    }

    p = buf;
    tpl_part *curpart = tpl->body->parts;
    tpl->body->nbparts = 0;
    /* FIXME: should look for variable names on the first line of the
     * data file and use their indexes. For now assume they're in the same 
     * order in the template and in the datafile, without repetitions. */
    int var = 0;
    while(size) {
        int verb_size;
        int var_size;
        q = vmemsearch(p, size, VAR_START, strlen(VAR_START));
        if (!q) {
            /* Last chunk */
            curpart->type = PART_VERBATIM;
            curpart->u.verbatim = part_verbatim_new(p, size);
            curpart++;
            tpl->body->nbparts++;
            break;
        }
        verb_size = q - p;
        if (verb_size) {
            curpart->type = PART_VERBATIM;
            curpart->u.verbatim = part_verbatim_new(p, verb_size);
            curpart++;
            tpl->body->nbparts++;
        }
        p += strlen(VAR_START) + verb_size;
        size -= strlen(VAR_START) + verb_size;
        q = vmemsearch(p, size, VAR_END, strlen(VAR_END));
        if (!q) {
            /* Closing tag not found... */
            goto error;
            break;
        }
        var_size = q - p;
        if (var_size) {
            curpart->type = PART_VARIABLE;
            /* FIXME: See above comment about var */
            curpart->u.variable = part_variable_new(var++);
            curpart++;
            tpl->body->nbparts++;
        }
        p += strlen(VAR_END) + var_size;
        size -= strlen(VAR_END) + var_size;
    }
    p_delete(&buf);

    int i;
    printf("nbparts:%d\n", tpl->body->nbparts);
    for(i = 0; i < tpl->body->nbparts; i++) {
        curpart = &tpl->body->parts[i];
        printf("[%02d]: ", i);
        switch(curpart->type) {
            case PART_VERBATIM:
                printf("VERBATIM: (%d) '%s'\n", curpart->u.verbatim->size,
                       curpart->u.verbatim->data);
            break;
            case PART_VARIABLE:
                printf("VARIABLE: %d\n", curpart->u.variable->index);
            break;
            case PART_MULTI:
                printf("MULTI: [skipped]\n");
            break;
        }
    }
    return tpl;

error:
    if (tplfd >= 0) {
        close(tplfd);
        tplfd = -1;
    }
    msg_template_delete(&tpl);
    return NULL;
}

void msg_template_delete(msg_template **tpl)
{
    if (!tpl || !*tpl) {
        return;
    }
    if ((*tpl)->datafd) {
        fclose((*tpl)->datafd);
    }
    if ((*tpl)->body) {
        part_multi_delete(&(*tpl)->body);
    }
    p_delete (tpl);
}

void part_multi_wipe(part_multi *multi)
{
    tpl_part *curpart;
    int i;
    for(i = 0; i < multi->nbparts; i++) {
        curpart = &multi->parts[i];
        switch(curpart->type) {
            case PART_VERBATIM:
                part_verbatim_delete(&curpart->u.verbatim);
            break;
            case PART_VARIABLE:
                part_variable_delete(&curpart->u.variable);
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

part_verbatim *part_verbatim_new(const char *src, int size)
{
    part_verbatim *verb;
    verb = p_new(part_verbatim, 1);
    verb->data = p_new(byte, size);
    memcpy(verb->data, src, size);
    verb->size = size;
    return verb;
}

void part_verbatim_wipe(part_verbatim __unused__ *vb)
{
    /* verbatim parts are allocated just once per template. Do not free them 
     * here. */
}

void part_verbatim_delete(part_verbatim **vb)
{
    part_verbatim_wipe(*vb);
    p_delete(vb);
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

int msg_template_getnext(msg_template *tpl, blob_t *output)
{
    tpl_part *curpart;
    int i, nbfields;
    char line[1024];
    char *fields[256], *p, *q;
    const char *line_end;
    if (!fgets(line, sizeof(line), tpl->datafd)) {
        return 0;
    }
    p = line;
    line_end = p + strlen(line);
    nbfields = 0;
    while (p < line_end) {
        fields[nbfields++] = p;
        q = strchr(p, ';');
        if (!q) {
            break;
        }
        *q = '\0';
        p = q + 1;
    }

    printf("Nbparts:%d\n", tpl->body->nbparts);
    for(i = 0; i < tpl->body->nbparts; i++) {
        curpart = &tpl->body->parts[i];
        printf("[%d] ", i);
        switch(curpart->type) {
            case PART_VERBATIM:
                printf("Verbatim:'%s'\n", curpart->u.verbatim->data);
                blob_append_data(output, curpart->u.verbatim->data, 
                                 curpart->u.verbatim->size);
            break;
            case PART_VARIABLE:
                if (curpart->u.variable->index > nbfields) {
                    /* Ignore non-specified fields */
                    break;
                }
                printf("Var:%d\n", curpart->u.variable->index);
                blob_append_cstr(output, fields[curpart->u.variable->index]);
            break;
            case PART_MULTI:
                /* FIXME */
                blob_append_cstr(output, "***MULTI***");
            break;
        }
    }
    return 0;
}

#ifdef CHECK
#include <check.h>
#include <stdio.h>

START_TEST(check_msg_template_simple)
{
    blob_t out;
    msg_template *tpl;
    tpl = msg_template_new("../samples/simple.tpl", "../samples/simple.csv");
    fail_if(tpl == NULL, "msg_template_new failed");

    blob_init(&out);
    fail_if (msg_template_getnext(tpl, &out), "getnext failed\n");
    printf("out:%s\n", out.data);
    blob_wipe(&out);
    fail_if (msg_template_getnext(tpl, &out), "getnext failed\n");
    printf("out:%s\n", out.data);
    blob_wipe(&out);
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
#if 0
/*
 gcc -g -o msg_template msg_template.c err_report.c  blob.c string_is.c
 */
int main(void)
{
    blob_t out;
    msg_template *tpl;
    tpl = msg_template_new("samples/simple.tpl", "samples/simple.csv");
    printf("tpl:%p\n", tpl);

    blob_init(&out);
    if (msg_template_getnext(tpl, &out)) {
        printf("getnext failed\n");
        return 1;
    }
    printf("out:%s\n", out.data);
    msg_template_delete(&tpl);
    return 0;
}
#endif
