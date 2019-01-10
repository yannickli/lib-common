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

#include <sys/mman.h>

#include "container.h"
#include "btree.h"
#include "unix.h"

#define BT_VERSION_MAJOR   1

#define BT_PAGE_SHIFT      10   /* 4 KB */
#define BT_PAGE_SIZE       (1 << BT_PAGE_SHIFT)
#define BT_GROW_SIZE       (1 << 20)
#define BT_INIT_NBPAGES    (BT_GROW_SIZE / BT_PAGE_SIZE)
#define BT_GROW_NBPAGES    (BT_GROW_SIZE / BT_PAGE_SIZE)

#if BT_PAGE_SIZE == 4096
#define BT_VERSION_MINOR  0
#else
#define BT_VERSION_MINOR  1
#endif

#define SPLIT_LEAF_HALF   0

/* minimum head size is 32 = 4*8 */
/* minimum node size is 40 = 4*4+(4+8)*2 */
/* minimum leaf size is 64 = 4*2+(1+8+1+4)*4 */

#if BT_PAGE_SIZE < 64
#error page size too small: BT_PAGE_SIZE
#endif
#if (BT_PAGE_SIZE * BT_INIT_NBPAGES) % 4096
#error initial index size too small: BT_INIT_NBPAGES
#endif
#if (BT_PAGE_SIZE * BT_GROW_NBPAGES) % 4096
#error index size increment too small: BT_GROW_NBPAGES
#endif

/* Choose BT_MAX_DLEN for better split properties in pathological cases */
#if SPLIT_LEAF_HALF
#define BT_MAX_DLEN       ((BT_PAGE_SIZE - 2 * 4) / 4 - 1 - 8 - 1)
#else
#define BT_MAX_DLEN       ((BT_PAGE_SIZE - 2 * 4) / 6 - 1 - 8 - 1)
#endif

#if BT_MAX_DLEN < 4
#undef  BT_MAX_DLEN
#define BT_MAX_DLEN       4
#elif BT_MAX_DLEN > 255
#undef  BT_MAX_DLEN
#define BT_MAX_DLEN       255
#endif

#define BT_ARITY          ((BT_PAGE_SIZE - 4 * 4) / (8 + 4))
                               /**< L constant in the b-tree terminology */

qvector_t(btkrr, struct bt_key_range_rec);

static const union {
    char     s[4];
    uint32_t magic;
} ISBT_MAGIC = { { 'I', 'S', 'B', 'T' } };

enum btpp_masks {
    BTPP_NODE_MASK = 0x80000000,
    BTPP_OFFS_MASK = 0x00ffffff,
    BTPP_NIL       = BTPP_OFFS_MASK,
};

typedef struct bt_node_t {
    int32_t next;
    int32_t flags; /* Never used... ? */

    int32_t nbkeys;
    int32_t ptrs[BT_ARITY + 1];
    uint64_t keys[BT_ARITY];
} bt_node_t;

typedef struct bt_leaf_t {
    int32_t next;
    int32_t used;
    byte data[BT_PAGE_SIZE - 4 * 2];
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
    int16_t  wrlock;    /**< holds the pid of the writer if any.           */
    /* fourth qword */
    uint64_t wrlockt;   /**< time associated to the lock                   */

    byte padding[BT_PAGE_SIZE - 4 * 8]; /**< padding up to page size       */

    bt_page_t pages[];
};
MMFILE_FUNCTIONS(btree_t, bt_real);

typedef struct intpair {
    int32_t page;
    int32_t pos;
} intpair;


/*---------------- btree generic page functions ----------------*/

/* btree page pointers are organized this way (bit n stands for position 2^n)
 *
 * bit  31   : points to a leaf (0) or a node (1) page.
 * bits 30-24: unused
 * bits  0-23: page number in data area
 */

/* BTPP stands fro BTree Page Pointer */
#define BTPP_IS_NODE(x)   ((x) & BTPP_NODE_MASK)
#define BTPP_OFFS(x)      ((x) & BTPP_OFFS_MASK)

static int32_t bt_page_new(btree_t *bt)
{
    int32_t page, from;

    /* OG: bogus test, empty freelist should not be 0, but BTPP_NIL */
    /* OG: the real issue is that BTPP_NIL should be 0 */
    if (!bt->area->freelist) {
        int len, res;

        len = BT_GROW_NBPAGES;
        res = bt_real_truncate_unlocked(bt, bt->size + len * BT_PAGE_SIZE);
        if (res < 0) {
            e_error("%s: cannot extend file to %zd: %m",
                    bt->path, bt->size + len * BT_PAGE_SIZE);
            if (!bt->area) {
                e_panic("%s: not enough memory!", bt->path);
            }
            return res;
        }
        from = bt->area->nbpages;
        bt->area->nbpages += len;

        bt->area->freelist = from;
        for (page = from; page < from + len - 1; page++) {
            bt->area->pages[page].leaf.next = page + 1;
        }
        bt->area->pages[page].leaf.next = 0;
    }
    page = bt->area->freelist;
    bt->area->freelist = bt->area->pages[page].leaf.next;

    bt->area->pages[page].leaf.next = BTPP_NIL;
    bt->area->pages[page].leaf.used = 0;
    return page;
}

static bt_page_t *vbt_deref(struct btree_priv *bt, int32_t ptr)
{
    int page = BTPP_OFFS(ptr);
    if (page == BTPP_NIL) {
        assert (false);
        return NULL;
    }
    if (page >= bt->nbpages) {
        assert (false);
        return NULL;
    }
    return bt->pages + page;
}

static const bt_page_t *bt_deref(const struct btree_priv *bt, int32_t ptr)
{
    return vbt_deref((struct btree_priv *)bt, ptr);
}

#define MAP_NODE(bt, n)        (&vbt_deref(bt, n)->node)
#define MAP_LEAF(bt, n)        (&vbt_deref(bt, n)->leaf)
#define MAP_CONST_NODE(bt, n)  (&bt_deref(bt, n)->node)
#define MAP_CONST_LEAF(bt, n)  (&bt_deref(bt, n)->leaf)


static void btn_shift(bt_node_t *node, int dst, int src, int count)
{
    assert (dst >= 0 && src >= 0 && count >= 0);
    assert (dst + count <= BT_ARITY);
    assert (src + count <= BT_ARITY);

    p_move2(node->ptrs, dst, src, count + 1);

    if (count > 0) {
        p_move2(node->keys, dst, src, count);
    }
}

static int
btn_update(btree_t *bt, const intpair nodes[], int level, uint64_t key)
{
    while (++level <= bt->area->depth) {
        int pos = nodes[level].pos;
        bt_node_t *node = MAP_NODE(bt->area, nodes[level].page);

        if (!node)
            return -1;

        if (pos >= node->nbkeys)
            break;

        node->keys[pos] = key;
        if (pos < node->nbkeys - 1)
            break;
    }
    return 0;
}

