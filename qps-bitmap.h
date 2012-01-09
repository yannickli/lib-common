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

#ifndef IS_LIB_COMMON_QPS_BITMAP_H
#define IS_LIB_COMMON_QPS_BITMAP_H

#include "qps.h"

/** \defgroup qkv__ll__bitmap QPS Bitmap
 * \ingroup qkv__ll
 * \brief QPS Bitmap
 *
 * \{
 *
 * This bitmap implementation is a 3-level trie mapping a key to a bit. It
 * supports both simple bitmaps and "nullable" bitmap. The nullable
 * implementation associate a pair of bit to each key with the following
 * possible combinations:
 *   * 00: NULL
 *   * 01: unused combination
 *   * 10: bit set at 0
 *   * 11: bit set at 1
 */

#define QPS_BITMAP_ROOTS     64
#define QPS_BITMAP_DISPATCH  2048
#define QPS_BITMAP_WORD      (QPS_PAGE_SIZE / 8)
#define QPS_BITMAP_LEAF      (8 * QPS_PAGE_SIZE)

/* Typedefs {{{ */

typedef qps_pg_t qps_bitmap_node_t;

typedef union qps_bitmap_key_t {
    struct {
#if __BYTE_ORDER == __BIG_ENDIAN
        unsigned root     : 6;
        unsigned dispatch : 11;
        unsigned word     : 9;
        unsigned bit      : 6;
#else
        unsigned bit      : 6;
        unsigned word     : 9;
        unsigned dispatch : 11;
        unsigned root     : 6;
#endif
    };
    struct {
#if __BYTE_ORDER == __BIG_ENDIAN
        unsigned root_null     : 6;
        unsigned dispatch_null : 11;
        unsigned word_null     : 10;
        unsigned bit_null      : 5;
#else
        unsigned bit_null      : 5;
        unsigned word_null     : 10;
        unsigned dispatch_null : 11;
        unsigned root_null     : 6;
#endif
    };
    uint32_t key;
} qps_bitmap_key_t;

typedef struct qps_bitmap_dispatch_node {
    qps_bitmap_node_t node        __attribute__((packed));
    uint16_t          active_bits __attribute__((packed));
} qps_bitmap_dispatch_t[QPS_BITMAP_DISPATCH];

#define QPS_BITMAP_SIG  "QPS_bmap/v01.00"
typedef struct qps_bitmap_root_t {
    /* Signature */
    uint8_t  sig[16];

    /* Structure description */
    flag_t nullable : 1;
    qps_bitmap_node_t roots[QPS_BITMAP_ROOTS];
} qps_bitmap_root_t;

typedef struct qps_bitmap_t {
    qps_t *qps;
    uint32_t struct_gen;

    union {
        qps_bitmap_root_t *root;
        qps_hptr_t         root_cache;
    };
} qps_bitmap_t;

typedef enum qps_bitmap_state_t {
    QPS_BITMAP_0,
    QPS_BITMAP_1,
    QPS_BITMAP_NULL,
} qps_bitmap_state_t;

/* }}} */
/* Public API {{{ */

qps_handle_t qps_bitmap_create(qps_t *qps, bool nullable);
void qps_bitmap_destroy(qps_bitmap_t *map);
void qps_bitmap_clear(qps_bitmap_t *map);

qps_bitmap_state_t qps_bitmap_get(qps_bitmap_t *map, uint32_t row);
qps_bitmap_state_t qps_bitmap_set(qps_bitmap_t *map, uint32_t row);
qps_bitmap_state_t qps_bitmap_reset(qps_bitmap_t *map, uint32_t row);
qps_bitmap_state_t qps_bitmap_remove(qps_bitmap_t *map, uint32_t row);

static inline void
qps_bitmap_init(qps_bitmap_t *map, qps_t *qps, qps_handle_t handle)
{
    p_clear(map, 1);
    map->qps = qps;
    map->struct_gen = 1;
    qps_hptr_init(qps, handle, &map->root_cache);
    assert (strequal(QPS_BITMAP_SIG, (const char *)map->root->sig));
}

