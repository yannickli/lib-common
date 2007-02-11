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

#include "mmappedfile.h"
#include "btree.h"

#define BT_ARITY          340  /**< L constant in the b-tree terminology */
#define BT_INIT_NBPAGES  1024  /**< initial number of pages in the btree */
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
    int64_t flags;

    int32_t nbkeys;
    int32_t ptrs[BT_ARITY + 1];
    byte    keys[8][BT_ARITY];
} bt_node_t;

typedef struct bt_leaf_t {
    int32_t next;
    int16_t used;
    int16_t offs;
    byte data[4096 - 4 * 2];
} bt_leaf_t;

typedef union bt_page_t {
    bt_node_t node;
    bt_leaf_t leaf;
} bt_page_t;

struct bt_t {
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
    uint32_t reserved_2;

    /* __future__: 512 - 3 qwords */
    uint64_t reserved_0[512 - 3]; /**< padding up to 4k                    */

    bt_page_t pages[];
};
struct btree_t MMFILE_ALIAS(struct bt_t);
MMFILE_FUNCTIONS(btree_t, bt_real);


/****************************************************************************/
/* usual functions                                                          */
/****************************************************************************/

static bt_node_t *bt_node_init(bt_node_t *node)
{
    node->nbkeys = 0;
    return node;
}

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
/* data structure                                                           */
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

/* searches the _last_ pos in the node keys array where key <= keys[pos] */
static int bt_node_findkey(const bt_node_t *node, const byte key[8])
{
    int l = 0, r = node->nbkeys;

    while (r > l) {
        int i = (l + r) / 2;

        switch (SIGN(memcmp(key, node->keys[i], 8))) {
          case NEGATIVE: /* key < node->keys[i] */
            r = i;
            continue;

          case ZERO:     /* XXX asumes unicity of the key in keys[] */
            return i;

          case POSITIVE: /* key > node->keys[i] */
            l = i + 1;
        }
    }

    return l;
}

static int bt_leaf_datalen(const bt_leaf_t *leaf, int pos)
{
    int datalen = 0;
    int trail = leaf->data[pos] & 0x80 ? 4 : 1;

    if (pos + trail >= leaf->used)
        return -1;

    datalen = leaf->data[pos++] & 0x7f;
    while (--trail) {
        datalen <<= 8;
        datalen  |= leaf->data[pos++];
    }

    return datalen;
}

static int bt_leaf_skipslot(const bt_leaf_t *leaf, int pos)
{
    int datalen;

    pos += leaf->data[pos] + 1;
    if (pos >= leaf->used)
        return -1;

    datalen = bt_leaf_datalen(leaf, pos);
    return pos + datalen >= leaf->used ? -1 : pos + datalen;
}

static int bt_leaf_findslot(const bt_leaf_t *leaf, const byte *key, uint8_t n)
{
    int pos = leaf->offs;

    while (pos >= 0 && pos < leaf->used) {
        if (pos + 1 + n >= leaf->used) /* page overflow */
            return -1;

        if (leaf->data[pos] != n || memcmp(key, leaf->data + pos + 1, n)) {
            pos = bt_leaf_skipslot(leaf, pos);
            continue;
        }

        return pos + 1 + n;
    }

    return -1;
}

static inline bt_page_t *vbtree_deref(struct bt_t *bt, int32_t ptr)
{
    int page = BTPP_OFFS(ptr);
    return page == BTPP_NIL || page > bt->nbpages ? NULL : &bt->pages[BTPP_OFFS(ptr)];
}

static inline bt_page_t *btree_deref(const struct bt_t *bt, int32_t ptr)
{
    return vbtree_deref((struct bt_t *)bt, ptr);
}

static int32_t btree_find_page(const struct bt_t *bt, const byte *key, uint8_t n)
{
    int32_t page = bt->root;

    /* TODO: deal with 'continuation' bit to follow up into a new b-tree for
             keys > 1 segment */
    while (BTPP_IS_NODE(page)) {
        const bt_node_t *node = &btree_deref(bt, page)->node;
        int keypos;

        if (!node)
            return -1;

        if (n < 8) {
            byte tmp[8] = { 0, 0, 0, 0, 0, 0, 0 };

            memcpy(tmp, key, MIN(8, n));
            keypos = bt_node_findkey(node, tmp);
        } else {
            keypos = bt_node_findkey(node, key);
        }

        page = node->ptrs[keypos];
    }

    return page;
}

int btree_fetch(const btree_t *bt, const byte *key, int n, blob_t *out)
{
    int page, len, pos, toread;
    bt_leaf_t *leaf;

    page  = btree_find_page(bt->area, key, n);
    leaf  = &vbtree_deref(bt->area, page)->leaf;
    if (!leaf)
        return -1;

    pos   = bt_leaf_findslot(leaf, key, n);
    len   = bt_leaf_datalen(leaf, pos);
    if (len < 0)
        return -1;
    pos  += leaf->data[pos] & 0x80 ? 4 : 1;

    blob_ensure_avail(out, len);
    blob_append_data(out, leaf->data + pos, MIN(len, leaf->used - pos));
    for (toread = len - (leaf->used - pos); toread > 0; ) {
        int chunk;

        leaf  = &vbtree_deref(bt->area, leaf->next)->leaf;
        if (!leaf) {
            blob_kill_last(out, len - toread);
            return -1;
        }

        chunk = MIN(toread, leaf->used);
        blob_append_data(out, leaf->data, chunk);
        toread -= chunk;
    }

    return len;
}

