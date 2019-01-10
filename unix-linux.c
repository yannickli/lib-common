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

#include <dirent.h>
#include <execinfo.h> /* backtrace_symbols_fd */
#include "unix.h"
#include "datetime.h"

#ifdef __linux__

#define XWRITE(s)  IGNORE(xwrite(fd, s, strlen(s)))

void ps_dump_backtrace(int signum, const char *prog, int fd, bool full)
{
    char  buf[256];
    void *arr[256];
    int   bt, n;
    int   bkp_errno = errno;

    n = snprintf(buf, sizeof(buf), "---> %s[%d] %s\n\n",
                 prog, getpid(), sys_siglist[signum]);
    if (xwrite(fd, buf, n) < 0)
        return;

    bt = backtrace(arr, countof(arr));
    backtrace_symbols_fd(arr, bt, fd);

    if (full) {
        int maps_fd;

        n = snprintf(buf, sizeof(buf), "\n--- Status:\n\nErrno: %s (%d)\n\n",
                     strerror(bkp_errno), bkp_errno);
        if (xwrite(fd, buf, n) < 0) {
            return;
        }

        maps_fd = open("/proc/self/smaps", O_RDONLY);
        if (maps_fd != -1) {
            XWRITE("\n--- Memory maps:\n\n");
            for (;;) {
                n = read(maps_fd, buf, sizeof(buf));
                if (n < 0) {
                    if (ERR_RW_RETRIABLE(errno))
                        continue;
                    break;
                }
                if (n == 0)
                    break;
                if (xwrite(fd, buf, n) < 0)
                    break;
            }
            close(maps_fd);
        }
    } else {
        XWRITE("\n");
    }
}

static void
ps_panic_sighandler_print_version(int fd, const core_version_t *version)
{
    XWRITE(version->name);
    XWRITE(" version: ");
    XWRITE(version->version);
    XWRITE(" (");
    XWRITE(version->git_revision);
    XWRITE(")");
    XWRITE("\n");
}

__attr_printf__(2, 3)
static void ps_print_file(const char *path, const char *fmt, ...)
{
    va_list va;
    char cmd[BUFSIZ];
    int len;

    va_start(va, fmt);
    len = vsnprintf(cmd, sizeof(cmd), fmt, va);
    va_end(va);

    snprintf(cmd + len, sizeof(cmd) - len, " >> %s", path);

    IGNORE(system(cmd));
}

/** XXX The backtrace() function calls an init() function which uses malloc()
 * and leads to deadlock in the signals handler. So we always call backtrace()
 * once outside of the signals hander. It's an horrible hack but it works.
 */
__attribute__((constructor)) static void fix_backtrace_init(void)
{
    void *arr[256];

    backtrace(arr, countof(arr));
}

void ps_panic_sighandler(int signum, siginfo_t *si, void *addr)
{
    static const struct sigaction sa = {
        .sa_flags   = SA_RESTART,
        .sa_handler = SIG_DFL,
    };

    char   path[PATH_MAX];
    int    fd;
    int    bkp_errno = errno;

    sigaction(signum, &sa, NULL);
    snprintf(path, sizeof(path), "/tmp/%s.%d.%ld.debug",
             program_invocation_short_name, (uint32_t)time(NULL),
             (long)getpid());
    fd = open(path, O_EXCL | O_CREAT | O_WRONLY | O_TRUNC, 0600);

    if (fd >= 0) {
        int main_versions_printed = 0;

        for (int i = 0; i < core_versions_nb_g; i++) {
            const core_version_t *version = &core_versions_g[i];

            if (version->is_main_version) {
                ps_panic_sighandler_print_version(fd, version);
                main_versions_printed++;
            }
        }
        if (main_versions_printed > 0) {
            XWRITE("\n");
        }
        for (int i = 0; i < core_versions_nb_g; i++) {
            const core_version_t *version = &core_versions_g[i];

            if (!version->is_main_version) {
                ps_panic_sighandler_print_version(fd, version);
            }
        }
        XWRITE("\n");

        errno = bkp_errno;
        ps_dump_backtrace(signum, program_invocation_short_name, fd, true);
        p_close(&fd);

        ps_print_file(path, "echo '\n--- File descriptors (using ls):\n'");
        ps_print_file(path, "ls -al /proc/self/fd");

        ps_print_file(path, "echo '\n--- File descriptors (using lsof):\n'");
        ps_print_file(path, "lsof -p %d", getpid());
    }
#ifndef NDEBUG
    errno = bkp_errno;
    ps_dump_backtrace(signum, program_invocation_short_name,
                      STDERR_FILENO, false);
#endif
    errno = bkp_errno;
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

#endif