/* }}} */
/* Bitmap enumeration {{{ */

typedef struct qps_bitmap_enumerator_t {
    qps_bitmap_t *map;
    qps_bitmap_key_t key;
    bool end;
    bool value;
    bool nullable;

    const uint64_t *leaf;
    const qps_bitmap_dispatch_t *dispatch;

    uint64_t current_word;
    uint32_t struct_gen;
} qps_bitmap_enumerator_t;

/* Generic implementation {{{ */

static inline
void qps_bitmap_enumeration_next_leaf(qps_bitmap_enumerator_t *en,
                                      int start_at);
static inline
void qps_bitmap_enumeration_next_bit(qps_bitmap_enumerator_t *en,
                                     int start_at);

static
void qps_bitmap_enumeration_next_dispatch(qps_bitmap_enumerator_t *en,
                                          int start_at)
{
    en->dispatch = NULL;
    for (int i = start_at; i < QPS_BITMAP_ROOTS; i++) {
        if (en->map->root->roots[i] != 0) {
            en->key.key  = 0;
            en->key.root = i;
            en->dispatch
                = (const qps_bitmap_dispatch_t *)qps_pg_deref(en->map->qps,
                                                              en->map->root->roots[i]);
            qps_bitmap_enumeration_next_leaf(en, 0);
            return;
        }
    }
    en->end = true;
}

static inline
void qps_bitmap_enumeration_next_leaf(qps_bitmap_enumerator_t *en,
                                      int start_at)
{
    en->leaf = NULL;
    assert (en->dispatch != NULL);
    for (int i = start_at; i < QPS_BITMAP_DISPATCH; i++) {
        if ((*en->dispatch)[i].node != 0) {
            en->key.word     = 0;
            en->key.bit      = 0;
            en->key.dispatch = i;
            en->leaf
                = (const uint64_t *)qps_pg_deref(en->map->qps,
                                                 (*en->dispatch)[i].node);
            qps_bitmap_enumeration_next_bit(en, 0);
            return;
        }
    }
    qps_bitmap_enumeration_next_dispatch(en, en->key.root + 1);
}

static inline
void qps_bitmap_enumeration_next_bit(qps_bitmap_enumerator_t *en,
                                     int start_at)
{
    int max = QPS_BITMAP_WORD;
    assert (en->leaf != NULL);

    if (en->nullable) {
        max *= 2;
    }
    for (int i = start_at; i < max; i++) {
        if (en->leaf[i] != 0) {
            if (en->map->root->nullable) {
                en->key.bit_null  = 0;
                en->key.word_null = i;
            } else {
                en->key.bit  = 0;
                en->key.word = i;
            }
            en->current_word = en->leaf[i];
            return;
        }
    }
    qps_bitmap_enumeration_next_leaf(en, en->key.dispatch + 1);
}

static inline
void qps_bitmap_enumeration_next(qps_bitmap_enumerator_t *en)
{
    int bit;
    if (en->end) {
        return;
    }
    if (unlikely(en->struct_gen != en->map->struct_gen)) {
        if (en->map->struct_gen == en->struct_gen + 2) {
            en->struct_gen = en->map->struct_gen;
            qps_bitmap_enumeration_next_leaf(en, en->key.dispatch + 1);
        } else {
            en->struct_gen = en->map->struct_gen;
            qps_bitmap_enumeration_next_dispatch(en, en->key.root + 1);
        }
        return;
    }
    if (en->nullable) {
        if (en->current_word == 0) {
            qps_bitmap_enumeration_next_bit(en, en->key.word_null + 1);
            if (en->end) {
                return;
            }
        }
        bit = bsf64(en->current_word);
        en->value = !(bit & 1);
        en->current_word >>= bit;
        en->current_word  &= ~UINT64_C(3);
        en->key.key += (bit >> 1);
    } else {
        if (en->current_word == 0) {
            qps_bitmap_enumeration_next_bit(en, en->key.word + 1);
            if (en->end) {
                return;
            }
        }
        bit = bsf64(en->current_word);
        en->key.key += bit;
        en->current_word >>= bit;
        en->current_word  &= ~UINT64_C(1);
    }
}

