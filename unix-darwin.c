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

#ifdef __APPLE__

#include "unix.h"
#include "datetime.h"

struct timeval starttime_g;

int pid_get_starttime(pid_t pid, struct timeval *tv)
{
    if (pid == getpid()) {
        *tv = starttime_g;
        return 0;
    }
    return -1;
}

static __attribute__((constructor))
void init_starttime(void)
{
    lp_gettv(&starttime_g);
}

#endif

