/***************************************************************************/
/*                                                                         */
/* Copyright 2022 INTERSEC SA                                              */
/*                                                                         */
/* Licensed under the Apache License, Version 2.0 (the "License");         */
/* you may not use this file except in compliance with the License.        */
/* You may obtain a copy of the License at                                 */
/*                                                                         */
/*     http://www.apache.org/licenses/LICENSE-2.0                          */
/*                                                                         */
/* Unless required by applicable law or agreed to in writing, software     */
/* distributed under the License is distributed on an "AS IS" BASIS,       */
/* WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.*/
/* See the License for the specific language governing permissions and     */
/* limitations under the License.                                          */
/*                                                                         */
/***************************************************************************/

#include "hpack-priv.h"
#include <lib-common/log.h>

/* {{{ Huffman encoding & decoding */

int hpack_encode_huffman(lstr_t str, void *out_, int len)
{
#define OUTPUT_BYTE(val)                                                     \
    do {                                                                     \
        *out++ = (val);                                                      \
        if (out == out_end) {                                                \
            return len;                                                      \
        }                                                                    \
    } while (0)

    uint8_t *out = (uint8_t *)out_;
    uint8_t *out_end = out + len;

    /* acts a bit buffer aligned on the left (i.e., MSB) */
    struct {
        uint64_t word;
        unsigned bits;
    } bitbuf = {0, 0};

    if (unlikely(len == 0)) {
        return 0;
    }
    for (int i = 0; i != str.len; i++) {
        uint8_t ch = str.s[i];
        uint64_t codeword64 = hpack_huffcode_tab_g[ch].codeword;
        unsigned bitlen = hpack_huffcode_tab_g[ch].bitlen;

        bitbuf.word |= codeword64 << (64 - bitlen - bitbuf.bits);
        bitbuf.bits += bitlen;
        if (bitbuf.bits > 32) {
            /* output full bytes from the left of our 64-bit bitbuf */
            for (; bitbuf.bits >= 8; bitbuf.word <<= 8, bitbuf.bits -= 8) {
                OUTPUT_BYTE(bitbuf.word >> (64 - 8));
            }
        }
    }
    /* pad to a byte boundary with one-valued bits */
    if (likely(bitbuf.bits % 8)) {
        uint64_t codeword64 = 0xFF;
        unsigned bitlen = 8 - (bitbuf.bits % 8);

        bitbuf.word |= codeword64 << (64 - 8 - bitbuf.bits);
        bitbuf.bits += bitlen;
    }
    /* output the remaining full bytes */
    for (; bitbuf.bits >= 8; bitbuf.word <<= 8, bitbuf.bits -= 8) {
        OUTPUT_BYTE(bitbuf.word >> (64 - 8));
    }
    return out - (uint8_t *)out_;
#undef OUTPUT_BYTE
}

int hpack_decode_huffman(lstr_t str, void *out_)
{
    uint8_t *out = (uint8_t *)out_;
    uint8_t state;
    const hpack_huffdec_trans_t *trans = NULL;

    if (unlikely(str.len == 0)) {
        return 0;
    }
    state = 0;
    for (int i = 0; i != str.len; i++) {
        uint8_t b = str.s[i];
        uint8_t nibbles[2];

        nibbles[0] = b >> 4;
        nibbles[1] = b & 0xF;

        for (int n = 0; n < 2; n++) {
            trans = &hpack_huffdec_trans_tab_g[state][nibbles[n]];
            THROW_ERR_IF(trans->error);
            if (trans->emitter) {
                *(out)++ = trans->sym;
            }
            state = trans->state;
        }
    }
    THROW_ERR_UNLESS(trans->final);
    return out - (uint8_t *)out_;
}

/* }}} */
/* {{{ Integer encoding & decoding */

int hpack_encode_int(uint32_t val, uint8_t prefix_bits, byte out[8])
{
    byte *out_ = out;
    uint32_t max_prefix_num;

    /* RFC 7541 §5.1: Integer Representation */
    assert(prefix_bits >= 1 && prefix_bits <= 8);
    max_prefix_num = (1u << prefix_bits) - 1;
    if (val < max_prefix_num) {
        *out++ = val;
    } else {
        *out++ = max_prefix_num;
        val -= max_prefix_num;
        for (; val >> 7; val >>= 7) {
            *out++ = 0x80u | val;
        }
        *out++ = val;
    }
    return out - out_;
}