static inline
void qps_bitmap_enumeration_go_to(qps_bitmap_enumerator_t *en, uint32_t row)
{
    qps_bitmap_key_t key;

    key.key = row;
    if (en->end || en->key.key == row) {
        return;
    }

    if (en->key.root < key.root) {
        qps_bitmap_enumeration_next_dispatch(en, key.root);
        if (en->end) {
            return;
        }
    }
    if (en->key.root != key.root) {
        assert (key.root < en->key.root);
        return;
    }

    if (en->key.dispatch < key.dispatch) {
        qps_bitmap_enumeration_next_leaf(en, key.dispatch);
        if (en->end) {
            return;
        }
    }
    if (en->key.root != key.root || en->key.dispatch != key.dispatch) {
        return;
    }

    if (!en->nullable && en->key.word < key.word) {
        en->key.bit = 0;
        while (en->key.word < key.word) {
            en->key.word++;
            en->current_word = en->leaf[en->key.word];
        }
    }
    if (en->key.key == row) {
        qps_bitmap_enumeration_next(en);
    }
    while (!en->end && en->key.key < row) {
        qps_bitmap_enumeration_next(en);
    }
}

static inline
qps_bitmap_enumerator_t qps_bitmap_start_enumeration(qps_bitmap_t *map)
{
    qps_bitmap_enumerator_t en;
    int bit;

    p_clear(&en, 1);
    en.map = map;
    en.struct_gen = map->struct_gen;
    qps_hptr_deref(map->qps, &map->root_cache);

    qps_bitmap_enumeration_next_dispatch(&en, 0);
    if (en.end) {
        return en;
    }
    assert (en.current_word != 0);
    en.nullable = en.map->root->nullable;
    if (en.nullable) {
        bit = bsf64(en.current_word);
        en.value = !(bit & 1);
        en.current_word >>= bit;
        en.current_word  &= ~UINT64_C(3);
        en.key.bit_null += (bit >> 1);
    } else {
        bit = bsf64(en.current_word);
        en.key.bit += bit;
        en.value    = 1;
        en.current_word >>= bit;
        en.current_word  &= ~UINT64_C(1);
    }
    return en;
}

/* }}} */
/* Non-nullable specialization {{{ */

static inline
void qps_bitmap_enumeration_next_leaf_nn(qps_bitmap_enumerator_t *en,
                                      int start_at);
static inline
void qps_bitmap_enumeration_next_bit_nn(qps_bitmap_enumerator_t *en,
                                        int start_at);

static
void qps_bitmap_enumeration_next_dispatch_nn(qps_bitmap_enumerator_t *en,
                                             int start_at)
{
    en->dispatch = NULL;
    for (int i = start_at; i < QPS_BITMAP_ROOTS; i++) {
        if (en->map->root->roots[i] != 0) {
            en->key.key  = 0;
            en->key.root = i;
            en->dispatch
                = (const qps_bitmap_dispatch_t *)qps_pg_deref(en->map->qps,
                                                              en->map->root->roots[i]);
            qps_bitmap_enumeration_next_leaf_nn(en, 0);
            return;
        }
    }
    en->end = true;
}

static inline
void qps_bitmap_enumeration_next_leaf_nn(qps_bitmap_enumerator_t *en,
                                         int start_at)
{
    en->leaf = NULL;
    assert (en->dispatch != NULL);
    for (int i = start_at; i < QPS_BITMAP_DISPATCH; i++) {
        if ((*en->dispatch)[i].node != 0) {
            en->key.word     = 0;
            en->key.bit      = 0;
            en->key.dispatch = i;
            en->leaf
                = (const uint64_t *)qps_pg_deref(en->map->qps,
                                                 (*en->dispatch)[i].node);
            qps_bitmap_enumeration_next_bit_nn(en, 0);
            return;
        }
    }
    qps_bitmap_enumeration_next_dispatch_nn(en, en->key.root + 1);
}

