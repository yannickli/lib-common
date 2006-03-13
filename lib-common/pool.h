#ifndef __SPOOL_H__
#define __SPOOL_H__

/* XXX : be very careful, avoid passing macros for Reset, Delete and New
 *       functions, as those are multievaluated.
 */

#define GENERIC_POOL(type, size, New, Reset, Delete, attr)              \
                                                                        \
struct {                                                                \
    int avail;                                                          \
    type *pool[size];                                                   \
} type##_pool;                                                          \
                                                                        \
attr type *type##_new(void)                                             \
{                                                                       \
    if (type##_pool.avail) {                                            \
        return type##_pool.pool[--type##_pool.avail];                   \
    }                                                                   \
    return New();                                                       \
}                                                                       \
                                                                        \
attr void type##_delete(type **value)                                   \
{                                                                       \
    if (value && *value) {                                              \
        if (type##_pool.avail < size) {                                 \
            Reset(*value);                                              \
            type##_pool.pool[type##_pool.avail++] = *value;             \
            value = NULL;                                               \
        } else {                                                        \
            Delete(value);                                              \
        }                                                               \
    }                                                                   \
}                                                                       \
                                                                        \
static inline void type##_pool_empty(void)                              \
{                                                                       \
    while (type##_pool.avail > 0) {                                     \
        --type##_pool.avail;                                            \
        Delete(&type##_pool.pool[type##_pool.avail]);                   \
    }                                                                   \
}

#define POOL(type, size, New, Reset, Delete)    \
    GENERIC_POOL(type, size, New, Reset, Delete,)

#define STATIC_POOL(type, size, New, Reset, Delete)    \
    GENERIC_POOL(type, size, New, Reset, Delete, static)

#endif
