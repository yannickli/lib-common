/**************************************************************************/
/*                                                                        */
/*  Copyright (C) 2004-2007 INTERSEC SAS                                  */
/*                                                                        */
/*  Should you receive a copy of this source code, you must check you     */
/*  have a proper, written authorization of INTERSEC to hold it. If you   */
/*  don't have such an authorization, you must DELETE all source code     */
/*  files in your possession, and inform INTERSEC of the fact you obtain  */
/*  these files. Should you not comply to these terms, you can be         */
/*  prosecuted in the extent permitted by applicable law.                 */
/*                                                                        */
/**************************************************************************/

#include <errno.h>
#include <alloca.h>

#include "btree.h"

#define BT_ARITY          340  /**< L constant in the b-tree terminology */
#define BT_INIT_NBPAGES  1024  /**< initial number of pages in the btree */
static const union {
    char     s[4];
    uint32_t i;
} ISBT_MAGIC = { { 'I', 'S', 'B', 'T' } };

enum btpp_masks {
    BTPP_NODE_MASK = 0x80000000,
    BTPP_FWD_MASK  = 0x40000000,
    BTPP_OFFS_MASK = 0x00ffffff,
    BTPP_NIL       = BTPP_OFFS_MASK,
};

typedef struct bt_node_t {
    int32_t next;
    int32_t flags;

    int32_t nbkeys;
    int32_t ptrs[BT_ARITY + 1];
    byte    keys[BT_ARITY][8];
} bt_node_t;

typedef struct bt_leaf_t {
    int32_t next;
    int32_t used;
    byte data[4096 - 4 * 2];
} bt_leaf_t;

typedef union bt_page_t {
    bt_node_t node;
    bt_leaf_t leaf;
} bt_page_t;

struct btree_priv {
    /* first qword */
    uint32_t magic;     /**< magic of the paginated file: 'I' 'S' 'B' 'T'. */
    uint8_t  major;     /**< major version of the file format              */
    uint8_t  minor;     /**< minor version of the file format              */
    uint16_t reserved_1;

    /* second qword */
    int32_t  root;      /**< pointer to the root page                      */
    int32_t  nbpages;   /**< number of allocated pages in the file         */

    /* third qword */
    int32_t  freelist;  /**< freelist of blank pages                       */
    int16_t  depth;     /**< max depth of the nodes                        */
    uint16_t reserved_2;

    /* __future__: 512 - 3 qwords */
    uint64_t reserved_0[512 - 3]; /**< padding up to 4k                    */

    bt_page_t pages[];
};
MMFILE_FUNCTIONS(btree_t, bt_real);


/****************************************************************************/
/* btree generic pages functions                                            */
/****************************************************************************/

/* btree page pointers are organized this way (bit n stands for position 2^n)
 * 
 * bit  31   : points to a leaf (0) or a node (1) page.
 * bits 30-24: unused
 * bits  0-23: number of the page
 */

/* BTPP stands fro BTree Page Pointer */
#define BTPP_IS_NODE(x)   ((x) & BTPP_NODE_MASK)
#define BTPP_OFFS(x)      ((x) & BTPP_OFFS_MASK)

#define REPEAT4(x)        x, x, x, x
#define REPEAT8(x)        REPEAT4(x), REPEAT4(x)
#define REPEAT16(x)       REPEAT8(x), REPEAT8(x)
#define REPEAT32(x)       REPEAT16(x), REPEAT16(x)

static bt_node_t *bt_node_init(bt_node_t *node)
{
    node->nbkeys = 0;
    node->next   = BTPP_NIL;
    return node;
}

