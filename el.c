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

#include <sys/wait.h>
#include <pthread.h>
#include "container.h"
#include "time.h"
#include "el.h"

#ifndef NDEBUG
//#define T_STACK_DEBUG
#endif

static struct {
    pthread_mutex_t mutex;
    pthread_t owner;
    int count;
} big_lock_g = {
    .mutex = PTHREAD_MUTEX_INITIALIZER,
};

static bool use_big_lock_g = false;

/** \addtogroup lc_el
 * \{
 */

/** \file lib-inet/el.c
 * \brief Event Loop module implementation.
 */

/*
 * TODO:
 *
 * - timer things:
 *   + monotonic clocks are very seldomly implemented (only on x86 for Linux).
 *     Have a way to detect timewarps, and avoid clock_gettime so that we can
 *     drop -lrt.
 *
 * - fd things:
 *   + implement a solaris port using event ports (see what libev does).
 *   + implement a windows port.
 *
 */

typedef enum ev_type_t {
    EV_UNUSED,
    EV_BLOCKER,
    EV_BEFORE,
    EV_AFTER,
    EV_SIGNAL,
    EV_CHILD,
    EV_FD,
    EV_TIMER,
    EV_PROXY,
} ev_type_t;

enum ev_flags_t {
    EV_FLAG_REFS          = (1U <<  0),

    EV_FLAG_TIMER_NOMISS  = (1U <<  8),
    EV_FLAG_TIMER_LOWRES  = (1U <<  9),
    EV_FLAG_TIMER_UPDATED = (1U << 10),

    EV_FLAG_FD_WATCHED    = (1U <<  8),
#define EV_FLAG_HAS(ev, f)   ((ev)->flags & EV_FLAG_##f)
#define EV_FLAG_SET(ev, f)   ((ev)->flags |= EV_FLAG_##f)
#define EV_FLAG_RST(ev, f)   ((ev)->flags &= ~EV_FLAG_##f)
};

typedef struct ev_t {
    uint8_t   type;             /* ev_type_t */
    uint8_t   signo;            /* EV_SIGNAL */
    uint16_t  flags;
    union {
        uint16_t  events_avail; /* EV_PROXY */
        uint16_t  events_act;   /* EV_FD    */
    };
    uint16_t  events_wanted;    /* EV_PROXY, EV_FD */

    union {
        el_cb_f     *cb;
        el_signal_f *signal;
        el_child_f  *child;
        el_fd_f     *fd;
        el_proxy_f  *prox;
    } cb;
    el_data_t priv;

    union {
        dlist_t ev_list;        /* EV_BEFORE, EV_AFTER, EV_SIGNAL, EV_PROXY */
        int     fd;             /* EV_FD */
        pid_t   pid;            /* EV_CHILD */
        struct {
            uint64_t expiry;
            int      repeat;
            int      heappos;
        } timer;                /* EV_TIMER */
    };
} ev_t;
qvector_t(ev, ev_t *);

qm_k32_t(ev_assoc, ev_t *);
qm_k64_t(ev, ev_t *);

static struct {
    volatile uint32_t gotsigs;
    int      active;          /* number of ev_t keeping the el_loop running */
    int      unloop;          /* @see el_unloop()                           */
    int      loop_depth;      /* depth of el_loop_timeout() recursion       */
    uint64_t lp_clk;          /* low precision monotonic clock              */

    dlist_t  before;          /* ev_t to run at the start of the loop       */
    dlist_t  after;           /* ev_t to run at the end of the loop         */
    dlist_t  sigs;            /* signals el_t's                             */
    dlist_t  proxy, proxy_ready;
    qv_t(ev) timers;          /* relative timers heap (see comments after)  */
    qv_t(ev) cache;
    qm_t(ev_assoc) childs;    /* el_t's watching for processes              */
    qm_t(ev) fd_act;          /* el_t's timers to el_t fds map              */

