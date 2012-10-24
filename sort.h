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
void    dsort8(uint8_t base[], size_t n);
void    dsort16(uint16_t base[], size_t n);
void    dsort32(uint32_t base[], size_t n);
void    dsort64(uint64_t base[], size_t n);

size_t  uniq8(uint8_t data[], size_t len);
size_t  uniq16(uint16_t data[], size_t len);
size_t  uniq32(uint32_t data[], size_t len);
size_t  uniq64(uint64_t data[], size_t len);

size_t  bisect8(uint8_t what, const uint8_t data[], size_t len, bool *found);
size_t  bisect16(uint16_t what, const uint16_t data[], size_t len,
                 bool *found);
size_t  bisect32(uint32_t what, const uint32_t data[], size_t len,
                 bool *found);
#define bisect32(what, data, len)  (bisect32)((what), (data), (len), NULL)
size_t  bisect64(uint64_t what, const uint64_t data[], size_t len,
                 bool *found);
#define bisect64(what, data, len)  (bisect64)((what), (data), (len), NULL)

bool    contains8(uint8_t what, const uint8_t data[], size_t len);
bool    contains16(uint16_t what, const uint16_t data[], size_t len);
bool    contains32(uint32_t what, const uint32_t data[], size_t len);
bool    contains64(uint64_t what, const uint64_t data[], size_t len);

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