static uint64_t btl_get_maxkey(const bt_leaf_t *leaf)
{
    int pos, keypos = -1;
    uint64_t maxkey = 0;

    for (pos = 0; pos < leaf->used;) {
        keypos = pos;
        pos += 1 + 8;
        assert (pos < leaf->used);
        pos += 1 + leaf->data[pos];
        assert (pos <= leaf->used);
    }
    if (keypos >= 0) {
        memcpy(&maxkey, leaf->data + keypos + 1, 8);
    }
    return maxkey;
}

static uint64_t bt_get_maxkey(const btree_t *bt, uint32_t page)
{
    if (BTPP_IS_NODE(page)) {
        const bt_node_t *node = MAP_CONST_NODE(bt->area, page);
        if (!node)
            return 0;
        return node->keys[node->nbkeys - 1];
    } else {
        const bt_leaf_t *leaf = MAP_CONST_LEAF(bt->area, page);
        if (!leaf)
            return 0;
        return btl_get_maxkey(leaf);
    }
}

static void btn_insert_aux(btree_t *bt, bt_node_t *node, int pos,
                           int32_t lpage, int32_t rpage)
{
    btn_shift(node, pos + 1, pos, node->nbkeys - pos);
    node->nbkeys++;

    node->ptrs[pos]     = lpage;
    node->keys[pos]     = bt_get_maxkey(bt, lpage);
    node->ptrs[pos + 1] = rpage;

    if (pos + 1 < node->nbkeys) {
        node->keys[pos + 1] = bt_get_maxkey(bt, rpage);
    }
}

/* OG: bad name for a function that inserts a node after the current
 * one at level (level+1)
 */
static int btn_insert(btree_t *bt, intpair nodes[], int level, int32_t rpage)
{
    int32_t page, lpage, npage;
    bt_node_t *node, *sibling;
    int pos, split;

    lpage = nodes[level++].page;
    if (level > bt->area->depth) {
        /* Need a new level */
        page = RETHROW(bt_page_new(bt));
        node = MAP_NODE(bt->area, page);
        if (!node)
            return -1;

        bt->area->depth++;
        bt->area->root = page | BTPP_NODE_MASK;
        node->next   = BTPP_NIL;
        node->nbkeys = 1;

        node->ptrs[0] = lpage;
        node->keys[0] = bt_get_maxkey(bt, lpage);
        node->ptrs[1] = rpage;
        return 0;
    }

    pos  = nodes[level].pos;
    page = nodes[level].page;
    node = MAP_NODE(bt->area, page);
    if (!node)
        return -1;

    if (node->nbkeys < BT_ARITY) {
        btn_insert_aux(bt, node, pos, lpage, rpage);
        if (pos + 1 == node->nbkeys - 1) {
            return btn_update(bt, nodes, level, node->keys[node->nbkeys - 1]);
        }
        return 0;
    }

    if (BTPP_OFFS(node->next) != BTPP_NIL) {
        sibling = MAP_NODE(bt->area, node->next);
        if (!sibling)
            return -1;

        if (sibling->nbkeys > 0 && sibling->nbkeys < BT_ARITY) {
            btn_shift(sibling, 1, 0, sibling->nbkeys);
            sibling->nbkeys++;
            if (pos < node->nbkeys - 1) {
                sibling->ptrs[0]    = node->ptrs[node->nbkeys - 1];
                sibling->keys[0]    = node->keys[node->nbkeys - 1];
                btn_shift(node, pos + 1, pos, node->nbkeys - 1 - pos);
                node->ptrs[pos]     = lpage;
                node->keys[pos]     = bt_get_maxkey(bt, lpage);
                node->ptrs[pos + 1] = rpage;
                node->keys[pos + 1] = bt_get_maxkey(bt, rpage);
            } else {
                node->ptrs[pos]     = lpage;
                node->keys[pos]     = bt_get_maxkey(bt, lpage);
                node->ptrs[pos + 1] = rpage;
                sibling->ptrs[0]    = rpage;
                sibling->keys[0]    = bt_get_maxkey(bt, rpage);
            }
            return btn_update(bt, nodes, level, node->keys[node->nbkeys - 1]);
        }
    }

    npage = RETHROW(bt_page_new(bt));
    npage |= BTPP_NODE_MASK;
    /* remap pages because bt_page_new may have moved bt->area */
    node    = MAP_NODE(bt->area, page);
    sibling = MAP_NODE(bt->area, npage);
    if (!node || !sibling)
        return -1;

    /* split node data evenly between node and new node */
    split = BT_ARITY / 2;

    if (node->next == BTPP_NIL && pos >= node->nbkeys - 1) {
        /* except split last node at this level on last key */
        split = node->nbkeys - 1;
    }

    sibling->next = node->next;
    node->next    = npage & ~BTPP_NODE_MASK;

    memcpy(sibling->ptrs, node->ptrs + split,
           (node->nbkeys - split + 1) * sizeof(node->ptrs[0]));
    memcpy(sibling->keys, node->keys + split,
           (node->nbkeys - split) * sizeof(node->keys[0]));
    sibling->nbkeys = node->nbkeys - split;
    node->nbkeys = split;

    /* Update new max key up the tree (maybe useless?) */
    btn_update(bt, nodes, level, node->keys[node->nbkeys - 1]);

    if (pos < node->nbkeys) {
        btn_insert_aux(bt, node, pos, lpage, rpage);
    } else {
        btn_insert_aux(bt, sibling, pos - node->nbkeys, lpage, rpage);
        node->ptrs[node->nbkeys] = sibling->ptrs[0];
    }
    return btn_insert(bt, nodes, level, npage);
}

/*---------------- Integrity checking code ----------------*/

static int bt_check_header(struct btree_priv *btp, int dofix,
                           const char *path, ssize_t size)
{
    bool did_a_fix = false;
    int32_t nbpages;

    if (btp->magic != ISBT_MAGIC.magic) {
        e_trace(1, "Bad magic");
        return -1;
    }

    /* hook conversions here! */
    if (btp->major != BT_VERSION_MAJOR || btp->minor != BT_VERSION_MINOR) {
        e_trace(1, "Bad version");
        return -1;
    }

    if (size < BT_PAGE_SIZE || (size_t)size % BT_PAGE_SIZE != 0) {
        e_trace(1, "Bad page size");
        return -1;
    }

    if (btp->wrlock) {
        struct timeval tv;

        if (btp->wrlock != getpid()) {
            e_trace(1, "Already locked");
            return -1;
        }

        if (pid_get_starttime(btp->wrlock, &tv))
            return -1;

        if (btp->wrlockt != MAKE64(tv.tv_sec, tv.tv_usec))
            return -1;

        dofix = 0;
    }

    nbpages = (size_t)size / BT_PAGE_SIZE - 1;
    if (btp->nbpages > nbpages) {
        if (!dofix)
            return -1;

        e_error("%s: corrupted header: nbpages was %d, fixed into %d",
                path, btp->nbpages, nbpages);

        btp->nbpages = nbpages;
        did_a_fix = true;
    }

    if (btp->freelist < 0 || btp->freelist >= btp->nbpages) {
        if (!dofix)
            return -1;

        e_error("%s: corrupted header: freelist %d out of bounds",
                path, btp->freelist);

        btp->freelist = 0;
        did_a_fix = true;
    }

    return did_a_fix;
}