    /*----- allocation stuff -----*/
#define EV_ALLOC_FACTOR  10   /* basic segment is 1024 events               */
    ev_t   evs_initial[1 << EV_ALLOC_FACTOR];
    ev_t  *evs_alloc_next, *evs_alloc_end;
    ev_t  *evs[32 - EV_ALLOC_FACTOR];
    int    evs_len;
    dlist_t evs_free;
    dlist_t evs_gc;
} el_g = {
#define _G el_g
    .evs[0]         = _G.evs_initial,
    .evs_len        = 1,
    .evs_alloc_next = &_G.evs_initial[0],
    .evs_alloc_end  = &_G.evs_initial[countof(_G.evs_initial)],

    .before         = DLIST_INIT(_G.before),
    .after          = DLIST_INIT(_G.after),
    .sigs           = DLIST_INIT(_G.sigs),
    .proxy          = DLIST_INIT(_G.proxy),
    .proxy_ready    = DLIST_INIT(_G.proxy_ready),
    .evs_free       = DLIST_INIT(_G.evs_free),
    .evs_gc         = DLIST_INIT(_G.evs_gc),
    .childs         = QM_INIT(ev_assoc, _G.childs, false),
    .fd_act         = QM_INIT(ev, _G.fd_act, false),
};

#define ASSERT(msg, expr)  assert (((void)msg, likely(expr)))
#define CHECK_EV(ev)   \
    ASSERT("ev is uninitialized", (ev)->type)
#define CHECK_EV_TYPE(ev, typ) \
    ASSERT("incorrect type", (ev)->type == typ)

static ev_t *ev_add(dlist_t *l, ev_t *ev)
{
    dlist_add(l, &ev->ev_list);
    return ev;
}

static void ev_cache_list(dlist_t *l)
{
    ev_t *ev;

    qv_clear(ev, &_G.cache);
    dlist_for_each_entry(ev, l, ev_list) {
        qv_append(ev, &_G.cache, ev);
    }
}

static ev_t *el_create(ev_type_t type, void *cb, el_data_t priv, bool ref)
{
    ev_t *res;

    if (unlikely(dlist_is_empty(&_G.evs_free))) {
        if (unlikely(_G.evs_alloc_next >= _G.evs_alloc_end)) {
            int   bucket_len = 1 << (_G.evs_len++ + EV_ALLOC_FACTOR);
            ev_t *bucket     = p_new(ev_t, bucket_len);

            if (unlikely(_G.evs_len > countof(_G.evs)))
                e_panic(E_PREFIX("insane amount of events"));
            _G.evs[_G.evs_len - 1] = bucket;
            _G.evs_alloc_next = bucket;
            _G.evs_alloc_end  = bucket + bucket_len;
        }
        res = _G.evs_alloc_next++;
    } else {
        res = container_of(_G.evs_free.next, ev_t, ev_list);
        dlist_remove(&res->ev_list);
    }

    *res = (ev_t){
        .type   = type,
        .cb.cb  = cb,
        .priv   = priv,
    };
    return ref ? el_ref(res) : res;
}

static el_data_t el_destroy(ev_t **evp, bool move)
{
    ev_t *ev = *evp;

    el_unref(ev);
    if (move) {
        dlist_move(&_G.evs_gc, &ev->ev_list);
    } else {
        dlist_add(&_G.evs_gc, &ev->ev_list);
    }
    ev->type  = EV_UNUSED;
    ev->flags = 0;
    *evp = NULL;
    return ev->priv;
}

static void ev_list_process(dlist_t *l)
{
    ev_cache_list(l);
    for (int i = 0; i < _G.cache.len; i++) {
        ev_t *ev = _G.cache.tab[i];

        if (unlikely(ev->type == EV_UNUSED))
            continue;
        (*ev->cb.cb)(ev, ev->priv);
    }
}

/*----- blockers, before and after events -----*/

ev_t *el_blocker_register(el_data_t priv)
{
    return el_create(EV_BLOCKER, NULL, priv, true);
}

ev_t *el_before_register(el_cb_f *cb, el_data_t priv)
{
    return ev_add(&_G.before, el_create(EV_BEFORE, cb, priv, true));
}

