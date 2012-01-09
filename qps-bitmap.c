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

#include "qps-bitmap.h"

/* Deref {{{ */

static
qps_bitmap_dispatch_t *w_deref_dispatch(qps_bitmap_t *map,
                                        qps_bitmap_key_t key,
                                        bool create)
{
    qps_bitmap_node_t dispatch_node;
    STATIC_ASSERT(sizeof(qps_bitmap_dispatch_t) == 3 * QPS_PAGE_SIZE);

    qps_hptr_deref(map->qps, &map->root_cache);
    dispatch_node = map->root->roots[key.root];
    if (dispatch_node == 0) {
        if (!create) {
            return NULL;
        }
        qps_hptr_w_deref(map->qps, &map->root_cache);
        dispatch_node = qps_pg_map(map->qps, 3);
        map->struct_gen += 2;
        qps_pg_zero(map->qps, dispatch_node, 3);
        map->root->roots[key.root] = dispatch_node;
    }
    return qps_pg_deref(map->qps, dispatch_node);
}

static
uint64_t *w_deref_leaf(qps_bitmap_t *map, qps_bitmap_dispatch_t **dispatch,
                       qps_bitmap_key_t key, bool create)
{
    qps_bitmap_node_t leaf_node;

    if (dispatch == NULL || *dispatch == NULL) {
        return NULL;
    }

    leaf_node = (*(*dispatch))[key.dispatch].node;
    if (leaf_node == 0) {
        const uint32_t pages = map->root->nullable ? 2 : 1;
        if (!create) {
            return NULL;
        }
        *dispatch = w_deref_dispatch(map, key, false);
        assert (*dispatch);
        leaf_node = qps_pg_map(map->qps, pages);
        map->struct_gen += 2;
        qps_pg_zero(map->qps, leaf_node, pages);
        (*(*dispatch))[key.dispatch].node = leaf_node;
        (*(*dispatch))[key.dispatch].active_bits = 0;
    }
    return qps_pg_deref(map->qps, leaf_node);
}

static
void delete_leaf(qps_bitmap_t *map, qps_bitmap_key_t key)
{
    qps_bitmap_dispatch_t *dispatch = w_deref_dispatch(map, key, false);
    qps_bitmap_node_t leaf_node;
    if (dispatch == NULL) {
        return;
    }

    leaf_node = (*dispatch)[key.dispatch].node;
    if (leaf_node == 0) {
        return;
    }

    qps_pg_unmap(map->qps, leaf_node);
    map->struct_gen += 2;
    (*dispatch)[key.dispatch].node = 0;
    for (int i = 0; i < QPS_BITMAP_DISPATCH; i++) {
        if ((*dispatch)[i].node != 0) {
            return;
        }
    }
    qps_hptr_w_deref(map->qps, &map->root_cache);
    qps_pg_unmap(map->qps, map->root->roots[key.root]);
    map->struct_gen += 2;
    map->root->roots[key.root] = 0;
}

static
void delete_nodes(qps_bitmap_t *map)
{
    for (int i = 0; i < QPS_BITMAP_ROOTS; i++) {
        const qps_bitmap_dispatch_t *dispatch;
        if (map->root->roots[i] == 0) {
            continue;
        }
        dispatch = qps_pg_deref(map->qps, map->root->roots[i]);
        for (int j = 0; j < QPS_BITMAP_DISPATCH; j++) {
            if ((*dispatch)[j].node == 0) {
                continue;
            }
            qps_pg_unmap(map->qps, (*dispatch)[j].node);
        }
        qps_pg_unmap(map->qps, map->root->roots[i]);
    }
}

/* }}} */
/* Public API {{{ */

qps_handle_t qps_bitmap_create(qps_t *qps, bool nullable)
{
    qps_bitmap_root_t *map;
    qps_hptr_t cache;

    map = qps_hptr_alloc(qps, sizeof(qps_bitmap_root_t), &cache);
    p_clear(map, 1);
    memcpy(map->sig, QPS_BITMAP_SIG, countof(map->sig));
    map->nullable = nullable;
    return cache.handle;
}

void qps_bitmap_destroy(qps_bitmap_t *map)
{
    qps_hptr_deref(map->qps, &map->root_cache);
    delete_nodes(map);
    qps_hptr_free(map->qps, &map->root_cache);
}

void qps_bitmap_clear(qps_bitmap_t *map)
{
    qps_hptr_w_deref(map->qps, &map->root_cache);
    delete_nodes(map);
    p_clear(map->root->roots, 1);
}

qps_bitmap_state_t qps_bitmap_get(qps_bitmap_t *map, uint32_t row)
{
    qps_bitmap_key_t key = { .key = row };
    const qps_bitmap_dispatch_t *dispatch;
    const uint64_t *leaf;
    qps_bitmap_node_t dispatch_node;
    qps_bitmap_node_t leaf_node;

    qps_hptr_deref(map->qps, &map->root_cache);
    dispatch_node = map->root->roots[key.root];
    if (dispatch_node == QPS_HANDLE_NULL) {
        return map->root->nullable ? QPS_BITMAP_NULL : QPS_BITMAP_0;
    }

    dispatch  = qps_pg_deref(map->qps, dispatch_node);
    leaf_node = (*dispatch)[key.dispatch].node;
    if (leaf_node == 0) {
        return map->root->nullable ? QPS_BITMAP_NULL : QPS_BITMAP_0;
    }

    leaf = qps_pg_deref(map->qps, leaf_node);
    if (map->root->nullable) {
        uint64_t word = leaf[key.word_null];
        word >>= (key.bit_null * 2);
        if (!(word & 0x2)) {
            return QPS_BITMAP_NULL;
        }
        return (qps_bitmap_state_t)(word & 0x1);
    } else {
        uint64_t word = leaf[key.word];
        word >>= key.bit;
        return (qps_bitmap_state_t)(word & 0x1);
    }
}