int hpack_decode_int(pstream_t *in, uint8_t prefix_bits, uint32_t *val)
{
    byte b;
    unsigned m;
    uint64_t res;
    uint32_t max_prefix_num;

    /* RFC 7541 §5.1: Integer Representation */
    /* Implementation limits: value <= UINT32_MAX && coded size <= 8 bytes */
    assert(prefix_bits >= 1 && prefix_bits <= 8);
    max_prefix_num = (1u << prefix_bits) - 1;
    THROW_ERR_IF(ps_done(in));
    b = max_prefix_num & __ps_getc(in);
    if (b < max_prefix_num) {
        *val = b;
        return 0;
    }
    res = 0;
    m = 0;
    for (int i = 1; i < 8; i++) {
        THROW_ERR_IF(ps_done(in));
        b = __ps_getc(in);
        res |= ((uint64_t)(0x7Fu & b)) << m;
        m += 7;
        if (b < 128) {
            res += max_prefix_num;
            /* check for overflow */
            THROW_ERR_IF(res > UINT32_MAX);
            *val = res;
            return 0;
        }
    }
    /* coded integer overruns the 8-byte size limit */
    return -1;
}

/* }}} */
/* {{{ Static header table */

/* Records for the tokenized strings of the static header table */
/* Format: 4-tuple string, first char, last char, index of the (1st) match */
/* XXX: All header tokens are unique with respect to the triple (1st char,
 * last char, len). */

/* pseudo-header entries (keys) */
#define STBL_TK_pAUTHORITY          ":authority", ':', 'y', 1
#define STBL_TK_pMETHOD             ":method", ':', 'd', 2
#define STBL_TK_pPATH               ":path", ':', 'h', 4
#define STBL_TK_pSCHEME             ":scheme", ':', 'e', 6
#define STBL_TK_pSTATUS             ":status", ':', 's', 8
/* regular header entries (keys) */
#define STBL_TK_ACCEPT_CHARSET      "accept-charset", 'a', 't', 15
#define STBL_TK_ACCEPT_ENCODING     "accept-encoding", 'a', 'g', 16
#define STBL_TK_ACCEPT_LANGUAGE     "accept-language", 'a', 'e', 17
#define STBL_TK_ACCEPT_RANGES       "accept-ranges", 'a', 's', 18
#define STBL_TK_ACCEPT              "accept", 'a', 't', 19
#define STBL_TK_ACCESS_CONTROL_ALLOW_ORIGIN                                  \
                                    "access-control-allow-origin", 'a','n', 20
#define STBL_TK_AGE                 "age", 'a', 'e', 21
#define STBL_TK_ALLOW               "allow", 'a', 'w', 22
#define STBL_TK_AUTHORIZATION       "authorization", 'a', 'n', 23
#define STBL_TK_CACHE_CONTROL       "cache-control", 'c', 'l', 24
#define STBL_TK_CONTENT_DISPOSITION "content-disposition", 'c', 'n', 25
#define STBL_TK_CONTENT_ENCODING    "content-encoding", 'c', 'g', 26
#define STBL_TK_CONTENT_LANGUAGE    "content-language", 'c', 'e', 27
#define STBL_TK_CONTENT_LENGTH      "content-length", 'c', 'h', 28
#define STBL_TK_CONTENT_LOCATION    "content-location", 'c', 'n', 29
#define STBL_TK_CONTENT_RANGE       "content-range", 'c', 'e', 30
#define STBL_TK_CONTENT_TYPE        "content-type", 'c', 'e', 31
#define STBL_TK_COOKIE              "cookie", 'c', 'e', 32
#define STBL_TK_DATE                "date", 'd', 'e', 33
#define STBL_TK_ETAG                "etag", 'e', 'g', 34
#define STBL_TK_EXPECT              "expect", 'e', 't', 35
#define STBL_TK_EXPIRES             "expires", 'e', 's', 36
#define STBL_TK_FROM                "from", 'f', 'm', 37
#define STBL_TK_HOST                "host", 'h', 't', 38
#define STBL_TK_IF_MATCH            "if-match", 'i', 'h', 39
#define STBL_TK_IF_MODIFIED_SINCE   "if-modified-since", 'i', 'e', 40
#define STBL_TK_IF_NONE_MATCH       "if-none-match", 'i', 'h', 41
#define STBL_TK_IF_RANGE            "if-range", 'i', 'e', 42
#define STBL_TK_IF_UNMODIFIED_SINCE "if-unmodified-since", 'i', 'e', 43
#define STBL_TK_LAST_MODIFIED       "last-modified", 'l', 'd', 44
#define STBL_TK_LINK                "link", 'l', 'k', 45
#define STBL_TK_LOCATION            "location", 'l', 'n', 46
#define STBL_TK_MAX_FORWARDS        "max-forwards", 'm', 's', 47
#define STBL_TK_PROXY_AUTHENTICATE  "proxy-authenticate", 'p', 'e', 48
#define STBL_TK_PROXY_AUTHORIZATION "proxy-authorization", 'p', 'n', 49
#define STBL_TK_RANGE               "range", 'r', 'e', 50
#define STBL_TK_REFERER             "referer", 'r', 'r', 51
#define STBL_TK_REFRESH             "refresh", 'r', 'h', 52
#define STBL_TK_RETRY_AFTER         "retry-after", 'r', 'r', 53
#define STBL_TK_SERVER              "server", 's', 'r', 54
#define STBL_TK_SET_COOKIE          "set-cookie", 's', 'e', 55
#define STBL_TK_STRICT_TRANSPORT_SECURITY                                    \
                                    "strict-transport-security", 's', 'y', 56
