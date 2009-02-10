/*
 * Check: a unit test framework for C
 * Copyright (C) 2001, 2002 Arien Malec
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2.1 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library; if not, write to the
 * Free Software Foundation, Inc., 59 Temple Place - Suite 330,
 * Boston, MA 02111-1307, USA.
 */

//#include "config.h"

#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>

#include <check.h>

//==> check_error.h <==

/* Print error message and die
   If fmt ends in colon, include system error information */
void eprintf(const char *fmt, const char *file, int line,...)
        __attribute__((format(printf, 1, 4)));

/* malloc or die */
void *emalloc(size_t n);
void *erealloc(void *, size_t n);
char *estrdup(const char *str);

//==> check_list.h <==

typedef struct List List;

/* Create an empty list */
List *check_list_create(void);

/* Is list at end? */
int list_at_end(List *lp);

/* Position list at front */
void list_front(List *lp);

/* Add a value to the front of the list,
   positioning newly added value as current value.
   More expensive than list_add_end, as it uses memmove. */
void list_add_front(List *lp, const void *val);

/* Add a value to the end of the list,
   positioning newly added value as current value */
void list_add_end(List *lp, const void *val);

/* Give the value of the current node */
void *list_val(List *lp);

/* Position the list at the next node */
void list_advance(List *lp);

/* Free a list, but don't free values */
void list_free(List *lp);

void list_apply(List *lp, void (*fp)(void *));

//==> check_impl.h <==

/* This header should be included by any module that needs
   to know the implementation details of the check structures
   Include stdio.h & list.h before this header
*/

/* magic values */

/* Unspecified fork status, used only internally */
#define CK_FORK_UNSPECIFIED  ((enum fork_status)-1)

typedef struct TF {
    TFun fn;
    int loop_start;
    int loop_end;
    const char *name;
    int signal;
} TF;

struct Suite {
    const char *name;
    List *tclst; /* List of test cases */
};

typedef struct Fixture {
    int ischecked;
    SFun fun;
} Fixture;

struct TCase {
    const char *name;
    int timeout;
    List *tflst; /* list of test functions */
    List *unch_sflst;
    List *unch_tflst;
    List *ch_sflst;
    List *ch_tflst;
};

typedef struct TestStats {
    int n_checked;
    int n_failed;
    int n_errors;
} TestStats;

struct TestResult {
    enum test_result rtype;     /* Type of result */
    enum ck_result_ctx ctx;     /* When the result occurred */
    char *file;    /* File where the test occurred */
    int line;      /* Line number where the test occurred */
    int iter;      /* The iteration value for looping tests */
    const char *tcname;  /* Test case that generated the result */
    const char *tname;  /* Test that generated the result */
    char *msg;     /* Failure message */
};

TestResult *tr_create(void);
void tr_reset(TestResult *tr);

enum cl_event {
    CLINITLOG_SR,
    CLENDLOG_SR,
    CLSTART_SR,
    CLSTART_S,
    CLEND_SR,
    CLEND_S,
    CLEND_T
};

typedef void (*LFun)(SRunner *, FILE*, enum print_output,
                     void *, enum cl_event);

typedef struct Log {
    FILE *lfile;
    LFun lfun;
    int close;
    enum print_output mode;
} Log;

struct SRunner {
    List *slst; /* List of Suite objects */
    TestStats *stats; /* Run statistics */
    List *resultlst; /* List of unit test results */
    const char *log_fname; /* name of log file */
    const char *xml_fname; /* name of xml output file */
    List *loglst; /* list of Log objects */
    enum fork_status fstat; /* controls if suites are forked or not
                               NOTE: Don't use this value directly,
                               instead use srunner_fork_status */
};

void set_fork_status(enum fork_status _fstat);
enum fork_status cur_fork_status(void);

//==> check_log.h <==

void log_srunner_start(SRunner *sr);
void log_srunner_end(SRunner *sr);
void log_suite_start(SRunner *sr, Suite *s);
void log_suite_end(SRunner *sr, Suite *s);
void log_test_end(SRunner *sr, TestResult *tr);

void stdout_lfun(SRunner *sr, FILE *file, enum print_output,
                 void *obj, enum cl_event evt);

void lfile_lfun(SRunner *sr, FILE *file, enum print_output,
                void *obj, enum cl_event evt);

void xml_lfun(SRunner *sr, FILE *file, enum print_output,
              void *obj, enum cl_event evt);

void srunner_register_lfun(SRunner *sr, FILE *lfile, int _close,
                           LFun lfun, enum print_output);

FILE *srunner_open_lfile(SRunner *sr);
FILE *srunner_open_xmlfile(SRunner *sr);
void srunner_init_logging(SRunner *sr, enum print_output print_mode);
void srunner_end_logging(SRunner *sr);

//==> check_msg.h <==

/* Functions implementing messaging during test runs */

void send_failure_info(const char *msg);
void send_loc_info(const char *file, int line);
void send_ctx_info(enum ck_result_ctx ctx);

TestResult *receive_test_result(int waserror);

void setup_messaging(void);
void teardown_messaging(void);

//==> check_pack.h <==

enum ck_msg_type {
    CK_MSG_CTX,
    CK_MSG_FAIL,
    CK_MSG_LOC,
    CK_MSG_LAST
};

typedef struct CtxMsg {
    enum ck_result_ctx ctx;
} CtxMsg;

typedef struct LocMsg {
    int line;
    char *file;
} LocMsg;

typedef struct FailMsg {
    char *msg;
} FailMsg;

typedef union {
    CtxMsg  ctx_msg;
    FailMsg fail_msg;
    LocMsg  loc_msg;
} CheckMsg;

typedef struct RcvMsg {
    enum ck_result_ctx lastctx;
    enum ck_result_ctx failctx;
    char *fixture_file;
    int fixture_line;
    char *test_file;
    int test_line;
    char *msg;
} RcvMsg;

void rcvmsg_free(RcvMsg *rmsg);

int pack(enum ck_msg_type type, char **buf, CheckMsg *msg);
int upack(char *buf, CheckMsg *msg, enum ck_msg_type *type);

void ppack(int fdes, enum ck_msg_type type, CheckMsg *msg);
RcvMsg *punpack(int fdes);

//==> check_print.h <==

void tr_fprint(FILE *file, TestResult *tr, enum print_output print_mode);
void tr_xmlprint(FILE *file, TestResult *tr, enum print_output print_mode);
void srunner_fprint(FILE *file, SRunner *sr, enum print_output print_mode);
enum print_output get_env_printmode(void);

//==> check_str.h <==

/* Return a string representation of the given TestResult.  Return
   value has been malloc'd, and must be freed by the caller */
char *tr_str(TestResult *tr);

/* Return a string representation of the given SRunner's run
   statistics (% passed, num run, passed, errors, failures). Return
   value has been malloc'd, and must be freed by the caller
*/
char *sr_stat_str(SRunner *sr);

char *ck_strdup_printf(const char *fmt, ...)
        __attribute__((format(printf, 1, 2)));

//==> check.c <==

//#include "check_error.h"
//#include "check_list.h"
//#include "check_impl.h"
//#include "check_msg.h"

#ifndef DEFAULT_TIMEOUT
#define DEFAULT_TIMEOUT 3
#endif

int check_major_version = CHECK_MAJOR_VERSION;
int check_minor_version = CHECK_MINOR_VERSION;
int check_micro_version = CHECK_MICRO_VERSION;

static int non_pass(int val);
static Fixture *fixture_create(SFun fun, int ischecked);
static void tcase_add_fixture(TCase *tc, SFun setup, SFun teardown,
                              int ischecked);
static void tr_init(TestResult *tr);
static void suite_free(Suite *s);
static void tcase_free(TCase *tc);

Suite *suite_create(const char *name)
{
    Suite *s;

    s = emalloc(sizeof(Suite)); /* freed in suite_free */
    if (name == NULL)
        s->name = "";
    else
        s->name = name;
    s->tclst = check_list_create();
    return s;
}

static void suite_free(Suite *s)
{
    List *l;

    if (s == NULL)
        return;
    l = s->tclst;
    for (list_front(l); !list_at_end(l); list_advance(l)) {
        tcase_free(list_val(l));
    }
    list_free(s->tclst);
    free(s);
}

