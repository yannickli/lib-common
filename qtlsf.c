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

/* allocator based on
 * http://rtportal.upv.es/rtmalloc/files/tlsf_paper_spe_2007.pdf
 */

#include "arith.h"
#include "container.h"
#include "qtlsf.h"

/*
 * Set this to 1 if you want malloc of sizes < BLK_SIZE_MIN to be allocated
 * in tiny blocks.
 *
 * pros:
 * - less memory overhead
 * cons:
 * - slightly slower
 * - tiny blocks are not put in any free-list, because there isn't enough
 *   space in those blocks to do so, wich has some trivial worst-case
 *   allocation scenarios, which is actually why the feature is disabled right
 *   now.
 */
#define QTLSF_CONFIG_TINY_BLOCKS  0

#define FLI_MAX        30U      /* deal with allocations as big as 1G    */
#define FLI_OFFSET     6U       /* Our first populated second level row  */

#define SLI_MAX_SHIFT  5U       /* 32 free lists per row at second level */
#define SLI_MAX        (1U << SLI_MAX_SHIFT)

                       /* The first row would be under-populated with free
                        * lists, so we deal with blocks on the first row
                        * differently. See paper §5.1.
                        */
#define BLK_SMALL      (1U << (FLI_OFFSET + 1))

#define BLK_OVERHEAD   (offsetof(blk_hdr_t, data))
#define BLK_SIZE_MIN   (sizeof(blk_hdr_t) - BLK_OVERHEAD)
#define BLK_ALIGN      BLK_OVERHEAD

#define MEM_ALIGN_DOWN(sz)    ((sz) & ~(BLK_ALIGN - 1))
#define MEM_ALIGN_UP(sz)      MEM_ALIGN_DOWN((sz) + BLK_ALIGN - 1)

#define PTR_IS_EMPTY(ptr)     ((uintptr_t)(ptr) < 1)
#define PTR_EMPTY             ((void *)1)

/*
 * TODO: use padding:
 * - to store informations from hierarchical allocator à la talloc ?
 *
 * FIXME:
 *   tlsf_pool_t is quite large (6296 octets, 6144 for the pointers). There we
 *   should use the fact that pointers always fit in 48bits to make it
 *   smaller, it would make the pointer array 4608 octets big.
 *
 *   OTOH maybe qdb can live with tow pools at all times: the pool for short
 *   lived values, and that is reset at each statement evaluation, and another
 *   with the long lived stuff (or the long lived stuff can live in libc's
 *   malloc-ed stuff ?). The idea being that the short-lived pool is reset
 *   when either returning from evaluation or when returning from longjmp.
 *
 *   With the memory limitation feature, combined with proper undoable binary
 *   log, we can probably achieve almost complete robustness wrt allocation
 *   failures without having to deal with memory explicitly at any time.
 */
typedef struct blk_hdr_t blk_hdr_t;
struct blk_hdr_t {
    uint32_t   asked;
    uint32_t   flags;

    union {
        struct {
            struct blk_hdr_t **prev_next;
            struct blk_hdr_t *next;
            /* We want to be sure to have enough space for this header,
             * but it's not really there ...
             */
            blk_hdr_t        *prev_hdr;
        };
        uint8_t data[1];
    };
};

#define BLK_STATE       0x3

#define BLK_FREE        0x1
#define BLK_USED        0x0

#define BLK_PREV_FREE   0x2
#define BLK_PREV_USED   0x0

typedef struct arena_t {
    dlist_t    arena_list;
    blk_hdr_t *end;
} arena_t;

typedef struct tlsf_pool_t {
    struct {
        mem_pool_t pool;
        spinlock_t lock;
        uint32_t   fl_bitmap;
        uint32_t   options;
        uint32_t   pool_size;
    } __attribute__((aligned(64)));
    uint32_t   sl_bitmap[FLI_MAX - FLI_OFFSET];
    blk_hdr_t *blks[FLI_MAX - FLI_OFFSET][SLI_MAX];

    dlist_t    arena_head;
    uint32_t   arena_minsize;
    size_t     total_size;
    size_t     memory_limit;

    /* used if TLSF_OOM_LONGJMP is set */
    int        jval;
    jmp_buf   *jbuf;

    blk_hdr_t  blk;
} tlsf_pool_t;