ev_t *el_after_register(el_cb_f *cb, el_data_t priv)
{
    return ev_add(&_G.after, el_create(EV_AFTER, cb, priv, true));
}

void el_before_set_hook(el_t ev, el_cb_f *cb)
{
    CHECK_EV_TYPE(ev, EV_BEFORE);
    ev->cb.cb = cb;
}

void el_after_set_hook(el_t ev, el_cb_f *cb)
{
    CHECK_EV_TYPE(ev, EV_AFTER);
    ev->cb.cb = cb;
}

el_data_t el_blocker_unregister(ev_t **evp)
{
    if (*evp) {
        CHECK_EV_TYPE(*evp, EV_BLOCKER);
        return el_destroy(evp, false);
    }
    return (el_data_t)NULL;
}

el_data_t el_before_unregister(ev_t **evp)
{
    if (*evp) {
        CHECK_EV_TYPE(*evp, EV_BEFORE);
        return el_destroy(evp, true);
    }
    return (el_data_t)NULL;
}

el_data_t el_after_unregister(ev_t **evp)
{
    if (*evp) {
        CHECK_EV_TYPE(*evp, EV_AFTER);
        return el_destroy(evp, true);
    }
    return (el_data_t)NULL;
}

/*----- signal events -----*/

static void el_sighandler(int signum)
{
    _G.gotsigs |= (1 << signum);
}

static void el_signal_process(void)
{
    uint32_t gotsigs = _G.gotsigs;
    struct timeval now;

    if (!gotsigs)
        return;

    _G.gotsigs &= ~gotsigs;
    lp_gettv(&now);

    ev_cache_list(&_G.sigs);
    for (int i = 0; i < _G.cache.len; i++) {
        ev_t *ev = _G.cache.tab[i];
        int signo = ev->signo;

        if (unlikely(ev->type == EV_UNUSED))
            continue;
        if (gotsigs & (1 << signo)) {
            (*ev->cb.signal)(ev, signo, ev->priv);
        }
    }
}

void el_signal_set_hook(el_t ev, el_signal_f *cb)
{
    CHECK_EV_TYPE(ev, EV_SIGNAL);
    ev->cb.signal = cb;
}

ev_t *el_signal_register(int signo, el_signal_f *cb, el_data_t priv)
{
    struct sigaction sa;
    ev_t *ev;

    sa.sa_handler = el_sighandler;
    sigfillset(&sa.sa_mask);
    sa.sa_flags = SA_RESTART;
    sigaction(signo, &sa, NULL);

    ev = el_create(EV_SIGNAL, cb, priv, false);
    ev->signo = signo;
    return ev_add(&_G.sigs, ev);
}

el_data_t el_signal_unregister(ev_t **evp)
{
    if (*evp) {
        CHECK_EV_TYPE(*evp, EV_SIGNAL);
        return el_destroy(evp, true);
    }
    return (el_data_t)NULL;
}

/*----- child events -----*/

static void el_sigchld_hook(ev_t *ev, int signo, el_data_t priv)
{
    pid_t pid;
    int status;

    while ((pid = waitpid(-1, &status, WNOHANG)) > 0) {
        int32_t pos = qm_del_key(ev_assoc, &_G.childs, pid);

        if (likely(pos >= 0)) {
            ev_t *e = _G.childs.values[pos];
            (*e->cb.child)(e, pid, status, e->priv);
            el_destroy(&_G.childs.values[pos], false);
        }
    }
}

void el_child_set_hook(el_t ev, el_child_f *cb)
{
    CHECK_EV_TYPE(ev, EV_CHILD);
    ev->cb.child = cb;
}

ev_t *el_child_register(pid_t pid, el_child_f *cb, el_data_t priv)
{
    static bool hooked;

    ev_t *ev = el_create(EV_CHILD, cb, priv, true);

    if (unlikely(!hooked)) {
        hooked = true;
        el_signal_register(SIGCHLD, el_sigchld_hook, NULL);
    }

    if (qm_add(ev_assoc, &_G.childs, pid, ev) < 0) {
        ASSERT("pid is already watched", false);
    }
    ev->pid = pid;
    return ev;
}