TCase *tcase_create(const char *name)
{
    char *env;
    int timeout = DEFAULT_TIMEOUT;
    TCase *tc = emalloc(sizeof(TCase)); /*freed in tcase_free */

    if (name == NULL)
        tc->name = "";
    else
        tc->name = name;

    env = getenv("CK_DEFAULT_TIMEOUT");
    if (env != NULL) {
        int tmp = atoi(env);
        if (tmp >= 0) {
            timeout = tmp;
        }
    }

    tc->timeout = timeout;
    tc->tflst = check_list_create();
    tc->unch_sflst = check_list_create();
    tc->ch_sflst = check_list_create();
    tc->unch_tflst = check_list_create();
    tc->ch_tflst = check_list_create();

    return tc;
}


static void tcase_free(TCase *tc)
{
    list_apply(tc->tflst, free);
    list_apply(tc->unch_sflst, free);
    list_apply(tc->ch_sflst, free);
    list_apply(tc->unch_tflst, free);
    list_apply(tc->ch_tflst, free);
    list_free(tc->tflst);
    list_free(tc->unch_sflst);
    list_free(tc->ch_sflst);
    list_free(tc->unch_tflst);
    list_free(tc->ch_tflst);

    free(tc);
}

void suite_add_tcase(Suite *s, TCase *tc)
{
    if (s == NULL || tc == NULL)
        return;
    list_add_end(s->tclst, tc);
}

void _tcase_add_test(TCase *tc, TFun fn, const char *name, int _signal, int start, int end)
{
    TF *tf;

    if (tc == NULL || fn == NULL || name == NULL)
        return;
    tf = emalloc(sizeof(TF)); /* freed in tcase_free */
    tf->fn = fn;
    tf->loop_start = start;
    tf->loop_end = end;
    tf->signal = _signal; /* 0 means no signal expected */
    tf->name = name;
    list_add_end(tc->tflst, tf);
}

static Fixture *fixture_create(SFun fun, int ischecked)
{
    Fixture *f;

    f = emalloc(sizeof(Fixture));
    f->fun = fun;
    f->ischecked = ischecked;

    return f;
}

void tcase_add_unchecked_fixture(TCase *tc, SFun setup, SFun teardown)
{
    tcase_add_fixture(tc,setup,teardown,0);
}

void tcase_add_checked_fixture(TCase *tc, SFun setup, SFun teardown)
{
    tcase_add_fixture(tc,setup,teardown,1);
}

static void tcase_add_fixture(TCase *tc, SFun setup, SFun teardown,
                              int ischecked)
{
    if (setup) {
        if (ischecked)
            list_add_end(tc->ch_sflst, fixture_create(setup, ischecked));
        else
            list_add_end(tc->unch_sflst, fixture_create(setup, ischecked));
    }

    /* Add teardowns at front so they are run in reverse order. */
    if (teardown) {
        if (ischecked)
            list_add_front(tc->ch_tflst, fixture_create(teardown, ischecked));
        else
            list_add_front(tc->unch_tflst, fixture_create(teardown, ischecked));
    }
}

void tcase_set_timeout(TCase *tc, int timeout)
{
    if (timeout >= 0)
        tc->timeout = timeout;
}

void tcase_fn_start(const char *fname, const char *file, int line)
{
    send_ctx_info(CK_CTX_TEST);
    send_loc_info(file, line);
}

void _mark_point(const char *file, int line)
{
    send_loc_info(file, line);
}

void _fail_unless(int result, const char *file,
                  int line, const char *expr,
                  const char *msg, ...)
{
    send_loc_info(file, line);
    if (!result) {
        va_list ap;
        char buf[BUFSIZ];

        if (msg == NULL || *msg == '\0') {
            snprintf(buf, BUFSIZ, "%s", expr);
        } else {
            va_start(ap,msg);
            vsnprintf(buf, BUFSIZ, msg, ap);
            va_end(ap);
        }
        send_failure_info(buf);
        if (cur_fork_status() == CK_FORK)
            exit(1);
    }
}

SRunner *srunner_create(Suite *s)
{
    SRunner *sr = emalloc(sizeof(SRunner)); /* freed in srunner_free */

    sr->slst = check_list_create();
    if (s != NULL)
        list_add_end(sr->slst, s);
    sr->stats = emalloc(sizeof(TestStats)); /* freed in srunner_free */
    sr->stats->n_checked = sr->stats->n_failed = sr->stats->n_errors = 0;
    sr->resultlst = check_list_create();
    sr->log_fname = NULL;
    sr->xml_fname = NULL;
    sr->loglst = NULL;
    sr->fstat = CK_FORK_UNSPECIFIED;
    return sr;
}

void srunner_add_suite(SRunner *sr, Suite *s)
{
    list_add_end(sr->slst, s);
}

void srunner_free(SRunner *sr)
{
    List *l;
    TestResult *tr;

    if (sr == NULL)
        return;

    free(sr->stats);
    l = sr->slst;
    for (list_front(l); !list_at_end(l); list_advance(l)) {
        suite_free(list_val(l));
    }
    list_free(sr->slst);

    l = sr->resultlst;
    for (list_front(l); !list_at_end(l); list_advance(l)) {
        tr = list_val(l);
        free(tr->file);
        free(tr->msg);
        free(tr);
    }
    list_free(sr->resultlst);

    free(sr);
}

int srunner_ntests_failed(SRunner *sr)
{
    return sr->stats->n_failed + sr->stats->n_errors;
}

int srunner_ntests_run(SRunner *sr)
{
    return sr->stats->n_checked;
}

TestResult **srunner_failures(SRunner *sr)
{
    int i = 0;
    TestResult **trarray;
    List *rlst;

    trarray = malloc(sizeof(trarray[0]) * srunner_ntests_failed(sr));

    rlst = sr->resultlst;
    for (list_front(rlst); !list_at_end(rlst); list_advance(rlst)) {
        TestResult *tr = list_val(rlst);
        if (non_pass(tr->rtype))
            trarray[i++] = tr;

    }
    return trarray;
}

TestResult **srunner_results(SRunner *sr)
{
    int i = 0;
    TestResult **trarray;
    List *rlst;

    trarray = malloc(sizeof(trarray[0]) * srunner_ntests_run(sr));

    rlst = sr->resultlst;
    for (list_front(rlst); !list_at_end(rlst); list_advance(rlst)) {
        trarray[i++] = list_val(rlst);
    }
    return trarray;
}

static int non_pass(int val)
{
    return val != CK_PASS;
}

TestResult *tr_create(void)
{
    TestResult *tr;

    tr = emalloc(sizeof(TestResult));
    tr_init(tr);
    return tr;
}

void tr_reset(TestResult *tr)
{
    tr_init(tr);
}

static void tr_init(TestResult *tr)
{
    tr->ctx = -1;
    tr->line = -1;
    tr->rtype = -1;
    tr->msg = NULL;
    tr->file = NULL;
    tr->tcname = NULL;
    tr->tname = NULL;
}

const char *tr_msg(TestResult *tr)
{
    return tr->msg;
}

int tr_lno(TestResult *tr)
{
    return tr->line;
}

const char *tr_lfile(TestResult *tr)
{
    return tr->file;
}

int tr_rtype(TestResult *tr)
{
    return tr->rtype;
}

enum ck_result_ctx tr_ctx(TestResult *tr)
{
    return tr->ctx;
}

const char *tr_tcname(TestResult *tr)
{
    return tr->tcname;
}

static int _fstat = CK_FORK;

void set_fork_status(enum fork_status lfstat)
{
    if (lfstat == CK_FORK || lfstat == CK_NOFORK)
        _fstat = lfstat;
    else
        eprintf("Bad status in set_fork_status", __FILE__, __LINE__);
}

enum fork_status cur_fork_status(void)
{
    return _fstat;
}

//==> check_error.c <==
//#include "config.h"

#include <stdarg.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <errno.h>

//#include "check_error.h"

