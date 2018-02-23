/**************************************************************************/
/*                                                                        */
/*  Copyright (C) 2004-2018 INTERSEC SA                                   */
/*                                                                        */
/*  Should you receive a copy of this source code, you must check you     */
/*  have a proper, written authorization of INTERSEC to hold it. If you   */
/*  don't have such an authorization, you must DELETE all source code     */
/*  files in your possession, and inform INTERSEC of the fact you obtain  */
/*  these files. Should you not comply to these terms, you can be         */
/*  prosecuted in the extent permitted by applicable law.                 */
/*                                                                        */
/**************************************************************************/

#include "ztst-swiftc.h"
#include "el.h"
#include "unix.h"

MODULE_SWIFT(swift_from_c, 12, swiftc, 6)

static int c_from_swift_initialize(void *arg)
{
    e_trace(0, "blih init");
    return 0;
}

static int c_from_swift_shutdown(void)
{
    e_trace(0, "blih shutdown");
    return 0;
}

MODULE_BEGIN(c_from_swift)
MODULE_END()

static void on_term(el_t ev, int signo, el_data_t arg)
{
}

int main(void)
{
    struct sigaction sa = {
        .sa_flags   = SA_RESTART,
        .sa_handler = SIG_IGN,
    };

    sigaction(SIGHUP,  &sa, NULL);
    sigaction(SIGUSR1, &sa, NULL);
    sigaction(SIGPIPE, &sa, NULL);
    sigaction(SIGALRM, &sa, NULL);

    el_signal_register(SIGTERM, on_term, NULL);
    el_signal_register(SIGINT,  on_term, NULL);
    el_signal_register(SIGQUIT, on_term, NULL);

    ps_install_panic_sighandlers();

    MODULE_REQUIRE(swift_from_c);
    el_loop();
    MODULE_RELEASE(swift_from_c);
    return 0;
}