static int32_t bt_page_new(btree_t *bt)
{
#define NB_PAGES_GROW  1024

    int32_t i, res;

    if (bt->area->freelist) {
        res = bt->area->freelist;
        bt->area->freelist = bt->area->pages[bt->area->freelist].leaf.next;
        bt->area->pages[res].leaf.next = 0;
        return res;
    }

    res = bt_real_truncate(bt, bt->size + NB_PAGES_GROW * 4096);
    if (res < 0) {
        if (!bt->area) {
            e_panic("Not enough memory !");
        }
        return res;
    }

    /* res == oldnbpages is the new free page, link the NB_PAGES_GROW - 1
       remaining pages like that:
       res + 1 -> ... -> res + NB_PAGES_GROW - 1 -> NIL
     */
    res = bt->area->nbpages;
    for (i = res + NB_PAGES_GROW - 1; i > res + 1; i--) {
        bt->area->pages[i - 1].leaf.next = i;
    }
    bt->area->freelist = res + 1;
    bt->area->nbpages += NB_PAGES_GROW;

    return res;
}

static inline bt_page_t *vbt_deref(struct btree_priv *bt, int32_t ptr)
{
    int page = BTPP_OFFS(ptr);
    if (page == BTPP_NIL || page > bt->nbpages)
        return NULL;
    return &bt->pages[BTPP_OFFS(ptr)];
}

static inline bt_page_t *bt_deref(const struct btree_priv *bt, int32_t ptr)
{
    return vbt_deref((struct btree_priv *)bt, ptr);
}

static inline void bt_page_release(btree_t *bt, int32_t page)
{
    bt_leaf_t *leaf = &vbt_deref(bt->area, page)->leaf;

    if (leaf) {
        leaf->next = bt->area->freelist;
        bt->area->freelist = BTPP_OFFS(page);
    }
}

static void btn_shift(bt_node_t *node, int dst, int src, int width)
{
    if (width >= 0) {
        memmove(node->ptrs + dst, node->ptrs + src,
                sizeof(node->ptrs[0]) * (width + 1));
    }
    if (width > 0) {
        memmove(node->keys + dst, node->keys + src,
                sizeof(node->keys[0]) * width);
    }
}

static void btn_set(bt_node_t *node, int pos, int32_t ptr,
                    const byte *key, int n)
{
    node->ptrs[pos] = ptr;
    if (pos < node->nbkeys) {
        memcpy(node->keys[pos], key, n);
        p_clear(node->keys[pos] + n, 8 - n);
    }
}

static void
btn_update_aux(btree_t *bt, int32_t nodes[], int pos,
               int32_t page1, int32_t page2, const byte *key, int n)
{
    while (pos >= 0) {
        int i = 0;
        bt_node_t *node = &vbt_deref(bt->area, nodes[pos])->node;

        while (i < node->nbkeys && BTPP_OFFS(node->ptrs[i]) != BTPP_OFFS(page1))
            i++;

        btn_set(node, i, page2, key, n);
        if (i != node->nbkeys - 1)
            return;

        page1 = page2 = nodes[pos--];
    }
}

static void btn_update(btree_t *bt, int32_t nodes[],
                       int32_t page1, int32_t page2, const byte *key, int n)
{
    int pos = -1;

    while (BTPP_IS_NODE(nodes[pos + 1]))
        pos++;

    btn_update_aux(bt, nodes, pos, page1, page2, key, n);
}

