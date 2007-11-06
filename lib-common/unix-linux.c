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

/* holds the linux-dependant implementations for unix.h functions */

#include <limits.h>
#include <unistd.h>
#include <elf.h>
#include <errno.h>
#include <fcntl.h>

#include "err_report.h"
#include "macros.h"
#include "string_is.h"
#include "unix.h"
#include "timeval.h"

static int hertz;
static int boot_time;

static FILE *pidproc_open(pid_t pid, const char *what)
{
    char path[PATH_MAX];
    snprintf(path, sizeof(path), "/proc/%d/%s", pid ?: getpid(), what);
    return fopen(path, "r");
}

/* for ELF executables, notes are pushed before environment and args */
static int find_elf_note(unsigned long findme, unsigned long *out)
{
    unsigned long *ep = (unsigned long *)__environ;
    while (*ep++);
    while (*ep) {
        if (ep[0] == findme) {
            *out = ep[1];
            return 0;
        }
        ep += 2;
    }
    return -1;
}

static void jiffies_to_tv(unsigned long long jiff, struct timeval *tv)
{
    if (!hertz) {
        unix_initialize();
    }

    tv->tv_sec  = jiff / hertz;
    tv->tv_usec = (jiff % hertz) * (1000000UL / hertz);
}

bool is_fd_open(int fd)
{
    return fcntl(fd, F_GETFD) != -1 || errno != EBADF;
}

void unix_initialize(void)
{
    struct timeval tv;
    gettimeofday(&tv, NULL);
    srand(tv.tv_usec);

    /* get the HZ value, needs linux 2.4 ;) */
    {
        unsigned long l;

        if (find_elf_note(AT_CLKTCK, &l))
            e_panic("Can't find ELF AT_CLKTCK note");
        hertz = l;
    }

    /* get the boot_time value */
    {
        char buf[BUFSIZ];
        const char *p;
        FILE *f = fopen("/proc/stat", "r");

        if (!f)
            e_panic("Can't open /proc/stat");
        while (fgets(buf, sizeof(buf), f)) {
            if (strstart(buf, "btime", &p)) {
                boot_time = strtoip(p, NULL);
                break;
            }
        }
        p_fclose(&f);
        if (boot_time == 0)
            e_panic("Could not parse boot time");
    }

    if (!is_fd_open(STDIN_FILENO)) {
        int fd = open("/dev/null", O_RDONLY);
        if (fd != STDIN_FILENO) {
            dup2(fd, STDIN_FILENO);
            close(fd);
        }
    }
    if (!is_fd_open(STDOUT_FILENO)) {
        int fd = open("/dev/null", O_WRONLY);
        if (fd != STDOUT_FILENO) {
            dup2(fd, STDOUT_FILENO);
            close(fd);
        }
    }
    if (!is_fd_open(STDERR_FILENO)) {
        int fd = open("/dev/null", O_WRONLY);
        if (fd != STDERR_FILENO) {
            dup2(fd, STDERR_FILENO);
            close(fd);
        }
    }
}

int pid_get_starttime(pid_t pid, struct timeval *tv)
{
    unsigned long long starttime;
    FILE *pidstat = pidproc_open(pid ? pid : getpid(), "stat");
    int res;

    if (!pidstat)
        return -1;

    if (!hertz) {
        unix_initialize();
    }

    /* see proc(3), here we want starttime */
    res = fscanf(pidstat,
                 "%*d %*s %*c %*d %*d %*d %*d %*d %*u %*u %*u %*u %*u %*u "
                 "%*u %*d %*d %*d %*d %*d 0 %llu ", &starttime);
    p_fclose(&pidstat);

    if (res < 1) {
        errno = EINVAL;
        return -1;
    }

    jiffies_to_tv(starttime, tv);
    tv->tv_sec += boot_time;

    return 0;
}