static ALWAYS_INLINE
size_t mapping_search(size_t size, uint32_t *fl, uint32_t *sl)
{
    if (size < BLK_SMALL) {
        *fl = 0;
        *sl = size / (BLK_SMALL / SLI_MAX);
    } else {
        uint32_t t = (1 << (bsr32(size) - SLI_MAX_SHIFT)) - 1;

        size += t;
        *fl   = bsr32(size);
        *sl   = (size >> (*fl - SLI_MAX_SHIFT)) - SLI_MAX;
        *fl  -= FLI_OFFSET;
        size &= ~t;
    }
    return size;
}

static ALWAYS_INLINE
void mapping_insert(size_t size, uint32_t *fl, uint32_t *sl)
{
    if (size < BLK_SMALL) {
        *fl = 0;
        *sl = size / (BLK_SMALL / SLI_MAX);
    } else {
        *fl  = bsr32(size);
        *sl  = (size >> (*fl - SLI_MAX_SHIFT)) - SLI_MAX;
        *fl -= FLI_OFFSET;
    }
}

static ALWAYS_INLINE blk_hdr_t *
find_suitable_block(tlsf_pool_t *mp, uint32_t *fl, uint32_t *sl)
{
    uint32_t tmp = mp->sl_bitmap[*fl] & (-1 << *sl);

    if (tmp) {
        *sl = bsf32(tmp);
        return mp->blks[*fl][*sl];
    }
    tmp = mp->fl_bitmap & (-1 << (*fl + 1));
    if (likely(tmp)) {
        *fl = bsf32(tmp);
        *sl = bsf32(mp->sl_bitmap[*fl]);
        return mp->blks[*fl][*sl];
    }
    return NULL;
}

static ALWAYS_INLINE blk_hdr_t *blk_of(void *ptr)
{
    return ((blk_hdr_t *)((char *)(ptr) - BLK_OVERHEAD));
}

static ALWAYS_INLINE uint32_t blk_size(blk_hdr_t *blk)
{
    return blk->flags & ~BLK_STATE;
}

static ALWAYS_INLINE blk_hdr_t *blk_next(blk_hdr_t *blk, size_t size)
{
    return (blk_hdr_t *)(blk->data + size);
}

static ALWAYS_INLINE void blk_set_prev(blk_hdr_t *blk, blk_hdr_t *prev)
{
    ((blk_hdr_t **)blk)[-1] = prev;
}

static ALWAYS_INLINE blk_hdr_t *blk_get_prev(blk_hdr_t *blk)
{
    return ((blk_hdr_t **)blk)[-1];
}

static inline size_t blk_remove(tlsf_pool_t *mp, blk_hdr_t *blk)
{
    uint32_t fl, sl;
    size_t bsz = blk_size(blk);

#if QTLSF_CONFIG_TINY_BLOCKS
    if (likely(bsz >= BLK_SIZE_MIN))
#endif
    {
        mapping_insert(bsz, &fl, &sl);
        *blk->prev_next = blk->next;
        if (blk->next) {
            blk->next->prev_next = blk->prev_next;
        } else
        if (blk->prev_next == &mp->blks[fl][sl]) {
            RST_BIT(mp->sl_bitmap + fl, sl);
            if (!mp->sl_bitmap[fl])
                RST_BIT(&mp->fl_bitmap, fl);
        }
    }

    return bsz;
}