static void btn_insert_aux(btree_t *bt, int32_t nodes[], int pos,
                           int32_t next, int32_t page, const byte *key, int n)
{
    int i = 0;
    bt_node_t *node;
    bt_node_t *sibling;

    if (pos < 0) {
        bt->area->depth++;
        // XXX: check if we got a new page !
        bt->area->root = bt_page_new(bt) | BTPP_NODE_MASK;
        node = &vbt_deref(bt->area, bt->area->root)->node;
        node->nbkeys = 1;
        btn_set(node, 0, page, key, n);
        btn_set(node, 1, next, NULL, 0);
        return;
    }

    node = &vbt_deref(bt->area, nodes[pos])->node;
    while (i < node->nbkeys && BTPP_OFFS(node->ptrs[i]) != BTPP_OFFS(next))
        i++;

    sibling = &vbt_deref(bt->area, node->next)->node;
    if (node->nbkeys == BT_ARITY && sibling && sibling->nbkeys < BT_ARITY) {
        btn_shift(sibling, 1, 0, sibling->nbkeys);
        sibling->nbkeys++;
        btn_set(sibling, 0, node->ptrs[node->nbkeys - 1],
                node->keys[node->nbkeys - 1], 8);

        btn_shift(node, node->nbkeys - 1, node->nbkeys, 0);
        node->nbkeys--;
    }

    if (node->nbkeys < BT_ARITY) {
        btn_shift(node, i + 1, i, node->nbkeys - i - 1);
        node->nbkeys++;
        node->ptrs[i] = page;
        if (i < node->nbkeys) {
            btn_update_aux(bt, nodes, pos, page, page, key, n);
        }
        return;
    }

    {
        int32_t npage;

        // XXX: check if we got a new page !
        npage   = bt_page_new(bt);
        node    = &vbt_deref(bt->area, nodes[pos])->node;
        sibling = &vbt_deref(bt->area, npage)->node;
        *sibling = *node;

        node->next   = BTPP_NODE_MASK | npage;
        node->nbkeys = BT_ARITY / 2;

        btn_shift(sibling, 0, BT_ARITY / 2, (BT_ARITY + 1) / 2);
        sibling->nbkeys = (BT_ARITY + 1) / 2;

        if (i < node->nbkeys) {
            btn_shift(node, i + 1, i, node->nbkeys - i - 1);
            node->nbkeys++;
            btn_set(node, i, page, key, n);
        } else {
            i -= node->nbkeys;
            btn_shift(sibling, i + 1, i, sibling->nbkeys - i - 1);
            sibling->nbkeys++;
            btn_set(sibling, i, page, key, n);
        }
        btn_update_aux(bt, nodes, pos - 1, nodes[pos], npage,
                       sibling->keys[sibling->nbkeys - 1], 8);
        btn_insert_aux(bt, nodes, pos - 1, npage,
                       page, node->keys[node->nbkeys - 1], 8);
    }
}

static void btn_insert(btree_t *bt, int32_t nodes[], int32_t next,
                       int32_t page, const byte *key, int n)
{
    int pos = -1;

    while (BTPP_IS_NODE(nodes[pos + 1]))
        pos++;

    btn_insert_aux(bt, nodes, pos, next, page, key, n);
}

#if 0 /* not used yet, and unfinished */
static void
btn_delete(btree_t *bt, int32_t nodes[], int32_t page)
{
    int pos = -1;

    while (BTPP_IS_NODE(nodes[pos + 1]))
        pos++;

    while (pos >= 0) {
        int i = 0;
        bt_node_t *node = &vbt_deref(bt->area, nodes[pos])->node;

        while (i < node->nbkeys && BTPP_OFFS(node->ptrs[i]) != BTPP_OFFS(page))
            i++;

        btn_shift(node, i, i + 1, node->nbkeys - i - 1);
        node->nbkeys--;
        if (i == node->nbkeys) {
            /* ex nbkeys - 1 */
            btn_update_aux(bt, nodes, pos - 1, page, node->ptrs[i - 1],
                                  node->keys[i - 1], 8);
        }

        if (pos == 0 /* aka we are at the root */) {
            if (node->nbkeys == 0) {
                bt->area->root = node->ptrs[0];
                bt_page_release(bt, node->ptrs[0]);
            }
            return;
        } else {
            /* TODO: try to merge us with the previous or the next sibling */
        }
    }
}
#endif

/****************************************************************************/
/* whole file related functions                                             */
/****************************************************************************/

int btree_fsck(btree_t *bt, int dofix)
{
    bool did_a_fix = false;

    if (bt->size & 4095 || !(bt->size / 4096))
        return -1;

    if (bt->area->magic != ISBT_MAGIC.i)
        return -1;

    /* hook conversions here ! */
    if (bt->area->major != 1 || bt->area->minor != 0)
        return -1;

    if (bt->area->nbpages > bt->size / 4096 - 1) {
        if (!dofix)
            return -1;

        did_a_fix = true;
        e_error("`%s': corrupted header (nbpages was %d, fixed into %d)",
                bt->path, bt->area->nbpages, (int)(bt->size / 4096 - 1));
        bt->area->nbpages = bt->size / 4096 - 1;
    }

    if (bt->area->freelist < 0 || bt->area->freelist >= bt->area->nbpages) {
        if (!dofix)
            return -1;

        e_error("`%s': corrupted header (freelist out of bounds). "
                "force fsck.", bt->path);
        bt->area->freelist = 0;
    }

    /* TODO: FSCK here ! */

    return did_a_fix;
}

