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

#include <sys/wait.h>

#include "thr.h"
#include "unix.h"

pid_t psinfo_get_tracer_pid(pid_t pid)
{
    return _psinfo_get_tracer_pid(pid);
}

void ps_dump_core_of_current_thread(void)
{
    pid_t pid = ifork();

    if (!pid) {
        abort();
    } else
    if (pid > 0) {
        while (waitpid(pid, NULL, 0) < 0) {
        }
        if (thr_is_on_queue(thr_queue_main_g)) {
            MODULE_METHOD_RUN_INT(at_fork_on_child_terminated, pid);
        }
    }
}

void ps_panic_sighandler(int signum, siginfo_t *si, void *addr)
{
    static const struct sigaction sa = {
        .sa_flags   = SA_RESTART,
        .sa_handler = SIG_DFL,
    };

    PROTECT_ERRNO(sigaction(signum, &sa, NULL));
    ps_write_backtrace(signum, true);
    raise(signum);
}

void ps_install_panic_sighandlers(void)
{
#ifndef __has_asan
    struct sigaction sa = {
        .sa_flags     = SA_RESTART | SA_SIGINFO,
        .sa_sigaction = &ps_panic_sighandler,
    };

    sigaction(SIGABRT,   &sa, NULL);
    sigaction(SIGILL,    &sa, NULL);
    sigaction(SIGFPE,    &sa, NULL);
    sigaction(SIGSEGV,   &sa, NULL);
    sigaction(SIGBUS,    &sa, NULL);
#if defined(__linux__)
    sigaction(SIGSTKFLT, &sa, NULL);
#endif
#endif
}
