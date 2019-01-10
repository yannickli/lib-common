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

#ifndef IS_LIB_COMMON_QPAGES_H
#define IS_LIB_COMMON_QPAGES_H

#include "core.h"

/* ACHTUNG MINEN: QPAGE_SIZE must be an unsigned literal to force unsigned
 * arithmetics in expressions, and an uintptr_t to allow dirty masking against
 * pointers.
 */
#define QPAGE_SHIFT         12U
#define QPAGE_SIZE          ((uintptr_t)1 << QPAGE_SHIFT)
#define QPAGE_MASK          (QPAGE_SIZE - 1)

#define QPAGE_COUNT_BITS    (15U)  /* up to 32k pages blocks    */
#define QPAGE_COUNT_MAX     (1U << QPAGE_COUNT_BITS)
#define QPAGE_ALLOC_MAX     (1U << (QPAGE_COUNT_BITS + QPAGE_SHIFT))

void qpage_shutdown(void);


void *qpage_alloc_align(size_t n, size_t shift, uint32_t *seg);
void *qpage_allocraw_align(size_t n, size_t shift, uint32_t *seg);

void *qpage_remap(void *ptr, size_t old_n, uint32_t old_seg,
                  uint32_t new_n, uint32_t *new_seg, bool may_move);
void *qpage_remap_raw(void *ptr, size_t old_n, uint32_t old_seg,
                      uint32_t new_n, uint32_t *new_seg, bool may_move);

void *qpage_dup_n(const void *ptr, size_t n, uint32_t *seg);
void  qpage_free_n(void *, size_t n, uint32_t seg);


static inline void *qpage_alloc_n(size_t n, uint32_t *seg) {
    return qpage_alloc_align(n, 0, seg);
}
static inline void *qpage_alloc(uint32_t *seg) {
    return qpage_alloc_n(1, seg);
}
static inline void *qpage_allocraw_n(size_t n, uint32_t *seg) {
    return qpage_allocraw_align(n, 0, seg);
}
static inline void *qpage_allocraw(uint32_t *seg) {
    return qpage_allocraw_n(1, seg);
}

static inline void *qpage_dup(const void *ptr, uint32_t *seg) {
    return qpage_dup_n(ptr, 1, seg);
}

static inline void qpage_free(void *ptr, uint32_t seg) {
    qpage_free_n(ptr, 1, seg);
}
static inline void qpage_free_n_noseg(void *ptr, size_t n) {
    qpage_free_n(ptr, n, -1);
}
static inline void qpage_free_noseg(void *ptr) {
    qpage_free_n(ptr, 1, -1);
}

#endif
