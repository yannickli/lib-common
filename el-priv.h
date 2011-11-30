/**************************************************************************/
/*                                                                        */
/*  Copyright (C) 2004-2011 INTERSEC SAS                                  */
/*                                                                        */
/*  Should you receive a copy of this source code, you must check you     */
/*  have a proper, written authorization of INTERSEC to hold it. If you   */
/*  don't have such an authorization, you must DELETE all source code     */
/*  files in your possession, and inform INTERSEC of the fact you obtain  */
/*  these files. Should you not comply to these terms, you can be         */
/*  prosecuted in the extent permitted by applicable law.                 */
/*                                                                        */
/**************************************************************************/

#ifndef IS_LIB_COMMON_EL_PRIV_H
#define IS_LIB_COMMON_EL_PRIV_H

#include "container.h"
#include "el.h"

typedef enum ev_type_t {
    EV_UNUSED,
    EV_BLOCKER,
    EV_BEFORE,
    EV_SIGNAL,
    EV_CHILD,
    EV_FD,
    EV_TIMER,
    EV_PROXY,
    EV_IDLE,
} ev_type_t;

enum ev_flags_t {
    EV_FLAG_REFS          = (1U <<  0),
    EV_FLAG_TRACE         = (1U <<  1),
    EV_FLAG_IS_BLK        = (1U <<  2),

    EV_FLAG_TIMER_NOMISS  = (1U <<  8),
    EV_FLAG_TIMER_LOWRES  = (1U <<  9),
    EV_FLAG_TIMER_UPDATED = (1U << 10),

    EV_FLAG_FD_WATCHED    = (1U <<  8),
#define EV_FLAG_HAS(ev, f)   ((ev)->flags & EV_FLAG_##f)
#define EV_FLAG_SET(ev, f)   ((ev)->flags |= EV_FLAG_##f)
#define EV_FLAG_RST(ev, f)   ((ev)->flags &= ~EV_FLAG_##f)

#ifndef NDEBUG
#define EV_IS_TRACED(ev)   unlikely(EV_FLAG_HAS(ev, TRACE))
#else
#define EV_IS_TRACED(ev)   0
#endif
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

        el_cb_b      cb_blk;
        el_signal_b  signal_blk;
        el_child_b   child_blk;
        el_fd_b      fd_blk;
        el_proxy_b   proxy_blk;
    } cb;
    union {
        el_data_t priv;
        block_t   wipe;
    };

    union {
        dlist_t ev_list;        /* EV_BEFORE, EV_SIGNAL, EV_PROXY */
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

#endif
