/**************************************************************************/
/*                                                                        */
/*  Copyright (C) 2004-2008 INTERSEC SAS                                  */
/*                                                                        */
/*  Should you receive a copy of this source code, you must check you     */
/*  have a proper, written authorization of INTERSEC to hold it. If you   */
/*  don't have such an authorization, you must DELETE all source code     */
/*  files in your possession, and inform INTERSEC of the fact you obtain  */
/*  these files. Should you not comply to these terms, you can be         */
/*  prosecuted in the extent permitted by applicable law.                 */
/*                                                                        */
/**************************************************************************/

#if !defined(IS_LIB_COMMON_CONTAINER_H) || defined(IS_LIB_COMMON_CONTAINER_RBTREE_H)
#  error "you must include <lib-common/container.h> instead"
#endif
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
 *   entry_t *rb_entry_search(rb_root_t *rb, key_t *key)
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
 *   void rb_entry_insert(rb_root_t *rb, entry_t *e)
 *   {
 *        rb_node_t **slot = &rb->root;
 *
 *        while (*slot) {
 *            entry_t *slot_e;
 *
 *            slot_e = rb_entry(*slot, entry_t, some_member);
 *
 *            if (e->key < slot_e->key) {
 *                slot = &(*slot)->left;
 *            } else
 *            if (e->ken > slot_e->key) {
 *                slot = &(*slot)->right;
 *            } else {
 *                // treat key duplicates here
 *            }
 *        }
 *        rb_add_node(rb, slot, &e->some_member);
 *    }
 *
 */

#define rb_parent(n)                 ((rb_node_t *)((n)->__parent & ~1))
#define	rb_entry(ptr, type, member)  container_of(ptr, type, member)
#define	rb_entry_of(ptr, n, member)  container_of(ptr, typeof(*n), member)

void rb_add_node(rb_t *, rb_node_t **slot, rb_node_t *node);
void rb_del_node(rb_t *, rb_node_t *);


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

rb_node_t *rb_next(rb_node_t *);
rb_node_t *rb_prev(rb_node_t *);
