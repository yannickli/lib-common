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
#ifdef HAVE_SYS_INOTIFY_H
#include <sys/inotify.h>
#endif


typedef struct ev_t *el_t;

/* XXX el_data_t is deprecated and will be removed in a future version
 * of the lib-common.
 */
typedef data_t el_data_t;

typedef void (el_cb_f)(el_t, data_t);
typedef void (el_signal_f)(el_t, int, data_t);
typedef void (el_child_f)(el_t, pid_t, int, data_t);
typedef int  (el_fd_f)(el_t, int, short, data_t);
typedef void (el_proxy_f)(el_t, short, data_t);
typedef void (el_fs_watch_f)(el_t, uint32_t mask, uint32_t cookie,
                             lstr_t name, data_t);
typedef void (el_worker_f)(int timeout);

#ifdef __has_blocks
typedef void (BLOCK_CARET el_cb_b)(el_t);
typedef void (BLOCK_CARET el_signal_b)(el_t, int);
typedef void (BLOCK_CARET el_child_b)(el_t, pid_t, int);
typedef int  (BLOCK_CARET el_fd_b)(el_t, int, short);
typedef void (BLOCK_CARET el_proxy_b)(el_t, short);
typedef void (BLOCK_CARET el_fs_watch_b)(el_t, uint32_t, uint32_t, lstr_t);
#endif

el_t el_blocker_register(void) __leaf;
el_t el_before_register_d(el_cb_f *, data_t) __leaf;
el_t el_idle_register_d(el_cb_f, data_t) __leaf;
el_t el_signal_register_d(int signo, el_signal_f *, data_t) __leaf;
el_t el_child_register_d(pid_t pid, el_child_f *, data_t) __leaf;

#ifdef __has_blocks
/* The block based API takes a block version of the callback and a second
 * optional block called when the el_t is unregistered. The purpose of this
 * second block is to wipe() the environment of the callback.
 *
 * You cannot change the blocks attached to an el_t after registration (that
 * is, el_set_priv and el_*_set_hook cannot be used on el_t initialized with
 * blocks.
 */

el_t el_before_register_blk(el_cb_b, block_t wipe) __leaf;
el_t el_idle_register_blk(el_cb_b, block_t wipe) __leaf;
el_t el_signal_register_blk(int signo, el_signal_b, block_t) __leaf;
el_t el_child_register_blk(pid_t pid, el_child_b, block_t) __leaf;
#endif

static inline el_t el_before_register(el_cb_f *f, void *ptr) {
    return el_before_register_d(f, (data_t){ ptr });
}
static inline el_t el_idle_register(el_cb_f *f, void *ptr) {
    return el_idle_register_d(f, (data_t){ ptr });
}
static inline el_t el_signal_register(int signo, el_signal_f *f, void *ptr) {
    return el_signal_register_d(signo, f, (data_t){ ptr });
}
static inline el_t el_child_register(pid_t pid, el_child_f *f, void *ptr) {
    return el_child_register_d(pid, f, (data_t){ ptr });
}

void el_before_set_hook(el_t, el_cb_f *) __leaf;
void el_idle_set_hook(el_t, el_cb_f *) __leaf;
void el_signal_set_hook(el_t, el_signal_f *) __leaf;
void el_child_set_hook(el_t, el_child_f *) __leaf;

data_t el_before_unregister(el_t *);
data_t el_idle_unregister(el_t *);
data_t el_signal_unregister(el_t *);
data_t el_child_unregister(el_t *);
void      el_blocker_unregister(el_t *);

/*----- idle related -----*/
void el_idle_unpark(el_t) __leaf;

/*----- child related -----*/
pid_t el_child_getpid(el_t) __leaf __attribute__((pure));

/*----- proxy related -----*/
el_t el_proxy_register_d(el_proxy_f *, data_t) __leaf;
#ifdef __has_blocks
el_t el_proxy_register_blk(el_proxy_b, block_t) __leaf;
#endif
static inline el_t el_proxy_register(el_proxy_f *f, void *ptr) {
    return el_proxy_register_d(f, (data_t){ ptr });
}
void el_proxy_set_hook(el_t, el_proxy_f *) __leaf;
data_t el_proxy_unregister(el_t *);
short el_proxy_set_event(el_t, short mask) __leaf;
short el_proxy_clr_event(el_t, short mask) __leaf;
short el_proxy_set_mask(el_t, short mask) __leaf;

/*----- stopper API -----*/
void el_stopper_register(void) __leaf;
bool el_stopper_is_waiting(void) __leaf;
void el_stopper_unregister(void) __leaf;

/*----- fd related -----*/
extern struct rlimit fd_limit_g;

typedef enum ev_priority_t {
    EV_PRIORITY_LOW    = 0,
    EV_PRIORITY_NORMAL = 1,
    EV_PRIORITY_HIGH   = 2
} ev_priority_t;

el_t el_fd_register_d(int fd, short events, el_fd_f *, data_t)
    __leaf;
#ifdef __has_blocks
el_t el_fd_register_blk(int fd, short events, el_fd_b, block_t)
    __leaf;
#endif
static inline el_t el_fd_register(int fd, short events, el_fd_f *f, void *ptr) {
    return el_fd_register_d(fd, events, f, (data_t){ ptr });
}
void el_fd_set_hook(el_t, el_fd_f *)
    __leaf;
data_t el_fd_unregister(el_t *, bool do_close);

