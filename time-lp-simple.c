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

#include "time.h"

static struct {
    char sec_str[24];
    time_t sec;
} lp_time_g;

const char *lp_getsec_str(void)
{
    return lp_time_g.sec_str;
}

time_t lp_getsec(void)
{
    if (unlikely(!lp_time_g.sec))
        return time(NULL);
    return lp_time_g.sec;
}

void lp_gettv(struct timeval *tv)
{
    gettimeofday(tv, NULL);
    if (lp_time_g.sec != tv->tv_sec) {
        lp_time_g.sec = tv->tv_sec;
        (sprintf)(lp_time_g.sec_str, "%lld", (long long)lp_time_g.sec);
    }
}