#define STBL_TK_TRANSFER_ENCODING   "transfer-encoding", 't', 'g', 57
#define STBL_TK_USER_AGENT          "user-agent", 'u', 't', 58
#define STBL_TK_VARY                "vary", 'v', 'y', 59
#define STBL_TK_VIA                 "via", 'v', 'a', 60
#define STBL_TK_WWW_AUTHENTICATE    "www-authenticate", 'w', 'e', 61
/* header values  */
#define STBL_TK_GET                 "GET", 'G', 'T', 2
#define STBL_TK_POST                "POST", 'P', 'T', 3
#define STBL_TK_slash               "/", '/', '/', 4
#define STBL_TK_sINDEXdHTML         "/index.html", '/', 'l', 5
#define STBL_TK_HTTP                "http", 'h', 'p', 6
#define STBL_TK_HTTPS               "https", 'h', 's', 7
#define STBL_TK_200                 "200", '2', '0', 8
#define STBL_TK_204                 "204", '2', '4', 9
#define STBL_TK_206                 "206", '2', '6', 10
#define STBL_TK_304                 "304", '3', '4', 11
#define STBL_TK_400                 "400", '4', '0', 12
#define STBL_TK_404                 "404", '4', '4', 13
#define STBL_TK_500                 "500", '5', '0', 14
#define STBL_TK_GZIP_DEFLATE        "gzip, deflate", 'g', 'e', 16

#define EXPAND(...) __VA_ARGS__
#define DEFER(id) id EXPAND()

#define TK_C1(c1, c2, c3, c4) c1
#define TK_C2(c1, c2, c3, c4) c2
#define TK_C3(c1, c2, c3, c4) c3
#define TK_C4(c1, c2, c3, c4) c4
#define TK_REC(k) STBL_TK_##k
#define TK_STR(k) EXPAND(DEFER(TK_C1)(TK_REC(k)))
#define TK_BEG(k) EXPAND(DEFER(TK_C2)(TK_REC(k)))
#define TK_END(k) EXPAND(DEFER(TK_C3)(TK_REC(k)))
#define TK_IDX(k) EXPAND(DEFER(TK_C4)(TK_REC(k)))
#define TK_LEN(k) ((sizeof "" TK_STR(k)) - 1)
#define TK_MIN_KEY_LEN TK_LEN(AGE)
#define TK_MAX_KEY_LEN TK_LEN(ACCESS_CONTROL_ALLOW_ORIGIN)
#define TK_MIN_VAL_LEN TK_LEN(slash)
#define TK_MAX_VAL_LEN TK_LEN(GZIP_DEFLATE)
#define TK_MIN_KEY_IDX 1 /* :authority */
#define TK_MIN_REG_KEY_IDX TK_IDX(ACCEPT_CHARSET)
#define TK_MAX_KEY_IDX TK_IDX(WWW_AUTHENTICATE)
#define TK_MIN_VAL_IDX TK_IDX(GET)
#define TK_MAX_VAL_IDX TK_IDX(GZIP_DEFLATE)

