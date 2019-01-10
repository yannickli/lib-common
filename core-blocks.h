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
#   define __has_blocks
#   define BLOCK_CARET  ^
    typedef void (^block_t)(void);
#elif defined(__block)  /* ugly works because it's defined by the rewriter */
#   define __has_blocks
#   define BLOCK_CARET  *
    typedef void (*block_t)(void);
#else
    typedef void * block_t;
#endif

#ifdef __has_blocks
static inline void block_run(void *blk_)
{
    block_t blk = (block_t)blk_;
    blk();
}

static inline void block_run_and_release(void *blk_)
{
    block_t blk = (block_t)blk_;
    blk();
    Block_release(blk);
}

#define Block_release_p(pBlock) \
    do {                                            \
        typeof(*(pBlock)) *pp = (pBlock);           \
        \
        if (*pp) {                                  \
            Block_release(*pp);                     \
            *pp = NULL;                             \
        }                                           \
    } while (0)

#endif

#endif
