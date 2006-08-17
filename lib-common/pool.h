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

#ifndef __POOL_H__
#define __POOL_H__

/* Pool idea : Keep a pool of non-allocated objects. Object are not
 * pre-allocated, but non-deallocated when you delete them, in order
 * to have <size> objects ready to be used again.
 *
 * XXX : be very careful, avoid passing macros for Reset, Delete and New
 *       functions, as those are multievaluated.
 */

#define GENERIC_POOL(type, size, New, Reset, Delete, attr)              \
                                                                        \
static struct {                                                         \
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