/* XXX: useful pragmas to help us detect collisions in the hand-crafted
 * perfect hash functions below. This is useful if we want to experiment with
 * their parameters and expressions and have compiler errors as feedback upon
 * collisions. However, we don't rely on them and instead rely on the unit
 * tests */
#pragma GCC diagnostic push
#pragma GCC diagnostic error "-Woverride-init"

#define STBL_ITEM_EMPTY_VAL(k) { LSTR_IMMED(TK_STR(k)), LSTR_EMPTY }
#define STBL_ITEM(k, v) { LSTR_IMMED(TK_STR(k)), LSTR_IMMED(TK_STR(v)) }

static const struct {
    lstr_t key;
    lstr_t val;
} hpack_stbl_g[] = {
    /* unused entry: index 0 signals unsuccessful search */
    {LSTR_EMPTY, LSTR_EMPTY},
    /* entries */
    STBL_ITEM_EMPTY_VAL(pAUTHORITY),
    STBL_ITEM(pMETHOD, GET),
    STBL_ITEM(pMETHOD, POST),
    STBL_ITEM(pPATH, slash),
    STBL_ITEM(pPATH, sINDEXdHTML),
    STBL_ITEM(pSCHEME, HTTP),
    STBL_ITEM(pSCHEME, HTTPS),
    STBL_ITEM(pSTATUS, 200),
    STBL_ITEM(pSTATUS, 204),
    STBL_ITEM(pSTATUS, 206),
    STBL_ITEM(pSTATUS, 304),
    STBL_ITEM(pSTATUS, 400),
    STBL_ITEM(pSTATUS, 404),
    STBL_ITEM(pSTATUS, 500),
    STBL_ITEM_EMPTY_VAL(ACCEPT_CHARSET),
    STBL_ITEM(ACCEPT_ENCODING, GZIP_DEFLATE),
    STBL_ITEM_EMPTY_VAL(ACCEPT_LANGUAGE),
    STBL_ITEM_EMPTY_VAL(ACCEPT_RANGES),
    STBL_ITEM_EMPTY_VAL(ACCEPT),
    STBL_ITEM_EMPTY_VAL(ACCESS_CONTROL_ALLOW_ORIGIN),
    STBL_ITEM_EMPTY_VAL(AGE),
    STBL_ITEM_EMPTY_VAL(ALLOW),
    STBL_ITEM_EMPTY_VAL(AUTHORIZATION),
    STBL_ITEM_EMPTY_VAL(CACHE_CONTROL),
    STBL_ITEM_EMPTY_VAL(CONTENT_DISPOSITION),
    STBL_ITEM_EMPTY_VAL(CONTENT_ENCODING),
    STBL_ITEM_EMPTY_VAL(CONTENT_LANGUAGE),
    STBL_ITEM_EMPTY_VAL(CONTENT_LENGTH),
    STBL_ITEM_EMPTY_VAL(CONTENT_LOCATION),
    STBL_ITEM_EMPTY_VAL(CONTENT_RANGE),
    STBL_ITEM_EMPTY_VAL(CONTENT_TYPE),
    STBL_ITEM_EMPTY_VAL(COOKIE),
    STBL_ITEM_EMPTY_VAL(DATE),
    STBL_ITEM_EMPTY_VAL(ETAG),
    STBL_ITEM_EMPTY_VAL(EXPECT),
    STBL_ITEM_EMPTY_VAL(EXPIRES),
    STBL_ITEM_EMPTY_VAL(FROM),
    STBL_ITEM_EMPTY_VAL(HOST),
    STBL_ITEM_EMPTY_VAL(IF_MATCH),
    STBL_ITEM_EMPTY_VAL(IF_MODIFIED_SINCE),
    STBL_ITEM_EMPTY_VAL(IF_NONE_MATCH),
    STBL_ITEM_EMPTY_VAL(IF_RANGE),
    STBL_ITEM_EMPTY_VAL(IF_UNMODIFIED_SINCE),
    STBL_ITEM_EMPTY_VAL(LAST_MODIFIED),
    STBL_ITEM_EMPTY_VAL(LINK),
    STBL_ITEM_EMPTY_VAL(LOCATION),
    STBL_ITEM_EMPTY_VAL(MAX_FORWARDS),
    STBL_ITEM_EMPTY_VAL(PROXY_AUTHENTICATE),
    STBL_ITEM_EMPTY_VAL(PROXY_AUTHORIZATION),
    STBL_ITEM_EMPTY_VAL(RANGE),
    STBL_ITEM_EMPTY_VAL(REFERER),
    STBL_ITEM_EMPTY_VAL(REFRESH),
    STBL_ITEM_EMPTY_VAL(RETRY_AFTER),
    STBL_ITEM_EMPTY_VAL(SERVER),
    STBL_ITEM_EMPTY_VAL(SET_COOKIE),
    STBL_ITEM_EMPTY_VAL(STRICT_TRANSPORT_SECURITY),
    STBL_ITEM_EMPTY_VAL(TRANSFER_ENCODING),
    STBL_ITEM_EMPTY_VAL(USER_AGENT),
    STBL_ITEM_EMPTY_VAL(VARY),
    STBL_ITEM_EMPTY_VAL(VIA),
    STBL_ITEM_EMPTY_VAL(WWW_AUTHENTICATE),
};