qps_bitmap_state_t qps_bitmap_set(qps_bitmap_t *map, uint32_t row)
{
    qps_bitmap_key_t key = { .key = row };
    qps_bitmap_dispatch_t *dispatch;
    uint64_t *leaf;

    dispatch = w_deref_dispatch(map, key, true);
    leaf     = w_deref_leaf(map, &dispatch, key, true);

    if (map->root->nullable) {
        uint64_t word = leaf[key.word_null];
        word >>= (key.bit_null * 2);
        if (!(word & 0x2)) {
            word = (UINT64_C(0x3) << (key.bit_null * 2));
            leaf[key.word_null] |= word;
            (*dispatch)[key.dispatch].active_bits++;
            return QPS_BITMAP_NULL;
        } else
        if (!(word & 0x1)) {
            word = (UINT64_C(0x3) << (key.bit_null * 2));
            leaf[key.word_null] |= word;
            return QPS_BITMAP_0;
        }
        return QPS_BITMAP_1;
    } else {
        uint64_t word = UINT64_C(1) << key.bit;
        if (!(leaf[key.word] & word)) {
            leaf[key.word] |= word;
            (*dispatch)[key.dispatch].active_bits++;
            return QPS_BITMAP_0;
        }
        return QPS_BITMAP_1;
    }
}

qps_bitmap_state_t qps_bitmap_reset(qps_bitmap_t *map, uint32_t row)
{
    qps_bitmap_key_t key = { .key = row };
    qps_bitmap_dispatch_t *dispatch;
    uint64_t *leaf;

    dispatch = w_deref_dispatch(map, key, map->root->nullable);
    leaf = w_deref_leaf(map, &dispatch, key, map->root->nullable);
    if (leaf == NULL) {
        return QPS_BITMAP_0;
    }

    if (map->root->nullable) {
        uint64_t word = leaf[key.word_null];
        uint64_t mask;
        word >>= (key.bit_null * 2);
        if (!(word & 0x2)) {
            mask = (UINT64_C(0x3) << (key.bit_null * 2));
            word = (UINT64_C(0x2) << (key.bit_null * 2));
            leaf[key.word_null] &= ~mask;
            leaf[key.word_null] |= word;
            (*dispatch)[key.dispatch].active_bits++;
            return QPS_BITMAP_NULL;
        } else
        if ((word & 0x1)) {
            mask = (UINT64_C(0x3) << (key.bit_null * 2));
            word = (UINT64_C(0x2) << (key.bit_null * 2));
            leaf[key.word_null] &= ~mask;
            leaf[key.word_null] |= word;
            return QPS_BITMAP_1;
        }
        return QPS_BITMAP_0;
    } else {
        uint64_t word = UINT64_C(1) << key.bit;
        if ((leaf[key.word] & word)) {
            leaf[key.word] &= ~word;
            (*dispatch)[key.dispatch].active_bits--;
            if ((*dispatch)[key.dispatch].active_bits == 0) {
                delete_leaf(map, key);
            }
            return QPS_BITMAP_1;
        }
        return QPS_BITMAP_0;
    }
}

qps_bitmap_state_t qps_bitmap_remove(qps_bitmap_t *map, uint32_t row)
{
    qps_bitmap_key_t key = { .key = row };
    qps_bitmap_dispatch_t *dispatch;
    uint64_t *leaf;

    dispatch = w_deref_dispatch(map, key, false);
    leaf = w_deref_leaf(map, &dispatch, key, false);
    if (leaf == NULL) {
        return map->root->nullable ? QPS_BITMAP_NULL : QPS_BITMAP_0;
    }

    if (map->root->nullable) {
        qps_bitmap_state_t previous;
        uint64_t word = leaf[key.word_null];
        uint64_t mask;
        word >>= (key.bit_null * 2);
        if (!(word & 0x2)) {
            return QPS_BITMAP_NULL;
        } else {
            mask = (UINT64_C(0x3) << (key.bit_null * 2));
            leaf[key.word_null] &= ~mask;
            (*dispatch)[key.dispatch].active_bits--;
            previous = (qps_bitmap_state_t)(word & 0x1);
            if ((*dispatch)[key.dispatch].active_bits == 0) {
                delete_leaf(map, key);
            }
            return previous;
        }
    } else {
        uint64_t word = UINT64_C(1) << key.bit;
        if ((leaf[key.word] & word)) {
            leaf[key.word] &= ~word;
            (*dispatch)[key.dispatch].active_bits--;
            if ((*dispatch)[key.dispatch].active_bits == 0) {
                delete_leaf(map, key);
            }
            return QPS_BITMAP_1;
        }
        return QPS_BITMAP_0;
    }

}

/* }}} */
/* Debugging tool {{{ */

void qps_bitmap_get_qps_roots(qps_bitmap_t *map, qps_roots_t *roots)
{
    qps_hptr_deref(map->qps, &map->root_cache);
    for (int i = 0; i < QPS_BITMAP_ROOTS; i++) {
        const qps_bitmap_dispatch_t *dispatch;

        if (map->root->roots[i] == 0) {
            continue;
        }
        qv_append(qps_pg, &roots->pages, map->root->roots[i]);
        dispatch = qps_pg_deref(map->qps, map->root->roots[i]);
        for (int j = 0; j < QPS_BITMAP_DISPATCH; j++) {
            if ((*dispatch)[j].node == 0) {
                continue;
            }
            qv_append(qps_pg, &roots->pages, (*dispatch)[j].node);
        }
    }
    qv_append(qps_handle, &roots->handles, map->root_cache.handle);
}

/* }}} */