btree_t *btree_open(const char *path, int flags)
{
    btree_t *bt;
    int res;

    if ((flags & O_CREAT) && access(path, R_OK))
        return btree_creat(path);

    bt  = bt_real_open(path, flags);
    res = btree_fsck(bt, !!(flags & O_WRONLY));
    if (res < 0) {
        btree_close(&bt);
        errno = EINVAL;
    }

    return bt;
}

btree_t *btree_creat(const char *path)
{
    btree_t *bt;
    int i;

    bt = bt_real_creat(path, sizeof(bt_page_t) * (BT_INIT_NBPAGES + 1));
    if (!bt)
        return NULL;

    bt->area->magic   = ISBT_MAGIC.i;
    bt->area->major   = 1;
    bt->area->minor   = 0;
    bt->area->nbpages = BT_INIT_NBPAGES;
    bt->area->root    = 0;

    /* this creates the links: 1 -> 2 -> ... -> nbpages - 1 -> NIL */
    bt->area->freelist = 1;
    for (i = 1; i < BT_INIT_NBPAGES - 1; i++) {
        bt->area->pages[i].leaf.next = i + 1;
    }

    bt_node_init(&bt->area->pages[0].node);
    return bt;
}

void btree_close(btree_t **bt)
{
    bt_real_close(bt);
}


/****************************************************************************/
/* code specific to the nodes                                               */
/****************************************************************************/

static inline int
bt_keycmp(const byte *k1, int n1, const byte *k2, int n2)
{
    int res = memcmp(k1, k2, MIN(n1, n2));
    return res ? res : n1 - n2;
}

/* searches the _last_ pos in the node keys array where key <= keys[pos] */
static int btn_bsearch(const bt_node_t *node, const byte *key, int n)
{
    int l = 0, r = node->nbkeys;

    while (r > l) {
        int i = (l + r) / 2;

        switch (SIGN(bt_keycmp(key, n, node->keys[i], 8))) {
          case NEGATIVE: /* key < node->keys[i] */
            r = i;
            break;

          case ZERO:     /* XXX asumes unicity of the key in keys[] */
            return i;

          case POSITIVE: /* key > node->keys[i] */
            l = i + 1;
            break;
        }
    }

    return l;
}

static int32_t btn_find_leaf(const struct btree_priv *bt,
                             const byte *key, int n, int32_t nodes[])
{
    int32_t page = bt->root;

    /* TODO: deal with 'continuation' bit to follow up into a new b-tree for
             keys > 1 segment */
    while (BTPP_IS_NODE(page)) {
        const bt_node_t *node = &bt_deref(bt, page)->node;

        if (nodes)
            *nodes++ = node ? page : -1;
        if (!node)
            return -1;

        page = node->ptrs[btn_bsearch(node, key, MIN(8, n))];
    }

    if (nodes)
        *nodes++ = page;
    return page;
}

/****************************************************************************/
/* code specific to the leaves                                              */
/****************************************************************************/

static inline int
btl_keycmp(const bt_leaf_t *leaf, const byte *key, int n, int pos)
{
    return bt_keycmp(key, n, leaf->data + pos + 1, leaf->data[pos]);
}

static int
btl_findslot(const bt_leaf_t *leaf, const byte *key, int n, int32_t *slot)
{
    int *pos = slot ? slot : p_alloca(int, 1);
    *pos = 0;

    while (*pos < leaf->used && !leaf->data[*pos]) {
        *pos += leaf->data[*pos] + 1; /* skip key  */
        *pos += leaf->data[*pos] + 1; /* skip data */
    }

    while (*pos < leaf->used) {
        switch (SIGN(btl_keycmp(leaf, key, n, *pos))) {
          case NEGATIVE:
            break;

          case ZERO:
            return *pos;

          case POSITIVE:
            return -1;
        }

        do {
            *pos += leaf->data[*pos] + 1; /* skip key  */
            *pos += leaf->data[*pos] + 1; /* skip data */
        } while (*pos < leaf->used && !leaf->data[*pos]);
    }

    return -1;
}

