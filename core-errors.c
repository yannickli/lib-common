/**************************************************************************/
/*                                                                        */
/*  Copyright (C) 2004-2019 INTERSEC SA                                   */
/*                                                                        */
/*  Should you receive a copy of this source code, you must check you     */
/*  have a proper, written authorization of INTERSEC to hold it. If you   */
/*  don't have such an authorization, you must DELETE all source code     */
/*  files in your possession, and inform INTERSEC of the fact you obtain  */
/*  these files. Should you not comply to these terms, you can be         */
/*  prosecuted in the extent permitted by applicable law.                 */
/*                                                                        */
/**************************************************************************/

#include <execinfo.h> /* backtrace_symbols_fd */

#include "core.h"
#include "unix.h"

#define XWRITE(s)  IGNORE(xwrite(fd, s, strlen(s)))

/** XXX The backtrace() function calls an init() function which uses malloc()
 * and leads to deadlock in the signals handler. So we always call backtrace()
 * once outside of the signals hander. It's an horrible hack but it works.
 */
__attribute__((constructor)) static void fix_backtrace_init(void)
{
    void *arr[256];

    backtrace(arr, countof(arr));
}

void ps_dump_backtrace(int signum, const char *prog, int fd, bool full)
{
    char  buf[256];
    void *arr[256];
    int   bt, n;

    if (signum >= 0) {
        n = snprintf(buf, sizeof(buf), "---> %s[%d] %s\n\n",
                     prog, getpid(), sys_siglist[signum]);
    } else {
        n = snprintf(buf, sizeof(buf), "---> %s[%d]\n\n",
                     prog, getpid());
    }
    if (xwrite(fd, buf, n) < 0) {
        return;
    }

    bt = backtrace(arr, countof(arr));
    backtrace_symbols_fd(arr, bt, fd);

    if (full) {
        int maps_fd = open("/proc/self/smaps", O_RDONLY);

        if (maps_fd != -1) {
            XWRITE("\n--- Memory maps:\n\n");
            for (;;) {
                n = read(maps_fd, buf, sizeof(buf));
                if (n < 0) {
                    if (ERR_RW_RETRIABLE(errno)) {
                        continue;
                    }
                    break;
                }
                if (n == 0) {
                    break;
                }
                if (xwrite(fd, buf, n) < 0) {
                    break;
                }
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

void ps_write_backtrace(int signum, bool allow_fork)
{
    char  path[PATH_MAX];
    int   fd;
    int   saved_errno = errno;

    snprintf(path, sizeof(path), "/tmp/%s.%d.%ld.debug",
             program_invocation_short_name, (uint32_t)time(NULL),
             (long)getpid());
    fd = open(path, O_EXCL | O_CREAT | O_WRONLY | O_TRUNC, 0600);

    if (fd >= 0) {
        int main_versions_printed = 0;
        char buf[256];

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

        snprintf(buf, sizeof(buf), "\n--- errno: %s (%d)\n",
                 strerror(saved_errno), saved_errno);
        XWRITE(buf);

        errno = saved_errno;
        ps_dump_backtrace(signum, program_invocation_short_name, fd, true);
        p_close(&fd);

        if (allow_fork) {
            ps_print_file(path,
                          "echo '\n--- File descriptors (using ls):\n'");
            ps_print_file(path, "ls -al /proc/self/fd");

            ps_print_file(path,
                          "echo '\n--- File descriptors (using lsof):\n'");
            ps_print_file(path, "lsof -p %d", getpid());
        }
    }
#ifndef NDEBUG
    errno = saved_errno;
    ps_dump_backtrace(signum, program_invocation_short_name,
                      STDERR_FILENO, false);
#endif
    errno = saved_errno;
}

#undef XWRITE
