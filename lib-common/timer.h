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

#ifndef IS_LIB_COMMON_TIMER_H
#define IS_LIB_COMMON_TIMER_H

#include <time.h>

#include "macros.h"

typedef void (istimer_func)(void *data);

typedef struct istimer_t istimer_t;

int istimers_initialize(void);
void istimers_shutdown(void);
int istimers_dispatch(const struct timeval *now);

istimer_t *istimer_add_absolute(const struct timeval *now,
                                const struct timeval *date,
                                istimer_func *callback, void *data);
istimer_t *istimer_add_relative(const struct timeval *now, int nbsec,
                                istimer_func *callback, void *data);
void istimer_cancel(istimer_t **timer);

/*[ CHECK ]::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::{{{*/
#ifdef CHECK
#include <check.h>

Suite *check_timer_suite(void);

#endif
/*:::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::}}}*/
#endif /* IS_LIB_COMMON_XML_H */