int btree_fetch(const btree_t *bt, const byte *key, int n, blob_t *out)
{
    int page, pos, len = 0;
    const bt_leaf_t *leaf;

    page = btn_find_leaf(bt->area, key, n, NULL);
    leaf = &bt_deref(bt->area, page)->leaf;
    if (!leaf)
        return -1;

    pos  = btl_findslot(leaf, key, n, NULL);
    if (pos < 0)
        return -1;

    do {
        int slot;

        pos = leaf->data[pos] + 1;
        if (EXPECT_FALSE(pos + 1 >= leaf->used))
            break;

        slot = leaf->data[pos++];
        if (EXPECT_FALSE(pos + slot > leaf->used))
            break;

        blob_append_data(out, leaf->data + pos, slot);
        len += slot;
        pos += slot;

        while (pos >= leaf->used) {
            pos  = 0;
            leaf = &bt_deref(bt->area, leaf->next)->leaf;
            if (!leaf)
                break;
        }
    } while (!leaf->data[pos] || !btl_keycmp(leaf, key, n, pos));

    return len;
}

/* -1 means not indexable, 0 means could not extend file */
static bt_leaf_t *btl_new(btree_t *bt, int32_t prev)
{
    int32_t page = bt_page_new(bt);
    bt_leaf_t *l, *r;

    if (page < 0)
        return NULL;

    l = &vbt_deref(bt->area, prev)->leaf;
    r = &vbt_deref(bt->area, page)->leaf;
    r->next = l->next;
    l->next = (~BTPP_NODE_MASK) & page;
    return r;
}

static void btl_maxkey(const bt_leaf_t *leaf, const byte **key, int *n)
{
    int pos = 0;

    for (;;) {
        while (pos < leaf->used && !leaf->data[pos]) {
            pos += 1 + leaf->data[pos];
            pos += 1 + leaf->data[pos];
        }

        if (pos >= leaf->used)
            break;

        if (btl_keycmp(leaf, *key, *n, pos) < 0) {
            *n   = leaf->data[pos];
            *key = leaf->data + pos + 1;
        }
        pos += 1 + leaf->data[pos];
        pos += 1 + leaf->data[pos];
    }
}

