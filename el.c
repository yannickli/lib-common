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

#include <sys/wait.h>
#include "container.h"
#include "time.h"
#include "el.h"

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
} ev_type_t;

typedef struct ev_t {
    flag_t    nomiss  : 1;
    flag_t    timerlp : 1;
    flag_t    refs    : 1;
    flag_t    updated : 1;
    ev_type_t type    : 4;

    union {
        el_cb_f *cb;
        el_signal_f *signal;
        el_child_f *child;
        el_fd_f *fd;
    } cb;
    el_data_t priv;

    union {
        el_t next; /* EV_BEFORE, EV_AFTER */
        struct {
            el_t next; /* XXX: must be first */
            int no;
        } sig;     /* EV_SIGNAL */
        struct {
            int fd;
            short events;
        } fd;      /* EV_FD */
        pid_t pid; /* EV_CHILD */
        struct {
            uint64_t expiry;
            int repeat;
            int heappos;
        } timer;   /* EV_TIMER */
    };
} ev_t;
DO_ARRAY(ev_t, ev, IGNORE);

typedef struct ev_assoc_t {
    uint64_t u;
    ev_t *ev;
} ev_assoc_t;
DO_HTBL_KEY(ev_assoc_t, uint64_t, ev_assoc, u);

static struct {
    int    active;          /* number of ev_t keeping the el_loop running   */
    int    in_poll;         /* have we processed the poll ?                 */
    uint64_t lp_clk;        /* low precision monotonic clock                */
    ev_t  *before;          /* list of ev_t to run at the start of the loop */
    ev_t  *after;           /* list of ev_t to run at the end of the loop   */
    ev_t  *signals[32];     /* signals el_t's, one per signal only          */
    ev_array timers;        /* relative timers heap (see comments after)    */
    ev_assoc_htbl childs;   /* el_t's watching for processes                */
    volatile uint32_t gotsigs;

    /*----- allocation stuff -----*/
#define EV_ALLOC_FACTOR  12 /* basic segment is 4096 events                 */
    ev_t   evs_initial[1 << EV_ALLOC_FACTOR];
    ev_t  *evs_alloc_next, *evs_alloc_end;
    ev_t  *evs[32 - EV_ALLOC_FACTOR];
    int    evs_len;
    ev_t  *evs_free;        /* free el_t's indices in evs                   */
    ev_t  *evs_timer_tmp;
    ev_t  *evs_fd_tmp;
} _G = {
    .evs[0]         = _G.evs_initial,
    .evs_len        = 1,
    .evs_alloc_next = &_G.evs_initial[0],
    .evs_alloc_end  = &_G.evs_initial[countof(_G.evs_initial)],
};

#define ASSERT(msg, expr)  assert (((void)msg, likely(expr)))
#define CHECK_EV(ev)   \
    ASSERT("ev is uninitialized", (ev)->type)
#define CHECK_EV_TYPE(ev, typ) \
    ASSERT("incorrect type", (ev)->type == typ)

static ev_t *el_list_push(ev_t **list, ev_t *ev)
{
    ev->next = *list;
    return *list = ev;
}

static ev_t *el_list_pop(ev_t **list)
{
    ev_t *res = *list;
    if (likely(res)) {
        *list = res->next;
        res->next = NULL;
    }
    return res;
}

static ev_t *el_create(ev_type_t type, void *cb, el_data_t priv, bool ref)
{
    ev_t *res = el_list_pop(&_G.evs_free);

    if (unlikely(!res)) {
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
    }

    *res = (ev_t){
        .type   = type,
        .cb.cb  = cb,
        .priv   = priv,
    };
    return ref ? el_ref(res) : res;
}

static el_data_t el_destroy(ev_t **evp, bool can_collect)
{
    ev_t *ev = *evp;
    el_unref(ev);
    if (can_collect)
        el_list_push(&_G.evs_free, ev);
    ev->type = EV_UNUSED;
#ifndef NDEBUG
    ev->priv.u64 = (uint64_t)-1;
#endif
    *evp = NULL;
    return ev->priv;
}

static inline void el_list_process(ev_t **list, int signo)
{
    while (*list) {
        ev_t *ev = *list;

        if (ev->type == EV_UNUSED) {
            el_list_push(&_G.evs_free, el_list_pop(list));
            continue;
        }

        if (signo >= 0) {
            (*ev->cb.signal)(ev, signo, ev->priv);
        } else {
            (*ev->cb.cb)(ev, ev->priv);
        }
        list = &ev->next;
    }
}