el_data_t el_child_unregister(ev_t **evp)
{
    if (*evp) {
        CHECK_EV_TYPE(*evp, EV_CHILD);
        if (qm_del_key(ev_assoc, &_G.childs, (*evp)->pid) < 0)
            ASSERT("event not found", false);
        return el_destroy(evp, false);
    }
    return (el_data_t)NULL;
}

pid_t el_child_getpid(el_t ev)
{
    CHECK_EV_TYPE(ev, EV_CHILD);
    return ev->pid;
}

/*----- timer events -----*/
/*
 * timers are extremely efficient, and use a binary-min-heap as a base
 * structure.
 *
 * Binary min-heaps are balanced binary trees stored in an array.
 * The invariant is that the "label" (for us the millisecond the timer has to
 * start at) of each parent node, is smaller than any of its children ones.
 *
 * Hence the next to be run timer is the root of the heap.
 *
 * Adding/Updating/... a timer is pseudo linear O(log(n)) in the number of
 * timers.
 */

#define EVT(pos)         _G.timers.tab[pos]
#define CHILD_L(pos)     (2 * (pos) + 1)
#define CHILD_R(pos)     (CHILD_L(pos) + 1)
#define PARENT(pos)      (((pos) - 1) / 2)
#define EVTSET(pos, ev)  do { EVT(pos) = (ev); \
                              (ev)->timer.heappos = (pos); } while (0)

static void el_timer_heapdown(int pos)
{
    ev_t *ev = EVT(pos);

    while (CHILD_L(pos) < _G.timers.len) {
        int child = CHILD_L(pos);
        ev_t *ec = EVT(child);

        if (child + 1 < _G.timers.len) {
            child += (ec->timer.expiry > EVT(child + 1)->timer.expiry);
            ec = EVT(child);
        }
        if (ev->timer.expiry <= ec->timer.expiry)
            break;

        EVTSET(pos, ec);
        pos = child;
    }

    EVTSET(pos, ev);
}

static void el_timer_heapup(int pos)
{
    ev_t *ev = EVT(pos);

    /* while we are not at the top */
    while (pos > 0) {
        int parent = PARENT(pos);
        ev_t *ep = EVT(parent);

        if (ep->timer.expiry <= ev->timer.expiry)
            break;

        EVTSET(pos, ep);
        pos = parent;
    }

    EVTSET(pos, ev);
}

static void el_timer_heapinsert(ev_t *ev)
{
    qv_append(ev, &_G.timers, ev);
    ev->timer.heappos = _G.timers.len - 1;
    el_timer_heapup(_G.timers.len - 1);
}

static void el_timer_heapfix(ev_t *ev)
{
    int pos = ev->timer.heappos;

    if (pos > 0 && EVT(PARENT(pos))->timer.expiry >= ev->timer.expiry) {
        el_timer_heapup(pos);
    } else {
        el_timer_heapdown(pos);
    }
}

static el_data_t el_timer_heapremove(ev_t **evp)
{
    int   pos = (*evp)->timer.heappos;
    ev_t *end = *qv_last(ev, &_G.timers);

    qv_shrink(ev, &_G.timers, 1);
    CHECK_EV_TYPE(*evp, EV_TIMER);
    if (*evp != end) {
        EVTSET(pos, end);
        el_timer_heapfix(end);
    }
    (*evp)->timer.heappos = -1;
    return el_destroy(evp, false);
}