#define STBL_ITEMS_COUNT (countof(hpack_stbl_g))
/* indexes into dynamic table are shifted by this amount */
#define HPACK_STBL_IDX_MAX (STBL_ITEMS_COUNT - 1) /* 61 */

#define KEY_HASH_SLOTS 253
#define KEY_HASH_EXP(b, e, len) ((b) + 16u * (e) + 38u * (len) + (b == 'd'))
#define KEY_HASH(k)                                                          \
    KEY_HASH_EXP((unsigned)TK_BEG(k), (unsigned)TK_END(k),                   \
                 (unsigned)TK_LEN(k))
#define KEY_HASH_ITEM(k) [KEY_HASH(k) % KEY_HASH_SLOTS] = TK_IDX(k)

static uint8_t hpack_stbl_key_hash_g[KEY_HASH_SLOTS] = {
    KEY_HASH_ITEM(pAUTHORITY),
    KEY_HASH_ITEM(pMETHOD),
    KEY_HASH_ITEM(pPATH),
    KEY_HASH_ITEM(pSCHEME),
    KEY_HASH_ITEM(pSTATUS),
    KEY_HASH_ITEM(ACCEPT_CHARSET),
    KEY_HASH_ITEM(ACCEPT_ENCODING),
    KEY_HASH_ITEM(ACCEPT_LANGUAGE),
    KEY_HASH_ITEM(ACCEPT_RANGES),
    KEY_HASH_ITEM(ACCEPT),
    KEY_HASH_ITEM(ACCESS_CONTROL_ALLOW_ORIGIN),
    KEY_HASH_ITEM(AGE),
    KEY_HASH_ITEM(ALLOW),
    KEY_HASH_ITEM(AUTHORIZATION),
    KEY_HASH_ITEM(CACHE_CONTROL),
    KEY_HASH_ITEM(CONTENT_DISPOSITION),
    KEY_HASH_ITEM(CONTENT_ENCODING),
    KEY_HASH_ITEM(CONTENT_LANGUAGE),
    KEY_HASH_ITEM(CONTENT_LENGTH),
    KEY_HASH_ITEM(CONTENT_LOCATION),
    KEY_HASH_ITEM(CONTENT_RANGE),
    KEY_HASH_ITEM(CONTENT_TYPE),
    KEY_HASH_ITEM(COOKIE),
    KEY_HASH_ITEM(DATE),
    KEY_HASH_ITEM(ETAG),
    KEY_HASH_ITEM(EXPECT),
    KEY_HASH_ITEM(EXPIRES),
    KEY_HASH_ITEM(FROM),
    KEY_HASH_ITEM(HOST),
    KEY_HASH_ITEM(IF_MATCH),
    KEY_HASH_ITEM(IF_MODIFIED_SINCE),
    KEY_HASH_ITEM(IF_NONE_MATCH),
    KEY_HASH_ITEM(IF_RANGE),
    KEY_HASH_ITEM(IF_UNMODIFIED_SINCE),
    KEY_HASH_ITEM(LAST_MODIFIED),
    KEY_HASH_ITEM(LINK),
    KEY_HASH_ITEM(LOCATION),
    KEY_HASH_ITEM(MAX_FORWARDS),
    KEY_HASH_ITEM(PROXY_AUTHENTICATE),
    KEY_HASH_ITEM(PROXY_AUTHORIZATION),
    KEY_HASH_ITEM(RANGE),
    KEY_HASH_ITEM(REFERER),
    KEY_HASH_ITEM(REFRESH),
    KEY_HASH_ITEM(RETRY_AFTER),
    KEY_HASH_ITEM(SERVER),
    KEY_HASH_ITEM(SET_COOKIE),
    KEY_HASH_ITEM(STRICT_TRANSPORT_SECURITY),
    KEY_HASH_ITEM(TRANSFER_ENCODING),
    KEY_HASH_ITEM(USER_AGENT),
    KEY_HASH_ITEM(VARY),
    KEY_HASH_ITEM(VIA),
    KEY_HASH_ITEM(WWW_AUTHENTICATE),
};

