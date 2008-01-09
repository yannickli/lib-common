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

#include <limits.h>

#include "macros.h"
#include "mem.h"
#include "log_limit.h"

void log_limit_init(log_limit_t *ll, int max_perline, int max_glob)
{
    ll->max_perline = max_perline;
    ll->max_glob = max_glob;
    log_line_array_init(&ll->lines);
    ll->count = 0;
    ll->maxlines = 30;
    ll->unmatched = 0;
}

static int log_line_hash(const char *msg)
{
    int len = 0;
    int hash = 0x5AC39610;

#define RIGHT_ROTATE_32(val, n) \
    (((unsigned int)(val) >> (n)) | ((unsigned int)(val) << (32 - (n))))
    while (*msg) {
        len++;
        hash = RIGHT_ROTATE_32(hash, 4);
        hash ^= *msg;
        msg++;
    }
    hash &= ~0xFF;
    return hash + len;
}

/* Returns 1 if the message should be printed, 0 otherwise
 */
int log_limit_log(log_limit_t *ll, const char *msg)
{
    int i, hash;
    log_line_t *cur;

    hash = log_line_hash(msg);

    /* Look for this message in our list */
    for (i = 0; i < ll->lines.len; i++) {
        cur = ll->lines.tab[i];
        if (cur->content_hash == hash && !strcmp(cur->content, msg)) {
            goto found;
        }
    }
    if (ll->count >= ll->max_glob || ll->lines.len > ll->maxlines) {
        ll->unmatched++;
        return 0;
    }
    cur = log_line_new();
    cur->content_hash = hash;
    cur->content = strdup(msg);
    cur->count = 0;
    log_line_array_append(&ll->lines, cur);
found:
    cur->count++;
    if (ll->count > ll->max_glob || cur->count > ll->max_perline) {
        return 0;
    }
    ll->count++;
    return 1;
}

/* Fill buffer with information about skipped lines.
 * Return buf or NULL if there's nothing to report.
 * */
char *log_limit_flushbuf(log_limit_t *ll, const char *lineprefix,
                       char *buf, int size)
{
    char *pos;
    int i, nb;
    log_line_t *cur;

    if (!lineprefix) {
        lineprefix = "";
    }
    if (size <= 0 || !buf) {
        return buf;
    }
    pos = buf;
    for (i = 0; i < ll->lines.len; i++) {
        cur = ll->lines.tab[i];
        if (cur->count <= 1) {
            continue;
        }
        if (cur->count <= ll->max_perline) {
            /* Message has been already printed. */
            continue;
        }
        nb = snprintf(pos, size, "%s%s (%d times)\n", lineprefix,
                      cur->content, cur->count - ll->max_perline);
        if (nb < size) {
            size -= nb;
            pos += nb;
        } else {
            *buf = '\0';
            goto end;
        }
    }
    if (ll->unmatched) {
        nb = snprintf(pos, size, "%sSkipped %d other messages\n", lineprefix,
                      ll->unmatched);
        if (nb < size) {
            size -= nb;
            pos += nb;
        } else {
            *buf = '\0';
            goto end;
        }
    }
end:
    log_limit_reset(ll);
    if (buf == pos || *buf == '\0') {
        /* Nothing/Too much to report. Return NULL */
        return NULL;
    }
    return buf;
}

void log_limit_reset(log_limit_t *ll)
{
    log_line_array_wipe(&ll->lines);
    log_line_array_init(&ll->lines);
    ll->count = 0;
    ll->unmatched = 0;
}

/*[ CHECK ]::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::{{{*/
#ifdef CHECK

START_TEST(check_init_flushbuf_wipe)
{
    log_limit_t ll;
    char *buf, *p;

    log_limit_init(&ll, 2, 10);
    buf = p_new(char, BUFSIZ);

    p = log_limit_flushbuf(&ll, "", buf, BUFSIZ);
    fail_if(p, "flushbuf() on just created ll should return NULL");
    p_delete (&buf);
    log_limit_wipe(&ll);
}
END_TEST

START_TEST(check_limit_oneline)
{
    log_limit_t ll;
    char *buf, *p;


    log_limit_init(&ll, 2, 10);
    buf = p_new(char, BUFSIZ);

    fail_if (log_limit_log(&ll, "Msg 1") != 1, "One line should be ok\n");
    fail_if (log_limit_log(&ll, "Msg 1") != 1, "Two lines should be ok\n");
    fail_if (log_limit_log(&ll, "Msg 1") != 0, "Three lines should not be ok\n");

    p = log_limit_flushbuf(&ll, "", buf, BUFSIZ);
    fail_if(!p, "flushbuf should not return NULL");

    p = log_limit_flushbuf(&ll, "", buf, BUFSIZ);
    fail_if(p, "Second flushbuf() should return NULL, not '%s'", buf);

    p_delete (&buf);
    log_limit_wipe(&ll);
}
END_TEST

START_TEST(check_limit_morelines)
{
    log_limit_t ll;
    char *buf, *p;


    log_limit_init(&ll, 2, 5);
    buf = p_new(char, BUFSIZ);

    fail_if (log_limit_log(&ll, "Msg 1") != 1, "1 line should be ok\n");
    fail_if (log_limit_log(&ll, "Msg 2") != 1, "2 lines should be ok\n");
    fail_if (log_limit_log(&ll, "Msg 3") != 1, "3 lines should be ok\n");
    fail_if (log_limit_log(&ll, "Msg 4") != 1, "4 lines should be ok\n");
    fail_if (log_limit_log(&ll, "Msg 5") != 1, "5 lines should be ok\n");
    fail_if (log_limit_log(&ll, "Msg 6") != 0, "6 lines should not be ok\n");

    p = log_limit_flushbuf(&ll, "", buf, BUFSIZ);
    fail_if(!p, "flushbuf should not return NULL");

    p = log_limit_flushbuf(&ll, "", buf, BUFSIZ);
    fail_if(p, "Second flushbuf() should return NULL, not '%s'", buf);

    p_delete (&buf);
    log_limit_wipe(&ll);
}
END_TEST

/*.....................................................................}}}*/
/* public testing API                                                  {{{*/

Suite *check_log_limit_suite(void)
{
    Suite *s  = suite_create("LogLimit");
    TCase *tc = tcase_create("Core");

    suite_add_tcase(s, tc);
    tcase_add_test(tc, check_init_flushbuf_wipe);
    tcase_add_test(tc, check_limit_oneline);
    tcase_add_test(tc, check_limit_morelines);

    return s;
}

/*.....................................................................}}}*/
#endif
/*:::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::}}}*/
