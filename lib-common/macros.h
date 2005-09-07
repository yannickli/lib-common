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
#  define __attr_unused__   __attribute__((unused))
#  define __attr_noreturn__ __attribute__((noreturn))
#else
#  define __attr_format__(format_idx, arg_idx)
#  define __attr_unused__
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

/******************************************************************************/
/* Memory functions                                                           */
/******************************************************************************/

#define free0(ptr) do { \
    free(ptr);          \
    ptr = NULL;         \
} while(0)

#endif