static inline
void qps_bitmap_enumeration_next_bit_nn(qps_bitmap_enumerator_t *en,
                                        int start_at)
{
    int max = QPS_BITMAP_WORD;
    assert (en->leaf != NULL);

    for (int i = start_at; i < max; i++) {
        if (en->leaf[i] != 0) {
            en->key.bit  = 0;
            en->key.word = i;
            en->current_word = en->leaf[i];
            return;
        }
    }
    qps_bitmap_enumeration_next_leaf_nn(en, en->key.dispatch + 1);
}

static inline
void qps_bitmap_enumeration_next_nn(qps_bitmap_enumerator_t *en)
{
    int bit;
    if (en->end) {
        return;
    }
    if (unlikely(en->struct_gen != en->map->struct_gen)) {
        if (en->map->struct_gen == en->struct_gen + 2) {
            en->struct_gen = en->map->struct_gen;
            qps_bitmap_enumeration_next_leaf_nn(en, en->key.dispatch + 1);
        } else {
            en->struct_gen = en->map->struct_gen;
            qps_bitmap_enumeration_next_dispatch_nn(en, en->key.root + 1);
        }
        return;
    }

    if (en->current_word == 0) {
        qps_bitmap_enumeration_next_bit_nn(en, en->key.word + 1);
        if (en->end) {
            return;
        }
    }
    bit = bsf64(en->current_word);
    en->key.key += bit;
    en->current_word >>= bit;
    en->current_word  &= ~UINT64_C(1);
}

static inline
void qps_bitmap_enumeration_go_to_nn(qps_bitmap_enumerator_t *en, uint32_t row)
{
    qps_bitmap_key_t key;

    key.key = row;
    if (en->end || en->key.key == row) {
        return;
    }

    if (en->key.root < key.root) {
        qps_bitmap_enumeration_next_dispatch_nn(en, key.root);
        if (en->end) {
            return;
        }
    }
    if (en->key.root != key.root) {
        assert (key.root < en->key.root);
        return;
    }

    if (en->key.dispatch < key.dispatch) {
        qps_bitmap_enumeration_next_leaf_nn(en, key.dispatch);
        if (en->end) {
            return;
        }
    }
    if (en->key.root != key.root || en->key.dispatch != key.dispatch) {
        return;
    }

    if (en->key.word < key.word) {
        en->key.bit = 0;
        while (en->key.word < key.word) {
            en->key.word++;
            en->current_word = en->leaf[en->key.word];
        }
    }
    if (en->key.key == row) {
        qps_bitmap_enumeration_next_nn(en);
    }
    while (!en->end && en->key.key < row) {
        qps_bitmap_enumeration_next_nn(en);
    }
}

static inline
qps_bitmap_enumerator_t qps_bitmap_start_enumeration_nn(qps_bitmap_t *map)
{
    qps_bitmap_enumerator_t en;
    int bit;

    p_clear(&en, 1);
    en.map = map;
    en.struct_gen = map->struct_gen;
    qps_hptr_deref(map->qps, &map->root_cache);

    qps_bitmap_enumeration_next_dispatch_nn(&en, 0);
    if (en.end) {
        return en;
    }
    assert (en.current_word != 0);
    assert (!en.map->root->nullable);
    en.nullable = false;
    bit = bsf64(en.current_word);
    en.key.key += bit;
    en.value    = 1;
    en.current_word >>= bit;
    en.current_word  &= ~UINT64_C(1);
    return en;
}


#define qps_bitmap_for_each(en, map)                                         \
        for (qps_bitmap_enumerator_t en = qps_bitmap_start_enumeration(map); \
             !en.end; qps_bitmap_enumeration_next(&en))

#define qps_bitmap_for_each_nn(en, map)                                      \
        for (qps_bitmap_enumerator_t en = qps_bitmap_start_enumeration_nn(map); \
             !en.end; qps_bitmap_enumeration_next_nn(&en))

/* }}} */
/* Debugging tools {{{ */

void qps_bitmap_get_qps_roots(qps_bitmap_t *map, qps_roots_t *roots);

/* }}} */

/** \} */
#endif