void eprintf(const char *fmt, const char *file, int line, ...)
{
    va_list args;

    fflush(stderr);

    fprintf(stderr,"%s:%d: ",file,line);
    va_start(args, line);
    vfprintf(stderr, fmt, args);
    va_end(args);

    /*include system error information if format ends in colon */
    if (fmt[0] != '\0' && fmt[strlen(fmt)-1] == ':')
        fprintf(stderr, " %s", strerror(errno));
    fprintf(stderr, "\n");

    exit(2);
}

void *emalloc(size_t n)
{
    void *p;

    p = malloc(n);
    if (p == NULL)
        eprintf("malloc of %zu bytes failed:", __FILE__, __LINE__, n);
    return p;
}

void *erealloc(void *ptr, size_t n)
{
    void *p;

    p = realloc(ptr, n);
    if (p == NULL)
        eprintf("realloc of %zu bytes failed:", __FILE__, __LINE__, n);
    return p;
}

char *estrdup(const char *str)
{
    size_t n = strlen(str) + 1;
    void *p = malloc(n);

    if (p == NULL)
        eprintf("strdup of %zu bytes failed:", __FILE__, __LINE__, n);

    return memcpy(p, str, n);
}

//==> check_list.c <==
//#include "config.h"

#include <stdlib.h>
#include <string.h>

//#include "check_list.h"
//#include "check_error.h"

enum {
    LINIT = 1,
    LGROW = 2
};

struct List {
    int n_elts;
    int max_elts;
    int current; /* pointer to the current node */
    int last; /* pointer to the node before END */
    const void **data;
};

static void maybe_grow(List *lp)
{
    if (lp->n_elts >= lp->max_elts) {
        lp->max_elts *= LGROW;
        lp->data = erealloc(lp->data, lp->max_elts * sizeof(lp->data[0]));
    }
}

List *check_list_create(void)
{
    List *lp;

    lp = emalloc(sizeof(List));
    lp->n_elts = 0;
    lp->max_elts = LINIT;
    lp->data = emalloc(sizeof(lp->data[0]) * LINIT);
    lp->current = lp->last = -1;
    return lp;
}

void list_add_front(List *lp, const void *val)
{
    if (lp == NULL)
        return;
    maybe_grow(lp);
    memmove(lp->data + 1, lp->data, lp->n_elts * sizeof lp->data[0]);
    lp->last++;
    lp->n_elts++;
    lp->current = 0;
    lp->data[lp->current] = val;
}

void list_add_end(List *lp, const void *val)
{
    if (lp == NULL)
        return;
    maybe_grow(lp);
    lp->last++;
    lp->n_elts++;
    lp->current = lp->last;
    lp->data[lp->current] = val;
}

int list_at_end(List *lp)
{
    if (lp->current == -1)
        return 1;
    else
        return (lp->current > lp->last);
}

void list_front(List *lp)
{
    if (lp->current == -1)
        return;
    lp->current = 0;
}

void list_free(List *lp)
{
    if (lp == NULL)
        return;

    free(lp->data);
    free(lp);
}

void *list_val(List *lp)
{
    if (lp == NULL)
        return NULL;
    if (lp->current == -1 || lp->current > lp->last)
        return NULL;

    return (void*)lp->data[lp->current];
}

void list_advance(List *lp)
{
    if (lp == NULL)
        return;
    if (list_at_end(lp))
        return;
    lp->current++;
}

void list_apply(List *lp, void (*fp)(void *))
{
    if (lp == NULL || fp == NULL)
        return;

    for (list_front(lp); !list_at_end(lp); list_advance(lp))
        fp (list_val(lp));
}

//==> check_log.c <==
//#include "config.h"

#include <stdlib.h>
#include <stdio.h>
#include <sys/time.h>
#include <time.h>
#include <check.h>

//#include "check_error.h"
//#include "check_list.h"
//#include "check_impl.h"
//#include "check_log.h"
//#include "check_print.h"

static void srunner_send_evt(SRunner *sr, void *obj, enum cl_event evt);

void srunner_set_log(SRunner *sr, const char *fname)
{
    if (sr->log_fname)
        return;
    sr->log_fname = fname;
}

int srunner_has_log(SRunner *sr)
{
    return sr->log_fname != NULL;
}

const char *srunner_log_fname(SRunner *sr)
{
    return sr->log_fname;
}

void srunner_set_xml(SRunner *sr, const char *fname)
{
    if (sr->xml_fname)
        return;
    sr->xml_fname = fname;
}

int srunner_has_xml(SRunner *sr)
{
    return sr->xml_fname != NULL;
}

const char *srunner_xml_fname(SRunner *sr)
{
    return sr->xml_fname;
}

void srunner_register_lfun(SRunner *sr, FILE *lfile, int _close,
                           LFun lfun, enum print_output printmode)
{
    Log *l = emalloc(sizeof(Log));

    if (printmode == CK_ENV) {
        printmode = get_env_printmode();
    }

    l->lfile = lfile;
    l->lfun = lfun;
    l->close = _close;
    l->mode = printmode;
    list_add_end(sr->loglst, l);
    return;
}

void log_srunner_start(SRunner *sr)
{
    srunner_send_evt(sr, NULL, CLSTART_SR);
}

void log_srunner_end(SRunner *sr)
{
    srunner_send_evt(sr, NULL, CLEND_SR);
}

void log_suite_start(SRunner *sr, Suite *s)
{
    srunner_send_evt(sr, s, CLSTART_S);
}

void log_suite_end(SRunner *sr, Suite *s)
{
    srunner_send_evt(sr, s, CLEND_S);
}

void log_test_end(SRunner *sr, TestResult *tr)
{
    srunner_send_evt(sr, tr, CLEND_T);
}

static void srunner_send_evt(SRunner *sr, void *obj, enum cl_event evt)
{
    List *l;
    Log *lg;

    l = sr->loglst;
    for (list_front(l); !list_at_end(l); list_advance(l)) {
        lg = list_val(l);
        fflush(lg->lfile);
        lg->lfun(sr, lg->lfile, lg->mode, obj, evt);
        fflush(lg->lfile);
    }
}

void stdout_lfun(SRunner *sr, FILE *file, enum print_output printmode,
                 void *obj, enum cl_event evt)
{
    TestResult *tr;
    Suite *s;

    if (printmode == CK_ENV) {
        printmode = get_env_printmode();
    }

    switch (evt) {
    case CLINITLOG_SR:
        break;
    case CLENDLOG_SR:
        break;
    case CLSTART_SR:
        if (printmode > CK_SILENT) {
            fprintf(file, "Running suite(s):");
        }
        break;
    case CLSTART_S:
        s = obj;
        if (printmode > CK_SILENT) {
            fprintf(file, " %s\n", s->name);
        }
        break;
    case CLEND_SR:
        if (printmode > CK_SILENT) {
            /* we don't want a newline before printing here, newlines should
            come after printing a string, not before.  it's better to add
            the newline above in CLSTART_S.
*/
            srunner_fprint(file, sr, printmode);
        }
        break;
    case CLEND_S:
        s = obj;
        break;
    case CLEND_T:
        tr = obj;
        break;
    default:
        eprintf("Bad event type received in stdout_lfun", __FILE__, __LINE__);
    }
}

void lfile_lfun(SRunner *sr, FILE *file, enum print_output printmode,
                void *obj, enum cl_event evt)
{
    TestResult *tr;
    Suite *s;

    switch (evt) {
    case CLINITLOG_SR:
        break;
    case CLENDLOG_SR:
        break;
    case CLSTART_SR:
        break;
    case CLSTART_S:
        s = obj;
        fprintf(file, "Running suite %s\n", s->name);
        break;
    case CLEND_SR:
        fprintf(file, "Results for all suites run:\n");
        srunner_fprint(file, sr, CK_MINIMAL);
        break;
    case CLEND_S:
        s = obj;
        break;
    case CLEND_T:
        tr = obj;
        tr_fprint(file, tr, CK_VERBOSE);
        break;
    default:
        eprintf("Bad event type received in stdout_lfun", __FILE__, __LINE__);
    }
}

