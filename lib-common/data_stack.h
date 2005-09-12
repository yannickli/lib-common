#ifndef IS_DATA_STRACK_H
#define IS_DATA_STRACK_H

#include <stdlib.h>

#include "err_report.h"

#define MISSING_DSPOP "missing ds_pop() call"

/* ds_push/ds_pop :
 *
 * ds_push create a new frame
 * ds_pop forgets everything that has been (re)get-ed or tie-d since last ds_push
 */
int ds_push(void);
int ds_pop(void);

/* this macro verifies that the pop-ed frame is the one we pushed
 *
 * this is a macro since it uses __LINE__/__FILE__ magic
 */
#define ds_pop_check(frame)                     \
    do {                                        \
        if (frame != ds_pop()) {                \
            e_panic(E_PREFIX(MISSING_DSPOP));   \
        }                                       \
    } while(0)                                  \

/* ds_get :
 *   allocate some new buffer.
 *   - if ds_get is called twice, the second buffer may reuse the space used by
 *     the first one
 *   - a call to ds_push will invalidate any get-ed buffer if not ds_tie-d
 *
 * ds_reget :
 *   expand the last get-ed buffer (with reallocation if needed).
 *   be carefull, the buffer address may change
 *
 *   ds_reget ensure that the rignt buffer is beeing expansed.
 *   calls to ds_reget without prior ds_get calls may cause strange behaviours
 *   if misunderstood.
 *
 * ds_try_reget :
 *   try to ds_reget, or return NULL if we tried to reget another buffer than
 *   the last one
 *
 * ds_tie :
 *   make last get/reget call permanent (until next ds_pop).
 */
void * ds_get(ssize_t size);
void * ds_reget(void * buffer, ssize_t size);
void * ds_try_reget(void * buffer, ssize_t size);
void ds_tie(ssize_t size);

/* roughly : ds_get followed by ds_tie */
void * ds_malloc(ssize_t size);

/* this function returns the maximum length that is safe to read from a pointer
 * meaning that it will search if the pointer is in a frame, and if yes, will
 * answer the size until the end of the frame.
 *
 * This is useful for internals things with memory management, and direct use of
 * this function in anything else than memory allocation systems is discouraged.
 */
ssize_t max_safe_size(void * mem);

/*
 * type safe macros for ds_get/reget
 */
#define ds_get_type(type, count)   ((type)*)(ds_get(sizeof(type)*(count)))
#define ds_reget_type(type, count) ((type)*)(ds_get(sizeof(type)*(count)))

/*
 * init/deinit functions
 *
 * init function is optional, since ds_push has to be called first,
 * and ds_push ensure that ds_init has been called.
 *
 * ds_deinit() has to be run in order to avoid valgrind (or alike)
 * memory leaks warnings.
 */
void ds_init(void);
void ds_deinit(void);

#endif
