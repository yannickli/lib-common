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

#include <pthread.h>
#include <sys/epoll.h>
#include "unix.h"

static struct {
    int fd;
    bool at_fork_registered;
    int generation;
} el_epoll_g = {
    .fd = -1,
};

static void el_fd_at_fork(void)
{
    p_close(&el_epoll_g.fd);
    el_epoll_g.generation++;
}

static void el_fd_initialize(void)
{
    if (unlikely(el_epoll_g.fd == -1)) {
#ifdef SIGPIPE
        signal(SIGPIPE, SIG_IGN);
#endif
        el_epoll_g.fd = epoll_create(1024);
        if (el_epoll_g.fd < 0)
            e_panic(E_UNIXERR("epoll_create"));
        fd_set_features(el_epoll_g.fd, O_CLOEXEC);

        if (!el_epoll_g.at_fork_registered) {
            pthread_atfork(NULL, NULL, el_fd_at_fork);
            el_epoll_g.at_fork_registered = true;
        }
    }
}

el_t el_fd_register_d(int fd, short events, el_fd_f *cb, el_data_t priv)
{
    ev_t *ev = el_create(EV_FD, cb, priv, true);
    struct epoll_event event = {
        .data.ptr = ev,
        .events   = events,
    };

    el_fd_initialize();
    ev->fd = fd;
    ev->generation = el_epoll_g.generation;
    ev->events_wanted = events;
    ev->priority = EV_PRIORITY_NORMAL;
    if (unlikely(epoll_ctl(el_epoll_g.fd, EPOLL_CTL_ADD, fd, &event)))
        e_panic(E_UNIXERR("epoll_ctl"));
    return ev;
}

short el_fd_set_mask(ev_t *ev, short events)
{
    short old = ev->events_wanted;

    if (EV_IS_TRACED(ev)) {
        e_trace(0, "ev-fd(%p): set mask to %s%s", ev,
                events & POLLIN ? "IN" : "", events & POLLOUT ? "OUT" : "");
    }
    CHECK_EV_TYPE(ev, EV_FD);
    if (old != events && likely(ev->generation == el_epoll_g.generation)) {
        struct epoll_event event = {
            .data.ptr = ev,
            .events   = ev->events_wanted = events,
        };
        if (unlikely(epoll_ctl(el_epoll_g.fd, EPOLL_CTL_MOD, ev->fd, &event)))
            e_panic(E_UNIXERR("epoll_ctl"));
    }
    return old;
}

ev_priority_t (el_fd_set_priority)(ev_t *ev, ev_priority_t priority)
{
    ev_priority_t p = ev->priority;

    if (EV_IS_TRACED(ev)) {
        static lstr_t priority_str[] = {
            [EV_PRIORITY_LOW]    = LSTR_IMMED("LOW"),
            [EV_PRIORITY_NORMAL] = LSTR_IMMED("NORMAL"),
            [EV_PRIORITY_HIGH]   = LSTR_IMMED("HIGH"),
        };

        e_trace(0, "ev-fd(%p): set priority to %*pM", ev,
                LSTR_FMT_ARG(priority_str[priority]));
    }
    CHECK_EV_TYPE(ev, EV_FD);
    ev->priority = priority;
    return p;
}

el_data_t el_fd_unregister(ev_t **evp, bool do_close)
{
    if (*evp) {
        ev_t *ev = *evp;

        CHECK_EV_TYPE(ev, EV_FD);
        if (el_epoll_g.generation == ev->generation) {
            epoll_ctl(el_epoll_g.fd, EPOLL_CTL_DEL, ev->fd, NULL);
        }
        if (likely(do_close))
            close(ev->fd);
        if (EV_FLAG_HAS(ev, FD_WATCHED))
            el_fd_act_timer_unregister(ev->priv.ptr);
        return el_destroy(evp, false);
    }
    return (el_data_t)NULL;
}

static void el_loop_fds(int timeout)
{
    ev_priority_t prio = EV_PRIORITY_LOW;
    struct epoll_event events[FD_SETSIZE];
    uint64_t before, now;
    int res, res2;

    el_fd_initialize();
    el_bl_unlock();
    before     = get_clock(false);
    res2 = res = epoll_wait(el_epoll_g.fd, events, countof(events),
                            _G.gotsigs ? 0 : timeout);
    now        = get_clock(false);
    el_bl_lock();
    assert (res >= 0 || ERR_RW_RETRIABLE(errno));

    if (now - before > 100)
        dlist_splice_tail(&_G.idle, &_G.idle_parked);

    _G.has_run = false;
    el_timer_process(now);
    while (res-- > 0) {
        ev_t *ev = events[res].data.ptr;
        int  evs = events[res].events;

        if (unlikely(ev->type != EV_FD))
            continue;
        if (ev->priority > prio)
            prio = ev->priority;
        if (ev->priority == EV_PRIORITY_HIGH)
            el_fd_fire(ev, evs);
    }

    if (prio == EV_PRIORITY_HIGH) {
        return;
    }
    while (res2-- > 0) {
        ev_t *ev = events[res2].data.ptr;
        int  evs = events[res2].events;

        if (unlikely(ev->type != EV_FD))
            continue;
        if (ev->priority == prio)
            el_fd_fire(ev, evs);
    }
}
