#include <string.h>
#include <stdio.h>
#include <limits.h>

#include "err_report.h"
#include "data_stack.h"
#include "macros.h"

/******************************************************************************/
/* private types / macros                                                     */
/******************************************************************************/

#define FRAME_COUNT     32
#define MIN_BLOCK_SIZE  4096
#define MEM_ALIGN_SIZE  8
#define MAX_DS_ALLOC    ((size_t)1 << (sizeof(size_t)*8-1))

typedef struct block_t {
    struct block_t * next;
    size_t size, left;

    /*  unsigned char data[]; */
} block_t;

#define SIZEOF_BLOCK MEM_ALIGN(sizeof(block))
#define BLOCK_CUR_PAYLOAD(block)                    \
    ((unsigned char *) (block) + SIZEOF_BLOCK) +    \
    (block->size - block->left)

typedef struct frame_t {
    struct frame_t * prev;
    block_t * block[FRAME_COUNT];
    size_t space_left[FRAME_COUNT];
} frame_t;

/******************************************************************************/
/* private variables                                                          */
/******************************************************************************/

static struct {
    int fpos;           /* our position in the current frame */
    frame_t * frame;    /* current frame */
    block_t * block;    /* current block */

    void * buffer;      /* the last data segment returned */
    size_t size;        /* the last requested size */
} current = { -1, NULL, NULL, NULL, 0 };

static struct {
    frame_t * frames;   /* unused frame*s* */
    block_t * block;    /* largest unused block */
} unused = { NULL, NULL };

/******************************************************************************/
/* private functions                                                          */
/******************************************************************************/

/* free_next_blocks(block)
 *
 * every block listed from block->next will be deallocated.
 *
 * One exception :
 *   if one of those blocks is bigger than the current unused.block,
 *   it will replace the current unused.block, be reseted in a state where it's
 *   immediately usable, and the current unused.block will be free'd instead.
 *
 * returns void
 */
static void free_next_blocks(block_t * block)
{
    block_t * next_next;
    int reset_block_invariant = 0;

    while (block->next != NULL) {
        next_next = block->next->next;

        if (unused.block == NULL || block->next->size > unused.block->size) {
            free(unused.block);
            unused.block = block->next;
            reset_block_invariant = 1;
        }
        else {
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
 * Force the allocation (don't use unused.block) of a block of size of at least
 * min_size bytes.
 *
 * returns a pointer to the allocated block_t
 */
static void * mem_block_alloc(size_t min_size)
{
    static size_t last_alloc_size = MIN_BLOCK_SIZE;
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

/* malloc_real(size_t, permanent)
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
static void * malloc_real(size_t size, int permanent)
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
        }
        else {
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

/******************************************************************************/
/* public API                                                                 */
/******************************************************************************/

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
        }
        else {
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
    }
    else {
        current.fpos  = FRAME_COUNT - 1;

        frame         = current.frame;
        current.frame = frame->prev;

        frame->prev   = unused.frames;
        unused.frames = frame;
    }

    return ret;
}

void * ds_get(size_t size)
{
    return malloc_real(size, 0);
}

void * ds_reget(void * buffer, size_t size)
{
    size_t old_size;
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

void ds_tie(size_t size)
{
    e_assert(current.buffer != NULL);
    e_assert(current.size >= size);
    e_assert(current.block->left >= size);

    malloc_real(size, 1);
}

void ds_init()
{
    if (current.fpos >= 0) {
        e_warning(E_PREFIX("ds_init() called twice"));
        return;
    }

    current.fpos = FRAME_COUNT - 1;
    current.block = mem_block_alloc(MIN_BLOCK_SIZE);
}

void ds_deinit()
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

