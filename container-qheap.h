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

#if !defined(IS_LIB_COMMON_CONTAINER_H) || defined(IS_LIB_COMMON_CONTAINER_HEAP_H)
#  error "you must include container.h instead"
#else
#define IS_LIB_COMMON_CONTAINER_HEAP_H

#include "str.h"
#include "core.h"

/* Binary Heap Container
 * ----------
 *
 * This container can be used as a binary max heap so as a binary min heap.
 * Its content is stored into a qvector.
 *
 * The nodes are defined by the user. A macro or function that compares
 * two nodes.
 * ----------
 *
 * This code is a re-write of lib-common event loop code min heap.
 * ----------
 *
 * Scalar heaps are defined at the end of this file.
 * ----------
 *
 * Example:
 * -----
 *
 * typedef struct test_node_t {
 *     int heap_pos;
 *     int val;
 * } test_node_t;
 *
 * #define CMP(a, op, b)       ((a)->val op (b)->val)
 * #define SET_POS(node, pos)  ((node)->heap_pos = pos)
 *
 * qhp_min_t(min_heap, test_node_t *, CMP, SET_POS);
 * qhp_max_t(max_heap, test_node_t *, CMP, SET_POS);
 *
 * ----------
 * More code examples into: lib-common/zchk-container.blk
 */

#define qhp_t(name)  qhp_##name##_t

/*{{{1 Type */

