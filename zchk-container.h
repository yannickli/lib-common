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

#ifndef IS_LIB_COMMON_ZCHK_CONTAINER_H
#define IS_LIB_COMMON_ZCHK_CONTAINER_H

#include "container.h"

typedef struct test_node_t {
    int pos;
    int val;
} test_node_t;

#define TEST_NODE_CMP(a, op, b)  ((a)->val op (b)->val)

static ALWAYS_INLINE void test_node_set_pos(test_node_t *node, int pos)
{
    node->pos = pos;
}

qhp_min_t(test_heap, test_node_t *, TEST_NODE_CMP, test_node_set_pos);

typedef struct test_node_inl_t {
    uint8_t val;
    int8_t  pos;
} test_node_inl_t;

#define TEST_NODE_INL_CMP(a, op, b)        ((a).val op (b).val)
#define TEST_NODE_INL_SET_POS(node, _pos)  ((node).pos = _pos)

qhp_min_t(inl_heap, test_node_inl_t, TEST_NODE_INL_CMP,
          TEST_NODE_INL_SET_POS);

#endif /* IS_LIB_COMMON_ZCHK_CONTAINER_H */
