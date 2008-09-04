/**************************************************************************/
/*                                                                        */
/*  Copyright (C) 2004-2008 INTERSEC SAS                                  */
/*                                                                        */
/*  Should you receive a copy of this source code, you must check you     */
/*  have a proper, written authorization of INTERSEC to hold it. If you   */
/*  don't have such an authorization, you must DEVETE all source code     */
/*  files in your possession, and inform INTERSEC of the fact you obtain  */
/*  these files. Should you not comply to these terms, you can be         */
/*  prosecuted in the extent permitted by applicable law.                 */
/*                                                                        */
/**************************************************************************/

#include <sys/epoll.h>
#include <lib-common/unix.h>

static int epollfd_g = -1;

el_t el_fd_register(int fd, short events, el_fd_f *cb, el_data_t priv)
{
    ev_t *ev = el_create(EV_FD, cb, priv, true);
    struct epoll_event event = {
        .data.ptr = ev,
        .events   = events,
    };

    if (unlikely(epollfd_g == -1)) {
#ifdef SIGPIPE
        signal(SIGPIPE, SIG_IGN);
#endif
        epollfd_g = epoll_create(1024);
        if (epollfd_g < 0)
            e_panic(E_UNIXERR("epoll_create"));
        fd_set_features(epollfd_g, O_CLOEXEC);
    }
    ev->fd.fd = fd;
    ev->fd.events = events;
    if (unlikely(epoll_ctl(epollfd_g, EPOLL_CTL_ADD, fd, &event)))
        e_panic(E_UNIXERR("epoll_ctl"));
    return ev;
}

void el_fd_set_mask(ev_t *ev, short events)
{
    CHECK_EV_TYPE(ev, EV_FD);
    if (ev->fd.events != events) {
        struct epoll_event event = {
            .data.ptr = ev,
            .events   = ev->fd.events = events,
        };
        if (unlikely(epoll_ctl(epollfd_g, EPOLL_CTL_MOD, ev->fd.fd, &event)))
            e_panic(E_UNIXERR("epoll_ctl"));
    }
}

el_data_t el_fd_unregister(ev_t **evp, bool do_close)
{
    if (*evp) {
        ev_t *ev = *evp;
        CHECK_EV_TYPE(ev, EV_FD);
        epoll_ctl(epollfd_g, EPOLL_CTL_DEL, ev->fd.fd, NULL);
        if (likely(do_close))
            close(ev->fd.fd);
        if (_G.in_poll)
            el_list_push(&_G.evs_fd_tmp, ev);
        return el_destroy(evp, !_G.in_poll);
    }
    return (el_data_t)NULL;
}

static void el_loop_fds(int timeout)
{
    struct epoll_event events[FD_SETSIZE];
    struct timeval now;
    int res;

    res = epoll_wait(epollfd_g, events, countof(events), timeout);
    assert (res >= 0 || errno == EAGAIN || errno == EINTR);

    _G.in_poll = true;
    if (_G.timers.len) {
        el_timer_process(get_clock(false));
    }

    lp_gettv(&now);
    while (--res >= 0) {
        ev_t *ev = events[res].data.ptr;
        int  evs = events[res].events;

        if (likely(ev->type == EV_FD))
            (*ev->cb.fd)(ev, ev->fd.fd, evs, ev->priv);
    }
    while (unlikely(_G.evs_fd_tmp != NULL)) {
        el_list_push(&_G.evs_free, el_list_pop(&_G.evs_fd_tmp));
    }
    _G.in_poll = false;
}