#define qhp_type_t(name, type_t)                       \
    qvector_t(qhp_##name, type_t);                     \
    typedef qv_t(qhp_##name) qhp_t(name);

/*1}}}*/
/*{{{1 Functions */

/*{{{2 Helpers */

#define QHP_CHILD_L(pos)           (2 * (pos) + 1)
#define QHP_CHILD_R(pos)           (QHP_CHILD_L(pos) + 1)
#define QHP_PARENT(pos)            (((pos) - 1) / 2)

#define QHP_CMP(is_min_heap, cmp, n1, op, n2)  \
    (is_min_heap ? cmp(n1, op, n2) : cmp(n2, op, n1))

/*2}}}*/

#define qhp_funcs_t(n, type_t, is_min_heap, cmp, set_pos)                   \
                                                                            \
    static ALWAYS_INLINE int                                                \
    __qhp_##n##_move(qhp_t(n) *heap, type_t node, int pos)                  \
    {                                                                       \
        set_pos(node, pos);                                                 \
        heap->tab[pos] = node;                                              \
                                                                            \
        return pos;                                                         \
    }                                                                       \
                                                                            \
    static inline int __qhp_##n##_down(qhp_t(n) *heap, int pos)             \
    {                                                                       \
        type_t node = heap->tab[pos];                                       \
                                                                            \
        for (;;) {                                                          \
            int    child_pos = QHP_CHILD_L(pos);                            \
            type_t child;                                                   \
                                                                            \
            if (child_pos >= heap->len)                                     \
                break;                                                      \
                                                                            \
            child = heap->tab[child_pos];                                   \
                                                                            \
            if (child_pos + 1 < heap->len) {                                \
                type_t r_child = heap->tab[child_pos + 1];                  \
                                                                            \
                if (QHP_CMP(is_min_heap, cmp, r_child, <, child)) {         \
                    child = r_child;                                        \
                    child_pos++;                                            \
                }                                                           \
            }                                                               \
                                                                            \
            if (QHP_CMP(is_min_heap, cmp, child, >=, node))                 \
                break;                                                      \
                                                                            \
            __qhp_move(n, heap, child, pos);                                \
            pos = child_pos;                                                \
        }                                                                   \
                                                                            \
        return __qhp_move(n, heap, node, pos);                              \
    }                                                                       \
                                                                            \
    static inline int __qhp_##n##_up(qhp_t(n) *heap, int pos)               \
    {                                                                       \
        type_t node = heap->tab[pos];                                       \
                                                                            \
        while (pos > 0) {                                                   \
            int    parent_pos = QHP_PARENT(pos);                            \
            type_t parent     = heap->tab[parent_pos];                      \
                                                                            \
            if (QHP_CMP(is_min_heap, cmp, node, >=, parent))                \
                break;                                                      \
                                                                            \
            __qhp_move(n, heap, parent, pos);                               \
            pos = parent_pos;                                               \
        }                                                                   \
                                                                            \
        return __qhp_move(n, heap, node, pos);                              \
    }                                                                       \
                                                                            \
    static inline int qhp_##n##_insert(qhp_t(n) *heap, type_t node)         \
    {                                                                       \
        set_pos(node, heap->len);                                           \
        qv_append(qhp_##n, heap, node);                                     \
                                                                            \
        return __qhp_up(n, heap, heap->len - 1);                            \
    }                                                                       \
                                                                            \
    static inline int qhp_##n##_fixup(qhp_t(n) *heap, int pos)              \
    {                                                                       \
        type_t node       = heap->tab[pos];                                 \
        int    parent_pos = QHP_PARENT(pos);                                \
        type_t parent     = heap->tab[parent_pos];                          \
                                                                            \
        if (pos > 0 && QHP_CMP(is_min_heap, cmp, parent, >=, node)) {       \
            return __qhp_up(n, heap, pos);                                  \
        } else {                                                            \
            return __qhp_down(n, heap, pos);                                \
        }                                                                   \
    }                                                                       \
                                                                            \
    static inline type_t qhp_##n##_remove(qhp_t(n) *heap, int pos)          \
    {                                                                       \
        type_t node;                                                        \
                                                                            \
        if (unlikely(pos < 0)) {                                            \
            p_clear(&node, 1);                                              \
            return node;                                                    \
        }                                                                   \
                                                                            \
        node = heap->tab[pos];                                              \
        if (pos == heap->len - 1) {                                         \
            qv_shrink(qhp_##n, heap, 1);                                    \
        } else {                                                            \
            type_t last =  *qv_last(qhp_##n, heap);                         \
                                                                            \
            qv_shrink(qhp_##n, heap, 1);                                    \
            __qhp_move(n, heap, last, pos);                                 \
            qhp_fixup(n, heap, pos);                                        \
        }                                                                   \
                                                                            \
        set_pos(node, -1);                                                  \
        return node;                                                        \
    }

/*1}}}*/

/* \macro  qhp_t
 * \brief  Declares a binary heap and its functions
 *
 * \param  name         Name of the binary heap
 *
 * \param  type_t       Node type
 *
 * \param  is_min_heap  Boolean. Defines whether the heap is a min heap or a
 *                      max heap.
 *
 * \param  cmp          Comparison macro. Take two nodes and a logical
 *                      operator. Examples: QHP_SCALAR_CMP and QHP_LSTR_CMP.
 *
 * \param  set_pos      Position setter function or macro. This setter is
 *                      optional: if user does not need to maintain the
 *                      position of his nodes into the heap, he just have to
 *                      pass QHP_SET_NO_POS for this parameter. This setter is
 *                      useful for users who want to remove or fix up an
 *                      object which is not the first of the heap.
 */
#define qhp_full_t(name, type_t, is_min_heap, cmp, set_pos)   \
    qhp_type_t(name, type_t);                                 \
    qhp_funcs_t(name, type_t, is_min_heap, cmp, set_pos);

#define qhp_min_t(name, type_t, cmp, set_pos)                 \
    qhp_full_t(name, type_t, true, cmp, set_pos)

#define qhp_max_t(name, type_t, cmp, set_pos)                 \
    qhp_full_t(name, type_t, false, cmp, set_pos)

/* Setup */
#define qhp_init(n, heap)         qv_init(qhp_##n, heap)
#define qhp_inita(n, heap, len)   qv_inita(qhp_##n, heap, len)
#define t_qhp_init(n, heap, len)  t_qv_init(qhp_##n, heap, len)
#define qhp_wipe(n, heap)         qv_wipe(qhp_##n, heap)
#define qhp_clear(n, heap)        qv_clear(qhp_##n, heap)

/* Low-level update functions */
#define __qhp_move(n, heap, node, pos)  __qhp_##n##_move(heap, node, pos)
#define __qhp_up(n, heap, pos)          __qhp_##n##_up(heap, pos)
#define __qhp_down(n, heap, pos)        __qhp_##n##_down(heap, pos)

/* Content modifiers */
#define qhp_insert(n, heap, node)       qhp_##n##_insert(heap, node)
#define qhp_fixup(n, heap, pos)         qhp_##n##_fixup(heap, pos)
#define qhp_fixup_first(n, heap)        qhp_fixup(n, heap, 0)
#define qhp_remove(n, heap, pos)        qhp_##n##_remove(heap, pos)
#define qhp_take_first(n, heap)         qhp_remove(n, heap, 0)

/* Getters */
#define qhp_len(n, heap)       ({  ASSERT_COMPATIBLE(qhp_t(n) *, heap);   \
                                    (heap)->len; })
#define qhp_is_empty(n, heap)  ({  ASSERT_COMPATIBLE(qhp_t(n) *, heap);   \
                                    !(heap)->len; })
#define qhp_get(n, heap, pos)  ({  ASSERT_COMPATIBLE(qhp_t(n) *, heap);   \
                                    (heap)->tab[pos]; })
