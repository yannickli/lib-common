#ifndef IS_ARRAY_H
#define IS_ARRAY_H

#include <stdlib.h>

#include "mem.h"

typedef struct _array {
    void ** const tab;
    ssize_t const len;

    ssize_t const __size;
} _array;
typedef void array_item_dtor_f(void *item);

/******************************************************************************/
/* Memory management                                                          */
/******************************************************************************/

#define array_new() array_init(p_new_raw(_array, 1))
_array *array_init(_array *array);
void array_wipe(_array *array, array_item_dtor_f *dtor);
void array_delete(_array **array, array_item_dtor_f *dtor);

/******************************************************************************/
/* Misc                                                                       */
/******************************************************************************/

void array_insert(_array *array, ssize_t pos, void *item);
static inline void array_append(_array *array, void *item)
{
    array_insert(array, array->len, item);
}
static inline void array_push(_array *array, void *item)
{
    array_insert(array, 0, item);
}

void *array_take(_array *array, ssize_t pos);

/******************************************************************************/
/* Typed Arrays                                                               */
/******************************************************************************/

#define ARRAY_TYPE(el_typ, prefix)                                             \
    typedef struct prefix##_array {                                            \
        el_typ ** const tab;                                                   \
        ssize_t const len;                                                     \
                                                                               \
        ssize_t const __size;                                                  \
    } prefix##_array

#define ARRAY_FUNCTIONS(el_typ, prefix)                                        \
                                                                               \
    /* legacy functions */                                                     \
    static inline prefix##_array *prefix##_array_new(void)                     \
    {                                                                          \
        return (prefix##_array *)array_new();                                  \
    }                                                                          \
    static inline prefix##_array *                                             \
    prefix##_array_init(prefix##_array *array)                                 \
    {                                                                          \
        return (prefix##_array *)array_init((_array *)array);                  \
    }                                                                          \
    static inline void                                                         \
    prefix##_array_wipe(prefix##_array *array, bool do_elts)                   \
    {                                                                          \
        array_wipe((_array*)array,                                             \
                do_elts ? (array_item_dtor_f *)prefix##_delete : NULL);        \
    }                                                                          \
    static inline void                                                         \
    prefix##_array_delete(prefix##_array **array, bool do_elts)                \
    {                                                                          \
        array_delete((_array **)array,                                         \
                do_elts ? (array_item_dtor_f *)prefix##_delete : NULL);        \
    }                                                                          \
                                                                               \
    /* module functions */                                                     \
    static inline void                                                         \
    prefix##_array_insert(prefix##_array *array, ssize_t pos, el_typ *item)    \
    {                                                                          \
        array_insert((_array *)array, pos, (void*)item);                       \
    }                                                                          \
    static inline void                                                         \
    prefix##_array_append(prefix##_array *array, el_typ *item)                 \
    {                                                                          \
        array_append((_array *)array, (void*)item);                            \
    }                                                                          \
    static inline void                                                         \
    prefix##_array_push(prefix##_array *array, el_typ *item)                   \
    {                                                                          \
        array_push((_array *)array, (void*)item);                              \
    }                                                                          \
    static inline el_typ *                                                     \
    prefix##_array_take(prefix##_array *array, ssize_t pos)                    \
    {                                                                          \
        return (el_typ *)array_take((_array *)array, pos);                     \
    }

#endif
