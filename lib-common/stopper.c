/**************************************************************************/
/*                                                                        */
/*  Copyright (C) 2004-2007 INTERSEC SAS                                  */
/*                                                                        */
/*  Should you receive a copy of this source code, you must check you     */
/*  have a proper, written authorization of INTERSEC to hold it. If you   */
/*  don't have such an authorization, you must DELETE all source code     */
/*  files in your possession, and inform INTERSEC of the fact you obtain  */
/*  these files. Should you not comply to these terms, you can be         */
/*  prosecuted in the extent permitted by applicable law.                 */
/*                                                                        */
/**************************************************************************/

#include <stdlib.h>
#include <signal.h>

#include "err_report.h"
#include "timeval.h"
#include "stopper.h"

typedef void (*sighandler_t)(int);

static struct {
    bool stopme;
    sighandler_t old;
} stopper = { false, NULL };

static void stopper_handler(int signum)
{
    const  struct timeval double_press_delay = {0, 500 * 1000};
    static struct timeval double_press_end;
    struct timeval now;

    if (!stopper.stopme) {
        e_info("^C requesting soft termination");
        stopper.stopme = true;
        gettimeofday(&double_press_end, NULL);
        double_press_end = timeval_add(double_press_end, double_press_delay);
    } else {
        gettimeofday(&now, NULL);
        if (!is_expired(&double_press_end, &now, NULL)) {
            /* The user really wants to abort. Do it. */
            e_info("^C abort. ");
            exit(1);
        } else {
            e_info("^C Soft termination already requested. "
                   "Double-interrupt to abort");
            double_press_end = timeval_add(now, double_press_delay);
        }
    }

    if (stopper.old)
        (*stopper.old)(signum);
}

bool is_stopper_waiting(void)
{
    return stopper.stopme;
}

void stopper_initialize(void)
{
    stopper.old = signal(SIGINT, &stopper_handler);
}

void stopper_shutdown(void)
{
    signal(SIGINT, stopper.old);
    stopper.old = NULL;
}
