/**************************************************************************/
/*                                                                        */
/*  Copyright (C) 2004-2019 INTERSEC SA                                   */
/*                                                                        */
/*  Should you receive a copy of this source code, you must check you     */
/*  have a proper, written authorization of INTERSEC to hold it. If you   */
/*  don't have such an authorization, you must DELETE all source code     */
/*  files in your possession, and inform INTERSEC of the fact you obtain  */
/*  these files. Should you not comply to these terms, you can be         */
/*  prosecuted in the extent permitted by applicable law.                 */
/*                                                                        */
/**************************************************************************/

#ifndef IS_LIB_COMMON_STR_OUTBUF_H
#define IS_LIB_COMMON_STR_OUTBUF_H

#include "core.h"
#include "container-htlist.h"

/****************************************************************************/
/* Outbuf                                                                   */
/****************************************************************************/

typedef struct outbuf_t {
    int      length;
    int      sb_trailing;
    sb_t     sb;
    htlist_t chunks_list;
} outbuf_t;

void ob_check_invariants(outbuf_t *ob) __leaf;

static inline outbuf_t *ob_init(outbuf_t *ob)
{
    ob->length      = 0;
    ob->sb_trailing = 0;
    htlist_init(&ob->chunks_list);
    sb_init(&ob->sb);
    return ob;
}
#define ob_inita(ob, sb_size) \
    ({ ob_init(ob); sb_inita(&ob->sb, sb_size); })

void ob_wipe(outbuf_t *ob) __leaf;
GENERIC_NEW(outbuf_t, ob);
GENERIC_DELETE(outbuf_t, ob);
void ob_merge(outbuf_t *dst, outbuf_t *src) __leaf;
void ob_merge_wipe(outbuf_t *dst, outbuf_t *src) __leaf;
void ob_merge_delete(outbuf_t *dst, outbuf_t **src) __leaf;
static inline bool ob_is_empty(const outbuf_t *ob)
{
    return ob->length == 0;
}

int ob_write(outbuf_t *ob, int fd) __leaf;
int ob_xread(outbuf_t *ob, int fd, int size) __leaf;


/****************************************************************************/
/* Chunks                                                                   */
/****************************************************************************/

#define OUTBUF_CHUNK_MIN_SIZE    (16 << 10)

enum outbuf_on_wipe {
    OUTBUF_DO_NOTHING,
    OUTBUF_DO_FREE,
    OUTBUF_DO_MUNMAP,
};

typedef struct outbuf_chunk_t {
    htnode_t  chunks_link;
    int       length;
    int       offset;
    int       sb_leading;
    int       on_wipe;
    union {
        const void    *p;
        const uint8_t *b;
        void          *vp;
    } u;
} outbuf_chunk_t;
void ob_chunk_wipe(outbuf_chunk_t *obc) __leaf;
GENERIC_DELETE(outbuf_chunk_t, ob_chunk);

static inline void ob_add_chunk(outbuf_t *ob, outbuf_chunk_t *obc)
{
    htlist_add_tail(&ob->chunks_list, &obc->chunks_link);
    ob->length += (obc->length - obc->offset);
    obc->sb_leading = ob->sb_trailing;
    ob->sb_trailing = 0;
}

static inline sb_t *outbuf_sb_start(outbuf_t *ob, int *oldlen)
{
    *oldlen = ob->sb.len;
    return &ob->sb;
}

static inline void outbuf_sb_end(outbuf_t *ob, int oldlen)
{
    ob->sb_trailing += ob->sb.len - oldlen;
    ob->length      += ob->sb.len - oldlen;
}

#define OB_WRAP(sb_fun, ob, ...) \
    do {                                             \
        outbuf_t *__ob = (ob);                       \
        int curlen = __ob->sb.len;                   \
                                                     \
        sb_fun(&__ob->sb, ##__VA_ARGS__);            \
        __ob->sb_trailing += __ob->sb.len - curlen;  \
        __ob->length      += __ob->sb.len - curlen;  \
    } while (0)

#define ob_add(ob, data, len)       OB_WRAP(sb_add,   ob, data, len)
#define ob_adds(ob, data)           OB_WRAP(sb_adds,  ob, data)
#define ob_addf(ob, fmt, ...)       OB_WRAP(sb_addf,  ob, fmt, ##__VA_ARGS__)
#define ob_addvf(ob, fmt, ap)       OB_WRAP(sb_addvf, ob, fmt, ap)
#define ob_addsb(ob, sb)            OB_WRAP(sb_addsb, ob, sb)
#define ob_add_urlencode(ob, s, l)  OB_WRAP(sb_add_urlencode, ob, s, l)
#define ob_adds_urlencode(ob, s)    OB_WRAP(sb_adds_urlencode, ob, s)

/* XXX: invalidated as sonn as the outbuf is consumed ! */
static inline int ob_reserve(outbuf_t *ob, unsigned len)
{
    int res = ob->sb.len;

    sb_growlen(&ob->sb, len);
    ob->sb_trailing += len;
    ob->length      += len;
    return res;
}

static inline
void ob_add_memchunk(outbuf_t *ob, const void *ptr, int len, bool is_const)
{
    if (len <= OUTBUF_CHUNK_MIN_SIZE) {
        ob_add(ob, ptr, len);
        if (!is_const)
            ifree((void *)ptr, MEM_LIBC);
    } else {
        outbuf_chunk_t *obc = p_new(outbuf_chunk_t, 1);

        obc->u.p    = ptr;
        obc->length = len;
        if (!is_const)
            obc->on_wipe = OUTBUF_DO_FREE;
        ob_add_chunk(ob, obc);
    }
}

static inline void ob_add_memmap(outbuf_t *ob, void *map, int len)
{
    if (len <= OUTBUF_CHUNK_MIN_SIZE) {
        ob_add(ob, map, len);
        munmap(map, len);
    } else {
        outbuf_chunk_t *obc = p_new(outbuf_chunk_t, 1);

        obc->u.p     = map;
        obc->length  = len;
        obc->on_wipe = OUTBUF_DO_MUNMAP;
        ob_add_chunk(ob, obc);
    }
}

int ob_add_file(outbuf_t *ob, const char *file, int size) __leaf;

#endif
