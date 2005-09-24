#ifndef IS_ARRAY_H
#define IS_ARRAY_H

#include <stdlib.h>

#include "mem.h"

typedef struct array {
    void ** const tab;
    ssize_t const len;

    ssize_t const __size;
} array_t;
typedef void array_item_dtor_f(void * item);

/******************************************************************************/
/* Memory management                                                          */
/******************************************************************************/

#define array_new() array_init(p_new_raw(array_t, 1))
array_t *array_init(array_t *array);
void array_wipe(array_t *array, array_item_dtor_f *dtor);
void array_delete(array_t **array, array_item_dtor_f *dtor);

/******************************************************************************/
/* Misc                                                                       */
/******************************************************************************/

void array_append(array_t *array, void *item);
void * array_take(array_t *array, ssize_t pos);

/******************************************************************************/
/* Typed Arrays                                                               */
/******************************************************************************/

#define ARRAY_TYPE(el_typ, prefix)                                             \
    typedef struct prefix##_array {                                            \
        el_typ ** const tab;                                                   \
        ssize_t const len;                                                     \
                                                                               \
        ssize_t const __size;                                                  \
    } prefix##_array_t

#define ARRAY_FUNCTIONS(el_typ, prefix)                                        \
                                                                               \
    /* legacy functions */                                                     \
    static inline prefix##_array_t *prefix##_array_new(void)                   \
    {                                                                          \
        return (prefix##_array_t *)array_new();                                \
    }                                                                          \
    static inline prefix##_array_t *                                           \
    prefix##_array_init(prefix##_array_t *array)                               \
    {                                                                          \
        return (prefix##_array_t *)array_init((array_t *)array);               \
    }                                                                          \
    static inline void                                                         \
    prefix##_array_wipe(prefix##_array_t *array, bool do_elts)                 \
    {                                                                          \
        array_wipe((array_t*)array,                                            \
                do_elts ? (array_item_dtor_f *)prefix##_delete : NULL);        \
    }                                                                          \
    static inline void                                                         \
    prefix##_array_delete(prefix##_array_t **array, bool do_elts)              \
    {                                                                          \
        array_delete((array_t **)array,                                        \
                do_elts ? (array_item_dtor_f *)prefix##_delete : NULL);        \
    }                                                                          \
                                                                               \
    /* module functions */                                                     \
    static inline void                                                         \
    prefix##_array_append(prefix##_array_t *array, el_typ *item)               \
    {                                                                          \
        array_append((array_t *)array, (void*)item);                           \
    }                                                                          \
    static inline el_typ *                                                     \
    prefix##_array_take(prefix##_array_t *array, ssize_t pos)                  \
    {                                                                          \
        return (el_typ *)array_take((array_t *)array, pos);                    \
    }

#endif