int btree_push(btree_t *bt, const byte *key, int n,
               const byte *data, int dlen)
{
    bool putkey = true, compress = false, reuse = false;
    int32_t page, slot, need, *nodes;
    bt_leaf_t *lleaf;

    nodes = p_alloca(int32_t, bt->area->depth + 1);
    page  = btn_find_leaf(bt->area, key, n, nodes);
    lleaf = &vbt_deref(bt->area, page)->leaf;

    if (lleaf) {
        int pos = btl_findslot(lleaf, key, n, &slot);

        need  = 1 + n + 1 + dlen;
        if (pos >= 0) {
            putkey = false;
            if (lleaf->data[pos + 1 + n] + dlen < 256) {
                reuse = true;
                need = dlen;
            } else {
                compress = true;
                need -= n;
            }
        }
    } else {
        return -1;
    }

    if (need + lleaf->used <= ssizeof(lleaf->data))
        goto easy;

    if (compress) {
        byte *p = lleaf->data + slot;

        p[0] = 0;
        memmove(p + 1, p + 1 + n, lleaf->used - (slot + 1 + n));
        lleaf->used -= n;
        need--;
        compress = false;
    }

    for (;;) {
        int next, oldpos, shift;
        bt_leaf_t *rleaf;

        rleaf = &vbt_deref(bt->area, lleaf->next)->leaf;
        if (!rleaf) {
            rleaf = btl_new(bt, page);
            lleaf = &vbt_deref(bt->area, page)->leaf;
            if (!rleaf)
                return -1;
        }

        oldpos = slot;
        next   = slot + reuse ? 1 + n + 1 + lleaf->data[slot + 1 + n] : 0;

        while (next <= lleaf->used && next + need <= ssizeof(lleaf->data)) {
            oldpos = next;
            next += 1 + lleaf->data[next];
            next += 1 + lleaf->data[next];
        }

        if (oldpos == slot) {
            shift = lleaf->used + need - oldpos;
        } else {
            shift = lleaf->used - oldpos;
        }

        if (shift <= ssizeof(rleaf->data)) {
            const byte *lk, *rk;
            int ln, rn;

            memmove(rleaf->data + shift, rleaf->data, rleaf->used);
            memcpy(rleaf->data, lleaf->data + oldpos, shift);
            lleaf->used -= shift;
            rleaf->used += shift;

            if (oldpos == slot) {
                lk = NULL, ln = 0;
                rk = key,  rn = n;
            } else {
                lk = key,  ln = n;
                rk = NULL, rn = 0;
            }
            btl_maxkey(lleaf, &lk, &ln);
            btl_maxkey(rleaf, &rk, &rn);

            if (rk) {
                btn_update(bt, nodes, page, lleaf->next, rk, rn);
            }
            if (lk) {
                if (rk) {
                    btn_insert(bt, nodes, lleaf->next, page, lk, ln);
                } else {
                    btn_update(bt, nodes, page, page, lk, ln);
                }
            }

            if (oldpos == slot) {
                slot  = 0;
                page  = lleaf->next;
                lleaf = rleaf;
            }

            break;
        }

        rleaf = btl_new(bt, page);
        if (!rleaf)
            return -1;
    }

  easy:
    {
        byte *p = lleaf->data + slot;

        if (reuse || compress) {
            p += 1 + n;
        }
        memmove(p + need, p, lleaf->used - (p - lleaf->data));
        if (putkey) {
            *p++ = n;
            memcpy(p, key, n);
            p += n;
        }
        *p = (reuse ? *p : 0) + dlen;
        memcpy(p + 1, data, dlen);
        p += 1 + dlen;
        if (compress) {
            *p = 0;
        }

        lleaf->used += need;
    }
    return 0;
}

void btree_dump(FILE *out, const btree_t *bt_pub,
                void (*k_fmt)(FILE *, const byte *, int),
                void (*d_fmt)(FILE *, const byte *, int))
{
    struct btree_priv *bt = bt_pub->area;
    int32_t lmost, depth = 0;

    lmost = bt->root;

    while (BTPP_IS_NODE(lmost)) {
        int32_t page = lmost;
        int i = 0;

        fprintf(out, "====== DEPTH %d =====\n", depth++);
        while (BTPP_OFFS(page) != BTPP_NIL) {
            int j;
            bt_node_t *node = &bt_deref(bt, page)->node;

            fprintf(out, "------ node %d: %d / %d\n", i++, node->nbkeys, BT_ARITY);
            for (j = 0; j < node->nbkeys; j++) {
                fputs("> ", out);
                (*k_fmt)(out, node->keys[i], 8);
                fputc('\n', out);
            }

            page = node->next;
        }
        fprintf(out, "------------------\n");

        lmost = bt_deref(bt, lmost)->node.ptrs[0];
    }

    fprintf(out, "====== DEPTH %d =====\n", depth);
    {
        int32_t page = lmost;
        int i = 0;

        while (BTPP_OFFS(page) != BTPP_NIL) {
            bt_leaf_t *leaf = &bt_deref(bt, page)->leaf;
            int pos;

            fprintf(out, "------ leaf %d: %d\n", i++, leaf->used);
            for (pos = 0; pos < leaf->used; ) {
                fputs("| ", out);
                (*k_fmt)(out, leaf->data + 1 + pos, leaf->data[pos]);
                fputs(": ", out);
                pos += 1 + leaf->data[pos];
                (*d_fmt)(out, leaf->data + 1 + pos, leaf->data[pos]);
                fputc('\n', out);
                pos += 1 + leaf->data[pos];
            }

            page = leaf->next;
        }

        fprintf(out, "------------------\n");
    }
}
