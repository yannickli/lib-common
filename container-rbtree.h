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

#if !defined(IS_LIB_COMMON_CONTAINER_H)
#  error "you must include container.h instead"
#endif
#ifndef IS_LIB_COMMON_CONTAINER_RBTREE_H
#define IS_LIB_COMMON_CONTAINER_RBTREE_H

typedef struct rb_t {
    struct rb_node_t *root;
} rb_t;

typedef struct rb_node_t {
    uintptr_t __parent;
    struct rb_node_t *left, *right;
} rb_node_t;

/*
 * Do-it-yourself rbtree:
 *
 *   for performance reasons, inlining the comparison is a huge win, you have
 *   to write the searches and insertion procedure this way with the helpers
 *   container-rbtree provides.
 *
 *   entry_t *rb_entry_search(rb_t *rb, key_t *key)
 *   {
 *        rb_node_t *n = rb->root;
 *
 *        while (n) {
 *            entry_t *e = rb_entry(n, entry_t, some_member);
 *
 *            if (key < e->key) {
 *                n = n->left;
 *            } else
 *            if (key > e->key) {
 *                n = n->right;
 *            } else {
 *                return e;
 *            }
 *        }
 *        return NULL;
 *   }
 *
 *
 *   void rb_entry_insert(rb_t *rb, entry_t *e)
 *   {
 *        rb_node_t **slot = &rb->root;
 *        rb_node_t *parent = NULL;
 *
 *        while (*slot) {
 *            entry_t *slot_e = rb_entry(*slot, entry_t, some_member);
 *
 *            parent = *slot;
 *            if (e->key < slot_e->key) {
 *                slot = &(*slot)->left;
 *            } else
 *            if (e->ken > slot_e->key) {
 *                slot = &(*slot)->right;
 *            } else {
 *                // treat key duplicates here
 *            }
 *        }
 *        rb_add_node(rb, parent, *slot = &e->some_member);
 *    }
 *
 */

#define rb_parent(n)                 ((rb_node_t *)((n)->__parent & ~1))
#define	rb_entry(ptr, type, member)  container_of(ptr, type, member)
#define	rb_entry_of(ptr, n, member)  container_of(ptr, typeof(*n), member)

void rb_add_node(rb_t *, rb_node_t *parent, rb_node_t *node) __leaf;
void rb_del_node(rb_t *, rb_node_t *) __leaf;

static inline rb_node_t *rb_first(rb_t *rb)
{
    struct rb_node_t *n = rb->root;

    if (!n)
        return NULL;
    while (n->left)
        n = n->left;
    return n;
}
static inline rb_node_t *rb_last(rb_t *rb)
{
    struct rb_node_t *n = rb->root;

    if (!n)
        return NULL;
    while (n->right)
        n = n->right;
    return n;
}

rb_node_t *rb_next(rb_node_t *) __leaf;
rb_node_t *rb_prev(rb_node_t *) __leaf;

#define __rb_for_each(it, rb, doit)                             \
    for (rb_node_t *it = rb_first(rb);                          \
         it && ({ doit; 1; }); it = rb_next(it))

#define __rb_for_each_safe(it, __next, rb, doit)                \
    for (rb_node_t *it = rb_first(rb), *__next;                 \
         it && ({ __next = rb_next(it); doit; 1; });            \
         it = __next)

#define rb_for_each(it, rb)       __rb_for_each(it, rb, )
#define rb_for_each_safe(it, rb)  __rb_for_each_safe(it, __next_##it, rb, )

#define rb_for_each_entry(it, rb, member)                       \
    __rb_for_each(__real_##it, rb,                              \
                  it = rb_entry_of(__real_##it, it, member))

#define rb_for_each_entry_safe(it, rb, member)                    \
    __rb_for_each_safe(__real_##it, __next_##it, rb,              \
                       it = rb_entry_of(__real_##it, it, member))


#define RB_SEARCH_I(entry_t, pfx, node_member, key_member) \
    entry_t *pfx##_search(rb_t rb, fieldtypeof(entry_t, key_member) key)     \
    {                                                                        \
        rb_node_t *n = rb.root;                                              \
                                                                             \
        while (n) {                                                          \
            entry_t *e = rb_entry(n, entry_t, node_member);                  \
                                                                             \
            if (key < e->key_member) {                                       \
                n = n->left;                                                 \
            } else                                                           \
            if (key > e->key_member) {                                       \
                n = n->right;                                                \
            } else {                                                         \
                return e;                                                    \
            }                                                                \
        }                                                                    \
        return NULL;                                                         \
    }

#define RB_SEARCH_P(entry_t, pfx, node_member, key_member, cmp_f) \
    entry_t *pfx##_search(rb_t rb, fieldtypeof(entry_t, key_member) *key)    \
    {                                                                        \
        rb_node_t *n = rb.root;                                              \
                                                                             \
        while (n) {                                                          \
            entry_t *e = rb_entry(n, entry_t, node_member);                  \
            int cmp = cmp_f(key, &e->key_member);                            \
                                                                             \
            if (cmp < 0) {                                                   \
                n = n->left;                                                 \
            } else                                                           \
            if (cmp > 0) {                                                   \
                n = n->right;                                                \
            } else {                                                         \
                return e;                                                    \
            }                                                                \
        }                                                                    \
        return NULL;                                                         \
    }

#define RB_INSERT_I(entry_t, pfx, node_member, key_member) \
    rb_node_t **pfx##_insert(rb_t *rb, entry_t *e)                           \
    {                                                                        \
        rb_node_t **slot = &rb->root;                                        \
        rb_node_t *parent = NULL;                                            \
                                                                             \
        while (*slot) {                                                      \
            entry_t *slot_e = rb_entry(*slot, entry_t, node_member);         \
                                                                             \
            parent = *slot;                                                  \
            if (e->key_member < slot_e->key_member) {                        \
                slot = &(*slot)->left;                                       \
            } else                                                           \
            if (e->key_member > slot_e->key_member) {                        \
                slot = &(*slot)->right;                                      \
            } else {                                                         \
                return slot;                                                 \
            }                                                                \
        }                                                                    \
        rb_add_node(rb, parent, *slot = &e->node_member);                    \
        return NULL;                                                         \
    }

#define RB_INSERT_P(entry_t, pfx, node_member, key_member, cmp_f) \
    rb_node_t **pfx##_insert(rb_t *rb, entry_t *e)                           \
    {                                                                        \
        rb_node_t **slot = &rb->root;                                        \
        rb_node_t *parent = NULL;                                            \
                                                                             \
        while (*slot) {                                                      \
            entry_t *slot_e = rb_entry(*slot, entry_t, node_member);         \
            int cmp = cmp_f(&e->key_member, &slot_e->key_member);            \
                                                                             \
            parent = *slot;                                                  \
            if (cmp < 0) {                                                   \
                slot = &(*slot)->left;                                       \
            } else                                                           \
            if (cmp > 0) {                                                   \
                slot = &(*slot)->right;                                      \
            } else {                                                         \
                return slot;                                                 \
            }                                                                \
        }                                                                    \
        rb_add_node(rb, parent, *slot = &e->node_member);                    \
        return NULL;                                                         \
    }

#endif
