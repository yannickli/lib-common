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
#define rb_is_red(n)     ({ rb_node_t *__n = (n); !rb_color(__n); })
#define rb_set_red(n)    ((n)->__parent &= ~1);
#define rb_set_black(n)  ((n)->__parent |= 1);

#define rb_set_parent(n, parent) \
    (n->__parent = (n->__parent & 1) | (uintptr_t)(parent))


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

void rb_fix_color(rb_t *rb, rb_node_t *z)
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


rb_node_t *rb_next(rb_node_t *n)
{
    rb_node_t *p;

    if (n->right) {
        n = n->right;
        while (n->left)
            n = n->left;
        return n;
    }

    for (p = rb_parent(n); n == p->right; n = p);
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

    for (p = rb_parent(n); n == p->left; n = p);
    return p;
}
