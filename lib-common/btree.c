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

#ifndef MINGCC

#include <errno.h>
#include <alloca.h>
#include <sys/mman.h>

#include "btree.h"

#define BT_ARITY          340  /**< L constant in the b-tree terminology */
#define BT_INIT_NBPAGES  1024  /**< initial number of pages in the btree */

#define O_ISWRITE(m)    (((m) & (O_RDONLY|O_WRONLY|O_RDWR)) != O_RDONLY)

static const union {
    char     s[4];
    uint32_t i;
} ISBT_MAGIC = { { 'I', 'S', 'B', 'T' } };

enum btpp_masks {
    BTPP_NODE_MASK = 0x80000000,
    BTPP_OFFS_MASK = 0x00ffffff,
    BTPP_NIL       = BTPP_OFFS_MASK,
};

typedef struct bt_node_t {
    int32_t next;
    int32_t flags;

    int32_t nbkeys;
    int32_t ptrs[BT_ARITY + 1];
    uint64_t keys[BT_ARITY];
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
    int16_t  dirty;     /**< are we dirty ?                                */

    /* __future__: 512 - 3 qwords */
    uint64_t reserved_0[512 - 3]; /**< padding up to 4k                    */

    bt_page_t pages[];
};
MMFILE_FUNCTIONS(btree_t, bt_real);

typedef struct intpair {
    int32_t page;
    int32_t i;
} intpair;


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
    return bt->pages + BTPP_OFFS(ptr);
}

static inline bt_page_t *bt_deref(const struct btree_priv *bt, int32_t ptr) {
    return vbt_deref((struct btree_priv *)bt, ptr);
}

static void btn_shift(bt_node_t *node, int dst, int src, int width)
{
    assert (dst >= 0 && src >= 0);

    if (width >= 0) {
        assert (dst + (width + 1) <= countof(node->ptrs));
        assert (src + (width + 1) <= countof(node->ptrs));
        assert (dst + (width + 1) >= 0);
        assert (src + (width + 1) >= 0);

        memmove(node->ptrs + dst, node->ptrs + src,
                sizeof(node->ptrs[0]) * (width + 1));
    }
    if (width > 0) {
        assert (dst + width <= countof(node->keys));
        assert (src + width <= countof(node->keys));
        assert (dst + width >= 0);
        assert (src + width >= 0);

        memmove(node->keys + dst, node->keys + src,
                sizeof(node->keys[0]) * width);
    }
}

static void btn_setk(bt_node_t *node, int pos, uint64_t key)
{
    assert (pos >= 0);
    if (pos < node->nbkeys) {
        node->keys[pos] = key;
    }
}

static void
btn_update(btree_t *bt, const intpair nodes[], int depth,
           int32_t page2, uint64_t key)
{
    int32_t page1 = nodes[depth].page;

    while (--depth >= 0) {
        int i = 0;
        bt_node_t *node = &vbt_deref(bt->area, nodes[depth].page)->node;

        node->ptrs[nodes[depth].i] = page2;
        btn_setk(node, nodes[depth].i, key);
        if (i != node->nbkeys - 1)
            return;

        page1 = page2 = nodes[depth].page;
    }
}


static void btl_maxkey(const bt_leaf_t *leaf, uint64_t *key);

static void btn_insert_aux(btree_t *bt, bt_node_t *node, int i, int32_t lpage, int32_t rpage)
{
    btn_shift(node, i + 1, i, node->nbkeys - i);
    node->nbkeys++;

    node->ptrs[i]     = lpage;
    node->ptrs[i + 1] = rpage;
    if (BTPP_IS_NODE(lpage)) {
        bt_node_t *lnode = &bt_deref(bt->area, lpage)->node;
        btn_setk(node, i, lnode->keys[lnode->nbkeys - 1]);
    } else {
        uint64_t key = 0;
        btl_maxkey(&bt_deref(bt->area, lpage)->leaf, &key);
        btn_setk(node, i, key);
    }
}