void xml_lfun(SRunner *sr, FILE *file, enum print_output printmode,
              void *obj, enum cl_event evt)
{
    TestResult *tr;
    Suite *s;
    static struct timeval inittv, endtv;
    static char t[sizeof "yyyy-mm-dd hh:mm:ss"] = {0};

    if (t[0] == 0) {
        struct tm now;

        gettimeofday(&inittv, NULL);
        localtime_r(&(inittv.tv_sec), &now);
        strftime(t, sizeof("yyyy-mm-dd hh:mm:ss"), "%Y-%m-%d %H:%M:%S", &now);
    }

    switch (evt) {
    case CLINITLOG_SR:
        fprintf(file, "<?xml version=\"1.0\"?>\n");
        fprintf(file, "<testsuites xmlns=\"http://check.sourceforge.net/ns\">\n");
        fprintf(file, "  <datetime>%s</datetime>\n", t);
        break;
    case CLENDLOG_SR:
        gettimeofday(&endtv, NULL);
        fprintf(file, "  <duration>%f</duration>\n",
                (endtv.tv_sec + (double)endtv.tv_usec / 1000000) -
                (inittv.tv_sec + (double)inittv.tv_usec / 1000000));
        fprintf(file, "</testsuites>\n");
        break;
    case CLSTART_SR:
        break;
    case CLSTART_S:
        s = obj;
        fprintf(file, "  <suite>\n");
        fprintf(file, "    <title>%s</title>\n", s->name);
        break;
    case CLEND_SR:
        break;
    case CLEND_S:
        fprintf(file, "  </suite>\n");
        s = obj;
        break;
    case CLEND_T:
        tr = obj;
        tr_xmlprint(file, tr, CK_VERBOSE);
        break;
    default:
        eprintf("Bad event type received in xml_lfun", __FILE__, __LINE__);
    }
}

FILE *srunner_open_lfile(SRunner *sr)
{
    FILE *f = NULL;

    if (srunner_has_log(sr)) {
        f = fopen(sr->log_fname, "w");
        if (f == NULL)
            eprintf("Could not open log file %s:", __FILE__, __LINE__,
                    sr->log_fname);
    }
    return f;
}

FILE *srunner_open_xmlfile(SRunner *sr)
{
    FILE *f = NULL;

    if (srunner_has_xml(sr)) {
        f = fopen(sr->xml_fname, "w");
        if (f == NULL)
            eprintf("Could not open xml file %s:", __FILE__, __LINE__,
                    sr->xml_fname);
    }
    return f;
}

void srunner_init_logging(SRunner *sr, enum print_output print_mode)
{
    FILE *f;

    sr->loglst = check_list_create();
    srunner_register_lfun(sr, stdout, 0, stdout_lfun, print_mode);
    f = srunner_open_lfile(sr);
    if (f) {
        srunner_register_lfun(sr, f, 1, lfile_lfun, print_mode);
    }
    f = srunner_open_xmlfile(sr);
    if (f) {
        srunner_register_lfun(sr, f, 2, xml_lfun, print_mode);
    }
    srunner_send_evt(sr, NULL, CLINITLOG_SR);
}

void srunner_end_logging(SRunner *sr)
{
    List *l;
    int rval;

    srunner_send_evt(sr, NULL, CLENDLOG_SR);

    l = sr->loglst;
    for (list_front(l); !list_at_end(l); list_advance(l)) {
        Log *lg = list_val(l);
        if (lg->close) {
            rval = fclose(lg->lfile);
            if (rval != 0)
                eprintf("Error closing log file:", __FILE__, __LINE__);
        }
        free(lg);
    }
    list_free(l);
    sr->loglst = NULL;
}

//==> check_msg.c <==

//#include "config.h"

#include <sys/types.h>
#include <unistd.h>
#include <stdlib.h>
#include <fcntl.h>
#include <stdio.h>

//#include "check_error.h"
//#include "check.h"
//#include "check_list.h"
//#include "check_impl.h"
//#include "check_msg.h"
//#include "check_pack.h"

/* 'Pipe' is implemented as a temporary file to overcome message
 * volume limitations outlined in bug #482012. This scheme works well
 * with the existing usage wherein the parent does not begin reading
 * until the child has done writing and exited.
 *
 * Pipe life cycle:
 * - The parent creates a tmpfile().
 * - The fork() call has the effect of duplicating the file descriptor
 *   and copying (on write) the FILE* data structures.
 * - The child writes to the file, and its dup'ed file descriptor and
 *   data structures are cleaned up on child process exit.
 * - Before reading, the parent rewind()'s the file to reset both
 *   FILE* and underlying file descriptor location data.
 * - When finished, the parent fclose()'s the FILE*, deleting the
 *   temporary file, per tmpfile()'s semantics.
 *
 * This scheme may break down if the usage changes to asynchronous
 * reading and writing.
 */

static FILE *send_file1;
static FILE *send_file2;

static FILE *get_pipe(void);
static void setup_pipe(void);
static void teardown_pipe(void);
static TestResult *construct_test_result(RcvMsg *rmsg, int waserror);
static void tr_set_loc_by_ctx(TestResult *tr, enum ck_result_ctx ctx,
                              RcvMsg *rmsg);
static FILE *get_pipe(void)
{
    if (send_file2 != NULL) {
        return send_file2;
    }

    if (send_file1 != NULL) {
        return send_file1;
    }

    eprintf("No messaging setup", __FILE__, __LINE__);

    return NULL;
}

void send_failure_info(const char *msg)
{
    FailMsg fmsg;

    fmsg.msg = (char *) msg;
    ppack(fileno(get_pipe()), CK_MSG_FAIL, (CheckMsg *) &fmsg);
}

void send_loc_info(const char *file, int line)
{
    LocMsg lmsg;

    lmsg.file = (char *) file;
    lmsg.line = line;
    ppack(fileno(get_pipe()), CK_MSG_LOC, (CheckMsg *) &lmsg);
}

void send_ctx_info(enum ck_result_ctx ctx)
{
    CtxMsg cmsg;

    cmsg.ctx = ctx;
    ppack(fileno(get_pipe()), CK_MSG_CTX, (CheckMsg *) &cmsg);
}

TestResult *receive_test_result(int waserror)
{
    FILE *fp;
    RcvMsg *rmsg;
    TestResult *result;

    fp = get_pipe();
    if (fp == NULL)
        eprintf("Couldn't find pipe",__FILE__, __LINE__);
    rewind(fp);
    rmsg = punpack(fileno(fp));
    teardown_pipe();
    setup_pipe();

    result = construct_test_result(rmsg, waserror);
    rcvmsg_free(rmsg);
    return result;
}

static void tr_set_loc_by_ctx(TestResult *tr, enum ck_result_ctx ctx,
                              RcvMsg *rmsg)
{
    if (ctx == CK_CTX_TEST) {
        tr->file = rmsg->test_file;
        tr->line = rmsg->test_line;
        rmsg->test_file = NULL;
        rmsg->test_line = -1;
    } else {
        tr->file = rmsg->fixture_file;
        tr->line = rmsg->fixture_line;
        rmsg->fixture_file = NULL;
        rmsg->fixture_line = -1;
    }
}

static TestResult *construct_test_result(RcvMsg *rmsg, int waserror)
{
    TestResult *tr;

    if (rmsg == NULL)
        return NULL;

    tr = tr_create();

    if (rmsg->msg != NULL || waserror) {
        tr->ctx = (cur_fork_status() == CK_FORK) ? rmsg->lastctx : rmsg->failctx;
        tr->msg = rmsg->msg;
        rmsg->msg = NULL;
        tr_set_loc_by_ctx(tr, tr->ctx, rmsg);
    } else if (rmsg->lastctx == CK_CTX_SETUP) {
        tr->ctx = CK_CTX_SETUP;
        tr->msg = NULL;
        tr_set_loc_by_ctx(tr, CK_CTX_SETUP, rmsg);
    } else {
        tr->ctx = CK_CTX_TEST;
        tr->msg = NULL;
        tr_set_loc_by_ctx(tr, CK_CTX_TEST, rmsg);
    }

    return tr;
}

void setup_messaging(void)
{
    setup_pipe();
}

void teardown_messaging(void)
{
    teardown_pipe();
}

