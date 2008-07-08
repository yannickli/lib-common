/**************************************************************************/
/*                                                                        */
/*  Copyright (C) 2004-2008 INTERSEC SAS                                  */
/*                                                                        */
/*  Should you receive a copy of this source code, you must check you     */
/*  have a proper, written authorization of INTERSEC to hold it. If you   */
/*  don't have such an authorization, you must DELETE all source code     */
/*  files in your possession, and inform INTERSEC of the fact you obtain  */
/*  these files. Should you not comply to these terms, you can be         */
/*  prosecuted in the extent permitted by applicable law.                 */
/*                                                                        */
/**************************************************************************/

#ifndef IS_LIB_COMMON_ARCHIVE_PRIV_H
#define IS_LIB_COMMON_ARCHIVE_PRIV_H

#define ARCHIVE_MAGIC0      'I'
#define ARCHIVE_MAGIC1      'S'
#define ARCHIVE_MAGIC2      '-'
#define ARCHIVE_MAGIC3      0x42
#define ARCHIVE_MAGIC_SIZE  4

#define ARCHIVE_MAGIC_STR "IS-\x42"

#define B4_TO_INT(b0, b1, b2, b3) \
         (((b0) << 24) +          \
          ((b1) << 16) +          \
          ((b2) << 8 ) +          \
          ((b3) << 0 ))

#define BYTESTAR_TO_INT(input)    \
        B4_TO_INT(*(input),       \
                  *((input) + 1), \
                  *((input) + 2), \
                  *((input) + 3))

#define UINT32_TO_B0(i)  ((byte) (((i) >> 24) & 0x000000FF))
#define UINT32_TO_B1(i)  ((byte) (((i) >> 16) & 0x000000FF))
#define UINT32_TO_B2(i)  ((byte) (((i) >> 8 ) & 0x000000FF))
#define UINT32_TO_B3(i)  ((byte) (((i) >> 0 ) & 0x000000FF))

#define ARCHIVE_TAG_SIZE     4
#define ARCHIVE_SIZE_SIZE    4
#define ARCHIVE_VERSION_1    1
#define ARCHIVE_VERSION_SIZE 4

#define ARCHIVE_TAG_FILE   (B4_TO_INT('F', 'I', 'L', 'E'))
#define ARCHIVE_TAG_HEAD   (B4_TO_INT('H', 'E', 'A', 'D'))
#define ARCHIVE_TAG_TPL    (B4_TO_INT('T', 'P', 'L', ' '))

#define ARCHIVE_TAG_FILE_STR   "FILE"
#define ARCHIVE_TAG_HEAD_STR   "HEAD"
#define ARCHIVE_TAG_TPL_STR    "TPL "

#endif /* IS_LIB_COMMON_ARCHIVE_PRIV_H */