static void btn_insert(btree_t *bt, intpair nodes[], int depth, int32_t rpage)
{
    int32_t lpage = nodes[depth--].page;
    bt_node_t *node;
    bt_node_t *sibling;

    if (depth < 0) {        // XXX: check if we got a new page !
        bt->area->depth++;
        bt->area->root = bt_page_new(bt) | BTPP_NODE_MASK;
        node = &vbt_deref(bt->area, bt->area->root)->node;
        node->next    = BTPP_NIL;
        node->nbkeys  = 0;

        btn_insert_aux(bt, node, 0, lpage, rpage);
        return;
    }

    node = &vbt_deref(bt->area, nodes[depth].page)->node;

    sibling = &vbt_deref(bt->area, node->next)->node;
    if (node->nbkeys == BT_ARITY && sibling && sibling->nbkeys < BT_ARITY - 1) {
        btn_shift(sibling, 1, 0, sibling->nbkeys);
        sibling->nbkeys++;
        sibling->ptrs[0] = node->ptrs[node->nbkeys - 1];
        btn_setk(sibling, 0, node->keys[node->nbkeys - 1]);
        btn_shift(node, node->nbkeys - 1, node->nbkeys, 0);
        node->nbkeys--;

        if (nodes[depth].i >= node->nbkeys) {
            btn_insert_aux(bt, sibling, nodes[depth].i - node->nbkeys, lpage, rpage);
            return;
        }
    }

    if (node->nbkeys < BT_ARITY) {
        btn_insert_aux(bt, node, nodes[depth].i, lpage, rpage);
        return;
    }

    {
        int32_t npage;

        // XXX: check if we got a new page !
        npage   = bt_page_new(bt) | BTPP_NODE_MASK;
        node    = &vbt_deref(bt->area, nodes[depth].page)->node;
        sibling = &vbt_deref(bt->area, npage)->node;
        *sibling = *node;

        node->next   = npage;
        node->nbkeys = BT_ARITY / 2;

        btn_shift(sibling, 0, BT_ARITY / 2, (BT_ARITY + 1) / 2);
        sibling->nbkeys = (BT_ARITY + 1) / 2;

        if (nodes[depth].i < node->nbkeys) {
            btn_insert_aux(bt, node, nodes[depth].i, lpage, rpage);
        } else {
            btn_insert_aux(bt, sibling, nodes[depth].i - node->nbkeys,
                           lpage, rpage);
        }
        btn_insert(bt, nodes, depth, npage);
    }
}

/****************************************************************************/
/* whole file related functions                                             */
/****************************************************************************/

int btree_fsck(btree_t *bt, int dofix)
{
    bool did_a_fix = false;

    if (bt->size & 4095 || !(bt->size / 4096))
        return -1;

#if 0
    if (bt->area->dirty)
        return -1;
#endif

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

    /* OG: creating and opening should be totally separate */
    /* MC: I disagree, if you don't have any creation parameters, which you
           don't need for the current implementation, needing create to create
           a file is just painful. creat() is just a shorthand for
           open(..., flags | O_EXCL | O_WRONLY | O_TRUNC) and needing to do
           the following sucks:
           if (access(path, F_OK)) {
               foo_open(path);
           } else {
               foo_creat(path);
           }

           You want a way to say "Open that btree, and if it does not exists,
           please create a new empty one. This is perfectly sensible.

           Then having a more clever _create() function with some parameters
           for tweaked creations is fine too. But supporting a creation with
           sane defaults _IS_ a good thing.
     */
     /* OG: Furthermore, opening the file for update should require exclusive
      *     access unless the code can handle concurrent access.
      */

    /* Create or truncate the index if opening for write and:
     * - it does not exist and flag O_CREAT is given
     * - or it does exist and flag O_TRUNC is given
     * bug: O_EXCL is not supported as it is not passed to btree_creat.
     */
    res = access(path, F_OK);
    if (O_ISWRITE(flags)) {
        if ((res && (flags & O_CREAT)) || (!res && (flags & O_TRUNC)))
            return btree_creat(path);
    }

    if (res) {
        errno = ENOENT;
        return NULL;
    }

    bt = bt_real_open(path, flags);
    if (!bt) {
        e_trace(2, "Could not open bt on %s: %m", path);
        return NULL;
    }
    res = btree_fsck(bt, O_ISWRITE(flags));
    if (res < 0) {
        btree_close(&bt);
        errno = EINVAL;
    }
#if 0
    bt->area->dirty = O_ISWRITE(flags);
    msync(bt->area, bt->size, MS_SYNC);
#endif

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

    bt->area->pages[0].node.next = BTPP_NIL;
#if 0
    bt->area->dirty = true;
    msync(bt->area, bt->size, MS_SYNC);
#endif
    return bt;
}

