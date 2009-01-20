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

static inline void
rb_add_node(rb_node_t *parent, rb_node_t **slot, rb_node_t *node)
{
    node->__parent = (uintptr_t)parent; /* insert it red */
    node->left = node->right = NULL;
}

void rb_fix_color(rb_t *, rb_node_t *);
void rb_remove(rb_t *, rb_node_t *);

#define rb_parent(n)                 ((rb_node_t *)((n)->__parent & ~1))
#define	rb_entry(ptr, type, member)  container_of(ptr, type, member)
#define	rb_entry_of(ptr, n, member)  container_of(ptr, typeof(*n), member)

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