static int hpack_stbl_find_hdr_by_key(const lstr_t *key)
{
    unsigned len = key->len;

    if (len >= TK_MIN_KEY_LEN && len <= TK_MAX_KEY_LEN) {
        unsigned b = tolower((unsigned char) key->s[0]);
        unsigned e = tolower((unsigned char) key->s[len - 1]);
        unsigned slot = KEY_HASH_EXP(b, e, len) % KEY_HASH_SLOTS;
        unsigned idx = hpack_stbl_key_hash_g[slot];

        if (idx) {
            assert(idx < STBL_ITEMS_COUNT);
            if (lstr_ascii_iequal(*key, hpack_stbl_g[idx].key)) {
                return idx;
            }
        }
    }
    return 0;
}

#define VAL_HASH_SLOTS 31
#define VAL_HASH_EXP(b, e, len) ((b) + 4u * (e) + (len))
#define VAL_HASH(k)                                                          \
    VAL_HASH_EXP((unsigned)TK_BEG(k), (unsigned)TK_END(k),                   \
                 (unsigned)TK_LEN(k))
#define VAL_HASH_ITEM(k) [VAL_HASH(k) % VAL_HASH_SLOTS] = TK_IDX(k)

static const uint8_t hpack_stbl_val_hash_g[VAL_HASH_SLOTS] = {
    VAL_HASH_ITEM(GET),   VAL_HASH_ITEM(POST),
    VAL_HASH_ITEM(slash), VAL_HASH_ITEM(sINDEXdHTML),
    VAL_HASH_ITEM(HTTP),  VAL_HASH_ITEM(HTTPS),
    VAL_HASH_ITEM(200),   VAL_HASH_ITEM(204),
    VAL_HASH_ITEM(206),   VAL_HASH_ITEM(304),
    VAL_HASH_ITEM(400),   VAL_HASH_ITEM(404),
    VAL_HASH_ITEM(500),   VAL_HASH_ITEM(GZIP_DEFLATE),
};

static int hpack_stbl_find_hdr_by_val(const lstr_t *val)
{
    unsigned len = val->len;

    if (len >= TK_MIN_VAL_LEN && len <= TK_MAX_VAL_LEN) {
        unsigned b = val->s[0];
        unsigned e = val->s[len - 1];
        unsigned slot = VAL_HASH_EXP(b, e, len) % VAL_HASH_SLOTS;
        unsigned idx = hpack_stbl_val_hash_g[slot];

        if (idx) {
            assert(idx <= TK_MAX_VAL_IDX);
            if (lstr_equal(*val, hpack_stbl_g[idx].val)) {
                return idx;
            }
        }
    }
    return 0;
}

int hpack_stbl_find_hdr(lstr_t key, lstr_t val)
{
    if (val.s) {
        int idx = hpack_stbl_find_hdr_by_val(&val);

        if (idx) {
            if (lstr_ascii_iequal(key, hpack_stbl_g[idx].key)) {
                return idx;
            }
        }
        idx = hpack_stbl_find_hdr_by_key(&key);
        /* full match for headers with empty values */
        if (idx && !val.len && !hpack_stbl_g[idx].val.len) {
            return idx;
        }
        /* partial match or none */
        return -idx;
    } else {
        return hpack_stbl_find_hdr_by_key(&key);
    }
}

#pragma GCC diagnostic pop

/* }}} */
/* {{{ Dynamic tables */