#define qhp_first(n, heap)     qhp_get(n, heap, 0)

/* XXX In qhp_for_each_pos, when using a function that modifies binary heap
 *     content, user must absolutly break the loop.
 */
#define qhp_for_each_pos(n, pos, heap)  qv_for_each_pos(qhp_##n, pos, heap)

#define QHP_SCALAR_CMP(a, op, b)  ((a) op (b))
#define QHP_IGNORE(...)

qhp_min_t(i8_min,     int8_t,   QHP_SCALAR_CMP, QHP_IGNORE);
qhp_max_t(i8_max,     int8_t,   QHP_SCALAR_CMP, QHP_IGNORE);
qhp_min_t(u8_min,     uint8_t,  QHP_SCALAR_CMP, QHP_IGNORE);
qhp_max_t(u8_max,     uint8_t,  QHP_SCALAR_CMP, QHP_IGNORE);
qhp_min_t(i16_min,    int16_t,  QHP_SCALAR_CMP, QHP_IGNORE);
qhp_max_t(i16_max,    int16_t,  QHP_SCALAR_CMP, QHP_IGNORE);
qhp_min_t(u16_min,    uint16_t, QHP_SCALAR_CMP, QHP_IGNORE);
qhp_max_t(u16_max,    uint16_t, QHP_SCALAR_CMP, QHP_IGNORE);
qhp_min_t(i32_min,    int32_t,  QHP_SCALAR_CMP, QHP_IGNORE);
qhp_max_t(i32_max,    int32_t,  QHP_SCALAR_CMP, QHP_IGNORE);
qhp_min_t(u32_min,    uint32_t, QHP_SCALAR_CMP, QHP_IGNORE);
qhp_max_t(u32_max,    uint32_t, QHP_SCALAR_CMP, QHP_IGNORE);
qhp_min_t(i64_min,    int64_t,  QHP_SCALAR_CMP, QHP_IGNORE);
qhp_max_t(i64_max,    int64_t,  QHP_SCALAR_CMP, QHP_IGNORE);
qhp_min_t(u64_min,    uint64_t, QHP_SCALAR_CMP, QHP_IGNORE);
qhp_max_t(u64_max,    uint64_t, QHP_SCALAR_CMP, QHP_IGNORE);
/* XXX Using float is bad, please use double instead */
qhp_min_t(double_min, double,   QHP_SCALAR_CMP, QHP_IGNORE);
qhp_max_t(double_max, double,   QHP_SCALAR_CMP, QHP_IGNORE);

#define QHP_LSTR_CMP(a, op, b)  (lstr_cmp((a), (b)) op 0)

qhp_min_t(lstr_min, lstr_t *, QHP_LSTR_CMP, QHP_IGNORE);
qhp_max_t(lstr_max, lstr_t *, QHP_LSTR_CMP, QHP_IGNORE);

#endif /* IS_LIB_COMMON_CONTAINER_HEAP_H */
