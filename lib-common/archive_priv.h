#ifndef IS_ARCHIVE_H
#define IS_ARCHIVE_H
#define ARCHIVE_MAGIC 0x42

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

#define UINT32_TO_B0(i) ((byte) (((i) >> 24) & 0x000000FF))
#define UINT32_TO_B1(i) ((byte) (((i) >> 16) & 0x000000FF))
#define UINT32_TO_B2(i) ((byte) (((i) >> 8 ) & 0x000000FF))
#define UINT32_TO_B3(i) ((byte) (((i) >> 0 ) & 0x000000FF))

#define ARCHIVE_MAGIC_SIZE 1
#define ARCHIVE_TAG_SIZE 4
#define ARCHIVE_SIZE_SIZE 4
#define ARCHIVE_VERSION_1 1
#define ARCHIVE_VERSION_SIZE 4

#define ARCHIVE_TAG_FILE (B4_TO_INT('F', 'I', 'L', 'E'))
#define ARCHIVE_TAG_HEAD (B4_TO_INT('H', 'E', 'A', 'D'))
#define ARCHIVE_TAG_TPL  (B4_TO_INT('T', 'P', 'L', ' '))

#endif

