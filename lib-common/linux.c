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

void unix_initialize(void)
{
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
}

int pid_get_starttime(pid_t pid, struct timeval *tv)
{
    unsigned long long starttime;
    FILE *stat = pidproc_open(pid ? pid : getpid(), "stat");
    int res;

    if (!stat)
        return -1;

    if (!hertz) {
        unix_initialize();
    }

    /* see proc(3), here we want starttime */
    res = fscanf(stat,
                 "%*d %*s %*c %*d %*d %*d %*d %*d %*u %*u %*u %*u %*u %*u "
                 "%*u %*d %*d %*d %*d %*d 0 %llu ", &starttime);
    p_fclose(&stat);

    if (res < 1) {
        errno = EINVAL;
        return -1;
    }

    jiffies_to_tv(starttime, tv);
    tv->tv_sec += boot_time;

    return 0;
}
