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

#if !defined(IS_LIB_COMMON_NET_H) || defined(IS_LIB_COMMON_NET_RATE_H)
#  error "you must include net.h instead"
#else
#define IS_LIB_COMMON_NET_RATE_H

typedef struct net_rctl_t {
    unsigned     rates[10];
    unsigned     rate;
    unsigned     slice_max;
    unsigned     remains;
    uint16_t     slot;
    bool         is_blk;
    struct ev_t * nonnull cron;
    union {
        void (*nonnull on_ready)(struct net_rctl_t * nonnull);
#ifdef __has_blocks
        block_t __unsafe_unretained nonnull blk;
#endif
    };
} net_rctl_t;

static ALWAYS_INLINE bool net_rctl_can_fire(net_rctl_t * nonnull rctl)
{
    return rctl->remains != 0;
}

static ALWAYS_INLINE void __net_rctl_fire(net_rctl_t * nonnull rctl)
{
    rctl->remains--;
}

static ALWAYS_INLINE bool net_rctl_fire(net_rctl_t * nonnull rctl)
{
    if (net_rctl_can_fire(rctl)) {
        __net_rctl_fire(rctl);
        return true;
    }
    return false;
}

void net_rctl_init(net_rctl_t * nonnull rctl, int rate,
                   void (*nonnull cb)(net_rctl_t * nonnull));
#ifdef __has_blocks
void net_rctl_init_blk(net_rctl_t * nonnull rctl, int rate,
                       block_t nonnull blk);
#endif

void net_rctl_start(net_rctl_t * nonnull rctl);
void net_rctl_stop(net_rctl_t * nonnull rctl);
void net_rctl_wipe(net_rctl_t * nonnull rctl);

#endif