int   el_fd_loop(el_t, int timeout);
short el_fd_get_mask(el_t) __leaf __attribute__((pure));
short el_fd_set_mask(el_t, short events) __leaf;
int   el_fd_get_fd(el_t) __leaf __attribute__((pure));
void  el_fd_mark_fired(el_t) __leaf;

ev_priority_t el_fd_set_priority(el_t, ev_priority_t priority);
#define el_fd_set_priority(el, prio)  \
    (el_fd_set_priority)((el), EV_PRIORITY_##prio)

/*
 * \param[in]  ev       a file descriptor el_t.
 * \param[in]  mask     the POLL* mask of events that resets activity to 0
 * \param[in]  timeout
 *    what to do with the activity watch timer:
 *    - 0 means unregister the activity timer ;
 *    - >0 means register (or reset) the activity timer with this timeout in
 *      miliseconds;
 *    - < 0 means reset the activity timer using the timeout it was registered
 *      with. In particular if no activity timer is set up for this given file
 *      descriptor el_t, then this is a no-op.
 */
#define EL_EVENTS_NOACT  ((short)-1)
int   el_fd_watch_activity(el_t, short mask, int timeout) __leaf;


#ifdef HAVE_SYS_INOTIFY_H
/**
 * \defgroup el_fs_watch FS activity notifications
 * \{
 */

/** Register a new watch for a list of events on a given path.
 *
 *  \warning you must not add more that one watch for a given path.
 */
el_t el_fs_watch_register_d(const char *, uint32_t, el_fs_watch_f *, data_t);
#ifdef __has_blocks
el_t el_fs_watch_register_blk(const char *, uint32_t, el_fs_watch_b, block_t);
#endif
static inline el_t el_fs_watch_register(const char *path, uint32_t flags,
                                        el_fs_watch_f *f, void *ptr)
{
    return el_fs_watch_register_d(path, flags, f, (data_t){ ptr });
}

data_t el_fs_watch_unregister(el_t *);

int el_fs_watch_change(el_t el, uint32_t flags);

/** \} */
#endif

/**
 * \defgroup el_timers Event Loop timers
 * \{
 */

enum {
    EL_TIMER_NOMISS = (1 << 0),
    EL_TIMER_LOWRES = (1 << 1),
};


/** \brief registers a timer
 *
 * There are two kinds of timers: one shot and repeating timers:
 * - One shot timers fire their callback once, when they time out.
 * - Repeating timers automatically rearm after being fired.
 *
 * One shot timers are automatically destroyed at the end of the callback if
 * they have not be rearmed in it. As a consequence, you must be careful to
 * cleanup all references to one-shot timers you don't rearm within the
 * callback.
 *
 * \param[in]  next    relative time in ms at which the timers fires.
 * \param[in]  repeat  repeat interval in ms, 0 means single shot.
 * \param[in]  flags   timer related flags (nomiss, lowres, ...)
 * \param[in]  cb      callback to call upon timer expiry.
 * \param[in]  priv    private data.
 * \return the timer handler descriptor.
 */
el_t el_timer_register_d(int next, int repeat, int flags, el_cb_f *, data_t)
    __leaf;
#ifdef __has_blocks
el_t el_timer_register_blk(int next, int repeat, int flags, el_cb_b, block_t)
    __leaf;
#endif
static inline
el_t el_timer_register(int next, int repeat, int flags, el_cb_f *f, void *ptr)
{
    return el_timer_register_d(next, repeat, flags, f, (data_t){ ptr });
}
bool el_timer_is_repeated(el_t ev) __leaf __attribute__((pure));

/** \brief restart a single shot timer.
 *
 * Note that if the timer hasn't expired yet, it just sets it to a later time.
 *
 * \param[in]  el      timer handler descriptor.
 * \param[in]  next    relative time in ms at wich the timers will fire.
 */
void el_timer_restart(el_t, int next) __leaf;
void el_timer_set_hook(el_t, el_cb_f *) __leaf;
/** \brief unregisters (and cancels) a timer given its index.
 *
 * \param[in]  el      the timer handler descriptor to cancel.
 * \return the private previously registered with the handler.
 *
 */
data_t el_timer_unregister(el_t *);
/**\}*/

el_t el_ref(el_t) __leaf;
el_t el_unref(el_t) __leaf;
#ifndef NDEBUG
bool el_set_trace(el_t, bool trace) __leaf;
#else
#define el_set_trace(ev, trace)
#endif
data_t el_set_priv(el_t, data_t) __leaf;

void el_bl_use(void) __leaf;
void el_bl_lock(void);
void el_bl_unlock(void);

/** Define the worker function.
 *
 * The worker is a unique function that get called whenever the el_loop would
 * get block waiting for activity. The worker can handle any workload it
 * wants, but it must ensure that:
 *  - it returns after the given timeout.
 *  - it returns when the event loop has new activity (pollable using
 *    \ref el_has_pending_events.
 *
 * \param[in] worker The new worker (NULL to unset the current worker)
 * \return The previous worker (NULL if there were no worker)
 */
el_worker_f *el_set_worker(el_worker_f *worker);

void el_cond_wait(pthread_cond_t *);
void el_cond_signal(pthread_cond_t *) __leaf;

void el_loop(void);
void el_unloop(void) __leaf;
void el_loop_timeout(int msecs);
bool el_has_pending_events(void);

/**\}*/
/* Module {{{ */

MODULE_DECLARE(el);

/* }}} */
#endif