void btree_close(btree_t **bt)
{
    if (*bt) {
#if 0
        if ((*bt)->area->dirty) {
            msync((*bt)->area, (*bt)->size, MS_SYNC);
            (*bt)->area->dirty = false;
            msync((*bt)->area, (*bt)->size, MS_SYNC);
        }
#endif
        bt_real_close(bt);
    }
}


/****************************************************************************/
/* code specific to the nodes                                               */
/****************************************************************************/

/* searches the _last_ pos in the node keys array where key <= keys[pos] */
static int btn_bsearch(const bt_node_t *node, uint64_t key)
{
    int l = 0, r = node->nbkeys;

    while (r > l) {
        int i = (l + r) / 2;

        switch (CMP(key, node->keys[i])) {
          case CMP_LESS: /* key < node->keys[i] */
            r = i;
            break;

          case CMP_EQUAL:
            while (i > 0 && !CMP(key, node->keys[i - 1]))
                i--;
            return i;

          case CMP_GREATER: /* key > node->keys[i] */
            l = i + 1;
            break;
        }
    }

    return l;
}

static int32_t btn_find_leaf(const struct btree_priv *bt, uint64_t key,
                             intpair nodes[], int *depth)
{
    int32_t page = bt->root;
    if (depth)
        *depth = -1;

    /* TODO: deal with 'continuation' bit to follow up into a new b-tree for
             keys > 1 segment */
    while (BTPP_IS_NODE(page)) {
        const bt_node_t *node = &bt_deref(bt, page)->node;
        int i;

        if (!node)
            return -1;

        i = btn_bsearch(node, key);
        if (nodes) {
            (*depth)++;
            nodes[*depth].page = page;
            nodes[*depth].i    = i;
        }
        page = node->ptrs[i];
    }

    if (nodes)
        nodes[++(*depth)].page = page;
    return page;
}

/****************************************************************************/
/* code specific to the leaves                                              */
/****************************************************************************/

static inline enum sign
btl_keycmp(uint64_t key, const bt_leaf_t *leaf, int pos)
{
    union {
        uint64_t u;
        byte c[8];
    } u;
    assert (0 <= pos && pos + 1 + 8 <= leaf->used);
    memcpy(&u, leaf->data + pos + 1, 8);
    return CMP(key, u.u);
}

static int
btl_findslot(const bt_leaf_t *leaf, uint64_t key, int32_t *slot)
{
    int *pos = slot ? slot : p_alloca(int, 1);
    *pos = 0;

    while (*pos < leaf->used && !leaf->data[*pos]) {
        *pos += leaf->data[*pos] + 1; /* skip key  */
        assert (*pos < leaf->used);
        *pos += leaf->data[*pos] + 1; /* skip data */
        assert (*pos <= leaf->used);
    }

    while (*pos < leaf->used) {
        switch (btl_keycmp(key, leaf, *pos)) {
          case CMP_GREATER:
            break;

          case CMP_EQUAL:
            return *pos;

          case CMP_LESS:
            return -1;
        }

        do {
            *pos += leaf->data[*pos] + 1; /* skip key  */
            assert (*pos < leaf->used);
            *pos += leaf->data[*pos] + 1; /* skip data */
            assert (*pos <= leaf->used);
        } while (*pos < leaf->used && !leaf->data[*pos]);
    }

    return -1;
}