static void el_timer_process(uint64_t until)
{
    struct timeval tv;

    lp_gettv(&tv);
    while (_G.timers.len) {
        ev_t *ev = EVT(0);
#ifdef T_STACK_DEBUG
        const void *ptr;
#endif

        ASSERT("should be a timer", ev->type == EV_TIMER);
        if (ev->timer.expiry > until)
            return;

        EV_FLAG_RST(ev, TIMER_UPDATED);
#ifdef T_STACK_DEBUG
        ptr = t_push();
#endif
        (*ev->cb.cb)(ev, ev->priv);
#ifdef T_STACK_DEBUG
        assert (ptr == t_pop());
#endif

        /* ev has been unregistered in (*cb) */
        if (ev->type == EV_UNUSED)
            continue;

        if (ev->timer.repeat > 0) {
            ev->timer.expiry += ev->timer.repeat;
            if (!EV_FLAG_HAS(ev, TIMER_NOMISS) && ev->timer.expiry < until) {
                uint64_t delta  = until - ev->timer.expiry;

                ev->timer.expiry += ROUND_UP(delta, (uint64_t)ev->timer.repeat);
            }
            el_timer_heapdown(0);
        } else
        if (!EV_FLAG_HAS(ev, TIMER_UPDATED)) {
            el_timer_heapremove(&ev);
        }
    }
}

static uint64_t get_clock(bool lowres)
{
    struct timespec ts;
    int res;

    if (_G.timers.len > 1 && lowres && likely(_G.lp_clk))
        return _G.lp_clk;

#if   defined(CLOCK_MONOTONIC) /* POSIX   */
    res = clock_gettime(CLOCK_MONOTONIC, &ts);
#elif defined(CLOCK_HIGHRES)   /* Solaris */
    res = clock_gettime(CLOCK_HIGHRES, &ts);
#else
#   error you need to find out how to get a monotonic clock for your system
#endif
    assert (res == 0);
    return _G.lp_clk = 1000ull * ts.tv_sec + ts.tv_nsec / 1000000;
}

/* XXX: if we reschedule in more than half a second, don't care about
        the high precision */
#define TIMER_IS_LOWRES(ev, next)   (EV_FLAG_HAS(ev, TIMER_LOWRES) || (next) >= 500)

ev_t *el_timer_register(int next, int repeat, int flags, el_cb_f *cb, el_data_t priv)
{
    ev_t *ev = el_create(EV_TIMER, cb, priv, true);

    if (flags & EL_TIMER_NOMISS)
        EV_FLAG_SET(ev, TIMER_NOMISS);
    if (flags & EL_TIMER_LOWRES)
        EV_FLAG_SET(ev, TIMER_LOWRES);
    if (repeat > 0) {
        ev->timer.repeat = repeat;
    } else {
        ev->timer.repeat = -next;
    }
    ev->timer.expiry = (uint64_t)next + get_clock(TIMER_IS_LOWRES(ev, next));
    el_timer_heapinsert(ev);
    return ev;
}

void el_timer_set_hook(el_t ev, el_cb_f *cb)
{
    CHECK_EV_TYPE(ev, EV_TIMER);
    ev->cb.cb = cb;
}

static ALWAYS_INLINE void el_timer_restart_fast(ev_t *ev, uint64_t restart)
{
    ev->timer.expiry = (uint64_t)restart + get_clock(TIMER_IS_LOWRES(ev, restart));
    EV_FLAG_SET(ev, TIMER_UPDATED);
    el_timer_heapfix(ev);
}

void el_timer_restart(ev_t *ev, int restart)
{
    CHECK_EV_TYPE(ev, EV_TIMER);
    ASSERT("timer isn't a oneshot timer", ev->timer.repeat <= 0);
    if (restart <= 0) {
        restart = -ev->timer.repeat;
    } else {
        ev->timer.repeat = -restart;
    }
    el_timer_restart_fast(ev, restart);
}

el_data_t el_timer_unregister(ev_t **evp)
{
    return *evp ? el_timer_heapremove(evp) : (el_data_t)NULL;
}

/*----- fd events -----*/

static ALWAYS_INLINE ev_t *el_fd_act_timer_unregister(ev_t *timer)
{
    int pos = qm_del_key(ev, &_G.fd_act, (uint64_t)(uintptr_t)timer);
    ev_t *ev;

    assert (pos >= 0);
    ev = _G.fd_act.values[pos];
    EV_FLAG_RST(ev, FD_WATCHED);
    ev->priv = timer->priv;
    el_timer_heapremove(&timer);
    return ev;
}