/*----- blockers, before and after events -----*/

ev_t *el_blocker_register(el_data_t priv)
{
    return el_create(EV_BLOCKER, NULL, priv, true);
}

ev_t *el_before_register(el_cb_f *cb, el_data_t priv)
{
    return el_list_push(&_G.before, el_create(EV_BEFORE, cb, priv, true));
}

ev_t *el_after_register(el_cb_f *cb, el_data_t priv)
{
    return el_list_push(&_G.after, el_create(EV_AFTER, cb, priv, true));
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
        return el_destroy(evp, true);
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
    struct timeval now;

    if (!_G.gotsigs)
        return;

    lp_gettv(&now);
    do {
        int sig = __builtin_ctz(_G.gotsigs);
        _G.gotsigs &= ~(1 << sig);
        el_list_process(&_G.signals[sig], sig);
    } while (_G.gotsigs);
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

    ASSERT("signo out of bounds", 0 <= signo && signo < countof(_G.signals));
    ev = el_create(EV_SIGNAL, cb, priv, false);
    ev->sig.no = signo;
    return el_list_push(&_G.signals[signo], ev);
}

el_data_t el_signal_unregister(ev_t **evp)
{
    if (*evp) {
        CHECK_EV_TYPE(*evp, EV_SIGNAL);
        return el_destroy(evp, false);
    }
    return (el_data_t)NULL;
}

/*----- child events -----*/

