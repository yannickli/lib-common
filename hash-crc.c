/**************************************************************************/
/*                                                                        */
/*  Copyright (C) 2004-2009 INTERSEC SAS                                  */
/*                                                                        */
/*  Should you receive a copy of this source code, you must check you     */
/*  have a proper, written authorization of INTERSEC to hold it. If you   */
/*  don't have such an authorization, you must DELETE all source code     */
/*  files in your possession, and inform INTERSEC of the fact you obtain  */
/*  these files. Should you not comply to these terms, you can be         */
/*  prosecuted in the extent permitted by applicable law.                 */
/*                                                                        */
/**************************************************************************/

#include "hash.h"

static uint32_t const crc32tab[256] = {
#define X(x) LE32_T(x)
    X(0x00000000), X(0x77073096), X(0xee0e612c), X(0x990951ba),
    X(0x076dc419), X(0x706af48f), X(0xe963a535), X(0x9e6495a3),
    X(0x0edb8832), X(0x79dcb8a4), X(0xe0d5e91e), X(0x97d2d988),
    X(0x09b64c2b), X(0x7eb17cbd), X(0xe7b82d07), X(0x90bf1d91),
    X(0x1db71064), X(0x6ab020f2), X(0xf3b97148), X(0x84be41de),
    X(0x1adad47d), X(0x6ddde4eb), X(0xf4d4b551), X(0x83d385c7),
    X(0x136c9856), X(0x646ba8c0), X(0xfd62f97a), X(0x8a65c9ec),
    X(0x14015c4f), X(0x63066cd9), X(0xfa0f3d63), X(0x8d080df5),
    X(0x3b6e20c8), X(0x4c69105e), X(0xd56041e4), X(0xa2677172),
    X(0x3c03e4d1), X(0x4b04d447), X(0xd20d85fd), X(0xa50ab56b),
    X(0x35b5a8fa), X(0x42b2986c), X(0xdbbbc9d6), X(0xacbcf940),
    X(0x32d86ce3), X(0x45df5c75), X(0xdcd60dcf), X(0xabd13d59),
    X(0x26d930ac), X(0x51de003a), X(0xc8d75180), X(0xbfd06116),
    X(0x21b4f4b5), X(0x56b3c423), X(0xcfba9599), X(0xb8bda50f),
    X(0x2802b89e), X(0x5f058808), X(0xc60cd9b2), X(0xb10be924),
    X(0x2f6f7c87), X(0x58684c11), X(0xc1611dab), X(0xb6662d3d),
    X(0x76dc4190), X(0x01db7106), X(0x98d220bc), X(0xefd5102a),
    X(0x71b18589), X(0x06b6b51f), X(0x9fbfe4a5), X(0xe8b8d433),
    X(0x7807c9a2), X(0x0f00f934), X(0x9609a88e), X(0xe10e9818),
    X(0x7f6a0dbb), X(0x086d3d2d), X(0x91646c97), X(0xe6635c01),
    X(0x6b6b51f4), X(0x1c6c6162), X(0x856530d8), X(0xf262004e),
    X(0x6c0695ed), X(0x1b01a57b), X(0x8208f4c1), X(0xf50fc457),
    X(0x65b0d9c6), X(0x12b7e950), X(0x8bbeb8ea), X(0xfcb9887c),
    X(0x62dd1ddf), X(0x15da2d49), X(0x8cd37cf3), X(0xfbd44c65),
    X(0x4db26158), X(0x3ab551ce), X(0xa3bc0074), X(0xd4bb30e2),
    X(0x4adfa541), X(0x3dd895d7), X(0xa4d1c46d), X(0xd3d6f4fb),
    X(0x4369e96a), X(0x346ed9fc), X(0xad678846), X(0xda60b8d0),
    X(0x44042d73), X(0x33031de5), X(0xaa0a4c5f), X(0xdd0d7cc9),
    X(0x5005713c), X(0x270241aa), X(0xbe0b1010), X(0xc90c2086),
    X(0x5768b525), X(0x206f85b3), X(0xb966d409), X(0xce61e49f),
    X(0x5edef90e), X(0x29d9c998), X(0xb0d09822), X(0xc7d7a8b4),
    X(0x59b33d17), X(0x2eb40d81), X(0xb7bd5c3b), X(0xc0ba6cad),
    X(0xedb88320), X(0x9abfb3b6), X(0x03b6e20c), X(0x74b1d29a),
    X(0xead54739), X(0x9dd277af), X(0x04db2615), X(0x73dc1683),
    X(0xe3630b12), X(0x94643b84), X(0x0d6d6a3e), X(0x7a6a5aa8),
    X(0xe40ecf0b), X(0x9309ff9d), X(0x0a00ae27), X(0x7d079eb1),
    X(0xf00f9344), X(0x8708a3d2), X(0x1e01f268), X(0x6906c2fe),
    X(0xf762575d), X(0x806567cb), X(0x196c3671), X(0x6e6b06e7),
    X(0xfed41b76), X(0x89d32be0), X(0x10da7a5a), X(0x67dd4acc),
    X(0xf9b9df6f), X(0x8ebeeff9), X(0x17b7be43), X(0x60b08ed5),
    X(0xd6d6a3e8), X(0xa1d1937e), X(0x38d8c2c4), X(0x4fdff252),
    X(0xd1bb67f1), X(0xa6bc5767), X(0x3fb506dd), X(0x48b2364b),
    X(0xd80d2bda), X(0xaf0a1b4c), X(0x36034af6), X(0x41047a60),
    X(0xdf60efc3), X(0xa867df55), X(0x316e8eef), X(0x4669be79),
    X(0xcb61b38c), X(0xbc66831a), X(0x256fd2a0), X(0x5268e236),
    X(0xcc0c7795), X(0xbb0b4703), X(0x220216b9), X(0x5505262f),
    X(0xc5ba3bbe), X(0xb2bd0b28), X(0x2bb45a92), X(0x5cb36a04),
    X(0xc2d7ffa7), X(0xb5d0cf31), X(0x2cd99e8b), X(0x5bdeae1d),
    X(0x9b64c2b0), X(0xec63f226), X(0x756aa39c), X(0x026d930a),
    X(0x9c0906a9), X(0xeb0e363f), X(0x72076785), X(0x05005713),
    X(0x95bf4a82), X(0xe2b87a14), X(0x7bb12bae), X(0x0cb61b38),
    X(0x92d28e9b), X(0xe5d5be0d), X(0x7cdcefb7), X(0x0bdbdf21),
    X(0x86d3d2d4), X(0xf1d4e242), X(0x68ddb3f8), X(0x1fda836e),
    X(0x81be16cd), X(0xf6b9265b), X(0x6fb077e1), X(0x18b74777),
    X(0x88085ae6), X(0xff0f6a70), X(0x66063bca), X(0x11010b5c),
    X(0x8f659eff), X(0xf862ae69), X(0x616bffd3), X(0x166ccf45),
    X(0xa00ae278), X(0xd70dd2ee), X(0x4e048354), X(0x3903b3c2),
    X(0xa7672661), X(0xd06016f7), X(0x4969474d), X(0x3e6e77db),
    X(0xaed16a4a), X(0xd9d65adc), X(0x40df0b66), X(0x37d83bf0),
    X(0xa9bcae53), X(0xdebb9ec5), X(0x47b2cf7f), X(0x30b5ffe9),
    X(0xbdbdf21c), X(0xcabac28a), X(0x53b39330), X(0x24b4a3a6),
    X(0xbad03605), X(0xcdd70693), X(0x54de5729), X(0x23d967bf),
    X(0xb3667a2e), X(0xc4614ab8), X(0x5d681b02), X(0x2a6f2b94),
    X(0xb40bbe37), X(0xc30c8ea1), X(0x5a05df1b), X(0x2d02ef8d),
#undef X
};