static const char *bt_keystr(uint64_t key)
{
    static char buf[64];
    static int pos;

    pos ^= sizeof(buf) / 2;

    if (key > 0xffffffff && (key & 0xffffffff) < 0x1000000) {
        snprintf(buf + pos, sizeof(buf) / 2, "%d:%d",
                 (int)(key >> 32), (int)(key & 0xffffffff));
    } else {
        snprintf(buf + pos, sizeof(buf) / 2, "%04llx",
                 (long long)key);
    }
    return buf + pos;
}

static int bt_void_printf(FILE *fp, const char *fmt, ...)
{
    return 0;
}

#define MAX_KEY 0xFFFFFFFFFFFFFFFFULL

static int bt_check_node(btree_t *bt, int level, int32_t page,
                         const uint64_t maxkey, int32_t next,
                         btree_print_fun *fun, FILE *arg);
static int bt_check_leaf(btree_t *bt, int level, int32_t page,
                         const uint64_t maxkey, int32_t next,
                         btree_print_fun *fun, FILE *arg);

static int bt_check_page(btree_t *bt, int level, int32_t page,
                         uint64_t maxkey, int32_t next,
                         btree_print_fun *fun, FILE *arg)
{
    if (BTPP_OFFS(page) == BTPP_NIL) {
        return 1;
    }
    if (BTPP_IS_NODE(page)) {
        if (level <= 0)
            return 1;
        return bt_check_node(bt, level, page, maxkey, next, fun, arg);
    } else {
        if (level > 0)
            return 1;
        return bt_check_leaf(bt, level, page, maxkey, next, fun, arg);
    }
}

static int bt_check_node(btree_t *bt, int level, int32_t page,
                         const uint64_t maxkey, int32_t next,
                         btree_print_fun *fun, FILE *arg)
{
    const bt_node_t *node;
    int32_t next1;
    int i;

    //fun(arg, "node L%d:%03d\n", level, BTPP_OFFS(page));

    node = MAP_CONST_NODE(bt->area, page);

    if (!node) {
        fun(arg, "%03d (node.L%d): cannot map\n",
               BTPP_OFFS(page), level);
        return -1;
    }
    if (node->nbkeys <= 0) {
        fun(arg, "%03d (node.L%d): invalid nbkeys=%d\n",
               BTPP_OFFS(page), level,
               node->nbkeys);
        return -1;
    }
    for (i = 0; i < node->nbkeys; i++) {
        if (node->keys[i] > maxkey) {
            fun(arg, "%03d (node.L%d): key[%d/%d]=%s > maxkey=%s\n",
                   BTPP_OFFS(page), level,
                   i, node->nbkeys, bt_keystr(node->keys[i]),
                   bt_keystr(maxkey));
            return 1;
        }
        if (i && node->keys[i] < node->keys[i - 1]) {
            fun(arg, "%03d (node.L%d): key[%d/%d]=%s < key[%d]=%s\n",
                   BTPP_OFFS(page), level,
                   i, node->nbkeys, bt_keystr(node->keys[i]),
                   i - 1, bt_keystr(node->keys[i]));
            return 1;
        }
        if (1 || i < node->nbkeys - 1 || BTPP_OFFS(node->next) == BTPP_NIL)
            next1 = node->ptrs[i + 1];
        else
            next1 = -1;

        if (bt_check_page(bt, level - 1, node->ptrs[i], node->keys[i], next1, fun, arg)) {
            fun(arg, "%03d (node.L%d): check_page failed, key[%d/%d]=%s ptr=%03d\n",
                   BTPP_OFFS(page), level,
                   i, node->nbkeys, bt_keystr(node->keys[i]),
                   BTPP_OFFS(node->ptrs[i]));
            return 1;
        }
    }
    if (BTPP_OFFS(node->next) != BTPP_NIL && maxkey != node->keys[i - 1]) {
        fun(arg, "%03d (node.L%d): key[%d/%d]=%s != maxkey=%s\n",
               BTPP_OFFS(page), level,
               i - 1, node->nbkeys, bt_keystr(node->keys[i - 1]),
               bt_keystr(maxkey));
        return 1;
    }
    if (next != -1 && BTPP_OFFS(node->next) != BTPP_OFFS(next)) {
        fun(arg, "%03d (node.L%d): node->next=%03d != next=%03d\n",
               BTPP_OFFS(page), level,
               BTPP_OFFS(node->next), BTPP_OFFS(next));
        return 1;
    }
    if (BTPP_OFFS(node->next) == BTPP_NIL) {
        if (maxkey != MAX_KEY) {
            fun(arg, "%03d (node.L%d): non rightmost page has no next\n",
                   BTPP_OFFS(page), level);
            return 1;
        }
        if (bt_check_page(bt, level - 1, node->ptrs[i], maxkey, BTPP_NIL, fun, arg)) {
            fun(arg, "%03d (node.L%d): check_page failed, ptr[%d/%d]=%03d\n",
                   BTPP_OFFS(page), level,
                   i, node->nbkeys, BTPP_OFFS(node->ptrs[i]));
            return 1;
        }
    }
    return 0;
}