static ALWAYS_INLINE void el_fd_fire(ev_t *ev, short evs)
{
#ifdef T_STACK_DEBUG
    const void *ptr = t_push();
#endif

    if (EV_FLAG_HAS(ev, FD_WATCHED)) {
        ev_t *timer = ev->priv.ptr;

        if (evs & ev->events_act)
            el_timer_restart_fast(timer, -timer->timer.repeat);
        (*ev->cb.fd)(ev, ev->fd, evs, timer->priv);
    } else {
        (*ev->cb.fd)(ev, ev->fd, evs, ev->priv);
    }
#ifdef T_STACK_DEBUG
    assert (ptr == t_pop());
#endif
}

static void el_act_timer(el_t ev, el_data_t priv)
{
    el_fd_fire(el_fd_act_timer_unregister(ev), EL_EVENTS_NOACT);
}

static ALWAYS_INLINE ev_t *el_fd_act_timer_register(ev_t *ev, int timeout)
{
    ev_t *timer = el_timer_register(timeout, 0, 0, &el_act_timer, ev->priv);

    ev->priv.ptr = el_unref(timer);
    EV_FLAG_SET(ev, FD_WATCHED);
    qm_replace(ev, &_G.fd_act, (uint64_t)(uintptr_t)timer, ev);
    return timer;
}

#ifdef __linux__
#  include "el-epoll.c"
#else
#  error one has to port el to your OS
#endif

void el_fd_set_hook(el_t ev, el_fd_f *cb)
{
    CHECK_EV_TYPE(ev, EV_FD);
    ev->cb.fd = cb;
}

short el_fd_get_mask(ev_t *ev)
{
    CHECK_EV_TYPE(ev, EV_FD);
    return ev->events_wanted;
}

int el_fd_get_fd(ev_t *ev)
{
    if (likely(ev)) {
        CHECK_EV_TYPE(ev, EV_FD);
        return ev->fd;
    }
    return -1;
}

int el_fd_watch_activity(el_t ev, short mask, int timeout)
{
    el_t timer;
    int res;

    CHECK_EV_TYPE(ev, EV_FD);

    ev->events_act = mask;
    if (!EV_FLAG_HAS(ev, FD_WATCHED)) {
        if (timeout <= 0)
            return 0;
        el_fd_act_timer_register(ev, timeout);
        return 0;
    }
    timer = ev->priv.ptr;
    res   = -timer->timer.repeat;

    if (timeout == 0) {
        el_fd_act_timer_unregister(timer);
    } else {
        el_timer_restart(timer, timeout);
    }
    return res;
}

int el_fd_loop(ev_t *ev, int timeout)
{
    struct pollfd pfd = { .fd = ev->fd, .events = ev->events_wanted };
    int res;

    CHECK_EV_TYPE(ev, EV_FD);

    res = poll(&pfd, 1, timeout);
    if (_G.timers.len)
        el_timer_process(get_clock(false));
    if (res == 1 && likely(ev->type == EV_FD))
        el_fd_fire(ev, pfd.revents);
    return res;
}

/*----- proxies events  -----*/

ev_t *el_proxy_register(el_proxy_f *cb, el_data_t priv)
{
    return ev_add(&_G.proxy, el_create(EV_PROXY, cb, priv, true));
}

void el_proxy_set_hook(el_t ev, el_proxy_f *cb)
{
    CHECK_EV_TYPE(ev, EV_PROXY);
    ev->cb.prox = cb;
}

el_data_t el_proxy_unregister(ev_t **evp)
{
    if (*evp) {
        CHECK_EV_TYPE(*evp, EV_PROXY);
        return el_destroy(evp, true);
    }
    return (el_data_t)NULL;
}

static void el_proxy_change_ready(ev_t *ev, bool was_ready)
{
    dlist_move(was_ready ? &_G.proxy : &_G.proxy_ready, &ev->ev_list);
}