/* 32 bytes are the additional overhead defined by the RFC 7541 */
#define HPACK_HDR_SIZE(keylen, vallen) ((keylen) + (vallen) + 32)

/* {{{ Encoder's DTBL */

static void hpack_enc_dtbl_ent_wipe(hpack_enc_dtbl_entry_t *e)
{
}

RING_FUNCTIONS(hpack_enc_dtbl_entry_t, hpack_enc_dtbl,
               hpack_enc_dtbl_ent_wipe)

hpack_enc_dtbl_t *hpack_enc_dtbl_init(hpack_enc_dtbl_t *dtbl)
{
    p_clear(dtbl, 1);
    qm_init(hpack_insertion_idx, &dtbl->ins_idx);
    return dtbl;
}

void hpack_enc_dtbl_wipe(hpack_enc_dtbl_t *dtbl)
{
    hpack_enc_dtbl_ring_wipe(&dtbl->entries);
    qm_wipe(hpack_insertion_idx, &dtbl->ins_idx);
}

static void hpack_enc_dtbl_evict_last_entry(hpack_enc_dtbl_t *dtbl)
{
    hpack_enc_dtbl_entry_t e;

    /* XXX: placate compilers which complain that e may be uninitialized */
    if (!hpack_enc_dtbl_ring_pop(&dtbl->entries, &e)) {
        assert(0 && "should not happen");
        return;
    }
    assert(dtbl->tbl_size >= e.sz);
    dtbl->tbl_size -= e.sz;
    assert(!e.mre_key || e.mre_val);
    if (e.mre_key) {
        assert(e.key_id);
        qm_del_key(hpack_insertion_idx, &dtbl->ins_idx, e.key_id);
    }
    if (e.mre_val) {
        assert(e.key_id && e.val_id);
        qm_del_key(hpack_insertion_idx, &dtbl->ins_idx,
                   e.key_id | e.val_id << 16);
    }
}

__unused__
static void hpack_enc_dtbl_resize(hpack_enc_dtbl_t *dtbl)
{
    assert(dtbl->tbl_size_limit <= dtbl->tbl_size_max);
    while (dtbl->tbl_size > dtbl->tbl_size_limit) {
        hpack_enc_dtbl_evict_last_entry(dtbl);
    }
}

static hpack_enc_dtbl_entry_t *
hpack_enc_dtbl_get_ent_at_ins(hpack_enc_dtbl_t *dtbl, uint32_t ins_idx)
{
    int idx = dtbl->ins_cnt - ins_idx;

    assert(idx >= 0 && idx < dtbl->entries.len);
    return hpack_enc_dtbl_ring_get_ptr(&dtbl->entries, idx);
}

void
hpack_enc_dtbl_add_hdr(hpack_enc_dtbl_t *dtbl, lstr_t key, lstr_t val,
                       uint16_t key_id, uint16_t val_id)
{
    uint32_t pos;
    uint32_t ins_idx;
    hpack_enc_dtbl_entry_t e = {
        .sz = HPACK_HDR_SIZE(key.len, val.len),
        .key_id = key_id,
        .val_id = val_id,
        .mre_key = key_id,
        .mre_val = val_id,
    };

    /*XXX: a big-sized entry may evict all entries without being added to the
     * dtbl */
    while (dtbl->tbl_size && dtbl->tbl_size + e.sz > dtbl->tbl_size_limit) {
        hpack_enc_dtbl_evict_last_entry(dtbl);
    }
    if (e.sz > dtbl->tbl_size_limit) {
        return;
    }
    hpack_enc_dtbl_ring_unshift(&dtbl->entries, e);
    dtbl->tbl_size += e.sz;
    dtbl->ins_cnt++;
    if (!key_id) {
        return;
    }
    pos = qm_reserve(hpack_insertion_idx, &dtbl->ins_idx, key_id, 0);
    if (pos & QHASH_COLLISION) {
        pos &= ~QHASH_COLLISION;
        ins_idx = dtbl->ins_idx.values[pos];
        hpack_enc_dtbl_get_ent_at_ins(dtbl, ins_idx)->mre_key = false;
    }
    dtbl->ins_idx.values[pos] = dtbl->ins_cnt;
    if (!val_id) {
        return;
    }
    pos = qm_reserve(hpack_insertion_idx, &dtbl->ins_idx,
                     key_id | val_id << 16u, 0);
    if (pos & QHASH_COLLISION) {
        pos &= ~QHASH_COLLISION;
        ins_idx = dtbl->ins_idx.values[pos];
        hpack_enc_dtbl_get_ent_at_ins(dtbl, ins_idx)->mre_val = false;
    }
    dtbl->ins_idx.values[pos] = dtbl->ins_cnt;
}