static int bt_check_leaf(btree_t *bt, int level, int32_t page,
                         const uint64_t maxkey, int32_t next,
                         btree_print_fun *fun, FILE *arg)
{
    const bt_leaf_t *leaf;
    int pos, nextpos;
    uint64_t lastkey = 0, key;

    //fun(arg, "p%d (leaf):\n", page);

    leaf = MAP_CONST_LEAF(bt->area, page);
    if (!leaf) {
        fun(arg, "%03d (leaf): cannot map\n",
               BTPP_OFFS(page));
        return -1;
    }

    if (leaf->used <= 1 + 8 + 1) {
        fun(arg, "%03d (leaf): invalid used=%d\n",
               BTPP_OFFS(page),
               leaf->used);
        return 1;
    }

    for (pos = 0; pos < leaf->used;) {
        if (leaf->data[pos] != 8) {
            fun(arg, "%03d (leaf): pos=%d/%d, invalid keylen=%d\n",
                   BTPP_OFFS(page), pos, leaf->used,
                   leaf->data[pos]);
            return 1;
        }
        if (pos + 1 + 8 + 1 > leaf->used) {
            fun(arg, "%03d (leaf): pos=%d/%d, overflow\n",
                   BTPP_OFFS(page), pos, leaf->used);
            return 1;
        }
        nextpos = pos + 1 + 8 + 1 + leaf->data[pos + 1 + 8];
        if (nextpos > leaf->used) {
            fun(arg, "%03d (leaf): pos=%d/%d, overflow dlen=%d\n",
                   BTPP_OFFS(page), pos, leaf->used,
                   leaf->data[pos + 1 + 8]);
            return 1;
        }
        memcpy(&key, leaf->data + pos + 1, 8);
        if (pos > 0 && key < lastkey) {
            fun(arg, "%03d (leaf): pos=%d/%d, key=%s < lastkey=%s\n",
                   BTPP_OFFS(page), pos, leaf->used,
                   bt_keystr(key), bt_keystr(lastkey));
            return 1;
        }
        if (key > maxkey) {
            fun(arg, "%03d (leaf): pos=%d/%d, key=%s > maxkey=%s\n",
                   BTPP_OFFS(page), pos, leaf->used,
                   bt_keystr(key), bt_keystr(maxkey));
            return 1;
        }
        if (nextpos == leaf->used && BTPP_OFFS(next) != BTPP_NIL && key != maxkey) {
            fun(arg, "%03d (leaf): pos=%d/%d, key=%s != maxkey=%s\n",
                   BTPP_OFFS(page), pos, leaf->used,
                   bt_keystr(key), bt_keystr(maxkey));
            return 1;
        }
        lastkey = key;
        pos = nextpos;
    }
    if (BTPP_OFFS(leaf->next) != BTPP_OFFS(next)) {
        fun(arg, "%03d (leaf): leaf->next=%03d != next=%03d\n",
               BTPP_OFFS(page),
               BTPP_OFFS(leaf->next), BTPP_OFFS(next));
        return 1;
    }
    return 0;
}

int btree_check_integrity(btree_t *bt, int dofix, btree_print_fun *fun, FILE *arg)
{
    return bt_check_page(bt, bt->area->depth, bt->area->root,
                         MAX_KEY, BTPP_NIL, fun ? fun : bt_void_printf, arg);
}

/*---------------- Memory mapped API functions ----------------*/

btree_t *btree_open(const char *path, int flags, bool check)
{
    btree_t *bt;
    int oflags = MMO_RANDOM | MMO_TLOCK;

    if (check) {
        oflags |= MMO_POPULATE;
    }

    bt = bt_real_open(path, flags, oflags,
                      sizeof(bt_page_t) * BT_INIT_NBPAGES);
    if (!bt) {
        e_trace(2, "Could not open bt on %s: %m", path);
        return NULL;
    }

    if ((flags & (O_TRUNC | O_CREAT)) && !bt->area->magic) {
        int i;

        bt->area->magic    = ISBT_MAGIC.magic;
        bt->area->major    = BT_VERSION_MAJOR;
        bt->area->minor    = BT_VERSION_MINOR;
        bt->area->root     = 0;
        bt->area->nbpages  = BT_INIT_NBPAGES - 1;
        bt->area->freelist = 1;
        bt->area->depth    = 0;
        bt->area->wrlock   = 0;
        bt->area->wrlockt  = 0;

        /* initial root page is an empty leaf */
        bt->area->pages[0].leaf.used = 0;
        bt->area->pages[0].leaf.next = BTPP_NIL;

        /* initialize free list */
        for (i = 1; i < BT_INIT_NBPAGES - 2; i++) {
            bt->area->pages[i].leaf.next = i + 1;
        }
        bt->area->pages[i].leaf.next = 0;
    } else {
        if (bt_check_header(bt->area, O_ISWRITE(flags), bt->path, bt->size) < 0) {
            btree_close(&bt);
            errno = EUCLEAN;
            return NULL;
        }

        if (check) {
            if (btree_check_integrity(bt, O_ISWRITE(flags), NULL, NULL) < 0) {
                btree_close(&bt);
                errno = EUCLEAN;
                return NULL;
            }
        }
    }

    if (O_ISWRITE(flags)) {
        struct timeval tv;
        pid_t pid = getpid();

        if (bt->area->wrlock) {
            bt_real_close(&bt);
            errno = EDEADLK;
            return NULL;
        }

        pid_get_starttime(pid, &tv);
        bt->area->wrlock = pid;
        bt->area->wrlockt = MAKE64(tv.tv_sec, tv.tv_usec);
        msync(bt->area, bt->size, MS_SYNC);
    }

    if (bt_real_unlockfile(bt) < 0) {
        int save_errno = errno;
        btree_close(&bt);
        errno = save_errno;
        return NULL;
    }

    return bt;
}

