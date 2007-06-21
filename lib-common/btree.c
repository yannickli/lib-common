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

#include "btree.h"
#include "unix.h"

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
    int32_t flags; /* Never used... ? */

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
    int16_t  wrlock;    /**< holds the pid of the writer if any.           */
    /* fourth qword */
    int64_t  wrlockt;   /**< time associated to the lock                   */

    /* __future__: 512 - 4 qwords */
    uint64_t reserved_0[512 - 4]; /**< padding up to 4k                    */

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

/* link len pages like that:
   from + 1 -> ... -> from + NB_PAGES_GROW - 1 -> NIL

   spare out from.
 */
/* OG: what a braindead API, bugs are looming... If you really need
 * this code, don't make it a separate function with such
 * boobytrapped semantics
 */
static int32_t bt_append_to_freelist(btree_t *bt, int32_t from, int32_t len)
{
    int32_t i;

    assert (bt->area->freelist == 0);

    bt->area->freelist = from + 1;
    for (i = from + 1; i < from + len - 1; i++) {
        bt->area->pages[i].leaf.next = i + 1;
    }
    bt->area->nbpages += len;

    return from;
}

static int32_t bt_page_new(btree_t *bt)
{
#define NB_PAGES_GROW  1024

    int32_t res, i, from, len;

    /* OG: bogus test, empty freelist should not be 0, but BTPP_NIL */
    /* OG: the real issue is that BTPP_NIL should be 0 */
    if (!bt->area->freelist || bt->area->freelist == BTPP_NIL) {
        len = NB_PAGES_GROW;
        res = bt_real_truncate(bt, bt->size + len * 4096);
        if (res < 0) {
            if (!bt->area) {
                e_panic("Not enough memory !");
            }
            return res;
        }
        from = bt->area->nbpages;
        bt->area->nbpages += len;

        bt->area->freelist = from;
        for (i = from; i < from + len - 1; i++) {
            bt->area->pages[i].leaf.next = i + 1;
        }
        bt->area->pages[i].leaf.next = 0;
    }
    res = bt->area->freelist;
    bt->area->freelist = bt->area->pages[bt->area->freelist].leaf.next;
    bt->area->pages[res].leaf.next = BTPP_NIL;
    bt->area->pages[res].leaf.used = 0;

    return res;
}

static inline bt_page_t *vbt_deref(struct btree_priv *bt, int32_t ptr) {
    int page = BTPP_OFFS(ptr);
    if (page == BTPP_NIL || page > bt->nbpages)
        return NULL;
    return bt->pages + BTPP_OFFS(ptr);
}

static inline
const bt_page_t *bt_deref(const struct btree_priv *bt, int32_t ptr) {
    return vbt_deref((struct btree_priv *)bt, ptr);
}

#define MAP_NODE(bt, n)        (&vbt_deref(bt, n)->node)
#define MAP_LEAF(bt, n)        (&vbt_deref(bt, n)->leaf)
#define MAP_CONST_NODE(bt, n)  (&bt_deref(bt, n)->node)
#define MAP_CONST_LEAF(bt, n)  (&bt_deref(bt, n)->leaf)


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
btn_update(btree_t *bt, const intpair nodes[], int level, uint64_t key)
{
    while (++level <= bt->area->depth) {
        int i = nodes[level].i;
        bt_node_t *node = MAP_NODE(bt->area, nodes[level].page);

        assert (node);

        if (i >= node->nbkeys)
            break;
        btn_setk(node, i, key);
        if (i < node->nbkeys - 1)
            break;
    }
}

static uint64_t btl_get_maxkey(const bt_leaf_t *leaf);

static void btn_insert_aux(btree_t *bt, bt_node_t *node, int i, int32_t lpage, int32_t rpage)
{
    btn_shift(node, i + 1, i, node->nbkeys - i);
    node->nbkeys++;

    node->ptrs[i]     = lpage;
    node->ptrs[i + 1] = rpage;
    if (BTPP_IS_NODE(lpage)) {
        bt_node_t *lnode = MAP_NODE(bt->area, lpage);
        btn_setk(node, i, lnode->keys[lnode->nbkeys - 1]);
    } else {
        uint64_t key = btl_get_maxkey(MAP_LEAF(bt->area, lpage));
        btn_setk(node, i, key);
    }
    if (i + 1 < node->nbkeys) {
        if (BTPP_IS_NODE(rpage)) {
            bt_node_t *rnode = MAP_NODE(bt->area, rpage);
            btn_setk(node, i + 1, rnode->keys[rnode->nbkeys - 1]);
        } else {
            uint64_t key = btl_get_maxkey(MAP_LEAF(bt->area, rpage));
            btn_setk(node, i + 1, key);
        }
    }
}