static void setup_pipe(void)
{
    if (send_file1 != NULL) {
        if (send_file2 != NULL)
            eprintf("Only one nesting of suite runs supported", __FILE__, __LINE__);
        send_file2 = tmpfile();
    } else {
        send_file1 = tmpfile();
    }
}

static void teardown_pipe(void)
{
    if (send_file2 != NULL) {
        fclose(send_file2);
        send_file2 = NULL;
    } else if (send_file1 != NULL) {
        fclose(send_file1);
        send_file1 = NULL;
    } else {
        eprintf("No messaging setup", __FILE__, __LINE__);
    }
}

//==> check_pack.c <==

//#include "config.h"

#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <unistd.h>
#include <stdint.h>

//#include "check.h"
//#include "check_error.h"
//#include "check_list.h"
//#include "check_impl.h"
//#include "check_pack.h"

/* typedef an unsigned int that has at least 4 bytes */
typedef uint32_t ck_uint32;

static void  pack_int(char **buf, int val);
static int   upack_int(char **buf);
static void  pack_str(char **buf, const char *str);
static char *upack_str(char **buf);

static int   pack_ctx(char **buf, CtxMsg  *cmsg);
static int   pack_loc(char **buf, LocMsg  *lmsg);
static int   pack_fail(char **buf, FailMsg *fmsg);
static void  upack_ctx(char **buf, CtxMsg  *cmsg);
static void  upack_loc(char **buf, LocMsg  *lmsg);
static void  upack_fail(char **buf, FailMsg *fmsg);

static void  check_type(int type, const char *file, int line);
static enum ck_msg_type upack_type(char **buf);
static void  pack_type(char **buf, enum ck_msg_type type);

static int   read_buf(int fdes, char **buf);
static int   get_result(char *buf, RcvMsg *rmsg);
static void  rcvmsg_update_ctx(RcvMsg *rmsg, enum ck_result_ctx ctx);
static void  rcvmsg_update_loc(RcvMsg *rmsg, const char *file, int line);
static RcvMsg *rcvmsg_create(void);

typedef int  (*pfun)(char **, CheckMsg *);
typedef void (*upfun)(char **, CheckMsg *);

static pfun pftab [] = {
    (pfun)pack_ctx,
    (pfun)pack_fail,
    (pfun)pack_loc
};

static upfun upftab [] = {
    (upfun)upack_ctx,
    (upfun)upack_fail,
    (upfun)upack_loc
};

int pack(enum ck_msg_type type, char **buf, CheckMsg *msg)
{
    if (buf == NULL)
        return -1;
    if (msg == NULL)
        return 0;

    check_type(type, __FILE__, __LINE__);

    return pftab[type](buf, msg);
}

int upack(char *buf, CheckMsg *msg, enum ck_msg_type *type)
{
    char *obuf;
    int nread;

    if (buf == NULL)
        return -1;

    obuf = buf;

    *type = upack_type(&buf);

    check_type(*type, __FILE__, __LINE__);

    upftab[*type](&buf, msg);

    nread = buf - obuf;
    return nread;
}

static void pack_int(char **buf, int val)
{
    unsigned char *ubuf = (unsigned char *) *buf;
    ck_uint32 uval = val;

    ubuf[0] = (uval >> 24) & 0xFF;
    ubuf[1] = (uval >> 16) & 0xFF;
    ubuf[2] = (uval >> 8)  & 0xFF;
    ubuf[3] = uval & 0xFF;

    *buf += 4;
}

static int upack_int(char **buf)
{
    unsigned char *ubuf = (unsigned char *) *buf;
    ck_uint32 uval;

    uval = ((ubuf[0] << 24) | (ubuf[1] << 16) | (ubuf[2] << 8) | ubuf[3]);

    *buf += 4;

    return (int) uval;
}

static void pack_str(char **buf, const char *val)
{
    int strsz;

    if (val == NULL)
        strsz = 0;
    else
        strsz = strlen(val);

    pack_int(buf, strsz);

    if (strsz > 0) {
        memcpy(*buf, val, strsz);
        *buf += strsz;
    }
}

static char *upack_str(char **buf)
{
    char *val;
    int strsz;

    strsz = upack_int(buf);

    if (strsz > 0) {
        val = emalloc(strsz + 1);
        memcpy(val, *buf, strsz);
        val[strsz] = 0;
        *buf += strsz;
    } else {
        val = emalloc(1);
        *val = 0;
    }

    return val;
}

static void pack_type(char **buf, enum ck_msg_type type)
{
    pack_int(buf, (int) type);
}

static enum ck_msg_type upack_type(char **buf)
{
    //return (enum ck_msg_type) upack_int(buf);
    return upack_int(buf);
}

static int pack_ctx(char **buf, CtxMsg *cmsg)
{
    char *ptr;
    int len;

    len = 4 + 4;
    *buf = ptr = emalloc(len);

    pack_type(&ptr, CK_MSG_CTX);
    pack_int(&ptr, (int) cmsg->ctx);

    return len;
}

static void upack_ctx(char **buf, CtxMsg *cmsg)
{
    cmsg->ctx = upack_int(buf);
}

static int pack_loc(char **buf, LocMsg *lmsg)
{
    char *ptr;
    int len;

    len = 4 + 4 + (lmsg->file ? strlen(lmsg->file) : 0) + 4;
    *buf = ptr = emalloc(len);

    pack_type(&ptr, CK_MSG_LOC);
    pack_str(&ptr, lmsg->file);
    pack_int(&ptr, lmsg->line);

    return len;
}

static void upack_loc(char **buf, LocMsg *lmsg)
{
    lmsg->file = upack_str(buf);
    lmsg->line = upack_int(buf);
}

static int pack_fail(char **buf, FailMsg *fmsg)
{
    char *ptr;
    int len;

    len = 4 + 4 + (fmsg->msg ? strlen(fmsg->msg) : 0);
    *buf = ptr = emalloc(len);

    pack_type(&ptr, CK_MSG_FAIL);
    pack_str(&ptr, fmsg->msg);

    return len;
}

static void upack_fail(char **buf, FailMsg *fmsg)
{
    fmsg->msg = upack_str(buf);
}

static void check_type(int type, const char *file, int line)
{
    if (type < 0 || type >= CK_MSG_LAST)
        eprintf("Bad message type arg", file, line);
}

void ppack(int fdes, enum ck_msg_type type, CheckMsg *msg)
{
    char *buf;
    int n;
    ssize_t r;

    n = pack(type, &buf, msg);
    r = write(fdes, buf, n);
    if (r == -1)
        eprintf("Error in ppack:",__FILE__,__LINE__);

    free(buf);
}

static int read_buf(int fdes, char **buf)
{
    char *readloc;
    int n;
    int nread = 0;
    int size = 1;
    int grow = 2;

    *buf = emalloc(size);
    readloc = *buf;
    while (1) {
        n = read(fdes, readloc, size - nread);
        if (n == 0)
            break;
        if (n == -1)
            eprintf("Error in read_buf:", __FILE__, __LINE__);

        nread += n;
        size *= grow;
        *buf = erealloc(*buf,size);
        readloc = *buf + nread;
    }

    return nread;
}


static int get_result(char *buf, RcvMsg *rmsg)
{
    enum ck_msg_type type;
    CheckMsg msg;
    int n;

    n = upack(buf, &msg, &type);
    if (n == -1)
        eprintf("Error in upack", __FILE__, __LINE__);

    if (type == CK_MSG_CTX) {
        CtxMsg *cmsg = (CtxMsg *) &msg;
        rcvmsg_update_ctx(rmsg, cmsg->ctx);
    } else
    if (type == CK_MSG_LOC) {
        LocMsg *lmsg = (LocMsg *) &msg;
        if (rmsg->failctx == (enum ck_result_ctx)-1)
        {
            rcvmsg_update_loc(rmsg, lmsg->file, lmsg->line);
        }
        free(lmsg->file);
    } else
    if (type == CK_MSG_FAIL) {
        FailMsg *fmsg = (FailMsg *) &msg;
        if (rmsg->msg == NULL) {
            rmsg->msg = emalloc(strlen(fmsg->msg) + 1);
            strcpy(rmsg->msg, fmsg->msg);
            rmsg->failctx = rmsg->lastctx;
        } else {
            /* Skip subsequent failure messages, only happens for CK_NOFORK */
        }
        free(fmsg->msg);
    } else {
        check_type(type, __FILE__, __LINE__);
    }

    return n;
}