static short el_proxy_set_event_full(ev_t *ev, short evt)
{
    short old = ev->events_avail;

    if (old != evt) {
        bool was_ready = (old & ev->events_wanted) != 0;
        bool is_ready  = (evt & ev->events_wanted) != 0;

        ev->events_avail = evt;
        if (was_ready != is_ready)
            el_proxy_change_ready(ev, was_ready);
    }
    return old;
}

short el_proxy_set_event(ev_t *ev, short mask)
{
    return el_proxy_set_event_full(ev, ev->events_avail | mask);
}

short el_proxy_clr_event(ev_t *ev, short mask)
{
    return el_proxy_set_event_full(ev, ev->events_avail & ~mask);
}

short el_proxy_set_mask(ev_t *ev, short mask)
{
    short old = ev->events_wanted;

    if (old != mask) {
        bool was_ready = (old & ev->events_avail) != 0;
        bool is_ready  = (mask & ev->events_avail) != 0;

        ev->events_wanted = mask;
        if (was_ready != is_ready)
            el_proxy_change_ready(ev, was_ready);
    }
    return old;
}

static void el_loop_proxies(void)
{
    ev_cache_list(&_G.proxy_ready);
    for (int i = 0; i < _G.cache.len; i++) {
        ev_t *ev = _G.cache.tab[i];

        if (unlikely(ev->type == EV_UNUSED))
            continue;
        if (likely(ev->events_avail & ev->events_wanted))
            (*ev->cb.prox)(ev, ev->events_avail, ev->priv);
    }
}

/*----- generic functions  -----*/

el_t el_ref(ev_t *ev)
{
    CHECK_EV(ev);
    if (!EV_FLAG_HAS(ev, REFS)) {
        _G.active++;
        EV_FLAG_SET(ev, REFS);
    }
    return ev;
}

el_t el_unref(ev_t *ev)
{
    CHECK_EV(ev);
    if (EV_FLAG_HAS(ev, REFS)) {
        _G.active--;
        EV_FLAG_RST(ev, REFS);
    }
    return ev;
}

el_data_t el_set_priv(ev_t *ev, el_data_t priv)
{
    CHECK_EV(ev);
    if (ev->type == EV_FD && EV_FLAG_HAS(ev, FD_WATCHED))
        return el_set_priv(ev->priv.ptr, priv);
    SWAP(el_data_t, ev->priv, priv);
    return priv;
}

#include "el-showflags.c"

void el_loop_timeout(int timeout)
{
    uint64_t clk;

    _G.loop_depth++;
    ev_list_process(&_G.before);
    if (_G.timers.len) {
        clk = get_clock(false);
        el_timer_process(clk);
    }
    if (_G.timers.len) {
        uint64_t nxt = EVT(0)->timer.expiry;
        if (nxt < (uint64_t)timeout + clk)
            timeout = nxt - clk;
    }
    if (!dlist_is_empty(&_G.proxy_ready)) {
        timeout = 0;
    }
    do_license_checks();
    if (unlikely(_G.unloop)) {
        _G.loop_depth--;
        return;
    }
    el_loop_fds(timeout);
    el_loop_proxies();
    el_signal_process();
    ev_list_process(&_G.after);
    if (_G.loop_depth <= 1) {
        /* To be reentrant we can't reuse unregistered el_t until we came back
         * to the main loop */
        assert (_G.loop_depth == 1);
        dlist_splice(&_G.evs_free, &_G.evs_gc);
    }
    _G.loop_depth--;
}

void el_bl_use(void)
{
    if (use_big_lock_g)
        e_panic("el bl use has been called twice !");

    use_big_lock_g = true;
    el_bl_lock();
}

void el_bl_lock(void)
{
    pthread_t self;

    if (!use_big_lock_g)
        return;

    self = pthread_self();

    if (big_lock_g.owner == self) {
        big_lock_g.count++;
    } else {
        pthread_mutex_lock(&big_lock_g.mutex);
        big_lock_g.count = 1;
        big_lock_g.owner = self;
    }
}