void btree_close(btree_t **btp)
{
    btree_t *bt = *btp;

    if (bt) {
        bt_real_wlock(bt);
        if (bt->writeable && bt->refcnt <= 1
        &&  bt->area->wrlock == getpid())
        {
            msync(bt->area, bt->size, MS_SYNC);
            bt->area->wrlock  = 0;
            bt->area->wrlockt = 0;
        }
        bt_real_close_wlocked(btp);
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
        int i = (l + r) >> 1;

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
                             intpair nodes[])
{
    int32_t page = bt->root;
    int level;

    for (level = bt->depth; level > 0; level--) {
        const bt_node_t *node;
        int pos;

        if (!BTPP_IS_NODE(page))
            return e_error("node.L%d page %d is not tagged", level, page);

        node = MAP_CONST_NODE(bt, page);
        if (!node)
            return -1;

        pos = btn_bsearch(node, key);
        if (nodes) {
            nodes[level].page = page;
            nodes[level].pos  = pos;
        }
        page = node->ptrs[pos];
    }

    if (nodes) {
        nodes[0].page = page;
    }
    return page;
}


/****************************************************************************/
/* code specific to the leaves                                              */
/****************************************************************************/

static ALWAYS_INLINE uint64_t btl_getkey(const bt_leaf_t *leaf, int pos)
{
    assert (0 <= pos && pos + 1 + 8 + 1 <= leaf->used);
    assert (leaf->data[pos] == 8);

    return get_unaligned_cpu64(leaf->data + pos + 1);
}

static enum sign
btl_keycmp(uint64_t key, const bt_leaf_t *leaf, int pos)
{
    return CMP(key, btl_getkey(leaf, pos));
}

static int
btl_findslot(const bt_leaf_t *leaf, uint64_t key, int32_t *slot)
{
    int32_t pos = 0;

    while (pos < leaf->used) {
        switch (btl_keycmp(key, leaf, pos)) {
          case CMP_GREATER:
            break;

          case CMP_EQUAL:
            if (slot) {
                *slot = pos;
            }
            return pos;

          case CMP_LESS:
            if (slot) {
                *slot = pos;
            }
            return -1;
        }

        pos += 8 + 1; /* skip key  */
        assert (pos < leaf->used);
        pos += 1 + leaf->data[pos]; /* skip data */
        assert (pos <= leaf->used);
    }

    if (slot) {
        *slot = pos;
    }
    return -1;
}

int btree_fetch(btree_t *bt, uint64_t key, sb_t *out)
{
    int page, pos, len = 0;
    const bt_leaf_t *leaf;

    bt_real_rlock(bt);
    page = btn_find_leaf(bt->area, key, NULL);
    if (page < 0)
        goto error;

    leaf = MAP_CONST_LEAF(bt->area, page);
    if (!leaf)
        goto error;

    pos = btl_findslot(leaf, key, NULL);
    if (pos < 0)
        goto error;

    do {
        int datalen;

        /* skip key */
        pos += 1 + 8;
        if (unlikely(pos >= leaf->used)) {
            /* should flag btree structure error: no data length */
            break;
        }

        datalen = leaf->data[pos++];
        if (unlikely(pos + datalen > leaf->used)) {
            /* should flag btree structure error: not enough data */
            break;
        }

        sb_add(out, leaf->data + pos, datalen);
        len += datalen;
        pos += datalen;

        if (pos >= leaf->used) {
            pos = 0;
            if (leaf->next == BTPP_NIL)
                break;
            leaf = MAP_CONST_LEAF(bt->area, leaf->next);
            if (!leaf || leaf->used <= 0) {
                /* should flag btree structure error */
                break;
            }
        }
    } while (btl_keycmp(key, leaf, pos) == CMP_EQUAL);

    bt_real_unlock(bt);
    return len;

  error:
    bt_real_unlock(bt);
    return -1;
}

int btree_fetch_range(btree_t *bt, uint64_t kmin, uint64_t kmax,
                      struct bt_key_range *btkr)
{
    int page, pos, len = 0;
    const bt_leaf_t *leaf;
    struct bt_key_range_rec btkrr = { .key = kmin };
    qv_t(btkrr) vec;
    sb_t out;

    sb_init(&out);
    qv_init(btkrr, &vec);

    bt_real_rlock(bt);
    page = btn_find_leaf(bt->area, kmin, NULL);
    if (page < 0)
        goto error;

    leaf = MAP_CONST_LEAF(bt->area, page);
    if (!leaf)
        goto error;

    btl_findslot(leaf, kmin, &pos);
    for (;;) {
        uint64_t key;
        int datalen;

        if (pos >= leaf->used) {
            pos = 0;
            if (leaf->next == BTPP_NIL)
                break;
            leaf = MAP_CONST_LEAF(bt->area, leaf->next);
            if (!leaf || leaf->used <= 0) {
                /* should flag btree structure error */
                break;
            }
        }

        /* skip key */
        key  = btl_getkey(leaf, pos);
        pos += 1 + 8;
        /* XXX: kmax may well be UINT64_MAX hence the underflow test */
        if (key > kmax || key < kmin)
            break;

        if (key != btkrr.key) {
            if (btkrr.dlen) {
                qv_append(btkrr, &vec, btkrr);
            }
            btkrr.key  = key;
            btkrr.dpos = out.len;
            btkrr.dlen = 0;
        }

        if (unlikely(pos >= leaf->used)) {
            /* should flag btree structure error: no data length */
            break;
        }

        datalen = leaf->data[pos++];
        if (unlikely(pos + datalen > leaf->used)) {
            /* should flag btree structure error: not enough data */
            break;
        }

        sb_add(&out, leaf->data + pos, datalen);
        btkrr.dlen += datalen;
        len        += datalen;
        pos        += datalen;
    }

    if (btkrr.dlen)
        qv_append(btkrr, &vec, btkrr);
    *btkr = (struct bt_key_range){
        .nkeys = vec.len,
        .keys  = vec.tab,
        { .data  = sb_detach(&out, NULL) },
    };
    bt_real_unlock(bt);
    return 0;

  error:
    sb_wipe(&out);
    qv_wipe(btkrr, &vec);
    bt_real_unlock(bt);
    return -1;
}

int btree_push(btree_t *bt, uint64_t key, const void *_data, int dlen)
{
    bool reuse;
    int32_t page, slot, need;
    int pos;
    intpair *nodes;
    bt_leaf_t *lleaf, *nleaf;
    const byte *data = _data;
    byte *p;

    if (dlen < 0) {
        /* invalid data length */
        return -1;
    }

    while (dlen > BT_MAX_DLEN) {
        btree_push(bt, key, data + dlen - BT_MAX_DLEN, BT_MAX_DLEN);
        dlen -= BT_MAX_DLEN;
    }

    if (dlen == 0) {
        return 0;
    }

    bt_real_wlock(bt);

  restart:
    nodes = p_alloca(intpair, bt->area->depth + 1);
    page  = btn_find_leaf(bt->area, key, nodes);
    lleaf = MAP_LEAF(bt->area, page);

    if (!lleaf)
        goto error;

    /* Find position where to insert the data */
    /* pos < 0 if key does not exist, slot has the offset of the next
     * higher key or the offset of the end of the last page in the
     * index.
     * pos >= 0 -> key exists, slot = pos is the offset of the first
     * key/data chunk for that key.
     */
    pos = btl_findslot(lleaf, key, &slot);

    reuse = false;
    need = 1 + 8 + 1 + dlen;
    if (pos >= 0) {
        /* key already exists: check if chunk is full */
        assert (pos + 1 + 8 + 1 <= lleaf->used);
        assert (pos + 1 + 8 + 1 + lleaf->data[pos + 1 + 8] <= lleaf->used);
        if (lleaf->data[pos + 1 + 8] + dlen <= BT_MAX_DLEN) {
            reuse = true;
            need = dlen;
            if (need == 0) {
                /* no data to add to already existing chunk */
                goto ok;
            }
        }
    }

    assert (0 <= slot && slot <= lleaf->used);

    if (need + lleaf->used > ssizeof(lleaf->data)) {
#if SPLIT_LEAF_HALF
        /* Split this page in 2 half-full pages */
        int32_t npage;
        int lastpos;

        npage = bt_page_new(bt);
        if (npage < 0)
            goto error;

        /* remap pages because bt_page_new may have moved bt->area */
        lleaf = MAP_LEAF(bt->area, page);
        nleaf = MAP_LEAF(bt->area, npage);

        if (!lleaf || !nleaf)
            goto error;

        for (lastpos = pos = 0;
             pos < lleaf->used && pos <= ssizeof(lleaf->data) / 2;
             ) {
            lastpos = pos;
            pos += 1 + 8;
            pos += 1 + lleaf->data[pos];
        }
        if (lastpos > 0 && (slot < lastpos || pos >= lleaf->used)) {
            pos = lastpos;
        }
        if (pos >= lleaf->used) {
            /* unlikely structure error: cannot split block */
            /* currently this cam only happen for very small
             * BT_PAGE_SIZE and large data chunks.
             */
            e_error("cannot split leaf %03d", page);
            goto error;
        }

        if (lleaf->next == BTPP_NIL) {
            int pos2 = pos;
            while (pos2 < lleaf->used) {
                lastpos = pos2;
                pos2 += 1 + 8;
                pos2 += 1 + lleaf->data[pos2];
            }

            /* Split page on last chunk if inserting at end of last page */
            if (slot >= lastpos) {
                pos = lastpos;
            }
        }

        nleaf->next = lleaf->next;
        lleaf->next = npage;

        memcpy(nleaf->data, lleaf->data + pos, lleaf->used - pos);
        nleaf->used = lleaf->used - pos;
        lleaf->used = pos;

        /* If this insert fails, the index is corrupted */
        if (btn_insert(bt, nodes, 0, npage) < 0)
            goto error;

        goto restart;
#else
        int32_t rpage;
        int oldpos, shift;
        bt_leaf_t *rleaf;

        /* Not enough room in target page: need to shift some data to
         * next page.
         */
        rpage = lleaf->next;
        if (rpage == BTPP_NIL) {
            /* No next page: allocate a new empty overflow page */
            rpage = bt_page_new(bt);
            if (rpage < 0)
                goto error;

            /* remap pages because bt_page_new may have moved bt->area */
            lleaf = MAP_LEAF(bt->area, page);
            rleaf = MAP_LEAF(bt->area, rpage);
            if (!lleaf || !rleaf)
                goto error;

            rleaf->next = lleaf->next;
            lleaf->next = rpage;

            if (btn_insert(bt, nodes, 0, rpage) < 0)
                goto error;

            /* Restart because target page may be the newly allocated
             * one, or depth may have changed, or path to page may have
             * changed...
             */
            goto restart;
        }

        rleaf = MAP_LEAF(bt->area, rpage);
        if (!rleaf)
            goto error;

        /* OG: find the least chunk to shift to the next page */
        for (pos = oldpos = slot;
             pos < lleaf->used && pos + need <= ssizeof(lleaf->data);
             ) {
            oldpos = pos;
            assert (pos + 1 + 8 + 1 <= lleaf->used);
            pos += 1 + 8 + 1 + lleaf->data[pos + 1 + 8];
            assert (pos <= lleaf->used);
        }

        /* oldpos is the offset of the chunk to shift to the next page */

        shift = lleaf->used - oldpos;
        assert (0 <= shift && shift <= lleaf->used);

        if (shift + rleaf->used + (slot == oldpos ? need : 0) >
              ssizeof(rleaf->data)) {
            /* allocate a new page and split data in [lleaf:rleaf] on
             * 3 pages [lleaf:nleaf:rleaf]
             */
            int32_t npage;
            int pos1, lastpos1, lastpos2, pos2;

            npage = bt_page_new(bt);
            if (npage < 0)
                goto error;

            /* remap pages because bt_page_new may have moved bt->area */
            lleaf = MAP_LEAF(bt->area, page);
            rleaf = MAP_LEAF(bt->area, rpage);
            nleaf = MAP_LEAF(bt->area, npage);

            if (!lleaf || !rleaf || !nleaf)
                goto error;

            for (lastpos1 = pos1 = 0;
                 pos1 < lleaf->used && pos1 <= ssizeof(lleaf->data) * 2 / 3;
                 ) {
                lastpos1 = pos1;
                pos1 += 1 + 8;
                pos1 += 1 + lleaf->data[pos1];
            }
            if (lastpos1 > 0 && (slot < lastpos1 || pos1 >= lleaf->used)) {
                pos1 = lastpos1;
            }
            if (pos1 >= lleaf->used) {
                /* unlikely structure error: cannot split block */
                /* currently this cam only happen for very small
                 * BT_PAGE_SIZE and large data chunks.
                 */
                e_error("cannot split leaf %03d", page);
                goto error;
            }

            for (lastpos2 = pos2 = 0;
                 pos2 < rleaf->used && pos2 <= ssizeof(rleaf->data) / 3;) {
                lastpos2 = pos2;
                pos2 += 1 + 8;
                pos2 += 1 + rleaf->data[pos2];
                if (lleaf->used - pos1 + pos2 > ssizeof(nleaf->data)) {
                    pos2 = lastpos2;
                    break;
                }
            }
            if (lastpos2 > 0 && (slot >= pos1 || pos2 >= lleaf->used)) {
                pos2 = lastpos2;
            }
            if (pos2 >= lleaf->used) {
                /* unlikely structure error: cannot split block */
                /* currently this cam only happen for very small
                 * BT_PAGE_SIZE and large data chunks.
                 */
                e_error("cannot split leaf %03d", rpage);
                goto error;
            }

            nleaf->next = lleaf->next;
            lleaf->next = npage;

            memcpy(nleaf->data, lleaf->data + pos1, lleaf->used - pos1);
            nleaf->used = lleaf->used - pos1;
            lleaf->used = pos1;

            memcpy(nleaf->data + nleaf->used, rleaf->data, pos2);
            p_move2(rleaf->data, 0, pos2, rleaf->used - pos2);
            nleaf->used += pos2;
            rleaf->used -= pos2;

            /* If this insert fails, the index is corrupted */
            if (btn_insert(bt, nodes, 0, npage) < 0)
                goto error;

            goto restart;
        }

        p_move2(rleaf->data, shift, 0, rleaf->used);
        memcpy(rleaf->data, lleaf->data + oldpos, shift);
        lleaf->used -= shift;
        rleaf->used += shift;

        /* Update max key up the tree */
        btn_update(bt, nodes, 0, btl_get_maxkey(lleaf));

        if (oldpos == slot && reuse) {
            goto restart;
        }
#endif
    }

    p = lleaf->data;

    pos = slot;

    assert (lleaf->used + need <= ssizeof(lleaf->data));

    if (reuse) {
        /* skip key */
        pos += 1 + 8 + 1;

        assert (pos <= lleaf->used);
        p_move2(p, pos + need, pos, lleaf->used - pos);

    } else {

        assert (pos <= lleaf->used);
        p_move2(p, pos + need, pos, lleaf->used - pos);

        /* insert key */
        p[pos] = 8;
        memcpy(p + pos + 1, &key, 8);
        p[pos + 1 + 8] = 0;
        pos += 1 + 8 + 1;
    }
    p[pos - 1] += dlen;
    memcpy(p + pos, data, dlen);
    lleaf->used += need;
    assert (pos + dlen <= lleaf->used);

    /* If we inserted the key/data at the end, update the nodes */
    if (pos + dlen == lleaf->used) {
        if (lleaf->next == BTPP_NIL) {
            /* no update necessary on last page */
        } else {
            /* key is the highest key in this page */
            btn_update(bt, nodes, 0, key);
        }
    }
  ok:
    bt_real_unlock(bt);
    return 0;

  error:
    bt_real_unlock(bt);
    return -1;
}

/*---------------- Iterator APIs ----------------*/

/* OG: need more iter functions, esp. btree_iter_end() to unlock the
 * btree_t.
 */

void btree_iter_begin(btree_t *_bt, btree_iter_t *iter)
{
    struct btree_priv *bt = _bt->area;
    int32_t page = bt->root;

    bt_real_rlock(_bt);

    while (BTPP_IS_NODE(page)) {
        const bt_node_t *node = MAP_CONST_NODE(bt, page);
        assert (node);
        page = node->ptrs[0];
    }

    iter->page = page;
    iter->pos  = 0;
}

int
btree_iter_next(btree_t *_bt, btree_iter_t *iter, uint64_t *key, sb_t *out)
{
    struct btree_priv *bt = _bt->area;
    const bt_leaf_t *leaf;

    if (iter->page == BTPP_NIL) {
        /* OG: unlock should be done in btree_iter_end() */
        bt_real_unlock(_bt);
        return -1;
    }

    leaf = MAP_CONST_LEAF(bt, iter->page);

    while (iter->pos >= leaf->used) {
        if (leaf->next == BTPP_NIL) {
            /* OG: unlock should be done in btree_iter_end() */
            bt_real_unlock(_bt);
            return -1;
        }
        iter->page = leaf->next;
        iter->pos  = 0;
        leaf = MAP_CONST_LEAF(bt, iter->page);
    }

    sb_reset(out);
    /* OG: should check unlikely(leaf->data[iter->pos] == 8 &&
     *                           iter->pos + 1 + 8 > leaf->used)
     */
    memcpy(key, leaf->data + iter->pos + 1, 8);

    do {
        int datalen;

        /* skip key */
        iter->pos += 1 + 8;
        if (unlikely(iter->pos >= leaf->used)) {
            /* should flag btree structure error: no data length */
            break;
        }

        datalen = leaf->data[iter->pos++];
        if (unlikely(iter->pos + datalen > leaf->used)) {
            /* should flag btree structure error: not enough data */
            break;
        }

        sb_add(out, leaf->data + iter->pos, datalen);
        iter->pos += datalen;

        if (iter->pos >= leaf->used) {
            iter->pos = 0;
            if ((iter->page = leaf->next) == BTPP_NIL)
                return 0;
            /* OG: redundant iter->page assignment */
            leaf = MAP_CONST_LEAF(bt, iter->page = leaf->next);
            if (!leaf || leaf->used <= 0) {
                /* should flag btree structure error */
                return 0;
            }
        }
    } while (btl_keycmp(*key, leaf, iter->pos) == CMP_EQUAL);

    /* OG: could return total data length */
    return 0;
}

/*---------------- File I/O based read-only API functions ----------------*/

struct fbtree_t {
    flag_t ismap : 1;
    union {
        struct {
            int fd;
            struct btree_priv *priv;
        };
        btree_t *bt;
    };
};

fbtree_t *fbtree_open(const char *path)
{
    struct stat st;
    fbtree_t *fbt = p_new(fbtree_t, 1);

    if (stat(path, &st) || (fbt->fd = open(path, O_RDONLY)) < 0)
        goto error;

    fbt->priv = mmap(NULL, ssizeof(*fbt->priv), PROT_READ, MAP_SHARED,
                     fbt->fd, 0);
    if (fbt->priv == MAP_FAILED)
        goto error;

    /* Just read and check the btree header.  Complete integrity check
     * would be too costly.
     */
    if (bt_check_header(fbt->priv, false, path, st.st_size))
        goto error;

#ifdef POSIX_FADV_RANDOM
    posix_fadvise(fbt->fd, 0, st.st_size, POSIX_FADV_RANDOM);
#endif
    return fbt;

  error:
    fbtree_close(&fbt);
    return NULL;
}

fbtree_t *fbtree_unlocked_dup(const char *path, btree_t *bt)
{
    btree_t *bt2 = bt_real_unlocked_dup(bt);
    fbtree_t *fbt;

    if (!bt2)
        return fbtree_open(path);
    fbt = p_new(fbtree_t, 1);
    fbt->ismap = 1;
    fbt->bt    = bt2;
    return fbt;
}

void fbtree_close(fbtree_t **fbtp)
{
    if (*fbtp) {
        fbtree_t *fbt = *fbtp;
        if (fbt->ismap) {
            btree_close(&fbt->bt);
        } else {
            p_close(&fbt->fd);
            if (fbt->priv && fbt->priv != MAP_FAILED) {
                munmap(fbt->priv, ssizeof(*fbt->priv));
            }
        }
        p_delete(fbtp);
    }
}

static int fbtree_readpage(fbtree_t *fbt, int32_t page, bt_page_t *buf)
{
    const off_t offs = (BTPP_OFFS(page) + 1) * ssizeof(*buf);

    for (int pos = 0; pos < ssizeof(*buf); ) {
        int nb = pread(fbt->fd, buf + pos, sizeof(*buf) - pos, offs + pos);
        if (nb < 0) {
            if (ERR_RW_RETRIABLE(errno))
                continue;
            return -1;
        }
        if (nb == 0)
            return -1;
        pos += nb;
    }
    return 0;
}

static int32_t fbtn_find_leaf(fbtree_t *fbt, uint64_t key)
{
    int32_t page = fbt->priv->root;
    int level;

    for (level = fbt->priv->depth; level > 0; level--) {
        bt_page_t buf;
        const bt_node_t *node = &buf.node;
        int pos;

        if (!BTPP_IS_NODE(page))
            return -1;

        node = &buf.node;
        if (fbtree_readpage(fbt, page, &buf))
            return -1;

        pos = btn_bsearch(node, key);
        page = node->ptrs[pos];
    }

    return page;
}

int fbtree_fetch(fbtree_t *fbt, uint64_t key, sb_t *out)
{
    bt_page_t buf;
    int page, pos, len = 0;
    const bt_leaf_t *leaf;

    if (fbt->ismap)
        return btree_fetch(fbt->bt, key, out);

    page = RETHROW(fbtn_find_leaf(fbt, key));

    leaf = &buf.leaf;
    if (fbtree_readpage(fbt, page, &buf))
        return -1;

    pos = RETHROW(btl_findslot(leaf, key, NULL));

    do {
        int datalen;

        /* skip key */
        pos += 1 + 8;
        /* should flag error if there is no data length */
        if (unlikely(pos >= leaf->used))
            break;

        datalen = leaf->data[pos++];
        /* should flag error if there is not enough data */
        if (unlikely(pos + datalen > leaf->used))
            break;

        sb_add(out, leaf->data + pos, datalen);
        len += datalen;
        pos += datalen;

        if (pos >= leaf->used) {
            pos  = 0;
            if (BTPP_OFFS(leaf->next) == BTPP_NIL)
                break;
            if (fbtree_readpage(fbt, leaf->next, &buf) || leaf->used <= 0)
                break;
        }
    } while (btl_keycmp(key, leaf, pos) == CMP_EQUAL);

    return len;
}

int fbtree_fetch_range(fbtree_t *fbt, uint64_t kmin, uint64_t kmax,
                       struct bt_key_range *btkr)
{
    bt_page_t buf;
    int page, pos, len = 0;
    const bt_leaf_t *leaf;
    struct bt_key_range_rec btkrr = { .key = kmin };
    qv_t(btkrr) vec;
    sb_t out;

    if (fbt->ismap)
        return btree_fetch_range(fbt->bt, kmin, kmax, btkr);

    sb_init(&out);
    qv_init(btkrr, &vec);

    page = fbtn_find_leaf(fbt, kmin);
    if (page < 0)
        goto error;

    leaf = &buf.leaf;
    if (fbtree_readpage(fbt, page, &buf))
        goto error;

    btl_findslot(leaf, kmin, &pos);
    for (;;) {
        uint64_t key;
        int datalen;

        if (pos >= leaf->used) {
            pos  = 0;
            if (BTPP_OFFS(leaf->next) == BTPP_NIL)
                break;
            if (fbtree_readpage(fbt, leaf->next, &buf) || leaf->used <= 0)
                break;
        }

        /* skip key */
        key  = btl_getkey(leaf, pos);
        pos += 1 + 8;
        /* XXX: kmax may well be UINT64_MAX hence the underflow test */
        if (key > kmax || key < kmin)
            break;

        if (key != btkrr.key) {
            if (btkrr.dlen) {
                qv_append(btkrr, &vec, btkrr);
            }
            btkrr.key  = key;
            btkrr.dpos = out.len;
            btkrr.dlen = 0;
        }

        if (unlikely(pos >= leaf->used)) {
            /* should flag btree structure error: no data length */
            break;
        }

        datalen = leaf->data[pos++];
        if (unlikely(pos + datalen > leaf->used)) {
            /* should flag btree structure error: not enough data */
            break;
        }

        sb_add(&out, leaf->data + pos, datalen);
        btkrr.dlen += datalen;
        len        += datalen;
        pos        += datalen;
    }

    if (btkrr.dlen)
        qv_append(btkrr, &vec, btkrr);
    *btkr = (struct bt_key_range){
        .nkeys = vec.len,
        .keys  = vec.tab,
        { .data  = sb_detach(&out, NULL) },
    };
    return 0;

  error:
    sb_wipe(&out);
    qv_wipe(btkrr, &vec);
    return -1;
}

/*---------------- Debug interface ----------------*/

#if 0
static void hexdump(const byte *p, int n,
                    int (*fun)(FILE *fp, const char *fmt, ...), FILE *arg)
{
    char buf[257];
    int i = 0;

    while (n > 0) {
        if (i >= sizeof(buf) - 3) {
            buf[i] = '\0';
            fun(arg, "%s", buf);
            i = 0;
        }
        buf[i++] = __str_digits_lower[*p >> 4];
        buf[i++] = __str_digits_lower[*p & 15];
        p++;
        n--;
    }
    if (i) {
        buf[i] = '\0';
        fun(arg, "%s", buf);
    }
}
#endif

void btree_dump(btree_t *bt_pub,
                int (*fun)(FILE *fp, const char *fmt, ...), FILE *arg)
{
    struct btree_priv *bt = bt_pub->area;
    int32_t lmost, level;
    uint64_t key;

    bt_real_rlock(bt_pub);
    lmost = bt->root;

    for (level = bt->depth; level > 0; level--) {
        int32_t page = lmost;

        fun(arg, "====== level %d: nodes =====\n", level);
        while (BTPP_OFFS(page) != BTPP_NIL) {
            int i;
            const bt_node_t *node = MAP_CONST_NODE(bt, page);

            fun(arg, "%03d: node.L%d  [%3d/%d]",
                  BTPP_OFFS(page), level, node->nbkeys, BT_ARITY);

            if (BTPP_OFFS(node->next) == BTPP_NIL) {
                fun(arg, " next=nil");
            } else {
                fun(arg, " next=%03d", BTPP_OFFS(node->next));
            }
            fun(arg, " {");

            for (i = 0; i < node->nbkeys; i++) {
                fun(arg, " %03d %s",
                      BTPP_OFFS(node->ptrs[i]), bt_keystr(node->keys[i]));
            }
            fun(arg, " %03d", BTPP_OFFS(node->ptrs[i]));
            fun(arg, " }\n");
            page = node->next;
        }
        lmost = MAP_CONST_NODE(bt, lmost)->ptrs[0];
    }

    fun(arg, "====== level %d: leaves =====\n", level);
    {
        int32_t page = lmost;

        while (BTPP_OFFS(page) != BTPP_NIL) {
            const bt_leaf_t *leaf = MAP_CONST_LEAF(bt, page);
            int pos;

            fun(arg, "%03d: leaf  [%4d/%d] ",
                    BTPP_OFFS(page), leaf->used,
                    (int)sizeof(leaf->data));

            if (BTPP_OFFS(leaf->next) == BTPP_NIL) {
                fun(arg, "next=nil ");
            } else {
                fun(arg, "next=%03d ", BTPP_OFFS(leaf->next));
            }
            fun(arg, "{ ");

            for (pos = 0; pos < leaf->used; ) {

                if (pos) {
                    fun(arg, ", ");
                }
                if (leaf->data[pos] == 8) {
                    memcpy(&key, leaf->data + 1 + pos, 8);
                    fun(arg, "%s ", bt_keystr(key));
                } else {
                    fun(arg, "BUG(keylen=%d) ", leaf->data[pos]);
                }

                pos += 1 + leaf->data[pos];
#if 0
                hexdump(out, leaf->data + pos + 1, leaf->data[pos], fun, arg);
#endif
                fun(arg, "%d", leaf->data[pos]);
                pos += 1 + leaf->data[pos];
            }
            page = leaf->next;
            fun(arg, " }\n");
        }
    }
    bt_real_unlock(bt_pub);
}