static void btree_putlen(bt_leaf_t *leaf, int pos, int len, int width)
{
    byte *p = leaf->data + pos;
    if (width  == 1) {
        assert (!(len & ~127));
        p[0] = len;
    } else {
        p[0] = 0x80 | (len >> 24);
        p[1] = len >> 16;
        p[2] = len >>  8;
        p[3] = len >>  0;
    }
}

int btree_append(btree_t *bt, const byte *key, int n,
                 const byte *data, int dlen)
{
    int page, pos, len, len2, szl, szl2;
    bt_leaf_t *leaf, *rleaf;
    byte *p;

    if (dlen <= 0)
        return dlen < 0 ? -1 : 0;

    page  = btree_find_page(bt->area, key, n);
    leaf  = &btree_deref(bt->area, page)->leaf;
    if (!leaf)       /* TODO: UNIMPLEMENTED */
        return -1;

    pos   = bt_leaf_findslot(leaf, key, n);
    len   = bt_leaf_datalen(leaf, pos);
    if (len < 0)
        return -1;   /* TODO: UNIMPLEMENTED */

    len2  = len + dlen;
    szl   = leaf->data[pos] & 0x80 ? 4 : 1;
    szl2  = len2 >= 0x80 ? 4 : szl;
    p     = leaf->data + pos;

    rleaf = &vbtree_deref(bt->area, leaf->next)->leaf;

    /* CASE 1: we have enough space locally {{{ */
    if (pos + szl2 + len + dlen < ssizeof(leaf->data)) {
        /*
           pos
            +-szl-+---len-----+xxxxxxxx+........]
            |     |           |\        \
            |     \           \  \        \
            +-szl2-+-----------oooo+xxxxxxxx+...]
         */
        if (pos + szl + len < leaf->used) {
            memmove(p + szl2 + len2, p + szl + len,
                    leaf->used - szl - len - pos);
        }
        memcpy(p + szl2 + len, data, dlen);

        if (szl != szl2) {
            memmove(p + szl2, p + szl, len);
        }
        btree_putlen(leaf, pos, len2, szl2);

        leaf->used += szl2 - szl + dlen;
        return 0;
    }
    /* }}} */
    /* CASE 2: we have enough space locally + right page {{{ */
    if (rleaf
    &&  pos + szl2 < ssizeof(leaf->data) /* DO NOT split sizes accross pages */
    &&  szl2 - szl + dlen < 2 * ssizeof(leaf->data) - leaf->used - rleaf->used)
    {
        /*
           pos                  used                       rl.pos    rl.used
            +-szl-+---------------+......]  ===> [=====------+xxxxxxxxxx+.........]
            |     |               |    ___________/  /      / \          \
            |     \               \   /            /      /     \          \
            +-szl2-+----------------=====]  ===> [-------oooooooo+xxxxxxxxxx+.....]
                                    <.lsh>               <.dlen..>
                                                 <....rlen.......>
         */
        int rlen = dlen + szl2 - szl - (ssizeof(leaf->data) - leaf->used);
        int lsh  = rleaf->offs - (rlen - dlen);

        if (rlen > rleaf->offs) {
            memmove(rleaf->data + rlen, rleaf->data + rleaf->offs,
                    rleaf->used - rleaf->offs);
        }

        if (EXPECT_TRUE(lsh < 0)) {
            memmove(rleaf->data + (-lsh), rleaf->data, rleaf->offs);
            memcpy(rleaf->data, leaf->data + leaf->used - (-lsh), -lsh);
        }
        if (EXPECT_FALSE(lsh > 0)) {
            memcpy(leaf->data + ssizeof(leaf->data) - lsh, rleaf->data, lsh);
            memmove(rleaf->data, rleaf->data + lsh, rleaf->offs);
        }

        if (szl != szl2) {
            memmove(p + szl2, p + szl, len - MIN(0, lsh));
        }
        btree_putlen(leaf, pos, len2, szl2);

        if (rleaf->offs < lsh) {
            int lpart = lsh - rleaf->offs;

            memcpy(leaf->data + ssizeof(leaf->data) - lpart, data, lpart);
            memcpy(rleaf->data, data + dlen - lpart, dlen - lpart);
        } else {
            memcpy(rleaf->data + rleaf->offs - lsh, data, dlen);
        }

        if (rlen < rleaf->offs) {
            memmove(rleaf->data + rlen, rleaf->data + rleaf->offs,
                    rleaf->used - rleaf->offs);
        }

        leaf->used   = ssizeof(leaf->data);
        rleaf->used += rlen - rleaf->offs;
        rleaf->offs  = rlen;
        return 0;
    }
    /* }}} */

    /* TODO: UNIMPLEMENTED */
    return -1;
}
