/***************************************************************************/
/*                                                                         */
/* Copyright 2021 INTERSEC SA                                              */
/*                                                                         */
/* Licensed under the Apache License, Version 2.0 (the "License");         */
/* you may not use this file except in compliance with the License.        */
/* You may obtain a copy of the License at                                 */
/*                                                                         */
/*     http://www.apache.org/licenses/LICENSE-2.0                          */
/*                                                                         */
/* Unless required by applicable law or agreed to in writing, software     */
/* distributed under the License is distributed on an "AS IS" BASIS,       */
/* WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.*/
/* See the License for the specific language governing permissions and     */
/* limitations under the License.                                          */
/*                                                                         */
/***************************************************************************/

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
static inline void block_run(void * nonnull blk_)
{
    block_t blk = (__bridge block_t)blk_;
    blk();
}

static inline void block_run_and_release(void * nonnull blk_)
{
    block_t blk = (__bridge block_t)blk_;
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
