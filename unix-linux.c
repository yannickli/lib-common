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

/* holds the linux-dependent implementations for unix.h functions */

#include <sys/syscall.h>
#include <sys/wait.h>
#include "unix.h"
#include "datetime.h"

#ifdef __linux__

#define XWRITE(s)  IGNORE(xwrite(fd, s, strlen(s)))

void ps_dump_core_of_current_thread(void)
{
    pid_t pid = fork();

    if (!pid) {
        abort();
    } else
    if (pid > 0) {
        while (waitpid(pid, NULL, 0) < 0) {
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
#undef XWRITE

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

static int hertz;

static FILE *pidproc_open(pid_t pid, const char *what)
{
    char path[PATH_MAX];
    snprintf(path, sizeof(path), "/proc/%ld/%s", (long)pid ?: (long)getpid(), what);
    return fopen(path, "r");
}

static void jiffies_to_tv(unsigned long long jiff, struct timeval *tv)
{
    if (!hertz)
        hertz = sysconf(_SC_CLK_TCK);

    tv->tv_sec  = jiff / hertz;
    tv->tv_usec = (jiff % hertz) * (1000000UL / hertz);
}

int pid_get_starttime(pid_t pid, struct timeval *tv)
{
    unsigned long long starttime;
    FILE *pidstat = pidproc_open(pid ? pid : getpid(), "stat");
    static int boot_time;
    int res;

    if (!pidstat)
        return -1;

    if (!hertz)
        hertz = sysconf(_SC_CLK_TCK);

    /* see proc(3), here we want starttime */
    res = fscanf(pidstat,
                 "%*d %*s %*c %*d %*d %*d %*d %*d %*u %*u %*u %*u %*u %*u "
                 "%*u %*d %*d %*d %*d %*d 0 %llu ", &starttime);
    p_fclose(&pidstat);

    if (res < 1) {
        errno = EINVAL;
        return -1;
    }

    if (!boot_time) {
        char buf[BUFSIZ];
        const char *p;
        FILE *f = fopen("/proc/stat", "r");

        if (!f)
            e_panic("cannot open /proc/stat");

        while (fgets(buf, sizeof(buf), f)) {
            if (strstart(buf, "btime", &p)) {
                boot_time = strtoip(p, NULL);
                break;
            }
        }
        p_fclose(&f);
        if (boot_time == 0)
            e_panic("could not parse boot time");
    }

    jiffies_to_tv(starttime, tv);
    tv->tv_sec += boot_time;

    return 0;
}

int close_fds_higher_than(int fd)
{
    DIR *proc_fds;
    struct dirent *entry;
    const char *p;
    int n, my_fd;

    proc_fds = opendir("/proc/self/fd");
    if (proc_fds) {
        /* XXX: opendir opens a fd. Do not close it while using it */
        my_fd = dirfd(proc_fds);
        while ((entry = readdir(proc_fds)) != NULL) {
            n = strtol(entry->d_name, &p, 10);
            if (!*p && n > fd && n != my_fd) {
                close(n);
            }
        }
        closedir(proc_fds);
    } else {
        /* Fall back to unix.c implementation */
        int maxfd = sysconf(_SC_OPEN_MAX);

        if (maxfd == -1) {
            /* Highly unlikely sysconf failure: default to 1024 */
            maxfd = 1024;
        }
        while (++fd < maxfd) {
            close(fd);
        }
    }
    return 0;
}

#ifdef SYS_eventfd

int eventfd(int initialvalue, int flags)
{
    int fd = RETHROW(syscall(SYS_eventfd, initialvalue));

    fd_set_features(fd, flags);
    return fd;
}

#else

int eventfd(int initialvalue, int flags)
{
    return -1;
}

#endif

#endif