static void reset_rcv_test(RcvMsg *rmsg)
{
    rmsg->test_line = -1;
    rmsg->test_file = NULL;
}

static void reset_rcv_fixture(RcvMsg *rmsg)
{
    rmsg->fixture_line = -1;
    rmsg->fixture_file = NULL;
}

static RcvMsg *rcvmsg_create(void)
{
    RcvMsg *rmsg;

    rmsg = emalloc(sizeof(RcvMsg));
    rmsg->lastctx = -1;
    rmsg->failctx = -1;
    rmsg->msg = NULL;
    reset_rcv_test(rmsg);
    reset_rcv_fixture(rmsg);
    return rmsg;
}

void rcvmsg_free(RcvMsg *rmsg)
{
    free(rmsg->fixture_file);
    free(rmsg->test_file);
    free(rmsg->msg);
    free(rmsg);
}

static void rcvmsg_update_ctx(RcvMsg *rmsg, enum ck_result_ctx ctx)
{
    if (rmsg->lastctx != (enum ck_result_ctx)-1) {
        free(rmsg->fixture_file);
        reset_rcv_fixture(rmsg);
    }
    rmsg->lastctx = ctx;
}

static void rcvmsg_update_loc(RcvMsg *rmsg, const char *file, int line)
{
    int flen = strlen(file);

    if (rmsg->lastctx == CK_CTX_TEST) {
        free(rmsg->test_file);
        rmsg->test_line = line;
        rmsg->test_file = emalloc(flen + 1);
        strcpy(rmsg->test_file, file);
    } else {
        free(rmsg->fixture_file);
        rmsg->fixture_line = line;
        rmsg->fixture_file = emalloc(flen + 1);
        strcpy(rmsg->fixture_file, file);
    }
}

RcvMsg *punpack(int fdes)
{
    int nread, n;
    char *buf;
    char *obuf;
    RcvMsg *rmsg;

    nread = read_buf(fdes, &buf);
    obuf = buf;
    rmsg = rcvmsg_create();

    while (nread > 0) {
        n = get_result(buf, rmsg);
        nread -= n;
        buf += n;
    }

    free(obuf);
    if (rmsg->lastctx == (enum ck_result_ctx)-1) {
        free(rmsg);
        rmsg = NULL;
    }

    return rmsg;
}

//==> check_print.c <==

//#include "config.h"

#include <stdio.h>
#include <string.h>
#include <stdlib.h>

//#include "check.h"
//#include "check_list.h"
//#include "check_impl.h"
//#include "check_str.h"
//#include "check_print.h"

static void srunner_fprint_summary(FILE *file, SRunner *sr,
                                   enum print_output print_mode);
static void srunner_fprint_results(FILE *file, SRunner *sr,
                                   enum print_output print_mode);


void srunner_print(SRunner *sr, enum print_output print_mode)
{
    srunner_fprint(stdout, sr, print_mode);
}

void srunner_fprint(FILE *file, SRunner *sr, enum print_output print_mode)
{
    if (print_mode == CK_ENV) {
        print_mode = get_env_printmode();
    }

    srunner_fprint_summary(file, sr, print_mode);
    srunner_fprint_results(file, sr, print_mode);
}

static void srunner_fprint_summary(FILE *file, SRunner *sr,
                                   enum print_output print_mode)
{
    if (print_mode >= CK_MINIMAL) {
        char *str;

        str = sr_stat_str(sr);
        fprintf(file, "%s\n", str);
        free(str);
    }
    return;
}

static void srunner_fprint_results(FILE *file, SRunner *sr,
                                   enum print_output print_mode)
{
    List *resultlst;

    resultlst = sr->resultlst;

    for (list_front(resultlst); !list_at_end(resultlst); list_advance(resultlst)) {
        TestResult *tr = list_val(resultlst);
        tr_fprint(file, tr, print_mode);
    }
    return;
}

void tr_fprint(FILE *file, TestResult *tr, enum print_output print_mode)
{
    if (print_mode == CK_ENV) {
        print_mode = get_env_printmode();
    }

    if ((print_mode >= CK_VERBOSE && tr->rtype == CK_PASS) ||
          (tr->rtype != CK_PASS && print_mode >= CK_NORMAL)) {
        char *trstr = tr_str(tr);
        fprintf(file,"%s\n", trstr);
        free(trstr);
    }
}

void tr_xmlprint(FILE *file, TestResult *tr, enum print_output print_mode)
{
    char result[10];
    char *path_name;
    char *file_name;
    char *slash;

    switch (tr->rtype) {
    case CK_PASS:
        strcpy(result, "success");
        break;
    case CK_FAILURE:
        strcpy(result, "failure");
        break;
    case CK_ERROR:
        strcpy(result, "error");
        break;
    }

    slash = strrchr(tr->file, '/');
    if (slash == NULL) {
        path_name = (char*)".";
        file_name = tr->file;
    } else {
        path_name = estrdup(tr->file);
        path_name[slash - tr->file] = 0; /* Terminate the temporary string. */
        file_name = slash + 1;
    }


    fprintf(file, "    <test result=\"%s\">\n", result);
    fprintf(file, "      <path>%s</path>\n", path_name);
    fprintf(file, "      <fn>%s:%d</fn>\n", file_name, tr->line);
    fprintf(file, "      <id>%s</id>\n", tr->tname);
    fprintf(file, "      <iteration>%d</iteration>\n", tr->iter);
    fprintf(file, "      <description>%s</description>\n", tr->tcname);
    fprintf(file, "      <message>%s</message>\n", tr->msg);
    fprintf(file, "    </test>\n");

    if (slash != NULL) {
        free(path_name);
    }
}

enum print_output get_env_printmode(void)
{
    char *env = getenv("CK_VERBOSITY");

    if (env == NULL)
        return CK_NORMAL;
    if (strcmp(env, "silent") == 0)
        return CK_SILENT;
    if (strcmp(env, "minimal") == 0)
        return CK_MINIMAL;
    if (strcmp(env, "verbose") == 0)
        return CK_VERBOSE;
    return CK_NORMAL;
}

//==> check_run.c <==

//#include "config.h"

#include <sys/types.h>
#include <sys/wait.h>
#include <unistd.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <stdarg.h>
#include <signal.h>

//#include "check.h"
//#include "check_error.h"
//#include "check_list.h"
//#include "check_impl.h"
//#include "check_msg.h"
//#include "check_log.h"


enum rinfo {
    CK_R_SIG,
    CK_R_PASS,
    CK_R_EXIT,
    CK_R_FAIL_TEST,
    CK_R_FAIL_FIXTURE
};

enum tf_type {
    CK_FORK_TEST,
    CK_NOFORK_TEST,
    CK_NOFORK_FIXTURE
};

static void srunner_run_init(SRunner *sr, enum print_output print_mode);
static void srunner_run_end(SRunner *sr, enum print_output print_mode);
static void srunner_iterate_suites(SRunner *sr,
                                    enum print_output print_mode);
static void srunner_run_tcase(SRunner *sr, TCase *tc);
static int srunner_run_unchecked_setup(SRunner *sr, TCase *tc);
static void srunner_run_unchecked_teardown(SRunner *sr, TCase *tc);
static TestResult *tcase_run_checked_setup(SRunner *sr, TCase *tc);
static void tcase_run_checked_teardown(TCase *tc);
static void srunner_iterate_tcase_tfuns(SRunner *sr, TCase *tc);
static void srunner_add_failure(SRunner *sr, TestResult *tf);
static TestResult *tcase_run_tfun_fork(SRunner *sr, TCase *tc, TF *tf, int i);
static TestResult *tcase_run_tfun_nofork(SRunner *sr, TCase *tc, TF *tf, int i);
static TestResult *receive_result_info_fork(const char *tcname,
                                            const char *tname,
                                            int iter,
                                            int status, int expected_signal);