static inline void blk_insert(tlsf_pool_t *mp, blk_hdr_t *blk, size_t bsz)
{
    uint32_t fl, sl;

    blk->flags = bsz | BLK_PREV_USED | BLK_FREE;
#if QTLSF_CONFIG_TINY_BLOCKS
    if (likely(bsz >= BLK_SIZE_MIN))
#endif
    {
        mapping_insert(bsz, &fl, &sl);
        if ((blk->next = mp->blks[fl][sl])) {
            blk->next->prev_next = &blk->next;
        } else {
            SET_BIT(&mp->fl_bitmap, fl);
            SET_BIT(mp->sl_bitmap + fl, sl);
        }
        mp->blks[fl][sl] = blk;
        blk->prev_next = &mp->blks[fl][sl];
    }
}

/**************************************************************************/
/* Arena stuff                                                            */
/**************************************************************************/

static NEVER_INLINE void *arena_map(tlsf_pool_t *mp, size_t *size)
{
    *size += 8 * BLK_OVERHEAD;
    if (*size < mp->arena_minsize) {
        *size = mp->arena_minsize;
    } else {
        *size = ROUND_UP(*size, getpagesize());
    }
    if (mp->options & TLSF_LIMIT_MEMORY) {
        if (mp->total_size + *size > mp->memory_limit)
            return MAP_FAILED;
    }
    return mmap(0, *size, PROT_READ | PROT_WRITE, MAP_PRIVATE | MAP_ANON, -1, 0);
}

static NEVER_INLINE size_t arena_size(arena_t *a)
{
    return (uintptr_t)a->end->data - (uintptr_t)(void *)blk_of(a);
}

/*
 * Splits arena in three:
 * - the block which holds the arena descriptor
 * - the block with the free space inside
 * - a terminal block header (with its data pointing to the very end of the
 *   areana buffer) to avoid special casing arena ends in the
 *   allocator/deallocator.
 */
static NEVER_INLINE arena_t *arena_prepare(void *ptr, size_t size)
{
    blk_hdr_t *ablk, *blk, *end;
    arena_t *a;

    STATIC_ASSERT(sizeof(arena_t) >= BLK_SIZE_MIN);
    ablk = ptr;
    ablk->flags = MEM_ALIGN_UP(sizeof(arena_t)) | BLK_PREV_USED | BLK_USED;

    blk = blk_next(ablk, blk_size(ablk));
    blk->flags      = MEM_ALIGN_DOWN(size - 3 * BLK_OVERHEAD - ablk->flags)
                    | BLK_PREV_USED | BLK_FREE;
    blk->prev_next  = NULL;
    blk->next       = NULL;

    end = blk_next(blk, blk_size(blk));
    end->flags = BLK_PREV_FREE | BLK_USED;
    blk_set_prev(end, blk);

    a = (arena_t *)ablk->data;
    a->end = end;
    return a;
}

#if __GNUC_PREREQ(4, 6)
#pragma GCC diagnostic ignored "-Wunused-but-set-variable"
#endif

static ALWAYS_INLINE void arena_insert(tlsf_pool_t *mp, arena_t *a)
{
    blk_hdr_t *blk = blk_of(a);

    tlsf_free(&mp->pool, blk_next(blk, blk_size(blk))->data, 0);
}

