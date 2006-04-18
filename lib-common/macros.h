#ifndef IS_MACROS_H
#define IS_MACROS_H

/******************************************************************************/
/* GNU EXTENSIONS fake                                                        */
/******************************************************************************/

/*
 * __attr_format__(pos1, pos2) => printf formats
 * __attr_unused__             => unused vars
 * __attr_noreturn__           => functions that perform abord()/exit()
 */

#define __attr_format__(format_idx, arg_idx)  \
    __attribute__((format(printf, format_idx, arg_idx)))
#define __unused__        __attribute__((unused))
#define __attr_noreturn__ __attribute__((noreturn))

#if !defined(__GNUC__) || __GNUC__ < 3
#  define __attribute__(foo)
#endif

/******************************************************************************/
/* TYPES                                                                      */
/******************************************************************************/

#if !defined(__cplusplus)
#ifndef bool
typedef int bool;
#endif

#ifndef false
#define false  (0)
#define true   (!false)
#endif
#endif

typedef unsigned char byte;

/******************************************************************************/
/* Memory functions                                                           */
/******************************************************************************/

#define MEM_ALIGN_SIZE  8
#define MEM_ALIGN(size) \
    (((size) + MEM_ALIGN_SIZE - 1) & ~((ssize_t)MEM_ALIGN_SIZE - 1))

#define FREE(ptr) do {  \
    free(ptr);          \
    (ptr) = NULL;       \
} while(0)

#define countof(table) ((int)(sizeof(table)) / sizeof((table)[0]))
#define ssizeof(foo)   ((ssize_t)sizeof(foo))

/******************************************************************************/
/* Misc                                                                       */
/******************************************************************************/
#ifndef MAX
#define MAX(a,b) (((a) > (b)) ? (a) : (b))
#endif

enum sign {
    POSITIVE = 1,
    ZERO     = 0,
    NEGATIVE = -1
};

#ifndef SIGN
#define SIGN(x) ((enum sign)(((x) > 0) - ((x) < 0)))
#endif

#endif
