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

#include <sys/epoll.h>
#include <lib-common/unix.h>

static int epollfd_g = -1;

static void el_fd_initialize(void)
{
    if (unlikely(epollfd_g == -1)) {
#ifdef SIGPIPE
        signal(SIGPIPE, SIG_IGN);
#endif
        epollfd_g = epoll_create(1024);
        if (epollfd_g < 0)
            e_panic(E_UNIXERR("epoll_create"));
        fd_set_features(epollfd_g, O_CLOEXEC);
    }
}

el_t el_fd_register(int fd, short events, el_fd_f *cb, el_data_t priv)
{
    ev_t *ev = el_create(EV_FD, cb, priv, true);
    struct epoll_event event = {
        .data.ptr = ev,
        .events   = events,
    };

    el_fd_initialize();
    ev->fd = fd;
    ev->events_wanted = events;
    if (unlikely(epoll_ctl(epollfd_g, EPOLL_CTL_ADD, fd, &event)))
        e_panic(E_UNIXERR("epoll_ctl"));
    return ev;
}

short el_fd_set_mask(ev_t *ev, short events)
{
    short old = ev->events_wanted;

    CHECK_EV_TYPE(ev, EV_FD);
    if (old != events) {
        struct epoll_event event = {
            .data.ptr = ev,
            .events   = ev->events_wanted = events,
        };
        if (unlikely(epoll_ctl(epollfd_g, EPOLL_CTL_MOD, ev->fd, &event)))
            e_panic(E_UNIXERR("epoll_ctl"));
    }
    return old;
}

el_data_t el_fd_unregister(ev_t **evp, bool do_close)
{
    if (*evp) {
        ev_t *ev = *evp;

        CHECK_EV_TYPE(ev, EV_FD);
        epoll_ctl(epollfd_g, EPOLL_CTL_DEL, ev->fd, NULL);
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
    struct epoll_event events[FD_SETSIZE];
    struct timeval now;
    int res;

    el_fd_initialize();
    el_bl_unlock();
    res = epoll_wait(epollfd_g, events, countof(events), timeout);
    el_bl_lock();
    assert (res >= 0 || ERR_RW_RETRIABLE(errno));

    if (_G.timers.len) {
        el_timer_process(get_clock(false));
    }

    lp_gettv(&now);
    while (--res >= 0) {
        ev_t *ev = events[res].data.ptr;
        int  evs = events[res].events;

        if (unlikely(ev->type != EV_FD))
            continue;
        el_fd_fire(ev, evs);
    }
}
