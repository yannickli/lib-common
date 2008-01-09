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

#include "mem.h"
#include "list.h"
#include "timeval.h"
#include "timer.h"

#define TIMERS_MAX_DELTA_SEC  20


struct istimer_t {
    istimer_t *prev;
    istimer_t *next;
    struct timeval duedate;
    int nbsec;

    istimer_func *func;
    void *data;
};

/* TODO: Could use a pool for istimer_t elements */
GENERIC_FUNCTIONS(istimer_t, istimer);

DLIST_FUNCTIONS(istimer_t, istimer);

/** Timer list array:
 *  (<1 sec) (<2 sec) (<3 sec) ... (<19 sec) (<20 sec)
 **/
static struct istimers_g {
    istimer_t *istimers[TIMERS_MAX_DELTA_SEC];
} istimers_g;

int istimers_initialize(void)
{
    p_clear(istimers_g.istimers, TIMERS_MAX_DELTA_SEC);
    return 0;
}

void istimers_shutdown(void)
{
}

void istimer_cancel(istimer_t **istimer)
{
    if (*istimer) {
        istimer_list_take(&istimers_g.istimers[(*istimer)->nbsec], *istimer);
        istimer_delete(istimer);
    }
}


int istimers_dispatch(const struct timeval *now)
{
    int i = 0;
    istimer_t *t;

    for (i = 0; i < TIMERS_MAX_DELTA_SEC; i++) {
        for (;;) {
            t = istimers_g.istimers[i];
            if (!t)
                break;

            if (!is_expired(&t->duedate, now, NULL))
                break;

            t->func(t->data);
            istimer_cancel(&t);
        }
    }

    return 0;
}

istimer_t *istimer_add_absolute(const struct timeval *now,
                                const struct timeval *date,
                                istimer_func *callback,
                                void *data)
{
    int msec = timeval_diff(date, now) / 1000000;

    return istimer_add_relative(now, msec, callback, data);
}

istimer_t *istimer_add_relative(const struct timeval *now, int nbsec,
                            istimer_func *callback, void *data)
{
    istimer_t *t;

    if (nbsec < 0 || nbsec > TIMERS_MAX_DELTA_SEC)
        return NULL;

    t = istimer_new();

    t->duedate = *now;
    t->duedate.tv_sec  += nbsec;
    t->nbsec = nbsec;
    t->func = callback;
    t->data = data;

    istimer_list_append(&istimers_g.istimers[nbsec], t);

    return t;
}

/*[ CHECK ]::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::{{{*/
#ifdef CHECK
/* {{{*/
#include <check.h>

static struct timercheck_g {
    int count;
} timercheck_g;

static void test_istimer_func(void *data)
{
    timercheck_g.count++;
}

START_TEST(check_timer)
{
    struct timeval now;
    struct timeval testdate;

    istimers_initialize();

    timercheck_g.count = 0;

    gettimeofday(&now, NULL);
    testdate = now;
    testdate.tv_sec += 1;

    fail_if(!istimer_add_absolute(&now, &testdate, &test_istimer_func, NULL),
            "Could not create trimer");

    gettimeofday(&now, NULL);
    istimers_dispatch(&now);
    fail_if(timercheck_g.count != 0, "timer triggered too soon\n");
    usleep(100);
    gettimeofday(&now, NULL);
    istimers_dispatch(&now);
    fail_if(timercheck_g.count != 0, "timer triggered too soon\n");
    usleep(100000);
    gettimeofday(&now, NULL);
    istimers_dispatch(&now);
    fail_if(timercheck_g.count != 0, "timer triggered too soon\n");
    usleep(900000);
    gettimeofday(&now, NULL);
    istimers_dispatch(&now);
    fail_if(timercheck_g.count != 1, "timer not triggered\n");

    istimers_shutdown();
}
END_TEST


Suite *check_timer_suite(void)
{
    Suite *s  = suite_create("Timer");
    TCase *tc = tcase_create("Core");

    suite_add_tcase(s, tc);
    tcase_add_test(tc, check_timer);
    return s;
}
#endif