int btree_fetch(const btree_t *bt, uint64_t key, blob_t *out)
{
    int page, pos, len = 0;
    const bt_leaf_t *leaf;

    page = btn_find_leaf(bt->area, key, NULL, NULL);
    leaf = &bt_deref(bt->area, page)->leaf;
    if (!leaf)
        return -1;

    pos  = btl_findslot(leaf, key, NULL);
    if (pos < 0)
        return -1;

    do {
        int slot;

        pos += 1 + leaf->data[pos];
        if (unlikely(pos + 1 >= leaf->used))
            break;

        slot = leaf->data[pos++];
        if (unlikely(pos + slot > leaf->used))
            break;

        blob_append_data(out, leaf->data + pos, slot);
        len += slot;
        pos += slot;

        while (pos >= leaf->used) {
            pos  = 0;
            leaf = &bt_deref(bt->area, leaf->next)->leaf;
            if (!leaf)
                return len;
        }
    } while (!leaf->data[pos] || !btl_keycmp(key, leaf, pos));

    return len;
}

static void btl_maxkey(const bt_leaf_t *leaf, uint64_t *key)
{
    int pos = 0;

    for (;;) {
        while (pos < leaf->used && !leaf->data[pos]) {
            pos += 1 + leaf->data[pos];
            assert (pos < leaf->used);
            pos += 1 + leaf->data[pos];
            assert (pos <= leaf->used);
        }

        if (pos >= leaf->used)
            break;

        if (btl_keycmp(*key, leaf, pos) < 0) {
            memcpy(key, leaf->data + pos + 1, 8);
        }
        pos += 1 + leaf->data[pos];
        assert (pos < leaf->used);
        pos += 1 + leaf->data[pos];
        assert (pos <= leaf->used);
    }
}

static bt_leaf_t *btl_new(btree_t *bt, intpair nodes[], int32_t depth)
{
    int32_t page = bt_page_new(bt);
    int32_t prev = nodes[depth].page;
    bt_leaf_t *l, *r;

    if (page < 0)
        return NULL;
    l = &vbt_deref(bt->area, prev)->leaf;
    r = &vbt_deref(bt->area, page)->leaf;
    r->next = l->next;
    r->used = 0;
    l->next = (~BTPP_NODE_MASK) & page;

    btn_insert(bt, nodes, depth, page);
    return r;
}

int btree_push(btree_t *bt, uint64_t key, const byte *data, int dlen)
{
    bool reuse = false;
    int32_t page, slot, need, depth;
    intpair *nodes;
    bt_leaf_t *lleaf, *rleaf;

    nodes = p_alloca(intpair, bt->area->depth + 1);
    page  = btn_find_leaf(bt->area, key, nodes, &depth);
    lleaf = &vbt_deref(bt->area, page)->leaf;

    if (lleaf) {
        int pos = btl_findslot(lleaf, key, &slot);

        need  = 1 + 8 + 1 + dlen;
        if (pos >= 0) {
            assert (pos + 1 + 8 < lleaf->used);
            assert (pos + 1 + 8 + lleaf->data[pos + 1 + 8] <= lleaf->used);
            if (lleaf->data[pos + 1 + 8] + dlen < 256) {
                reuse = true;
                need = dlen;
            }
        }
    } else {
        return -1;
    }

    assert (need >= 0);
    assert (0 <= slot && slot <= lleaf->used);
    if (need + lleaf->used <= ssizeof(lleaf->data))
        goto easy;

    rleaf = &vbt_deref(bt->area, lleaf->next)->leaf;
    if (!rleaf) {
        rleaf = btl_new(bt, nodes, depth);
        if (!rleaf)
            return -1;
        return btree_push(bt, key, data, dlen);
    }

    {
        int next, oldpos, shift;

        oldpos = slot;
        next   = slot;

        while (next < lleaf->used && next + need <= ssizeof(lleaf->data)) {
            oldpos = next;
            next += 1 + lleaf->data[next];
            assert (next < lleaf->used);
            next += 1 + lleaf->data[next];
            assert (next <= lleaf->used);
        }

        shift = lleaf->used - oldpos;
        assert (0 <= shift && shift <= lleaf->used);

        if (shift + rleaf->used + (slot == oldpos ? need : 0) > ssizeof(rleaf->data)) {
            rleaf = btl_new(bt, nodes, depth);
            if (!rleaf)
                return -1;
            return btree_push(bt, key, data, dlen);
        }

        memmove(rleaf->data + shift, rleaf->data, rleaf->used);
        memcpy(rleaf->data, lleaf->data + oldpos, shift);
        lleaf->used -= shift;
        rleaf->used += shift;

        {
            uint64_t lk = oldpos == slot ? 0 : key;
            btl_maxkey(lleaf, &lk);
            btn_update(bt, nodes, depth, page, lk);
        }

        if (oldpos == slot) {
            slot  = 0;
            page  = lleaf->next;
            lleaf = rleaf;
        }
    }

  easy:
    {
        byte *p = lleaf->data;
        int pos = slot;

        if (reuse) {
            pos += 1 + 8;
        }

        assert (pos <= lleaf->used);
        assert (lleaf->used + need <= ssizeof(lleaf->data));
        memmove(p + pos + need, p + pos, lleaf->used - pos);

        if (reuse) {
            p[pos++] += dlen;
        } else {
            p[pos++] = 8;
            memcpy(p + pos, &key, 8);
            pos += 8;
            p[pos++] = dlen;
        }
        memcpy(p + pos, data, dlen);
        lleaf->used += need;
        assert (pos + dlen <= lleaf->used);
    }
    return 0;
}

