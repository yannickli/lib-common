/**************************************************************************/
/*                                                                        */
/*  Copyright (C) 2004-2013 INTERSEC SA                                   */
/*                                                                        */
/*  Should you receive a copy of this source code, you must check you     */
/*  have a proper, written authorization of INTERSEC to hold it. If you   */
/*  don't have such an authorization, you must DELETE all source code     */
/*  files in your possession, and inform INTERSEC of the fact you obtain  */
/*  these files. Should you not comply to these terms, you can be         */
/*  prosecuted in the extent permitted by applicable law.                 */
/*                                                                        */
/**************************************************************************/

#if !defined(IS_LIB_COMMON_HASH_H) || defined(IS_LIB_COMMON_HASH_IOP_H)
#  error "you must include <lib-common/hash.h> instead"
#else
#define IS_LIB_COMMON_HASH_IOP_H

struct iop_struct_t;

typedef void (*iop_hash_f)(void *ctx, const void *input, int ilen);

enum {
    IOP_HASH_SKIP_MISSING = 1 << 0,
};

void iop_hash(const struct iop_struct_t *st, const void *v,
              iop_hash_f hfun, void *ctx, unsigned flags) __leaf;

#define HASH(pfx, ...) \
    ({  pfx##_ctx ctx;                                                 \
        pfx##_starts(&ctx, ##__VA_ARGS__);                             \
        iop_hash(st, v, (iop_hash_f)pfx##_update, (void *)&ctx, flags);\
        pfx##_finish(&ctx, buf); })

#define HMAC(pfx, ...) \
    ({  pfx##_ctx ctx;                                                 \
        pfx##_hmac_starts(&ctx, k.s, k.len, ##__VA_ARGS__);            \
        iop_hash(st, v, (iop_hash_f)pfx##_hmac_update, (void *)&ctx,   \
                 flags);                                               \
        pfx##_hmac_finish(&ctx, buf); })

#ifdef __cplusplus
#define HASH_ARGS(sz) \
    const struct iop_struct_t *st, const void *v, uint8_t buf[sz], \
    unsigned flags
#define HMAC_ARGS(sz) \
    const struct iop_struct_t *st, const void *v, lstr_t k, uint8_t buf[sz], \
    unsigned flags
#else
#define HASH_ARGS(sz) \
    const struct iop_struct_t *st, const void *v, uint8_t buf[static sz], \
    unsigned flags
#define HMAC_ARGS(sz) \
    const struct iop_struct_t *st, const void *v, lstr_t k, \
    uint8_t buf[static sz], unsigned flags
#endif

static inline void iop_hash_md2(HASH_ARGS(16))    { HASH(md2); }
static inline void iop_hmac_md2(HMAC_ARGS(16))    { HMAC(md2); }
#define iop_hash_md2(...) (iop_hash_md2)(__VA_ARGS__, 0)
#define iop_hmac_md2(...) (iop_hmac_md2)(__VA_ARGS__, 0)

static inline void iop_hash_md4(HASH_ARGS(16))    { HASH(md4); }
static inline void iop_hmac_md4(HMAC_ARGS(16))    { HMAC(md4); }
#define iop_hash_md4(...) (iop_hash_md4)(__VA_ARGS__, 0)
#define iop_hmac_md4(...) (iop_hmac_md4)(__VA_ARGS__, 0)

static inline void iop_hash_md5(HASH_ARGS(16))    { HASH(md5); }
static inline void iop_hmac_md5(HMAC_ARGS(16))    { HMAC(md5); }
#define iop_hash_md5(...) (iop_hash_md5)(__VA_ARGS__, 0)
#define iop_hmac_md5(...) (iop_hmac_md5)(__VA_ARGS__, 0)

static inline void iop_hash_sha1(HASH_ARGS(20))   { HASH(sha1); }
static inline void iop_hmac_sha1(HMAC_ARGS(20))   { HMAC(sha1); }
#define iop_hash_sha1(...) (iop_hash_sha1)(__VA_ARGS__, 0)
#define iop_hmac_sha1(...) (iop_hmac_sha1)(__VA_ARGS__, 0)

static inline void iop_hash_sha224(HASH_ARGS(28)) { HASH(sha2, true); }
static inline void iop_hmac_sha224(HMAC_ARGS(28)) { HMAC(sha2, true); }
#define iop_hash_sha224(...) (iop_hash_sha224)(__VA_ARGS__, 0)
#define iop_hmac_sha224(...) (iop_hmac_sha224)(__VA_ARGS__, 0)

static inline void iop_hash_sha256(HASH_ARGS(32)) { HASH(sha2, false); }
static inline void iop_hmac_sha256(HMAC_ARGS(32)) { HMAC(sha2, false); }
#define iop_hash_sha256(...) (iop_hash_sha256)(__VA_ARGS__, 0)
#define iop_hmac_sha256(...) (iop_hmac_sha256)(__VA_ARGS__, 0)

static inline void iop_hash_sha384(HASH_ARGS(48)) { HASH(sha4, true); }
static inline void iop_hmac_sha384(HMAC_ARGS(48)) { HMAC(sha4, true); }
#define iop_hash_sha384(...) (iop_hash_sha384)(__VA_ARGS__, 0)
#define iop_hmac_sha384(...) (iop_hmac_sha384)(__VA_ARGS__, 0)

static inline void iop_hash_sha512(HASH_ARGS(64)) { HASH(sha4, false); }
static inline void iop_hmac_sha512(HMAC_ARGS(64)) { HMAC(sha4, false); }
#define iop_hash_sha512(...) (iop_hash_sha512)(__VA_ARGS__, 0)
#define iop_hmac_sha512(...) (iop_hmac_sha512)(__VA_ARGS__, 0)

#undef HMAC_ARGS
#undef HASH_ARGS
#undef HMAC
#undef HASH

#endif
