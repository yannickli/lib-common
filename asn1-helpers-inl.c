/**************************************************************************/
/*                                                                        */
/*  Copyright (C) INTERSEC SA                                             */
/*                                                                        */
/*  Should you receive a copy of this source code, you must check you     */
/*  have a proper, written authorization of INTERSEC to hold it. If you   */
/*  don't have such an authorization, you must DELETE all source code     */
/*  files in your possession, and inform INTERSEC of the fact you obtain  */
/*  these files. Should you not comply to these terms, you can be         */
/*  prosecuted in the extent permitted by applicable law.                 */
/*                                                                        */
/**************************************************************************/

#ifndef IS_LIB_COMMON_ASN1_TOOLS_H
#define IS_LIB_COMMON_ASN1_TOOLS_H

static ALWAYS_INLINE size_t asn1_int32_size(int32_t i32)
{
    int32_t zzi = (i32 >> 31) ^ (i32 << 1);

    return 1 + bsr32(zzi | 1) / 8;
}

static ALWAYS_INLINE size_t asn1_int64_size(int64_t i64)
{
    int64_t zzi = (i64 >> 63) ^ (i64 << 1);

    return 1 + bsr64(zzi | 1) / 8;
}

static ALWAYS_INLINE size_t asn1_uint32_size(uint32_t u32)
{
    return asn1_int64_size(u32);
}

static ALWAYS_INLINE size_t asn1_uint64_size(uint64_t u64)
{
    if (unlikely((0x1ULL << 63) & u64))
        return 9;

    return asn1_int64_size(u64);
}

static ALWAYS_INLINE size_t asn1_length_size(uint32_t len)
{
    if (len < 0x80)
        return 1;

    return 2 + bsr32(len) / 8;
}

static ALWAYS_INLINE size_t u64_blen(int64_t u64)
{
    if (likely(u64)) {
        return bsr64(u64) + 1;
    }

    return 0;
}

static ALWAYS_INLINE size_t u16_blen(uint16_t u16)
{
    if (likely(u16)) {
        return bsr16(u16) + 1;
    }

    return 0;
}

static ALWAYS_INLINE size_t i64_olen(int64_t i)
{
    return asn1_int64_size(i);
}

static ALWAYS_INLINE size_t u64_olen(uint64_t u)
{
    if (!u) {
        return 1;
    }

    return 1 + (bsr64(u) / 8);
}

#endif
