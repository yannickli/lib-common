/**************************************************************************/
/*                                                                        */
/*  Copyright (C) INTERSEC SA                                             */
/*                                                                        */
/*  Should you receive a copy of this source code, you must check you     */
/*  have a proper, written authorization of INTERSEC to hold it. If you   */
/*  don't have such an authorization, you must DELETE all source code     */
/*  files in your possession, and inform INTERSEC of the fact you obtain  */
/*  these files. Should you not comply to these terms, you can be         */
/*  prosecuted in the extent permitted by applicable law.                 */
/*                                                                        */
/**************************************************************************/

#include "el.h"

static struct {
    el_t  stopper;
    int   stopme;
} stopper_g;

static void stopper_dbl_timer(el_t ev, el_data_t priv)
{
    stopper_g.stopme = 1;
}

static void stopper_handler(el_t ev, int signum, el_data_t priv)
{
    if (!stopper_g.stopme) {
        e_info("^C requesting soft termination");
    }

    if (stopper_g.stopme == 2) {
        e_info("^C abort.");
        exit(1);
    }

    stopper_g.stopme = 2;
    el_unref(el_timer_register(1000, 0, 0, &stopper_dbl_timer, NULL));
    e_info("^C Soft termination already requested. "
           "Double-interrupt to abort");
}

void el_stopper_register(void)
{
    stopper_g.stopper = el_signal_register(SIGINT, &stopper_handler, NULL);
    el_unref(stopper_g.stopper);
}
void el_stopper_unregister(void)
{
    el_signal_unregister(&stopper_g.stopper);
}

bool el_stopper_is_waiting(void)
{
    return stopper_g.stopme != 0;
}
