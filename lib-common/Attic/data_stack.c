/**************************************************************************/
/*                                                                        */
/*  Copyright (C) 2004-2006 INTERSEC                                      */
/*                                                                        */
/*  Should you receive a copy of this source code, you must check you     */
/*  have a proper, written authorization of INTERSEC to hold it. If you   */
/*  don't have such an authorization, you must DELETE all source code     */
/*  files in your possession, and inform INTERSEC of the fact you obtain  */
/*  these files. Should you not comply to these terms, you can be         */
/*  prosecuted in the extent permitted by applicable law.                 */
/*                                                                        */
/**************************************************************************/

#include <stdio.h>
#include <limits.h>

#include "err_report.h"
#include "data_stack.h"
#include "macros.h"
#include "string_is.h"

/**************************************************************************/
/* private types / macros                                                 */
/**************************************************************************/

#define FRAME_COUNT     32
#define MIN_BLOCK_SIZE  4096
#define MAX_DS_ALLOC    ((ssize_t)1 << (sizeof(ssize_t)*8-1))

typedef struct block_t {
    struct block_t * next;
    ssize_t size, left;

    /*  unsigned char data[]; */
} block_t;

#define SIZEOF_BLOCK MEM_ALIGN(sizeof(block))
#define BLOCK_CUR_PAYLOAD(block)                    \
    ((unsigned char *) (block) + SIZEOF_BLOCK) +    \
    (block->size - block->left)

#define BLOCK_PAYLOAD(block) ((unsigned char *)(block) + SIZEOF_BLOCK)

typedef struct frame_t {
    struct frame_t * prev;
    block_t * block[FRAME_COUNT];
    ssize_t space_left[FRAME_COUNT];
} frame_t;

/**************************************************************************/
/* private variables                                                      */
/**************************************************************************/

static struct {
    int fpos;           /* our position in the current frame */
    frame_t * frame;    /* current frame */
    block_t * block;    /* current block */

    void * buffer;      /* the last data segment returned */
    ssize_t size;       /* the last requested size */
} current = { -1, NULL, NULL, NULL, 0 };

static struct {
    frame_t * frames;   /* unused frame*s* */
    block_t * block;    /* largest unused block */
} unused = { NULL, NULL };

/**************************************************************************/
/* private functions                                                      */
/**************************************************************************/

/* free_next_blocks(block)
 *
 * every block listed from block->next will be deallocated.
 *
 * One exception :
 *   if one of those blocks is bigger than the current unused.block,
 *   it will replace the current unused.block, be reseted in a state
 *   where it is immediately usable, and the current unused.block will
 *   be free'd instead.
 *
 * returns void
 */
static void free_next_blocks(block_t * block)
{
    block_t *next_next;
    int reset_block_invariant = 0;

    while (block->next != NULL) {
        next_next = block->next->next;

        if (unused.block == NULL || block->next->size > unused.block->size) {
            free(unused.block);
            unused.block = block->next;
            reset_block_invariant = 1;
        } else {
            free(block->next);
        }

        block->next = next_next;
    }

    if (reset_block_invariant) {
        /* new block invariant */
        unused.block->left = unused.block->size;
        unused.block->next = NULL;
    }
}

/* mem_block_alloc(min_size)
 *
 * Force the allocation (don't use unused.block) of a block of size of
 * at least min_size bytes.
 *
 * returns a pointer to the allocated block_t
 */
static void * mem_block_alloc(ssize_t min_size)
{
    static ssize_t last_alloc_size = MIN_BLOCK_SIZE;
    block_t * block;

    min_size += last_alloc_size;
    e_assert( min_size <= MAX_DS_ALLOC );

    while (last_alloc_size <= min_size) {
        last_alloc_size <<= 1;
    }

    block = malloc(SIZEOF_BLOCK + last_alloc_size);
    if (block == NULL) {
        e_fatal(FATAL_NOMEM, E_PREFIX("not enough memory"));
    }

    /* new block invariant */
    block->size = last_alloc_size;
    block->left = last_alloc_size;
    block->next = NULL;

    return block;
}

/* malloc_real(ssize_t, permanent)
 *
 * the pseudo-allocation function : tries to reuse unused.block, and if not
 * possible, allocate a new block.
 *
 * if bloc is made permanent, it is protected from further ds_get/ds_reget.
 * this function is responsible of maintaining the `current.size' and
 * `current.buffer' invariants for ds_get/reget/tie functions.
 *
 * returns a pointer to the usable buffer.
 */
