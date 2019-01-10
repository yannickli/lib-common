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

#ifdef __sun

/* procfs is not supported in large file mode */
#undef _LARGEFILE64_SOURCE
#undef _FILE_OFFSET_BITS

#include "err_report.h"
#include "macros.h"
#include "unix.h"
#include "time.h"

#include <procfs.h>

int pid_get_starttime(pid_t pid, struct timeval *tv)
{
    int fd;
    psinfo_t info;

    fd = open("/proc/self/psinfo", O_RDONLY);
    if (!fd)
        return -1;

    if (read(fd, &info, sizeof(info)) != sizeof(info)) {
        p_close(&fd);
        return -1;
    }
    p_close(&fd);

    return info.pr_start.tv_sec;
}

#endif