/* Simplistic crc32 calculator, almost compatible with zlib version,
 * except for crc type as uint32_t instead of unsigned long
 */
static uint32_t naive_icrc32(uint32_t crc, const uint8_t *b, ssize_t len)
{
    while (len-- > 0) {
        crc = (crc >> 8) ^ crc32tab[(crc ^ *b++) & 0xff];
    }
    return crc;
}

/* http://www.cl.cam.ac.uk/research/srg/bluebook/21/crc/node6.html, algo 4 */
static uint32_t fast_icrc32(uint32_t crc, const uint8_t *p, size_t len)
{
    size_t words;

    crc = cpu_to_le32(crc);

#if __BYTE_ORDER == __LITTLE_ENDIAN
#   define CRC(x)  crc = crc32tab[(crc ^ (x)) & 0xff] ^ (crc >> 8)
#else
#   define CRC(x)  crc = crc32tab[(crc >> 24) ^ (x)]  ^ (crc << 8)
#endif

    if (unlikely((uintptr_t)p & 3)) {
        len -= (uintptr_t)p & 3;
        do {
            CRC(*p++);
        } while ((uintptr_t)p & 3);
    }

    words = len >> 2;
    do {
        crc ^= *(const uint32_t *)p;
        CRC(0);
        CRC(0);
        CRC(0);
        CRC(0);
        p += 4;
    } while (--words);
    len &= 3;

    if (len) {
        do {
            CRC(*p++);
        } while (--len);
    }
#undef CRC

    return le_to_cpu32(crc);
}

__attribute__((flatten))
uint32_t icrc32(uint32_t crc, const void *data, ssize_t len)
{
    if (len < 64)
        return ~naive_icrc32(~crc, data, len);
    return ~fast_icrc32(~crc, data, len);
}
