#include <unistd.h>

/* Semantic of a pool :
 *
 * Any implementation of a pool should have the caracteristic that every
 * function never fails but may never return.
 * 
 * pool_t::malloc(size_t size)
 * 
 *   allocates size bytes of memory, and returns a pointer to that memory.
 *   the memory is not *necessarily* cleared.
 *   Note that unlike malloc(3) :
 *     - pool_t::malloc never fail (if we are out of memory, the program is stopped)
 *     - pool_t::malloc(0) will return NULL
 *   
 *   
 * pool_t::calloc(size_t size)
 * 
 *   same as pool_t::malloc, but the memory is set to 0.
 *
 *
 * pool_t::free(void * ptr) :
 * 
 *   frees the he memory space pointed to by ptr, which must have been returned
 *   by a previous call to pool_t::malloc(), pool_t::calloc() or
 *   pool_t::realloc().
 *
 *   if ptr is NULL, no operation is performed.
 *
 *   pool_t::free always returns NULL, so that :
 *      ptr = pool->free(ptr);
 *   can work.
 *
 *
 * pool_t::realloc(void * ptr, size_t size)
 *
 *   changes the size of the memory block pointed to by ptr to size bytes.
 *   The contents will be unchanged to the minimum of the old and new sizes;
 *
 *   If ptr is NULL, the call is equivalent to malloc(size)
 *   If size is equal to zero, the call is equivalent to free(ptr).
 *   Unless ptr is NULL, it must have been returned by an earlier call to
 *   pool_t::malloc(), pool_t::calloc()  or pool_t::realloc().
 *   If the area pointed to was moved, a free(ptr) is done.
 *
 *   the function never fails (see pool_t::malloc) and returns a pointer to the
 *   new memory location.
 *
 *   newly allocated memory will *NOT* be set to 0.
 *
 * 
 * pool_t::realloc0(void * ptr, size_t oldsize, size_t newsize)
 * 
 *   same as pool_t::realloc(ptr, newsize) + sets the last (newsize - oldsize)
 *   bytes to 0 (if oldsize < newsize)
 */

typedef struct pool pool_t;

struct pool {
    const char * const name;

    void * (*malloc)  (size_t size);
    void * (*calloc)  (size_t size);
    void * (*free)    (void * mem);
    void * (*realloc) (void * mem, size_t newsize);
    void * (*realloc0)(void * mem, size_t oldsize, size_t newsize);
};

#define p_new(pool, type, count) (type *)(pool)->calloc(sizeof(type)*(count))

/* An implementation of pool_t that uses internally
 * malloc(3), calloc(3), free(3) and realloc(3)
 */
extern const pool_t * system_pool;

