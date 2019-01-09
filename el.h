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

#ifndef IS_LIB_COMMON_EL_H
#define IS_LIB_COMMON_EL_H

/** \defgroup lc_el Intersec Event Loop
 *
 * \brief This module provides a very efficient, OS-independant event loop.
 *
 * \{
 */

/** \file lib-inet/el.h
 * \brief Event Loop module header.
 */

#include "core.h"
#ifdef HAVE_SYS_POLL_H
#include <sys/poll.h>
#else
# define POLLIN      0x0001    /* There is data to read        */
# define POLLPRI     0x0002    /* There is urgent data to read */
# define POLLOUT     0x0004    /* Writing now will not block   */
# define POLLERR     0x0008    /* Error condition              */
# define POLLHUP     0x0010    /* Hung up                      */
# define POLLNVAL    0x0020    /* Invalid request: fd not open */
#endif
#ifndef POLLRDHUP
# ifdef OS_LINUX
#  define POLLRDHUP 0x2000
# else
#  define POLLRDHUP 0
# endif
#endif
#define POLLINOUT  (POLLIN | POLLOUT)

typedef struct ev_t *el_t;

typedef union el_data_t {
    uint64_t u64;
    void *ptr;
} el_data_t __attribute__((transparent_union));

typedef void (el_cb_f)(el_t, el_data_t);
typedef void (el_signal_f)(el_t, int, el_data_t);
typedef void (el_child_f)(el_t, pid_t, int, el_data_t);
typedef int  (el_fd_f)(el_t, int, short, el_data_t);
typedef void (el_proxy_f)(el_t, short, el_data_t);

el_t el_blocker_register(el_data_t);
el_t el_before_register(el_cb_f *, el_data_t);
el_t el_after_register(el_cb_f *, el_data_t);
el_t el_signal_register(int signo, el_signal_f *, el_data_t);
el_t el_child_register(pid_t pid, el_child_f *, el_data_t);

void el_before_set_hook(el_t, el_cb_f *);
void el_after_set_hook(el_t, el_cb_f *);
void el_signal_set_hook(el_t, el_signal_f *);
void el_child_set_hook(el_t, el_child_f *);

el_data_t el_before_unregister(el_t *);
el_data_t el_after_unregister(el_t *);
el_data_t el_signal_unregister(el_t *);
el_data_t el_child_unregister(el_t *);
el_data_t el_blocker_unregister(el_t *);

/*----- child related -----*/
pid_t el_child_getpid(el_t);

/*----- proxy related -----*/
el_t el_proxy_register(el_proxy_f *, el_data_t);
void el_proxy_set_hook(el_t, el_proxy_f *);
el_data_t el_proxy_unregister(el_t *);
short el_proxy_set_event(el_t, short mask);
short el_proxy_clr_event(el_t, short mask);
short el_proxy_set_mask(el_t, short mask);

/*----- stopper API -----*/
void el_stopper_register(void);
bool el_stopper_is_waiting(void);
void el_stopper_unregister(void);

/*----- fd related -----*/
el_t el_fd_register(int fd, short events, el_fd_f *, el_data_t);
void el_fd_set_hook(el_t, el_fd_f *);
el_data_t el_fd_unregister(el_t *, bool do_close);

int   el_fd_loop(el_t, int timeout);
short el_fd_get_mask(el_t);
short el_fd_set_mask(el_t, short events);
int   el_fd_get_fd(el_t);

/**
 * \defgroup el_timers Event Loop timers
 * \{
 */

enum {
    EL_TIMER_NOMISS = (1 << 0),
    EL_TIMER_LOWRES = (1 << 1),
};

/** \brief registers a timer.
 *
 * \param[in]  next    relative time in ms at which the timers fires.
 * \param[in]  repeat  repeat interval in ms, 0 means single shot.
 * \param[in]  flags   timer related flags (nomiss, lowres, ...)
 * \param[in]  cb      callback to call upon timer expiry.
 * \param[in]  priv    private data.
 * \return the timer handler descriptor.
 */
el_t el_timer_register(int next, int repeat, int flags, el_cb_f *, el_data_t);
/** \brief restart a single shot timer.
 *
 * Note that if the timer hasn't expired yet, it just sets it to a later time.
 *
 * \param[in]  el      timer handler descriptor.
 * \param[in]  next    relative time in ms at wich the timers will fire.
 */
void el_timer_restart(el_t, int next);
void el_timer_set_hook(el_t, el_cb_f *);
/** \brief unregisters (and cancels) a timer given its index.
 *
 * \param[in]  el      the timer handler descriptor to cancel.
 * \return the private previously registered with the handler.
 *
 */
el_data_t el_timer_unregister(el_t *);
/**\}*/

el_t el_ref(el_t);
el_t el_unref(el_t);
el_data_t el_set_priv(el_t, el_data_t);

void el_bl_use(void);
void el_bl_lock(void);
void el_bl_unlock(void);

void el_cond_wait(pthread_cond_t *);
void el_cond_signal(pthread_cond_t *);

void el_loop(void);
void el_unloop(void);
void el_loop_timeout(int msecs);

/**\}*/
#endif