#if 0
#include "strconv.h"
static void hexdump(FILE *out, const byte *p, int n)
{
    while (n > 0) {
        fputc(__str_digits_lower[*p >> 4], out);
        fputc(__str_digits_lower[*p & 15], out);
        p++, n--;
    }
}
#endif

void btree_dump(const btree_t *bt_pub, FILE *out)
{
    struct btree_priv *bt = bt_pub->area;
    int32_t lmost, depth = 0;

    lmost = bt->root;

    while (BTPP_IS_NODE(lmost)) {
        int32_t page = lmost;

        fprintf(out, "====== DEPTH %d =====\n", depth++);
        while (BTPP_OFFS(page) != BTPP_NIL) {
            int i;
            bt_node_t *node = &bt_deref(bt, page)->node;

            fprintf(out, "----- node p%d: %d / %d",
                    BTPP_OFFS(page), node->nbkeys, BT_ARITY);

            for (i = 0; i < node->nbkeys; i++) {
                if (!(i & 0xf)) {
                    fputc('\n', out);
                }
                fprintf(out, " %03d ", BTPP_OFFS(node->ptrs[i]));
                fprintf(out, "%04llx", (long long)node->keys[i]);
            }

            fprintf(out, " %03d\n", BTPP_OFFS(node->ptrs[i]));
            page = node->next;
        }

        lmost = bt_deref(bt, lmost)->node.ptrs[0];
    }

    fprintf(out, "====== DEPTH %d =====\n", depth);
    {
        int32_t page = lmost;

        while (BTPP_OFFS(page) != BTPP_NIL) {
            bt_leaf_t *leaf = &bt_deref(bt, page)->leaf;
            int pos;

            fprintf(out, "%03d [%04do] ",
                    BTPP_OFFS(page), leaf->used);
            for (pos = 0; pos < leaf->used; ) {
                int size = 0;

                fputs(pos == 0 ? "[" : ", ", out);
                switch (leaf->data[pos]) {
                    uint64_t key;
                  case 0:
                    fprintf(out, "--->");
                    break;
                  case 8:
                    memcpy(&key, leaf->data + 1 + pos, 8);
                    fprintf(out, "%04llx", (long long)key);
                    break;
                  default:
                    fprintf(out, "@#()*!@#^");
                    break;
                }

                pos += 1 + leaf->data[pos];
                fputs(": ", out);

                size += leaf->data[pos];
#if 0
                hexdump(out, leaf->data + pos + 1, leaf->data[pos]);
#endif
                pos += 1 + leaf->data[pos];

                while (pos < leaf->used && !leaf->data[pos]) {
                    pos += 1 + leaf->data[pos];
#if 0
                    hexdump(out, leaf->data + pos + 1, leaf->data[pos]);
#endif
                    size += leaf->data[pos];
                    pos += 1 + leaf->data[pos];
                }

                fprintf(out, "%d", size);
            }

            page = leaf->next;
            fputs("]\n", out);
        }
    }
}

#endif