static void * malloc_real(ssize_t size, int permanent)
{
    block_t * block;
    void * ret;

    if (size == 0) return NULL;

    size = MEM_ALIGN(size);

    if (current.block->left < size) {
        /* we need a new block */
        if (unused.block != NULL && unused.block->size >= size) {
            block        = unused.block;
            unused.block = NULL;
        } else {
            block = mem_block_alloc(size);
        }

        current.block->next = block;
        current.block       = block;
    }

    ret = BLOCK_CUR_PAYLOAD(current.block);
    if (permanent) {
        current.block->left -= size;
        current.buffer       = NULL;
        current.size         = 0;
    } else {
        current.buffer       = ret;
        current.size         = size;
    }
    return ret;
}

/**************************************************************************/
/* public API                                                             */
/**************************************************************************/

int ds_push()
{
    frame_t * frame;

    if (current.fpos < 0) {
        /* kludge to force ds_init() when forgeted */
        ds_init();
    }

    current.fpos ++;

    if (current.fpos == FRAME_COUNT) {
        current.fpos = 0;
        if (unused.frames == NULL) {
            /* allocate new block */
            frame = (frame_t *)calloc(1, sizeof(frame_t));
            if (frame == NULL) {
                e_fatal(FATAL_NOMEM, E_PREFIX("not enough memory"));
            }
        } else {
            /* use existing unused frame */
            frame         = unused.frames;
            unused.frames = unused.frames->prev;
        }

        frame->prev   = current.frame;
        current.frame = frame;
    }

    current.frame->block[current.fpos]      = current.block;
    current.frame->space_left[current.fpos] = current.block->left;
    current.size   = 0;
    current.buffer = NULL;

    return current.fpos;
}

int ds_pop()
{
    frame_t * frame;
    int ret;

    if (current.frame == NULL) {
        e_panic("ds_pop() called with empty stack");
    }

    current.size        = 0;
    current.buffer      = NULL;
    current.block       = current.frame->block[current.fpos];
    current.block->left = current.frame->space_left[current.fpos];
    free_next_blocks(current.block->next);

    ret = current.fpos;

    if (current.fpos > 0) {
        current.fpos--;
    } else {
        current.fpos  = FRAME_COUNT - 1;

        frame         = current.frame;
        current.frame = frame->prev;

        frame->prev   = unused.frames;
        unused.frames = frame;
    }

    return ret;
}

void * ds_get(ssize_t size)
{
    return malloc_real(size, 0);
}

void * ds_reget(void * buffer, ssize_t size)
{
    ssize_t old_size;
    void * old_buffer;

    e_assert(buffer == current.buffer);
    
    old_buffer = current.buffer;
    old_size   = current.size;
    if (old_buffer != ds_get(size)) {
        /* old_size < size stands : else we would'nt have needed realloc */
        memcpy(current.buffer, old_buffer, old_size);
    }

    return current.buffer;
}

void * ds_try_reget(void * buffer, ssize_t size)
{
    return (buffer == current.buffer) ? ds_reget(buffer, size) : NULL;
}

void ds_tie(ssize_t size)
{
    e_assert(current.buffer != NULL);
    e_assert(current.size >= size);
    e_assert(current.block->left >= size);

    malloc_real(size, 1);
}

void * ds_malloc(ssize_t size)
{
    return malloc_real(size, 1);
}

ssize_t max_safe_size(void * mem)
{
    frame_t * frame = current.frame;
    int       fpos  = current.fpos;

    unsigned char * ptr = mem;

    while (frame != NULL) {
        while (fpos >= 0) {
            block_t * block = frame->block[fpos];
            unsigned char * bck = BLOCK_PAYLOAD(block);

            if (bck < ptr && ptr < bck + block->size) {
                return bck + block->size - ptr;
            }
            fpos --;
        }
        fpos  = FRAME_COUNT;
        frame = frame->prev;
    }

    return 0;
}

void ds_initialize()
{
    if (current.fpos >= 0) {
        e_warning(E_PREFIX("ds_init() called twice"));
        return;
    }

    current.fpos = FRAME_COUNT - 1;
    current.block = mem_block_alloc(MIN_BLOCK_SIZE);
}

void ds_shutdown()
{
    frame_t * frame;

    if (current.fpos != FRAME_COUNT-1 && current.frame != NULL) {
        e_panic(MISSING_DSPOP);
    }

    while (unused.frames != NULL) {
        frame         = unused.frames;
        unused.frames = unused.frames->prev;
        free(frame);
    }

    if (current.block != NULL) {
        FREE(current.block);
    }
    if (unused.block  != NULL) {
        FREE(unused.block);
    }

    current.fpos   = -1;
    current.buffer = NULL;
    current.size   = 0;
}