ssize_t NEVER_INLINE tlsf_pool_add_arena(mem_pool_t *_mp, void *ptr, size_t size)
{
    tlsf_pool_t *mp = container_of(_mp, tlsf_pool_t, pool);
    blk_hdr_t *ablk0, *blk0;
    blk_hdr_t *ablk1, *blk1;
    arena_t   *a0, *a1;

    if (MEM_ALIGN_UP(size) != size) {
        errno = EINVAL;
        return (size_t)-1;
    }

    mp->total_size += size;
    a0    = arena_prepare(ptr, size);
    ablk0 = blk_of(a0);
    blk0  = blk_next(ablk0, blk_size(ablk0));

    dlist_for_each_entry_safe(a1, &mp->arena_head, arena_list) {
        ablk1 = blk_of(a1);
        blk1  = blk_next(ablk1, blk_size(ablk1));

        /* merge with next contiguous arena if exists */
        if ((void *)ablk1 == (void *)a0->end->data) {
            dlist_remove(&a1->arena_list);

            blk0->flags += (ablk1->flags & ~BLK_STATE) + 2 * BLK_OVERHEAD;
            blk_set_prev(blk1, blk0);
            a0->end = a1->end;
            continue;
        }

        /* merge with the previous contiguous arena if exists */
        if ((void *)a1->end->data == (void *)ablk0) {
            blk_hdr_t *end1 = a1->end;

            dlist_remove(&a1->arena_list);
            end1->flags += (blk0->flags & ~BLK_STATE)
                        +  (ablk0->flags & ~BLK_STATE)
                        + 2 * BLK_OVERHEAD;
            blk_set_prev(blk_next(end1, blk_size(end1)), end1);

            a1->end = a0->end;
            a0      = a1;
            ablk0   = ablk1;
            blk0    = end1;
            continue;
        }
    }

    dlist_add(&mp->arena_head, &a0->arena_list);
    tlsf_free(&mp->pool, blk0->data, 0);
    return blk_size(blk0);
}

void *tlsf_malloc(mem_pool_t *_mp, size_t size, mem_flags_t flags)
{
    tlsf_pool_t *mp = container_of(_mp, tlsf_pool_t, pool);
    uint32_t fl, sl;
    blk_hdr_t *blk, *next, *split;
    size_t tmp, asked = size;

    if (unlikely(size == 0))
        return PTR_EMPTY;
    if (unlikely(size > MEM_ALLOC_MAX))
        return NULL;

#if !QTLSF_CONFIG_TINY_BLOCKS
    if (size < BLK_SIZE_MIN) {
        size = BLK_SIZE_MIN;
    } else
#endif
    {
        size = MEM_ALIGN_UP(size);
    }
    size = mapping_search(size, &fl, &sl);

    blk = find_suitable_block(mp, &fl, &sl);
    if (unlikely(!blk)) {
        size_t asize = size;
        void *ptr = arena_map(mp, &asize);

        if (unlikely(ptr == MAP_FAILED)) {
            if (mp->options & TLSF_OOM_LONGJMP)
                longjmp(*mp->jbuf, mp->jval);
            if (mp->options & TLSF_OOM_ABORT)
                e_panic("tlsf: out of memory");
            return NULL;
        }
        tlsf_pool_add_arena(&mp->pool, ptr, asize);
        size = mapping_search(size, &fl, &sl);
        blk  = find_suitable_block(mp, &fl, &sl);
    }

    /* remove the block from the free-list and update bitfields */
    if ((mp->blks[fl][sl] = blk->next)) {
        blk->next->prev_next = &mp->blks[fl][sl];
    } else {
        RST_BIT(mp->sl_bitmap + fl, sl);
        if (!mp->sl_bitmap[fl])
            RST_BIT(&mp->fl_bitmap, fl);
    }

    tmp  = blk_size(blk);
    next = blk_next(blk, tmp);
    tmp -= size;
    if (tmp >= sizeof(blk_hdr_t)) {
        split = blk_next(blk, size);
        blk_insert(mp, split, tmp - BLK_OVERHEAD);
        blk_set_prev(next, split);
        blk->flags = size;
    } else {
        next->flags &= ~BLK_PREV_FREE;
        blk->flags  &= ~BLK_FREE;
    }

    if (!(flags & MEM_RAW))
        memset(blk->data, 0, asked);
    blk->asked = asked;
    return blk->data;
}

