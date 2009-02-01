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

#include "container.h"

#define rb_color(n)      (n->__parent & 1);
#define rb_is_red(n)     ({ rb_node_t *__n = (n); __n && !rb_color(__n); })
#define rb_is_black(n)   ({ rb_node_t *__n = (n); !__n || rb_color(__n); })
#define rb_set_red(n)    ((n)->__parent &= ~1);
#define rb_set_black(n)  ((n)->__parent |= 1);
#define rb_set_black2(n) ({ rb_node_t *__n = (n); if (__n) rb_set_black(__n); })

static inline void rb_set_parent(rb_node_t *n, rb_node_t *p) {
    n->__parent = (n->__parent & 1) | (uintptr_t)p;
}
static inline void rb_copy_color(rb_node_t *n, rb_node_t *n2) {
    n->__parent = (n->__parent & ~1) | (n2->__parent & 1);
}


/*
 *
 *      p                                              p
 *      |                                              |
 *      x        ----[ rotate_left(rb, x) ]--->        y
 *     / \                                            / \
 *    a   y      <---[ rotate_right(rb, y) ]---      x   c
 *       / \                                        / \
 *      b   c                                      a   b
 *
 */

__attribute__((always_inline))
static void rb_reparent(rb_t *rb, rb_node_t *p,
                        rb_node_t *old, rb_node_t *new)
{
    if (p) {
        if (old == p->left) {
            p->left = new;
        } else {
            p->right = new;
        }
    } else {
        rb->root = new;
    }
}

static void rb_rotate_left(rb_t *rb, rb_node_t *x)
{
    rb_node_t *p = rb_parent(x);
    rb_node_t *y = x->right;

    if ((x->right = y->left))
        rb_set_parent(y->left, x);
    y->left = x;
    rb_set_parent(y, p);
    rb_reparent(rb, p, x, y);
    rb_set_parent(x, y);
}

static void rb_rotate_right(rb_t *rb, rb_node_t *y)
{
    rb_node_t *p = rb_parent(y);
    rb_node_t *x = y->left;

    if ((y->left = x->right))
        rb_set_parent(x->right, y);
    x->right = y;
    rb_set_parent(x, p);
    rb_reparent(rb, p, y, x);
    rb_set_parent(y, x);
}

__attribute__((always_inline))
static void rb_add_fix_color(rb_t *rb, rb_node_t *z)
{
    rb_node_t *p_z, *y;

    while (rb_is_red(p_z = rb_parent(z))) {
        rb_node_t *gp_z = rb_parent(p_z);

        if (p_z == gp_z->left) {
            if (rb_is_red(y = gp_z->right)) {
                rb_set_black(p_z);
                rb_set_black(y);
                rb_set_red(gp_z);
                z = gp_z;
                continue;
            }

            if (p_z->right == z) {
                rb_rotate_left(rb, p_z);
                SWAP(rb_node_t *, z, p_z);
            }

            rb_set_black(p_z);
            rb_set_red(gp_z);
            rb_rotate_right(rb, gp_z);
        } else {
            if (rb_is_red(y = gp_z->left)) {
                rb_set_black(y);
                rb_set_black(p_z);
                rb_set_red(gp_z);
                z = gp_z;
                continue;
            }

            if (p_z->left == z) {
                rb_rotate_right(rb, p_z);
                SWAP(rb_node_t *, z, p_z);
            }

            rb_set_black(p_z);
            rb_set_red(gp_z);
            rb_rotate_left(rb, gp_z);
        }
    }

    rb_set_black(rb->root);
}

void rb_add_node(rb_t *rb, rb_node_t **slot, rb_node_t *node)
{
    rb_set_parent(node, *slot); /* insert it red */
    node->left = node->right = NULL;
    *slot = node;
    rb_add_fix_color(rb, node);
}


__attribute__((always_inline))
static void rb_del_fix_color(rb_t *rb, rb_node_t *p, rb_node_t *z)
{
    rb_node_t *w;

    while (rb_is_black(z) && z != rb->root) {
        if (p->left == z) {
            if (rb_is_red(w = p->right)) {
                rb_set_black(w);
                rb_set_red(p);
                rb_rotate_left(rb, p);
                w = p->right;
            }
            if (rb_is_black(w->left) && rb_is_black(w->right)) {
                rb_set_red(w);
                z = p;
                p = rb_parent(z);
            } else {
                if (rb_is_black(w->right)) {
                    rb_set_black2(w->left);
                    rb_set_red(w);
                    rb_rotate_right(rb, w);
                    w = p->right;
                }
                rb_copy_color(w, p);
                rb_set_black(p);
                rb_set_black2(w->right);
                rb_rotate_left(rb, p);
                z = rb->root;
                break;
            }
        } else {
            if (rb_is_red(w = p->left)) {
                rb_set_black(w);
                rb_set_red(p);
                rb_rotate_right(rb, p);
                w = p->left;
            }
            if (rb_is_black(w->left) && rb_is_black(w->right)) {
                rb_set_red(w);
                z = p;
                p = rb_parent(z);
            } else {
                if (rb_is_black(w->left)) {
                    rb_set_black2(w->right);
                    rb_set_red(w);
                    rb_rotate_left(rb, w);
                    w = p->left;
                }
                rb_copy_color(w, p);
                rb_set_black(p);
                rb_set_black2(w->left);
                rb_rotate_right(rb, p);
                z = rb->root;
                break;
            }
        }
    }
    rb_set_black2(z);
}

void rb_del_node(rb_t *rb, rb_node_t *z)
{
    struct rb_node_t *child, *p;
    int color;

    if (z->left && z->right) {
        rb_node_t *old = z;

        z     = rb_next(z);
        child = z->right;
        p     = rb_parent(z);
        color = rb_color(z);

        if (child)
            rb_set_parent(child, p);
        if (p == old) {
            p->right = child;
            p = z;
        } else {
            p->left = child;
        }
        *z = *old;

        rb_reparent(rb, rb_parent(old), old, z);
        rb_set_parent(old->left, z);
        if (old->right)
            rb_set_parent(old->right, z);
    } else {
        child = z->right ?: z->left;
        p     = rb_parent(z);
        color = rb_color(z);
        if (child)
            rb_set_parent(child, p);
        rb_reparent(rb, p, z, child);
    }

    if (!color) /* it's black */
        rb_del_fix_color(rb, p, child);
}


rb_node_t *rb_next(rb_node_t *n)
{
    rb_node_t *p;

    if (n->right) {
        n = n->right;
        while (n->left)
            n = n->left;
        return n;
    }

    for (p = rb_parent(n); p && n == p->right; n = p);
    return p;
}
rb_node_t *rb_prev(rb_node_t *n)
{
    rb_node_t *p;

    if (n->left) {
        n = n->left;
        while (n->right)
            n = n->right;
        return n;
    }

    for (p = rb_parent(n); p && n == p->left; n = p);
    return p;
}
