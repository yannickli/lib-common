/**************************************************************************/
/*                                                                        */
/*  Copyright (C) 2004-2012 INTERSEC SAS                                  */
/*                                                                        */
/*  Should you receive a copy of this source code, you must check you     */
/*  have a proper, written authorization of INTERSEC to hold it. If you   */
/*  don't have such an authorization, you must DELETE all source code     */
/*  files in your possession, and inform INTERSEC of the fact you obtain  */
/*  these files. Should you not comply to these terms, you can be         */
/*  prosecuted in the extent permitted by applicable law.                 */
/*                                                                        */
/**************************************************************************/

#ifndef IS_LIB_COMMON_SORT_H
#define IS_LIB_COMMON_SORT_H

#include "core.h"

/* Numeric optimized versions */
#define type_t   uint8_t
#define dsort    dsort8
#define uniq     uniq8
#define bisect   bisect8
#define contains contains8
#include "sort-numeric.in.h"

#define type_t   uint16_t
#define dsort    dsort16
#define uniq     uniq16
#define bisect   bisect16
#define contains contains16
#include "sort-numeric.in.h"

#define type_t   uint32_t
#define dsort    dsort32
#define uniq     uniq32
#define bisect   bisect32
#define contains contains32
#include "sort-numeric.in.h"
#define bisect32(what, data, len)  (bisect32)((what), (data), (len), NULL)

#define type_t   uint64_t
#define dsort    dsort64
#define uniq     uniq64
#define bisect   bisect64
#define contains contains64
#include "sort-numeric.in.h"
#define bisect64(what, data, len)  (bisect64)((what), (data), (len), NULL)

/* Generic implementations */
typedef int (cmp_r_t)(const void *a, const void *b, void *arg);

size_t  uniq(void *data, size_t size, size_t nmemb, cmp_r_t *cmp, void *arg);
size_t  bisect(const void *what, const void *data, size_t size,
               size_t nmemb, bool *found, cmp_r_t *cmp, void *arg);
bool    contains(const void *what, const void *data, size_t size,
                 size_t nmemb, cmp_r_t *cmp, void *arg);

#ifdef __has_blocks
typedef int (BLOCK_CARET cmp_b)(const void *a, const void *b);

size_t  uniq_blk(void *data, size_t size, size_t nmemb, cmp_b cmp);
size_t  bisect_blk(const void *what, const void *data, size_t size,
                   size_t nmemb, bool *found, cmp_b cmp);
bool    contains_blk(const void *what, const void *data, size_t size,
                     size_t nmemb, cmp_b cmp);
#endif

#endif