void *tlsf_realloc(mem_pool_t *_mp, void *ptr,
                   size_t oldsize, size_t newsize, mem_flags_t flags)
{
    tlsf_pool_t *mp = container_of(_mp, tlsf_pool_t, pool);
    blk_hdr_t *blk, *next, *split;
    size_t tsize, asked = newsize;
    void *res;

    if (PTR_IS_EMPTY(ptr))
        return newsize ? tlsf_malloc(&mp->pool, newsize, flags) : NULL;
    if (!newsize) {
        tlsf_free(&mp->pool, ptr, 0);
        return NULL;
    }

    blk  = blk_of(ptr);
    next = blk_next(blk, blk_size(blk));

#if !QTLSF_CONFIG_TINY_BLOCKS
    if (newsize < BLK_SIZE_MIN) {
        newsize = BLK_SIZE_MIN;
    } else
#endif
    {
        newsize = MEM_ALIGN_UP(newsize);
    }
    tsize   = blk_size(blk);
    if (newsize <= tsize) {
        if (next->flags & BLK_FREE) {
            size_t bsz = blk_remove(mp, next);

            tsize += bsz + BLK_OVERHEAD;
            next   = blk_next(next, bsz);
            goto split;
        }
        if (tsize >= sizeof(blk_hdr_t) + newsize)
            goto split;
        blk->asked = asked;
        res = blk->data;
    } else
    if ((next->flags & BLK_FREE) && newsize <= tsize + blk_size(next)) {
        size_t nsz = blk_remove(mp, next);

        tsize += nsz + BLK_OVERHEAD;
        next   = blk_next(next, nsz);

        if (tsize >= sizeof(blk_hdr_t) + newsize) {
          split:
            split = blk_next(blk, newsize);
            blk_insert(mp, split, tsize - BLK_OVERHEAD - newsize);
            blk_set_prev(next, split);
            blk->flags = newsize | (blk->flags & BLK_PREV_FREE);
        } else {
            blk->flags = tsize | (blk->flags & BLK_PREV_FREE);
            next->flags &= ~BLK_PREV_FREE;
        }
        blk->asked = asked;
        res = blk->data;
    } else {
        res = tlsf_malloc(&mp->pool, newsize, 0);
        if (!res)
            return NULL;
        if (oldsize == MEM_UNKNOWN)
            oldsize = blk->asked;
        memcpy(res, ptr, MIN(oldsize, newsize));
        tlsf_free(&mp->pool, ptr, 0);
    }
    if (!(flags & MEM_RAW) && oldsize < newsize)
        memset((char *)res + oldsize, 0, newsize - oldsize);
    return res;
}

void tlsf_free(mem_pool_t *_mp, void *ptr, mem_flags_t flags)
{
    tlsf_pool_t *mp = container_of(_mp, tlsf_pool_t, pool);
    blk_hdr_t *blk, *tmp;
    size_t bsz;

    if (unlikely(PTR_IS_EMPTY(ptr)))
        return;

    blk = blk_of(ptr);
    bsz = blk_size(blk);
    tmp = blk_next(blk, bsz);

    if (tmp->flags & BLK_FREE) {
        bsz += blk_remove(mp, tmp) + BLK_OVERHEAD;
    }
    if (blk->flags & BLK_PREV_FREE) {
        blk  = blk_get_prev(blk);
        bsz += blk_remove(mp, blk) + BLK_OVERHEAD;
    }
    blk_insert(mp, blk, bsz);

    tmp = blk_next(blk, bsz);
    tmp->flags |= BLK_PREV_FREE;
    blk_set_prev(tmp, blk);
}

size_t tlsf_sizeof(const void *ptr)
{
    return blk_of((void *)ptr)->asked;
}

size_t tlsf_extrasizeof(const void *ptr)
{
    blk_hdr_t *blk = blk_of((void *)ptr);
    return blk_size(blk) - blk->asked;
}

ssize_t tlsf_setsizeof(void *ptr, size_t asked)
{
    blk_hdr_t *blk = blk_of((void *)ptr);
    size_t size = blk_size(blk);

    if (unlikely(blk->asked + asked > size))
        return -1;
    blk->asked = asked;
    return size;
}

