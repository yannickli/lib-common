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

#if !defined(IS_LIB_COMMON_CORE_H) || defined(IS_LIB_COMMON_CORE_BLOCKS_H)
#  error "you must include core.h instead"
#else
#define IS_LIB_COMMON_CORE_BLOCKS_H

#ifdef __BLOCKS__
typedef void (^block_t)(void);
#else
typedef void (*block_t)(void);
#endif

static inline void block_run(void *blk_)
{
    block_t blk = blk_;
    blk();
}

static inline void block_run_and_release(void *blk_)
{
    block_t blk = blk_;
    blk();
    Block_release(blk);
}

#endif