int hpack_enc_dtbl_find_hdr(hpack_enc_dtbl_t *dtbl, uint16_t key_id,
                            uint16_t val_id)
{
    int pos;
    int idx;

    pos = qm_find(hpack_insertion_idx, &dtbl->ins_idx, key_id | val_id << 16);
    if (pos >= 0) {
        idx = dtbl->ins_cnt - dtbl->ins_idx.values[pos];
        assert(idx >= 0 && idx < dtbl->entries.len);
        return 1 + idx;
    }
    if (!val_id) {
        return 0;
    }
    pos = qm_find(hpack_insertion_idx, &dtbl->ins_idx, key_id);
    if (pos >= 0) {
        idx = dtbl->ins_cnt - dtbl->ins_idx.values[pos];
        assert(idx >= 0 && idx < dtbl->entries.len);
        return -(1 + idx);
    }
    return 0;
}

/* }}} */
/* {{{ Decoder's DTBL */

static void hpack_dec_dtbl_ent_wipe(hpack_dec_dtbl_entry_t *e)
{
    lstr_wipe(&e->key);
    lstr_wipe(&e->val);
}

RING_FUNCTIONS(hpack_dec_dtbl_entry_t, hpack_dec_dtbl,
               hpack_dec_dtbl_ent_wipe)

hpack_dec_dtbl_t *hpack_dec_dtbl_init(hpack_dec_dtbl_t *dtbl)
{
    p_clear(dtbl, 1);
    return dtbl;
}

void hpack_dec_dtbl_wipe(hpack_dec_dtbl_t *dtbl)
{
    hpack_dec_dtbl_ring_wipe(&dtbl->entries);
}

static void hpack_dec_dtbl_evict_last_entry(hpack_dec_dtbl_t *dtbl)
{
    hpack_dec_dtbl_entry_t e;

    /* XXX: placate compilers which complain that e may be uninitialized */
    if (!hpack_dec_dtbl_ring_pop(&dtbl->entries, &e)) {
        assert(0 && "should not happen");
        return;
    }
    assert(dtbl->tbl_size >= (uint32_t)HPACK_HDR_SIZE(e.key.len, e.val.len));
    dtbl->tbl_size -= HPACK_HDR_SIZE(e.key.len, e.val.len);
    lstr_wipe(&e.key);
    lstr_wipe(&e.val);
}

__unused__
static void hpack_dec_dtbl_resize(hpack_dec_dtbl_t *dtbl)
{
    assert(dtbl->tbl_size_limit <= dtbl->tbl_size_max);
    while (dtbl->tbl_size > dtbl->tbl_size_limit) {
        hpack_dec_dtbl_evict_last_entry(dtbl);
    }
}

void hpack_dec_dtbl_add_hdr(hpack_dec_dtbl_t *dtbl, lstr_t key, lstr_t val)
{
    uint32_t e_sz = HPACK_HDR_SIZE(key.len, val.len);
    hpack_dec_dtbl_entry_t e = {
        .key = key,
        .val = val,
    };

    /*XXX: a big-sized entry may evict all entries without being added to the
     * dtbl */
    while (dtbl->tbl_size && dtbl->tbl_size + e_sz > dtbl->tbl_size_limit) {
        hpack_dec_dtbl_evict_last_entry(dtbl);
    }
    if (e_sz > dtbl->tbl_size_limit) {
        lstr_wipe(&key);
        lstr_wipe(&val);
        return;
    }
    hpack_dec_dtbl_ring_unshift(&dtbl->entries, e);
    dtbl->tbl_size += e_sz;
}

hpack_dec_dtbl_entry_t *
hpack_dec_dtbl_get_ent(hpack_dec_dtbl_t *dtbl, int idx)
{
    assert(idx >= 0 && idx < dtbl->entries.len);
    return hpack_dec_dtbl_ring_get_ptr(&dtbl->entries, idx);
}

/* }}} */
/* }}} */
