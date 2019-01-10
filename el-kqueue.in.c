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

#include <sys/types.h>
#include <sys/event.h>
#include <sys/time.h>
#include "unix.h"

qvector_t(kevent, struct kevent);

static struct {
    qv_t(kevent) events;
    qv_t(kevent) chlist;

    int kq;
    int pending;
} kqueue_g = {
    .kq = -1
};

static void el_fd_at_fork(void)
{
    kqueue_g.kq = -1;
}

static void el_fd_initialize(void)
{
    if (unlikely(kqueue_g.kq == -1)) {
#ifdef SIGPIPE
        signal(SIGPIPE, SIG_IGN);
#endif
        kqueue_g.kq = kqueue();
        if (kqueue_g.kq < 0)
            e_panic(E_UNIXERR("kqueue_g"));
        fd_set_features(kqueue_g.kq, O_CLOEXEC);
    }
}

static void el_fd_kqueue_register(int fd, int filter, int flags,
                                  int fflags, ev_t *ev)
{
    struct kevent ke;

    EV_SET(&ke, fd, filter, flags, fflags, 0, ev);
    qv_append(kevent, &kqueue_g.chlist, ke);
}

el_t el_fd_register_d(int fd, short events, el_fd_f *cb, data_t priv)
{
    ev_t *ev = el_create(EV_FD, cb, priv, true);

    if (events & POLLIN) {
        el_fd_kqueue_register(fd, EVFILT_READ, EV_ADD | EV_ENABLE, 0, ev);
    }
    if (events & POLLOUT) {
        el_fd_kqueue_register(fd, EVFILT_WRITE, EV_ADD | EV_ENABLE, 0, ev);
    }
    ev->fd = fd;
    ev->events_wanted = events;
    ev->priority = EV_PRIORITY_NORMAL;
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
    if (old != events) {
        if ((old & POLLIN) != (events & POLLIN)) {
            if (events & POLLIN) {
                el_fd_kqueue_register(ev->fd, EVFILT_READ, EV_ADD | EV_ENABLE, 0, ev);
            } else {
                el_fd_kqueue_register(ev->fd, EVFILT_READ, EV_DELETE, 0, ev);
            }
        }

        if ((old & POLLOUT) != (events & POLLOUT)) {
            if (events & POLLOUT) {
                el_fd_kqueue_register(ev->fd, EVFILT_WRITE, EV_ADD | EV_ENABLE, 0, ev);
            } else {
                el_fd_kqueue_register(ev->fd, EVFILT_WRITE, EV_DELETE, 0, ev);
            }
        }
        ev->events_wanted = events;
    }
    return old;
}

data_t el_fd_unregister(ev_t **evp, bool do_close)
{
    if (*evp) {
        ev_t *ev = *evp;

        CHECK_EV_TYPE(ev, EV_FD);
        el_fd_set_mask(ev, 0);
        if (likely(do_close)) {
            close(ev->fd);
        }
        if (EV_FLAG_HAS(ev, FD_WATCHED)) {
            el_fd_act_timer_unregister(ev->priv.ptr);
        }
        if (EV_FLAG_HAS(ev, FD_FIRED)) {
            dlist_remove(&ev->ev_list);
        }
        return el_destroy(evp);
    }
    return (data_t)NULL;
}


static void el_loop_fds_poll(int timeout)
{
    int size = MAX(64, kqueue_g.chlist.len);
    struct timespec ts;

    el_fd_initialize();
    if (kqueue_g.events.size < size) {
        qv_grow(kevent, &kqueue_g.events, size - kqueue_g.events.size);
    }

    el_bl_unlock();
    timeout = el_signal_has_pending_events() ? 0 : timeout;
    ts.tv_sec = timeout / 1000;
    ts.tv_nsec = (timeout % 1000) * 1000000;
    errno = 0;
    kqueue_g.pending = kevent(kqueue_g.kq, kqueue_g.chlist.tab,
                              kqueue_g.chlist.len,
                              kqueue_g.events.tab, kqueue_g.events.size,
                              timeout < 0 ? NULL : &ts);
    if (kqueue_g.pending < 0) {
        assert (ERR_RW_RETRIABLE(errno));
    } else {
        kqueue_g.chlist.len = 0;
    }
    el_bl_lock();
}

static bool el_fds_has_pending_events(void)
{
    if (kqueue_g.pending == 0) {
        el_loop_fds_poll(0);
    }
    return kqueue_g.pending != 0;
}

static void el_loop_fds(int timeout)
{
    ev_priority_t prio = EV_PRIORITY_LOW;
    int res, res2;
    uint64_t before, now;

    if (kqueue_g.pending == 0) {
        before = get_clock();
        el_loop_fds_poll(timeout);
        now    = get_clock();
        if (now - before > 100) {
            dlist_splice_tail(&_G.idle, &_G.idle_parked);
        }
    } else {
        now = get_clock();
    }

    res = res2 = kqueue_g.pending;
    kqueue_g.pending = 0;

    _G.has_run = false;
    el_timer_process(now);

    while (res-- > 0) {
        struct kevent ke = kqueue_g.events.tab[res];
        ev_t *ev = ke.udata;
        short events = 0;

        if (unlikely(ev->type != EV_FD)) {
            continue;
        }

        if (ke.filter == EVFILT_READ) {
            events = POLLIN;
        } else
        if (ke.filter == EVFILT_WRITE) {
            events = POLLOUT;
        } else {
            assert (false);
        }

        if (ev->priority > prio) {
            prio = ev->priority;
        }
        if (ev->priority == EV_PRIORITY_HIGH) {
            el_fd_fire(ev, events);
        }
    }

    if (prio == EV_PRIORITY_HIGH) {
        return;
    }
    while (res2-- > 0) {
        struct kevent ke = kqueue_g.events.tab[res2];
        ev_t *ev = ke.udata;
        short events = 0;

        if (unlikely(ev->type != EV_FD))
            continue;

        if (ke.filter == EVFILT_READ) {
            events = POLLIN;
        } else
        if (ke.filter == EVFILT_WRITE) {
            events = POLLOUT;
        } else {
            assert (false);
        }

        if (ev->priority == prio) {
            el_fd_fire(ev, events);
        }
    }
}