mem_pool_t *tlsf_pool_new(size_t minpagesize)
{
    tlsf_pool_t *mp;
    arena_t *a;
    size_t mpsize;

    minpagesize = ROUND_UP(minpagesize, getpagesize());
    mpsize      = minpagesize + ROUND_UP(sizeof(*mp), getpagesize());
    mp = mmap(0, mpsize, PROT_READ | PROT_WRITE, MAP_PRIVATE | MAP_ANON, -1, 0);
    if (mp == MAP_FAILED)
        return NULL;

    STATIC_ASSERT(offsetof(tlsf_pool_t, pool) == 0);
    mp->pool.malloc   = (fieldtypeof(mem_pool_t, malloc))&tlsf_malloc;
    mp->pool.realloc  = (fieldtypeof(mem_pool_t, realloc))&tlsf_realloc;
    mp->pool.free     = (fieldtypeof(mem_pool_t, free))&tlsf_free;
    mp->pool_size     = mpsize;
    mp->total_size    = mpsize;
    mp->arena_minsize = minpagesize;
    dlist_init(&mp->arena_head);

    a = arena_prepare(&mp->blk, mpsize - offsetof(tlsf_pool_t, blk));
    dlist_add(&mp->arena_head, &a->arena_list);
    arena_insert(mp, a);
    return &mp->pool;
}

int tlsf_pool_set_options(mem_pool_t *_mp, int options)
{
    tlsf_pool_t *mp = container_of(_mp, tlsf_pool_t, pool);
    int ret = mp->options;

    mp->options |= (options & TLSF_OOM_PUBLIC_MASK);
    return ret;
}

int tlsf_pool_rst_options(mem_pool_t *_mp, int options)
{
    tlsf_pool_t *mp = container_of(_mp, tlsf_pool_t, pool);
    int ret = mp->options;

    mp->options &= ~(options & TLSF_OOM_PUBLIC_MASK);
    return ret;
}

void tlsf_pool_limit_memory(mem_pool_t *_mp,  size_t memory_limit)
{
    tlsf_pool_t *mp = container_of(_mp, tlsf_pool_t, pool);
    mp->memory_limit = memory_limit;
    if (memory_limit) {
        mp->options |= TLSF_LIMIT_MEMORY;
    } else {
        mp->options &= ~TLSF_LIMIT_MEMORY;
    }
}

void tlsf_pool_set_jmpbuf(mem_pool_t *_mp, jmp_buf *jbuf, int jval)
{
    tlsf_pool_t *mp = container_of(_mp, tlsf_pool_t, pool);
    if (jbuf) {
        mp->options |= TLSF_OOM_LONGJMP;
    } else {
        mp->options &= ~TLSF_OOM_LONGJMP;
    }
    mp->jbuf = jbuf;
    mp->jval = jval;
}

void tlsf_pool_reset(mem_pool_t *_mp)
{
    tlsf_pool_t *mp = container_of(_mp, tlsf_pool_t, pool);
    memset(&mp->fl_bitmap, 0,
           offsetof(tlsf_pool_t, blk) - offsetof(tlsf_pool_t, fl_bitmap));

    dlist_for_each(it, &mp->arena_head) {
        arena_t *a = dlist_entry(it, arena_t, arena_list);

        arena_prepare(blk_of(a), arena_size(a));
        arena_insert(mp, a);
    }
}

static void tlsf_pool_wipe(tlsf_pool_t *mp)
{
    dlist_for_each_safe(it, &mp->arena_head) {
        arena_t *a = dlist_entry(it, arena_t, arena_list);

        dlist_remove(&a->arena_list);
        if (blk_of(a) == &mp->blk)
            continue;
        munmap(blk_of(a), arena_size(a));
    }
}

void tlsf_pool_delete(mem_pool_t **_mpp)
{
    if (*_mpp) {
        tlsf_pool_t *mp = container_of(*_mpp, tlsf_pool_t, pool);

        tlsf_pool_wipe(mp);
        munmap(mp, mp->pool_size);
        *_mpp = NULL;
    }
}