static TestResult *receive_result_info_nofork(const char *tcname,
                                              const char *tname,
                                              int iter);
static void set_fork_info(TestResult *tr, int status, int expected_signal);
static void set_nofork_info(TestResult *tr);
static char *signal_msg(int sig);
static char *signal_error_msg(int signal_received, int signal_expected);
static char *pass_msg(void);
static char *exit_msg(int exitstatus);
static int waserror(int status, int expected_signal);

#define MSG_LEN 100

static int alarm_received;
static pid_t group_pid;

static void sig_handler(int sig_nr)
{
    switch (sig_nr) {
    case SIGALRM:
        alarm_received = 1;
        killpg(group_pid, SIGKILL);
        break;
    default:
        eprintf("Unhandled signal: %d", __FILE__, __LINE__, sig_nr);
    }
}

static void srunner_run_init(SRunner *sr, enum print_output print_mode)
{
    set_fork_status(srunner_fork_status(sr));
    setup_messaging();
    srunner_init_logging(sr, print_mode);
    log_srunner_start(sr);
}

static void srunner_run_end(SRunner *sr, enum print_output print_mode)
{
    log_srunner_end(sr);
    srunner_end_logging(sr);
    teardown_messaging();
    set_fork_status(CK_FORK);
}

static void srunner_iterate_suites(SRunner *sr,
                                    enum print_output print_mode)

{
    List *slst;
    List *tcl;
    TCase *tc;

    slst = sr->slst;

    for (list_front(slst); !list_at_end(slst); list_advance(slst)) {
        Suite *s = list_val(slst);

        log_suite_start(sr, s);

        tcl = s->tclst;

        for (list_front(tcl);!list_at_end(tcl); list_advance(tcl)) {
            tc = list_val(tcl);
            srunner_run_tcase(sr, tc);
        }

        log_suite_end(sr, s);
    }
}

void srunner_run_all(SRunner *sr, enum print_output print_mode)
{
    struct sigaction old_action;
    struct sigaction new_action;

    if (sr == NULL)
        return;
    if ((int)print_mode < 0 || print_mode >= CK_LAST)
        eprintf("Bad print_mode argument to srunner_run_all: %d",
                __FILE__, __LINE__, print_mode);

    memset(&new_action, 0, sizeof new_action);
    new_action.sa_handler = sig_handler;
    sigaction(SIGALRM, &new_action, &old_action);
    srunner_run_init(sr, print_mode);
    srunner_iterate_suites(sr, print_mode);
    srunner_run_end(sr, print_mode);
    sigaction(SIGALRM, &old_action, NULL);
}

static void srunner_add_failure(SRunner *sr, TestResult *tr)
{
    list_add_end(sr->resultlst, tr);
    /* If the context is either of these, the test has run. */
    if ((tr->ctx == CK_CTX_TEST) || (tr->ctx == CK_CTX_TEARDOWN))
        sr->stats->n_checked++;
    if (tr->rtype == CK_FAILURE)
        sr->stats->n_failed++;
    else if (tr->rtype == CK_ERROR)
        sr->stats->n_errors++;
}

static void srunner_iterate_tcase_tfuns(SRunner *sr, TCase *tc)
{
    List *tfl;
    TF *tfun;
    TestResult *tr = NULL;

    tfl = tc->tflst;

    for (list_front(tfl); !list_at_end(tfl); list_advance(tfl)) {
        int i;
        tfun = list_val(tfl);

        for (i = tfun->loop_start; i < tfun->loop_end; i++) {
            switch (srunner_fork_status(sr)) {
            case CK_FORK:
                tr = tcase_run_tfun_fork(sr, tc, tfun, i);
                break;
            case CK_NOFORK:
                tr = tcase_run_tfun_nofork(sr, tc, tfun, i);
                break;
            default:
                eprintf("Bad fork status in SRunner", __FILE__, __LINE__);
            }
            srunner_add_failure(sr, tr);
            log_test_end(sr, tr);
        }
    }
}

static int srunner_run_unchecked_setup(SRunner *sr, TCase *tc)
{
    TestResult *tr;
    List *l;
    Fixture *f;
    int rval = 1;

    set_fork_status(CK_NOFORK);

    l = tc->unch_sflst;

    for (list_front(l); !list_at_end(l); list_advance(l)) {
        send_ctx_info(CK_CTX_SETUP);
        f = list_val(l);
        f->fun();

        tr = receive_result_info_nofork(tc->name, "unchecked_setup", 0);

        if (tr->rtype != CK_PASS) {
            srunner_add_failure(sr, tr);
            rval = 0;
            break;
        }
        free(tr->file);
        free(tr->msg);
        free(tr);
    }

    set_fork_status(srunner_fork_status(sr));
    return rval;
}

static TestResult *tcase_run_checked_setup(SRunner *sr, TCase *tc)
{
    TestResult *tr = NULL;
    List *l;
    Fixture *f;
    enum fork_status fstat_ = srunner_fork_status(sr);

    l = tc->ch_sflst;
    if (fstat_ == CK_FORK) {
        send_ctx_info(CK_CTX_SETUP);
    }

    for (list_front(l); !list_at_end(l); list_advance(l)) {
        if (fstat_ == CK_NOFORK) {
            send_ctx_info(CK_CTX_SETUP);
        }
        f = list_val(l);
        f->fun();

        /* Stop the setup and return the failure if nofork mode. */
        if (fstat_ == CK_NOFORK) {
            tr = receive_result_info_nofork(tc->name, "checked_setup", 0);
            if (tr->rtype != CK_PASS) {
                break;
            }

            free(tr->file);
            free(tr->msg);
            free(tr);
            tr = NULL;
        }
    }

    return tr;
}

static void tcase_run_checked_teardown(TCase *tc)
{
    List *l;
    Fixture *f;

    l = tc->ch_tflst;

    send_ctx_info(CK_CTX_TEARDOWN);

    for (list_front(l); !list_at_end(l); list_advance(l)) {
        f = list_val(l);
        f->fun();
    }
}

static void srunner_run_unchecked_teardown(SRunner *sr, TCase *tc)
{
    List *l;
    Fixture *f;

    set_fork_status(CK_NOFORK);
    l = tc->unch_tflst;

    for (list_front(l); !list_at_end(l); list_advance(l)) {
        f = list_val(l);
        send_ctx_info(CK_CTX_TEARDOWN);
        f->fun();
    }
    set_fork_status(srunner_fork_status(sr));
}

static void srunner_run_tcase(SRunner *sr, TCase *tc)
{
    if (srunner_run_unchecked_setup(sr,tc)) {
        srunner_iterate_tcase_tfuns(sr,tc);
        srunner_run_unchecked_teardown(sr, tc);
    }
}

static TestResult *receive_result_info_fork(const char *tcname,
                                            const char *tname,
                                            int iter,
                                            int status, int expected_signal)
{
    TestResult *tr;

    tr = receive_test_result(waserror(status, expected_signal));
    if (tr == NULL)
        eprintf("Failed to receive test result", __FILE__, __LINE__);
    tr->tcname = tcname;
    tr->tname = tname;
    tr->iter = iter;
    set_fork_info(tr, status, expected_signal);

    return tr;
}

static TestResult *receive_result_info_nofork(const char *tcname,
                                              const char *tname,
                                              int iter)
{
    TestResult *tr;

    tr = receive_test_result(0);
    if (tr == NULL)
        eprintf("Failed to receive test result", __FILE__, __LINE__);
    tr->tcname = tcname;
    tr->tname = tname;
    tr->iter = iter;
    set_nofork_info(tr);

    return tr;
}

