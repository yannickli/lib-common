/**************************************************************************/
/*                                                                        */
/*  Copyright (C) 2004-2008 INTERSEC SAS                                  */
/*                                                                        */
/*  Should you receive a copy of this source code, you must check you     */
/*  have a proper, written authorization of INTERSEC to hold it. If you   */
/*  don't have such an authorization, you must DELETE all source code     */
/*  files in your possession, and inform INTERSEC of the fact you obtain  */
/*  these files. Should you not comply to these terms, you can be         */
/*  prosecuted in the extent permitted by applicable law.                 */
/*                                                                        */
/**************************************************************************/

#ifndef IS_LIB_COMMON_LOG_LIMIT_H
#define IS_LIB_COMMON_LOG_LIMIT_H

#include "array.h"

typedef struct log_line_t {
    int content_hash;
    char *content; /* XXX: Should be char content[1024] or so */
    int count;
} log_line_t;

static log_line_t *log_line_init(log_line_t *line)
{
    line->content = NULL;
    return line;
}
GENERIC_NEW(log_line_t, log_line);

static void log_line_wipe(log_line_t *line)
{
    p_delete(&line->content);
}
GENERIC_DELETE(log_line_t, log_line);

/* XXX: We should not use an array here. It forces us to do
 * too many malloc/free */
ARRAY_TYPE(log_line_t, log_line);
ARRAY_FUNCTIONS(log_line_t, log_line, log_line_delete);

typedef struct log_limit_t {
    int max_perline;
    int max_glob;

    /* Lines that we currently remember */
    log_line_array lines;
    /* Nb of lines that we saw since last reset */
    int count;
    /* Max number of lines that we are allowed to remember */
    int maxlines;
    /* Lines that did not match any and that we could not store as
     * maxlines had been reached. */
    int unmatched;
} log_limit_t;

void log_limit_init(log_limit_t *ll, int max_perline, int max_glob);
int log_limit_log(log_limit_t *ll, const char *msg);
char *log_limit_flushbuf(log_limit_t *ll, const char *lineprefix,
                         char *buf, int size);
void log_limit_reset(log_limit_t *ll);

static void log_limit_wipe(log_limit_t *ll)
{
    log_line_array_wipe(&ll->lines);
}

GENERIC_DELETE(log_limit_t, log_limit);

/*[ CHECK ]::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::{{{*/
#ifdef CHECK
#include <check.h>

Suite *check_log_limit_suite(void);

#endif
/*:::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::}}}*/
#endif /* IS_LIB_COMMON_LOG_LIMIT_H */
