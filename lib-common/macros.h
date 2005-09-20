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

#if __GNUC__ > 2 || (__GNUC__ == 2 && __GNUC_MINOR__ > 4)
#  define __attr_format__(format_idx, arg_idx)  \
        __attribute__((format (printf, format_idx, arg_idx)))
#  define __unused__        __attribute__((unused))
#  define __attr_noreturn__ __attribute__((noreturn))
#else
#  define __attr_format__(format_idx, arg_idx)
#  define __unused__
#  define __attr_noreturn__
#endif

/******************************************************************************/
/* TYPES                                                                      */
/******************************************************************************/

#ifndef bool
typedef int bool;
#endif

#ifndef false
#define false (0)
#define true (!false)
#endif

typedef unsigned char byte;

/******************************************************************************/
/* Memory functions                                                           */
/******************************************************************************/

#define MEM_ALIGN_SIZE  8
#define MEM_ALIGN(size) \
    ( ((size) + MEM_ALIGN_SIZE - 1) & ~((ssize_t) MEM_ALIGN_SIZE-1) )

#define FREE(ptr) do {  \
    free(ptr);          \
    ptr = NULL;         \
} while(0)

#define countof(table) ((int)(sizeof(table)) / sizeof(table[0]))

#endif