/* OG: bad name for a function that inserts a node after the current
 * one at level (level+1)
 */
static void btn_insert(btree_t *bt, intpair nodes[], int level, int32_t rpage)
{
    int32_t lpage = nodes[level++].page;
    bt_node_t *node;
    bt_node_t *sibling;

    if (level > bt->area->depth) {
        /* Need a new level */
        bt->area->depth++;
        // XXX: check if we got a new page !
        bt->area->root = bt_page_new(bt) | BTPP_NODE_MASK;
        node = MAP_NODE(bt->area, bt->area->root);
        node->next   = BTPP_NIL;
        node->nbkeys = 0;

        btn_insert_aux(bt, node, 0, lpage, rpage);
        return;
    }

    node = MAP_NODE(bt->area, nodes[level].page);
    sibling = MAP_NODE(bt->area, node->next);

#if 1
    if (node->nbkeys == BT_ARITY && sibling && sibling->nbkeys < BT_ARITY - 1) {
        btn_shift(sibling, 1, 0, sibling->nbkeys);
        sibling->nbkeys++;
        sibling->ptrs[0] = node->ptrs[node->nbkeys - 1];
        btn_setk(sibling, 0, node->keys[node->nbkeys - 1]);
        /* Shift overflow page */
        /* OG: if node has a sibling, it should not have one */
        //*btn_shift(node, node->nbkeys - 1, node->nbkeys, 0);
        node->nbkeys--;

        /* Update max key up the tree */
        btn_update(bt, nodes, level, node->keys[node->nbkeys - 1]);

        if (nodes[level].i >= node->nbkeys) {
            btn_insert_aux(bt, sibling, nodes[level].i - node->nbkeys, lpage, rpage);
            return;
        }
    }
#endif

    if (node->nbkeys < BT_ARITY) {
        btn_insert_aux(bt, node, nodes[level].i, lpage, rpage);
        if (nodes[level].i == node->nbkeys - 1) {
            btn_update(bt, nodes, level, node->keys[node->nbkeys - 1]);
        }
        return;
    }

    {
        int32_t npage;

        // XXX: check if we got a new page !
        npage   = bt_page_new(bt) | BTPP_NODE_MASK;
        /* remap pages because bt_page_new may have moved bt->area */
        node    = MAP_NODE(bt->area, nodes[level].page);
        sibling = MAP_NODE(bt->area, npage);

        /* split node data between node and new node */
        *sibling = *node;
        node->next   = npage & ~BTPP_NODE_MASK;
        node->nbkeys = BT_ARITY / 2;

        btn_shift(sibling, 0, BT_ARITY / 2, (BT_ARITY + 1) / 2);
        sibling->nbkeys = (BT_ARITY + 1) / 2;

        /* Update max key up the tree */
        btn_update(bt, nodes, level, node->keys[node->nbkeys - 1]);

        if (nodes[level].i < node->nbkeys) {
            btn_insert_aux(bt, node, nodes[level].i, lpage, rpage);
        } else {
            btn_insert_aux(bt, sibling, nodes[level].i - node->nbkeys,
                           lpage, rpage);
        }
        btn_insert(bt, nodes, level, npage);
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

    if (bt->area->wrlock) {
        struct timeval tv;

        if (pid_get_starttime(bt->area->wrlock, &tv))
            return -1;

        if (bt->area->wrlockt != (((int64_t)tv.tv_sec << 32) | tv.tv_usec))
            return -1;

        dofix = 0;
    }

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

    if (btree_check_integrity(bt, dofix, NULL, NULL)) {
        return -1;
    }

    return did_a_fix;
}

static const char *bt_keystr(uint64_t key)
{
    static char buf[64];
    static int pos;

    pos ^= sizeof(buf) / 2;

    if (key > 0xffffffff && (key & 0xffffffff) < 0x10000) {
        snprintf(buf + pos, sizeof(buf) / 2, "%d:%d",
                 (int)(key >> 32), (int)(key & 0xffffffff));
    } else {
        snprintf(buf + pos, sizeof(buf) / 2, "%04llx",
                 (long long)key);
    }
    return buf + pos;
}

/* OG: Should check node/leaf ptr integrity */

static int void_printf(FILE *fp, const char *fmt, ...)
{
    return 0;
}

#define MAX_UINT64_T 0xFFFFFFFFFFFFFFFFULL

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
        fun(arg, "%03d (node L%d): cannot map\n",
               BTPP_OFFS(page), level);
        return -1;
    }
    if (node->nbkeys <= 0) {
        fun(arg, "%03d (node L%d): invalid nbkeys=%d\n",
               BTPP_OFFS(page), level,
               node->nbkeys);
        return -1;
    }
    for (i = 0; i < node->nbkeys; i++) {
        if (node->keys[i] > maxkey) {
            fun(arg, "%03d (node L%d): key[%d/%d]=%s > maxkey=%s\n",
                   BTPP_OFFS(page), level,
                   i, node->nbkeys, bt_keystr(node->keys[i]),
                   bt_keystr(maxkey));
            return 1;
        }
        if (i && node->keys[i] < node->keys[i - 1]) {
            fun(arg, "%03d (node L%d): key[%d/%d]=%s < key[%d]=%s\n",
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
            fun(arg, "%03d (node L%d): check_page failed, key[%d/%d]=%s ptr=%03d\n",
                   BTPP_OFFS(page), level,
                   i, node->nbkeys, bt_keystr(node->keys[i]),
                   BTPP_OFFS(node->ptrs[i]));
            return 1;
        }
    }
    if (BTPP_OFFS(node->next) != BTPP_NIL && maxkey != node->keys[i - 1]) {
        fun(arg, "%03d (node L%d): key[%d/%d]=%s != maxkey=%s\n",
               BTPP_OFFS(page), level,
               i - 1, node->nbkeys, bt_keystr(node->keys[i - 1]),
               bt_keystr(maxkey));
        return 1;
    }
    if (next != -1 && BTPP_OFFS(node->next) != BTPP_OFFS(next)) {
        fun(arg, "%03d (node L%d): node->next=%03d != next=%03d\n",
               BTPP_OFFS(page), level,
               BTPP_OFFS(node->next), BTPP_OFFS(next));
        return 1;
    }
    if (BTPP_OFFS(node->next) == BTPP_NIL) {
        if (maxkey != MAX_UINT64_T) {
            fun(arg, "%03d (node L%d): non rightmost page has no next\n",
                   BTPP_OFFS(page), level);
            return 1;
        }
        if (bt_check_page(bt, level - 1, node->ptrs[i], maxkey, BTPP_NIL, fun, arg)) {
            fun(arg, "%03d (node L%d): check_page failed, ptr[%d/%d]=%03d\n",
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
    int32_t root = bt->area->root;

    if (!fun) {
        fun = void_printf;
    }

//    fun(arg, "root: 0x%08X\n", root);
    return bt_check_page(bt, bt->area->depth, root, MAX_UINT64_T, BTPP_NIL, fun, arg);
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

    bt = bt_real_open(path, flags & ~(O_NONBLOCK));
    if (!bt) {
        e_trace(2, "Could not open bt on %s: %m", path);
        return NULL;
    }
    if (!(flags & O_NONBLOCK)) {
        res = btree_fsck(bt, O_ISWRITE(flags));
        if (res < 0) {
            btree_close(&bt);
            errno = EUCLEAN;
            return NULL;
        }
    }
    if (O_ISWRITE(flags)) {
        struct timeval tv;
        pid_t pid = getpid();

        if (bt->area->wrlock) {
            btree_close(&bt);
            errno = EDEADLK;
            return NULL;
        }

        bt->area->wrlock = pid;
        msync(bt->area, bt->size, MS_SYNC);
        if (bt->area->wrlock != pid) {
            btree_close(&bt);
            errno = EDEADLK;
            return NULL;
        }
        pid_get_starttime(pid, &tv);
        bt->area->wrlockt = ((int64_t)tv.tv_sec << 32) | tv.tv_usec;
        msync(bt->area, bt->size, MS_SYNC);
    }

    return bt;
}

btree_t *btree_creat(const char *path)
{
    struct timeval tv;
    btree_t *bt;
    pid_t pid = getpid();

    bt = bt_real_creat(path, sizeof(bt_page_t) * (BT_INIT_NBPAGES + 1));
    if (!bt)
        return NULL;

    bt->area->magic    = ISBT_MAGIC.i;
    bt->area->major    = 1;
    bt->area->minor    = 0;
    bt->area->root     = 0;
    bt->area->nbpages  = 0;
    bt->area->freelist = 0;
    bt->area->depth    = 0;
    bt_append_to_freelist(bt, 0, BT_INIT_NBPAGES);

    bt->area->pages[0].node.next = BTPP_NIL;
    pid_get_starttime(pid, &tv);
    bt->area->wrlock = pid;
    msync(bt->area, bt->size, MS_SYNC);
    if (bt->area->wrlock != pid) {
        btree_close(&bt);
        errno = EDEADLK;
        return NULL;
    }
    bt->area->wrlock  = pid;
    bt->area->wrlockt = ((int64_t)tv.tv_sec << 32) | tv.tv_usec;
    msync(bt->area, bt->size, MS_SYNC);
    return bt;
}

void btree_close(btree_t **bt)
{
    if (*bt) {
        if ((*bt)->area->wrlock && !(*bt)->ro) {
            pid_t pid = getpid();

            if ((*bt)->area->wrlock == pid) {
                struct timeval tv;

                pid_get_starttime(pid, &tv);
                if ((*bt)->area->wrlockt ==
                    (((int64_t)tv.tv_sec << 32) | tv.tv_usec))
                {
                    msync((*bt)->area, (*bt)->size, MS_SYNC);
                    (*bt)->area->wrlock  = 0;
                    (*bt)->area->wrlockt = 0;
                } else {
                    /* OG: if same pid but different starttime, should
                     * unlock as well!
                     */
                }
            }
        }
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
        int i;

        if (!BTPP_IS_NODE(page))
            return -1;

        node = MAP_CONST_NODE(bt, page);
        if (!node)
            return -1;

        i = btn_bsearch(node, key);
        if (nodes) {
            nodes[level].page = page;
            nodes[level].i    = i;
        }
        page = node->ptrs[i];
    }

    if (nodes) {
        nodes[0].page = page;
    }
    return page;
}


/****************************************************************************/
/* code specific to the leaves                                              */
/****************************************************************************/

static inline enum sign
btl_keycmp(uint64_t key, const bt_leaf_t *leaf, int pos)
{
    union {
        uint64_t key;
        byte c[8];
    } u;
    assert (0 <= pos && pos + 1 + 8 + 1 <= leaf->used);
    assert (leaf->data[pos] == 8);
    memcpy(u.c, leaf->data + pos + 1, 8);

    return CMP(key, u.key);
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
        pos += leaf->data[pos] + 1; /* skip data */
        assert (pos <= leaf->used);
    }

    if (slot) {
        *slot = pos;
    }
    return -1;
}

int btree_fetch(const btree_t *bt, uint64_t key, blob_t *out)
{
    int page, pos, len = 0;
    const bt_leaf_t *leaf;

    page = btn_find_leaf(bt->area, key, NULL);
    leaf = MAP_CONST_LEAF(bt->area, page);
    if (!leaf)
        return -1;

    pos = btl_findslot(leaf, key, NULL);
    if (pos < 0)
        return -1;

    do {
        int datalen;

        /* skip key */
        pos += 1 + 8;
        /* should flag error if there is no data length */
        if (unlikely(pos + 1 >= leaf->used))
            break;

        datalen = leaf->data[pos++];
        /* should flag error if there is not enough data */
        if (unlikely(pos + datalen > leaf->used))
            break;

        blob_append_data(out, leaf->data + pos, datalen);
        len += datalen;
        pos += datalen;

        if (pos >= leaf->used) {
            leaf = MAP_CONST_LEAF(bt->area, leaf->next);
            if (!leaf)
                break;
            assert (leaf->used > 0);
            pos  = 0;
        }
    } while (btl_keycmp(key, leaf, pos) == CMP_EQUAL);

    return len;
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

int btree_push(btree_t *bt, uint64_t key, const byte *data, int dlen)
{
    bool reuse;
    int32_t page, rpage, slot, need;
    int pos;
    intpair *nodes;
    bt_leaf_t *lleaf, *rleaf, *nleaf;
    byte *p;

    if (dlen < 0) {
        /* invalid data length */
        return -1;
    }

  restart:
    nodes = p_alloca(intpair, bt->area->depth + 1);
    page  = btn_find_leaf(bt->area, key, nodes);
    lleaf = MAP_LEAF(bt->area, page);

    if (!lleaf) {
        /* Internal structure error */
        return -1;
    }

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
        if (lleaf->data[pos + 1 + 8] + dlen < 256) {
            reuse = true;
            need = dlen;
            if (need == 0) {
                /* no data to add to already existing chunk */
                return 0;
            }
        }
    }

    assert (0 <= slot && slot <= lleaf->used);

    if (need + lleaf->used > ssizeof(lleaf->data)) {
        /* Not enough room in target page: need to shift some data to
         * next page.
         */
        rpage = lleaf->next;
        rleaf = MAP_LEAF(bt->area, rpage);
        if (!rleaf) {
            /* No next page: allocate a new empty overflow page */
            rpage = bt_page_new(bt);
            if (rpage < 0)
                return -1;

            /* remap pages because bt_page_new may have moved bt->area */
            lleaf = MAP_LEAF(bt->area, page);
            rleaf = MAP_LEAF(bt->area, rpage);
            rleaf->next = lleaf->next;
            lleaf->next = rpage;

            btn_insert(bt, nodes, 0, rpage);

            /* Restart because target page may be the newly allocated
             * one, or depth may have changed, or path to page may have
             * changed...
             */
            goto restart;
        }

        {
            int oldpos, shift;

            /* OG: find the least chunk to shift to the next page */
            for (pos = oldpos = slot; pos < lleaf->used && pos + need <= ssizeof(lleaf->data);) {
                oldpos = pos;
                assert (pos + 1 + 8 + 1 <= lleaf->used);
                pos += 1 + 8 + 1 + lleaf->data[pos + 1 + 8];
                assert (pos <= lleaf->used);
            }

            /* oldpos is the offset of the chunk to shift to the next
             * page
             */

            shift = lleaf->used - oldpos;
            assert (0 <= shift && shift <= lleaf->used);

            if (shift + rleaf->used + (slot == oldpos ? need : 0) > ssizeof(rleaf->data)) {
                /* allocate a new page and split data in [lleaf:rleaf] on
                 * 3 pages [lleaf:nleaf:rleaf]
                 */
                int32_t npage;
                uint64_t maxkey, maxkey2;
                int keypos, keypos2, pos2;

                npage = bt_page_new(bt);
                if (npage < 0)
                    return -1;

                /* remap pages because bt_page_new may have moved bt->area */
                lleaf = MAP_LEAF(bt->area, page);
                rleaf = MAP_LEAF(bt->area, rpage);
                nleaf = MAP_LEAF(bt->area, npage);

                if (!lleaf || !rleaf || !nleaf)
                    return -1;

                for (keypos = pos = 0; pos < lleaf->used && pos < ssizeof(lleaf->data) * 2 / 3;) {
                    keypos = pos;
                    pos += 1 + 8;
                    pos += 1 + lleaf->data[pos];
                }
                if (pos >= lleaf->used)
                    return -1;

                memcpy(&maxkey, &lleaf->data[keypos + 1], 8);

                for (keypos2 = pos2 = 0; pos2 < rleaf->used && pos2 < ssizeof(rleaf->data) / 3;) {
                    keypos2 = pos2;
                    pos2 += 1 + 8;
                    pos2 += 1 + rleaf->data[pos2];
                }

                memcpy(&maxkey2, &rleaf->data[keypos2 + 1], 8);

                nleaf->next = lleaf->next;
                lleaf->next = npage;

                memcpy(nleaf->data, lleaf->data + pos, lleaf->used - pos);
                nleaf->used = lleaf->used - pos;
                lleaf->used = pos;

                memcpy(nleaf->data + nleaf->used, rleaf->data, pos2);
                memmove(rleaf->data, rleaf->data + pos2, rleaf->used - pos2);
                nleaf->used += pos2;
                rleaf->used -= pos2;

                /* If this insert fails, the index is corrupted */
                btn_insert(bt, nodes, 0, npage);

                goto restart;
            }

            memmove(rleaf->data + shift, rleaf->data, rleaf->used);
            memcpy(rleaf->data, lleaf->data + oldpos, shift);
            lleaf->used -= shift;
            rleaf->used += shift;

            /* Update max key up the tree */
            btn_update(bt, nodes, 0, btl_get_maxkey(lleaf));

            if (oldpos == slot) {
                slot  = 0;
                page  = rpage;
                lleaf = rleaf;
                nodes[0].page = page;
                nodes[1].i += 1;
            }
        }
    }

    p = lleaf->data;

    pos = slot;

    assert (lleaf->used + need <= ssizeof(lleaf->data));

    if (reuse) {
        /* skip key */
        pos += 1 + 8 + 1;

        assert (pos <= lleaf->used);
        memmove(p + pos + need, p + pos, lleaf->used - pos);

    } else {

        assert (pos <= lleaf->used);
        memmove(p + pos + need, p + pos, lleaf->used - pos);

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
    return 0;
}

#if 0
#include "strconv.h"
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

void btree_dump(const btree_t *bt_pub,
                int (*fun)(FILE *fp, const char *fmt, ...), FILE *arg)
{
    struct btree_priv *bt = bt_pub->area;
    int32_t lmost, level;
    uint64_t key;

    lmost = bt->root;

    for (level = bt->depth; level > 0; level--) {
        int32_t page = lmost;

        fun(arg, "====== level %d: nodes =====\n", level);
        while (BTPP_OFFS(page) != BTPP_NIL) {
            int i;
            const bt_node_t *node = MAP_CONST_NODE(bt, page);

            fun(arg, "node %03d  [%3d/%d]",
                  BTPP_OFFS(page), node->nbkeys, BT_ARITY);

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

            fun(arg, "leaf %03d  [%4d/%d] ",
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
}


/****************************************************************************/
/* FILE *  read interface                                                   */
/****************************************************************************/

struct fbtree_t {
    FILE *f;
    struct btree_priv priv;
};

fbtree_t *fbtree_open(const char *path)
{
    fbtree_t *fbt = p_new(fbtree_t, 1);

    fbt->f = fopen(path, "rb");
    if (!fbt->f) {
        p_delete(&fbt);
        return NULL;
    }

    if (fread(&fbt->priv, sizeof(fbt->priv), 1, fbt->f) != 1) {
        fbtree_close(&fbt);
        return NULL;
    }

    /* TODO: fsck it */
    return fbt;
}

void fbtree_close(fbtree_t **fbt)
{
    if (*fbt) {
        p_fclose(&(*fbt)->f);
        p_delete(fbt);
    }
}

static int fbtree_readpage(fbtree_t *fbt, int32_t page, bt_page_t *buf)
{
    if (fseek(fbt->f, (BTPP_OFFS(page) + 1) * ssizeof(*buf), SEEK_SET))
        return -1;

    if (fread(buf, sizeof(*buf), 1, fbt->f) != 1)
        return -1;

    return 0;
}

static int32_t fbtn_find_leaf(fbtree_t *fbt, uint64_t key)
{
    int32_t page = fbt->priv.root;
    int level;

    for (level = fbt->priv.depth; level > 0; level--) {
        bt_page_t node;
        int i;

        if (!BTPP_IS_NODE(page))
            return -1;

        if (fbtree_readpage(fbt, page, &node))
            return -1;

        i = btn_bsearch(&node.node, key);
        page = node.node.ptrs[i];
    }

    return page;
}

int fbtree_fetch(fbtree_t *fbt, uint64_t key, blob_t *out)
{
    int page, pos, len = 0;
    bt_page_t leaf;

    page = fbtn_find_leaf(fbt, key);
    if (page < 0)
        return -1;

    if (fbtree_readpage(fbt, page, &leaf))
        return -1;

    pos = btl_findslot(&leaf.leaf, key, NULL);
    if (pos < 0)
        return -1;

    do {
        int slot;

        pos += 1 + leaf.leaf.data[pos];
        if (unlikely(pos + 1 >= leaf.leaf.used))
            break;

        slot = leaf.leaf.data[pos++];
        if (unlikely(pos + slot > leaf.leaf.used))
            break;

        blob_append_data(out, leaf.leaf.data + pos, slot);
        len += slot;
        pos += slot;

        while (pos >= leaf.leaf.used) {
            pos  = 0;
            if (fbtree_readpage(fbt, leaf.leaf.next, &leaf))
                return len;
        }
    } while (!leaf.leaf.data[pos] || !btl_keycmp(key, &leaf.leaf, pos));

    return len;
}