void el_bl_unlock(void)
{
    if (!use_big_lock_g)
        return;

    assert(big_lock_g.count > 0);

    big_lock_g.count--;
    if (big_lock_g.count == 0) {
        big_lock_g.owner = (pthread_t)0;
        pthread_mutex_unlock(&big_lock_g.mutex);
    }
}

void el_cond_wait(pthread_cond_t *cond)
{
    int count;

    count = big_lock_g.count;
    big_lock_g.count = 0;

    pthread_cond_wait(cond, &big_lock_g.mutex);

    big_lock_g.count = count;
}

void el_cond_signal(pthread_cond_t *cond)
{
    pthread_cond_signal(cond);
}

void el_loop(void)
{
    while (likely(_G.active) && likely(!_G.unloop)) {
        el_loop_timeout(59000); /* arbitrary: 59 seconds */
    }
    _G.unloop = false;
}

void el_unloop(void)
{
    _G.unloop = true;
}

#ifndef NDEBUG
static char const * const typenames[] = {
    [EV_BLOCKER] = "blocker",
    [EV_BEFORE]  = "before loop",
    [EV_AFTER]   = "after loop",
    [EV_SIGNAL]  = "signal",
    [EV_CHILD]   = "child",
    [EV_FD]      = "file desc.",
    [EV_TIMER]   = "timer",
    [EV_PROXY]   = "proxy",
};

static void el_show_blockers(el_t evh, int signo, el_data_t priv)
{
    e_trace(0, "el blocking summary");
    e_trace(0, "===================");

    for (int i = 0; i < _G.evs_len; i++) {
        int bucket_len = 1 << (i + EV_ALLOC_FACTOR);

        for (int j = 0; j < bucket_len; j++) {
            ev_t *ev = &_G.evs[i][j];

            if (ev->type == EV_UNUSED || !EV_FLAG_HAS(ev, REFS))
                continue;

            e_trace_start(0, "ev %p │ cb %p │ data %p │ type %d %s",
                          ev, (void *)ev->cb.cb, ev->priv.ptr, ev->type,
                          typenames[ev->type]);

            switch (ev->type) {
              case EV_SIGNAL:
                e_trace_end(0, ":%d", ev->signo);
                break;
              case EV_CHILD:
                e_trace_end(0, ":%d", ev->pid);
                break;
              case EV_FD:
                e_trace_end(0, ":%d (%04x)", ev->fd, ev->events_wanted);
                break;
              case EV_TIMER:
                if (ev->timer.repeat < 0) {
                    e_trace_end(0, " one shot @%"PRIu64, ev->timer.expiry / 1000);
                } else {
                    e_trace_end(0, " repeat:%dms next @%"PRIu64, ev->timer.repeat,
                                ev->timer.expiry / 1000);
                }
                break;
              case EV_PROXY:
                e_trace_end(0, ":(w:%04x a:%04x)", ev->events_wanted, ev->events_avail);
                break;
              default:
                e_trace_end(0, "");
                break;
            }
        }
    }
}

static __attribute__((constructor)) void
hook_sigpwr(void)
{
    el_signal_register(SIGPWR, el_show_blockers, NULL);
}
#endif

__attribute__((constructor))
static void el_initialize(void)
{
    struct timeval tm;
    struct rlimit lim;

    gettimeofday(&tm, NULL);
    srand(tm.tv_sec + tm.tv_usec + getpid());
    ha_srand();

    if (getrlimit(RLIMIT_NOFILE, &lim) < 0)
        e_panic(E_UNIXERR("getrlimit"));
    if (lim.rlim_cur < lim.rlim_max) {
        lim.rlim_cur = lim.rlim_max;
        if (setrlimit(RLIMIT_NOFILE, &lim) < 0)
            e_error(E_UNIXERR("setrlimit"));
    }

    if (!is_fd_open(STDIN_FILENO)) {
        devnull_dup(STDIN_FILENO);
    }
    if (!is_fd_open(STDOUT_FILENO)) {
        devnull_dup(STDOUT_FILENO);
    }
    if (!is_fd_open(STDERR_FILENO)) {
        dup2(STDOUT_FILENO, STDERR_FILENO);
    }
}

/**\}*/