static void set_fork_info(TestResult *tr, int status, int signal_expected)
{
    int was_sig = WIFSIGNALED(status);
    int was_exit = WIFEXITED(status);
    int exit_status = WEXITSTATUS(status);
    int signal_received = WTERMSIG(status);

    if (was_sig) {
        if (signal_expected == signal_received) {
            if (alarm_received) {
                /* Got alarm instead of signal */
                tr->rtype = CK_ERROR;
                tr->msg = signal_error_msg(signal_received, signal_expected);
            } else {
                tr->rtype = CK_PASS;
                tr->msg = pass_msg();
            }
        } else
        if (signal_expected != 0) {
            /* signal received, but not the expected one */
            tr->rtype = CK_ERROR;
            tr->msg = signal_error_msg(signal_received, signal_expected);
        } else {
            /* signal received and none expected */
            tr->rtype = CK_ERROR;
            tr->msg = signal_msg(signal_received);
        }
    } else if (signal_expected == 0) {
        if (was_exit && exit_status == 0) {
            tr->rtype = CK_PASS;
            tr->msg = pass_msg();
        } else
        if (was_exit && exit_status != 0) {
            if (tr->msg == NULL) { /* early exit */
                tr->rtype = CK_ERROR;
                tr->msg = exit_msg(exit_status);
            } else {
                tr->rtype = CK_FAILURE;
            }
        }
    } else { /* a signal was expected and none raised */
        if (was_exit) {
            tr->msg = exit_msg(exit_status);
            if (exit_status == 0)
                tr->rtype = CK_FAILURE; /* normal exit status */
            else
                tr->rtype = CK_FAILURE; /* early exit */
        }
    }
}

static void set_nofork_info(TestResult *tr)
{
    if (tr->msg == NULL) {
        tr->rtype = CK_PASS;
        tr->msg = pass_msg();
    } else {
        tr->rtype = CK_FAILURE;
    }
}

static TestResult *tcase_run_tfun_nofork(SRunner *sr, TCase *tc, TF *tfun, int i)
{
    TestResult *tr;

    tr = tcase_run_checked_setup(sr, tc);
    if (tr == NULL) {
        tfun->fn(i);
        tcase_run_checked_teardown(tc);
        return receive_result_info_nofork(tc->name, tfun->name, i);
    }

    return tr;
}

static TestResult *tcase_run_tfun_fork(SRunner *sr, TCase *tc, TF *tfun, int i)
{
    pid_t pid_w;
    pid_t pid;
    int status = 0;

    pid = fork();
    if (pid == -1)
        eprintf("Unable to fork:", __FILE__, __LINE__);
    if (pid == 0) {
        setpgid(0, 0);
        group_pid = getpgid(0);
        tcase_run_checked_setup(sr, tc);
        tfun->fn(i);
        tcase_run_checked_teardown(tc);
        exit(EXIT_SUCCESS);
    } else {
        group_pid = getpgid(pid);
    }

    alarm_received = 0;
    alarm(tc->timeout);
    do {
        pid_w = waitpid(pid, &status, 0);
    } while (pid_w == -1);

    killpg(pid, SIGKILL); /* Kill remaining processes. */

    return receive_result_info_fork(tc->name, tfun->name, i, status, tfun->signal);
}

static char *signal_error_msg(int signal_received, int signal_expected)
{
    char *sig_r_str;
    char *sig_e_str;
    char *msg = emalloc(MSG_LEN); /* free'd by caller */

    sig_r_str = estrdup(strsignal(signal_received));
    sig_e_str = estrdup(strsignal(signal_expected));

    if (alarm_received) {
        snprintf(msg, MSG_LEN, "Test timeout expired, expected signal %d (%s)",
                 signal_expected, sig_e_str);
    } else {
        snprintf(msg, MSG_LEN, "Received signal %d (%s), expected %d (%s)",
                 signal_received, sig_r_str, signal_expected, sig_e_str);
    }
    free(sig_r_str);
    free(sig_e_str);
    return msg;
}

static char *signal_msg(int sig)
{
    char *msg = emalloc(MSG_LEN); /* free'd by caller */

    if (alarm_received) {
        snprintf(msg, MSG_LEN, "Test timeout expired");
    } else {
        snprintf(msg, MSG_LEN, "Received signal %d (%s)",
                 sig, strsignal(sig));
    }
    return msg;
}

static char *exit_msg(int exitval)
{
    char *msg = emalloc(MSG_LEN); /* free'd by caller */

    snprintf(msg, MSG_LEN,
             "Early exit with return value %d", exitval);
    return msg;
}

static char *pass_msg(void)
{
    char *msg = emalloc(sizeof("Passed"));
    strcpy(msg, "Passed");
    return msg;
}

enum fork_status srunner_fork_status(SRunner *sr)
{
    if (sr->fstat == CK_FORK_UNSPECIFIED) {
        char *env = getenv("CK_FORK");

        if (env == NULL)
            return CK_FORK;
        if (strcmp(env,"no") == 0)
            return CK_NOFORK;
        else
            return CK_FORK;
    } else {
        return sr->fstat;
    }
}

void srunner_set_fork_status(SRunner *sr, enum fork_status fstat_)
{
    sr->fstat = fstat_;
}

pid_t check_fork(void)
{
    pid_t pid = fork();

    /* Set the process to a process group to be able to kill it easily. */
    setpgid(pid, group_pid);
    return pid;
}

void check_waitpid_and_exit(pid_t pid)
{
    pid_t pid_w;
    int status;

    if (pid > 0) {
        do {
            pid_w = waitpid(pid, &status, 0);
        } while (pid_w == -1);
        if (waserror(status, 0))
            exit(EXIT_FAILURE);
    }
    exit(EXIT_SUCCESS);
}

static int waserror(int status, int signal_expected)
{
    int was_sig = WIFSIGNALED(status);
    int was_exit = WIFEXITED(status);
    int exit_status = WEXITSTATUS(status);
    int signal_received = WTERMSIG(status);

    return ((was_sig && (signal_received != signal_expected)) ||
            (was_exit && exit_status != 0));
}

//==> check_str.c <==

//#include "config.h"

#include <stdio.h>
#include <stdarg.h>

//#include "check.h"
//#include "check_list.h"
//#include "check_error.h"
//#include "check_impl.h"
//#include "check_str.h"

static const char *tr_type_str(TestResult *tr);
static int percent_passed(TestStats *t);

char *tr_str(TestResult *tr)
{
    const char *exact_msg;
    char *rstr;

    exact_msg = (tr->rtype == CK_ERROR) ? "(after this point) ": "";

    rstr = ck_strdup_printf("%s:%d:%s:%s:%s:%d: %s%s",
                            tr->file, tr->line,
                            tr_type_str(tr), tr->tcname, tr->tname, tr->iter,
                            exact_msg, tr->msg);

    return rstr;
}

char *sr_stat_str(SRunner *sr)
{
    char *str;
    TestStats *ts;

    ts = sr->stats;

    str = ck_strdup_printf("%d%%: Checks: %d, Failures: %d, Errors: %d",
                           percent_passed(ts), ts->n_checked, ts->n_failed,
                           ts->n_errors);

    return str;
}

char *ck_strdup_printf(const char *fmt, ...)
{
    /* Guess we need no more than 100 bytes. */
    int n, size = 100;
    char *p;
    va_list ap;

    p = emalloc(size);

    while (1) {
        /* Try to print in the allocated space. */
        va_start(ap, fmt);
        n = vsnprintf(p, size, fmt, ap);
        va_end(ap);
        /* If that worked, return the string. */
        if (n > -1 && n < size)
            return p;

        /* Else try again with more space. */
        if (n > -1)   /* C99 conform vsnprintf() */
            size = n + 1; /* precisely what is needed */
        else          /* glibc 2.0 */
            size *= 2;  /* twice the old size */

        p = erealloc(p, size);
    }
}

static const char *tr_type_str(TestResult *tr)
{
    const char *str = NULL;

    if (tr->ctx == CK_CTX_TEST) {
        if (tr->rtype == CK_PASS)
            str = "P";
        else
        if (tr->rtype == CK_FAILURE)
            str = "F";
        else
        if (tr->rtype == CK_ERROR)
            str = "E";
    } else {
        str = "S";
    }

    return str;
}

static int percent_passed(TestStats *t)
{
    if (t->n_failed == 0 && t->n_errors == 0)
        return 100;
    else
    if (t->n_checked == 0)
        return 0;
    else
        return (int)((double)(t->n_checked - (t->n_failed + t->n_errors)) /
                     (double)t->n_checked * 100);
}