static void el_sigchld_hook(ev_t *ev, int signo, el_data_t priv)
{
    pid_t pid;
    int status;

    while ((pid = waitpid(-1, &status, WNOHANG)) > 0) {
        ev_assoc_t *ec = ev_assoc_htbl_find(&_G.childs, pid);
        if (likely(ec)) {
            ev_t *e = ec->ev;
            (*e->cb.child)(e, pid, status, e->priv);
            ev_assoc_htbl_ll_remove(&_G.childs, ec);
            el_destroy(&e, true);
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
    ev_assoc_t *ec;

    if (unlikely(!hooked)) {
        hooked = true;
        el_signal_register(SIGCHLD, el_sigchld_hook, NULL);
    }

    ec = ev_assoc_htbl_insert(&_G.childs, (ev_assoc_t){ .u = pid, .ev = ev });
    ev->pid = pid;
    ASSERT("pid is already watched", !ec);
    return ev;
}

el_data_t el_child_unregister(ev_t **evp)
{
    if (*evp) {
        ev_assoc_t *ec;

        CHECK_EV_TYPE(*evp, EV_SIGNAL);
        ec = ev_assoc_htbl_find(&_G.childs, (*evp)->pid);
        ASSERT("event not found", ec);
        ev_assoc_htbl_ll_remove(&_G.childs, ec);
        return el_destroy(evp, true);
    }
    return (el_data_t)NULL;
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
    ev_array_append(&_G.timers, ev);
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
    ev_t *end = ev_array_take(&_G.timers, _G.timers.len - 1);

    CHECK_EV_TYPE(*evp, EV_TIMER);
    if (*evp != end) {
        EVTSET(pos, end);
        el_timer_heapfix(end);
    }
    (*evp)->timer.heappos = -1;
    el_list_push(&_G.evs_timer_tmp, *evp);
    return el_destroy(evp, false);
}

static void el_timer_process(uint64_t until)
{
    while (_G.evs_timer_tmp)
        el_list_push(&_G.evs_free, el_list_pop(&_G.evs_timer_tmp));
    while (_G.timers.len) {
        ev_t *ev = EVT(0);

        ASSERT("should be a timer", ev->type == EV_TIMER);
        if (ev->timer.expiry > until)
            return;

        ev->updated = false;
        (*ev->cb.cb)(ev, ev->priv);

        /* ev has been unregistered in (*cb) */
        if (ev->type == EV_UNUSED)
            continue;

        if (ev->timer.repeat) {
            ev->timer.expiry += ev->timer.repeat;
            if (!ev->nomiss && ev->timer.expiry < until) {
                uint64_t delta  = until - ev->timer.expiry;
                uint64_t missed = ((uint64_t)ev->timer.repeat + delta - 1) / delta;

                e_trace(1, "timer %p missed %"PRIu64" events", ev, missed);
                ev->timer.expiry += missed * ev->timer.repeat;
            }
            el_timer_heapdown(0);
        } else
        if (!ev->updated) {
            el_timer_heapremove(&ev);
        }
    }
}

static uint64_t get_clock(bool lowres)
{
    struct timespec ts;
    int res;

    if (lowres && likely(_G.lp_clk))
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
#define TIMER_IS_LOWRES(ev, next)   ((ev)->timerlp || (next) >= 500)

ev_t *el_timer_register(int next, int repeat, int flags, el_cb_f *cb, el_data_t priv)
{
    ev_t *ev = el_create(EV_TIMER, cb, priv, true);
    ev->nomiss  = (flags & EL_TIMER_NOMISS) != 0;
    ev->timerlp = (flags & EL_TIMER_LOWRES) != 0;
    ev->timer.repeat = repeat;
    ev->timer.expiry = (uint64_t)next + get_clock(TIMER_IS_LOWRES(ev, next));
    el_timer_heapinsert(ev);
    return ev;
}

void el_timer_set_hook(el_t ev, el_cb_f *cb)
{
    CHECK_EV_TYPE(ev, EV_TIMER);
    ev->cb.cb = cb;
}

void el_timer_restart(ev_t *ev, int restart)
{
    CHECK_EV_TYPE(ev, EV_TIMER);
    ASSERT("timer isn't a oneshot timer", !ev->timer.repeat);
    ev->timer.expiry = (uint64_t)restart + get_clock(TIMER_IS_LOWRES(ev, restart));
    ev->updated = true;
    el_timer_heapfix(ev);
}

el_data_t el_timer_unregister(ev_t **evp)
{
    return *evp ? el_timer_heapremove(evp) : (el_data_t)NULL;
}

/*----- fd events -----*/

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

int el_fd_get_fd(ev_t *ev)
{
    if (likely(ev)) {
        CHECK_EV_TYPE(ev, EV_FD);
        return ev->fd.fd;
    }
    return -1;
}

/*----- generic functions  -----*/

el_t el_ref(ev_t *ev)
{
    CHECK_EV(ev);
    _G.active += !ev->refs;
    ev->refs = true;
    return ev;
}

el_t el_unref(ev_t *ev)
{
    CHECK_EV(ev);
    _G.active -= ev->refs;
    ev->refs = false;
    return ev;
}

el_data_t el_set_priv(ev_t *ev, el_data_t priv)
{
    CHECK_EV(ev);
    SWAP(el_data_t, ev->priv, priv);
    return priv;
}

void el_loop_timeout(int timeout)
{
    uint64_t clk;

    el_list_process(&_G.before, -1);
    if (_G.timers.len) {
        clk = get_clock(false);
        el_timer_process(clk);
    }
    if (_G.timers.len) {
        uint64_t nxt = EVT(0)->timer.expiry;
        if (nxt < (uint64_t)timeout + clk)
            timeout = nxt - clk;
    }
    el_loop_fds(timeout);
    el_signal_process();
    el_list_process(&_G.after, -1);
}

void el_loop(void)
{
    while (likely(_G.active)) {
        el_loop_timeout(59000); /* arbitrary: 59 seconds */
    }
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
};

static void el_show_blockers(el_t evh, int signo, el_data_t priv)
{
    e_trace(0, "el blocking summary");
    e_trace(0, "===================");

    for (int i = 0; i < _G.evs_len; i++) {
        int bucket_len = 1 << (i + EV_ALLOC_FACTOR);

        for (int j = 0; j < bucket_len; j++) {
            ev_t *ev = &_G.evs[i][j];

            if (ev->type == EV_UNUSED || !ev->refs)
                continue;

            e_trace_start(0, "ev %p │ cb %p │ data %p │ type %d %s",
                          ev, (void *)ev->cb.cb, ev->priv.ptr, ev->type,
                          typenames[ev->type]);

            switch (ev->type) {
              case EV_SIGNAL:
                e_trace_end(0, ":%d", ev->sig.no);
                break;
              case EV_CHILD:
                e_trace_end(0, ":%d", ev->pid);
                break;
              case EV_FD:
                e_trace_end(0, ":%d", ev->fd.fd);
                break;
              case EV_TIMER:
                if (ev->timer.repeat) {
                    e_trace_end(0, " one shot @%"PRIu64, ev->timer.expiry / 1000);
                } else {
                    e_trace_end(0, " repeat:%dms next @%"PRIu64, ev->timer.repeat,
                                ev->timer.expiry / 1000);
                }
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

/**\}*/
