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

#if !defined(IS_LIB_COMMON_STR_H) || defined(IS_LIB_COMMON_STR_BUF_H)
#  error "you must include <lib-common/str.h> instead"
#else
#define IS_LIB_COMMON_STR_BUF_H

/* sb_t is a wrapper type for a reallocatable byte array.  Its internal
 * representation is accessible to client code but care must be exercised to
 * preserve strbuf invariants and avoid dangling pointers when strbuf data is
 * reallocated by strbuf functions.
 *
 * Here is a description of the strbuf fields:
 *
 * - <data> is a pointer to the active portion of the byte array.  As <data>
 *   may be reallocated by strbuf operations, it is error prone to copy its
 *   value to a local variable for content parsing or other such
 *   manipulations.  As <data> may point to non allocated storage, static or
 *   dynamic or automatic, returning its value is guaranteed to cause
 *   potential problems, current of future.  If some strbuf data was skipped,
 *   <data> may no longer point to the beginning of the original or allocated
 *   array.  <data> cannot be NULL.  It is initialized as the address of a
 *   global 1 byte array, or a buffer with automatic storage.
 *
 * - <len> is the length in bytes of the strbuf contents.  A strbuf invariant
 *   specifies that data[len] == '\0'.  This invariant must be kept at all
 *   times, and implies that data storage extend at least one byte beyond
 *   <len>. <len> is always positive or zero. <len> is initialized to zero for
 *   an empty strbuf.
 *
 * - <size> is the number of bytes available for strbuf contents starting at
 *   <data>.  The strbuf invariant implies that size > len.
 *
 * - <skip> is a count of byte skipped from the beginning of the original data
 *   buffer.  It is used for efficient pruning of initial bytes.
 *
 * - <allocated> is a 1 bit boolean to indicate if strbuf data was allocated
 *   with p_new and must be freed.  Actual allocated buffer starts at .data -
 *   .skip.
 *
 * strbuf invariants:
 * - sb.data != NULL
 * - sb.len >= 0
 * - sb.size > sb.len
 * - sb.data - sb.skip points to an array of at least sb.size + sb.skip bytes.
 * - sb.data[sb.len] == '\0'
 * - if sb.must_free, sb.data - sb.skip is a pointer handled by ialloc /
 *   ifree.
 */
typedef struct sb_t {
    char *data;
    int len, size;
    flag_t must_free  :  1;
    unsigned int skip : 31;
} sb_t;

/* Default byte array for empty strbufs. It should always stay equal to
 * \0 and is writeable to simplify some strbuf functions that enforce the
 * invariant by writing \0 at data[len].
 * This data is thread specific for obscure and ugly reasons: some strbuf
 * functions may temporarily overwrite the '\0' and would cause
 * spurious bugs in other threads sharing the same strbuf data.
 * It would be much cleaner if this array was const.
 */
extern __thread char __sb_slop[1];


/**************************************************************************/
/* Initialization                                                         */
/**************************************************************************/

static inline sb_t *
sb_init_full(sb_t *sb, void *buf, int blen, int bsize, bool own)
{
    *sb = (sb_t){
        .data = buf,
        .len = blen,
        .size = bsize,
        .must_free = own,
    };
    assert (blen < bsize);
    sb->data[blen] = '\0';
    return sb;
}

/* Warning: size is evaluated multiple times, but use of alloca
 * requires implementation as a macro.  size should be a constant
 * anyway.
 */
#define sb_inita(sb, sz)  sb_init_full(sb, alloca(sz), 0, (sz), false)

#define SB(name, sz) \
    sb_t name = {                                       \
        .data = alloca(sz),                             \
        .size = (STATIC_ASSERT((sz) < (64 << 10)), sz), \
    }

#define SB_1k(name)    SB(name, 1 << 10)
#define SB_8k(name)    SB(name, 8 << 10)

static inline sb_t *sb_init(sb_t *sb)
{
    return sb_init_full(sb, __sb_slop, 0, 1, false);
}
GENERIC_NEW(sb_t, sb);
static inline void sb_reset(sb_t *sb)
{
    sb_init_full(sb, sb->data - sb->skip, 0, sb->size + sb->skip, sb->must_free);
    sb->data[0] = '\0';
}
#ifndef NDEBUG
#  define __sb_wipe(sb)  assert(!(sb)->must_free)
#else
#  define __sb_wipe(sb)  (void)0
#endif

static inline void sb_wipe(sb_t *sb)
{
    if (sb->must_free) {
        ifree(sb->data - sb->skip, MEM_LIBC);
        sb_init(sb);
    } else {
        sb_reset(sb);
    }
}
static inline void sb_delete(sb_t **sbp)
{
    if (*sbp) {
        sb_t *sb = *sbp;
        if (sb->must_free)
            ifree(sb->data - sb->skip, MEM_LIBC);
        p_delete(sbp);
    }
}

/**************************************************************************/
/* str/mem-functions wrappers                                             */
/**************************************************************************/

static inline int sb_cmp(const sb_t *sb1, const sb_t *sb2)
{
    int len = MIN(sb1->len, sb2->len);
    int res = memcmp(sb1->data, sb2->data, len);
    return res ? res : sb1->len - sb2->len;
}

int sb_search(const sb_t *sb, int pos, const void *what, int wlen);


/**************************************************************************/
/* buffer raw manipulations                                               */
/**************************************************************************/

static inline char *sb_end(sb_t *sb)
{
    return sb->data + sb->len;
}
static inline int sb_avail(sb_t *sb)
{
    return sb->size - sb->len - 1;
}

char *sb_detach(sb_t *sb, int *len);

int  __sb_rewind_adds(sb_t *sb, const sb_t *orig);
void __sb_grow(sb_t *sb, int extra);
static inline void __sb_fixlen(sb_t *sb, int len)
{
    sb->len = len;
    sb->data[sb->len] = '\0';
}
static inline char *sb_grow(sb_t *sb, int extra)
{
    if (sb->len + extra >= sb->size)
        __sb_grow(sb, extra);
    return sb_end(sb);
}
static inline char *sb_growlen(sb_t *sb, int extra)
{
    if (sb->len + extra >= sb->size)
        __sb_grow(sb, extra);
    __sb_fixlen(sb, sb->len + extra);
    return sb_end(sb) - extra;
}

/**************************************************************************/
/* splicing                                                               */
/**************************************************************************/

static inline void sb_add(sb_t *sb, const void *data, int dlen)
{
    memcpy(sb_growlen(sb, dlen), data, dlen);
}
static inline void sb_addsb(sb_t *sb, const sb_t *sb2)
{
    sb_add(sb, sb2->data, sb2->len);
}
static inline void sb_addc(sb_t *sb, unsigned char c)
{
    sb_add(sb, &c, 1);
}
static inline void sb_adduc(sb_t *sb, int c)
{
    int len = __pstrputuc(sb_grow(sb, 4), c);
    __sb_fixlen(sb, sb->len + len);
}
static inline void sb_addnc(sb_t *sb, int extralen, unsigned char c)
{
    memset(sb_growlen(sb, extralen), c, extralen);
}
static inline void sb_add0s(sb_t *sb, int extralen)
{
    sb_addnc(sb, extralen, 0);
}

static inline void sb_adds(sb_t *sb, const char *s)
{
    sb_add(sb, s, strlen(s));
}

/* data == NULL means: please fill with raw data.  */
char *__sb_splice(sb_t *sb, int pos, int len, int dlen);
static inline char *
sb_splice(sb_t *sb, int pos, int len, const void *data, int dlen)
{
    char *res;

    assert (pos >= 0 && len >= 0 && dlen >= 0);
    assert ((unsigned)pos <= (unsigned)sb->len && (unsigned)pos + (unsigned)len <= (unsigned)sb->len);
    if (__builtin_constant_p(dlen)) {
        if (dlen == 0 || (__builtin_constant_p(len) && len >= dlen)) {
            p_move(sb->data, pos + dlen, pos + len, sb->len - pos - len);
            __sb_fixlen(sb, sb->len + dlen - len);
            return sb->data + pos;
        }
    }
    if (__builtin_constant_p(len) && len == 0 && pos == sb->len) {
        res = sb_growlen(sb, dlen);
    } else {
        res = __sb_splice(sb, pos, len, dlen);
    }
    return data ? memcpy(res, data, dlen) : res;
}

static inline void
sb_splicenc(sb_t *sb, int pos, int len, int extralen, unsigned char c)
{
    memset(sb_splice(sb, pos, len, NULL, extralen), c, extralen);
}
static inline void sb_splice0s(sb_t *sb, int pos, int len, int extralen)
{
    sb_splicenc(sb, pos, len, extralen, 0);
}


static inline void sb_skip(sb_t *sb, int len)
{
    assert (len >= 0 && len <= sb->len);
    sb->skip += len;
    sb->data += len;
    sb->size -= len;
    sb->len  -= len;
}
static inline void sb_skip_upto(sb_t *sb, const void *where)
{
    sb_skip(sb, (const char *)where - sb->data);
}

static inline void sb_clip(sb_t *sb, int len)
{
    assert (len >= 0 && len <= sb->len);
    __sb_fixlen(sb, len);
}
static inline void sb_shrink(sb_t *sb, int len)
{
    assert (len >= 0 && len <= sb->len);
    __sb_fixlen(sb, sb->len - len);
}
static inline void sb_shrink_upto(sb_t *sb, const void *where)
{
    sb_shrink(sb, (const char *)where - sb->data);
}


int sb_addvf(sb_t *sb, const char *fmt, va_list ap) __attr_printf__(2, 0);
int sb_addf(sb_t *sb, const char *fmt, ...)         __attr_printf__(2, 3);

#define sb_setvf(sb, fmt, ap) \
    ({ sb_t *__b = (sb); sb_reset(__b); sb_addvf(__b, fmt, ap); })
#define sb_setf(sb, fmt, ...) \
    ({ sb_t *__b = (sb); sb_reset(__b); sb_addf(__b, fmt, ##__VA_ARGS__); })

static inline void sb_set(sb_t *sb, const void *data, int dlen)
{
    sb->len = 0;
    sb_add(sb, data, dlen);
}
static inline void sb_setsb(sb_t *sb, const sb_t *sb2)
{
    sb_set(sb, sb2->data, sb2->len);
}
static inline void sb_sets(sb_t *sb, const char *s)
{
    sb_set(sb, s, strlen(s));
}


/**************************************************************************/
/* syscall/system wrappers                                                */
/**************************************************************************/

struct sockaddr;

int sb_getline(sb_t *sb, FILE *f);
int sb_fread(sb_t *sb, int size, int nmemb, FILE *f);
int sb_read_file(sb_t *sb, const char *filename);
int sb_write_file(const sb_t *sb, const char *filename);

int sb_read(sb_t *sb, int fd, int hint);
int sb_recv(sb_t *sb, int fd, int hint, int flags);
int sb_recvfrom(sb_t *sb, int fd, int hint, int flags,
                struct sockaddr *addr, socklen_t *alen);


/**************************************************************************/
/* usual quoting mechanisms (base64, addslashes, ...)                     */
/**************************************************************************/

#define __SB_DEFINE_ADDS(sfx) \
    static inline void sb_adds_##sfx(sb_t *sb, const char *s) { \
        sb_add_##sfx(sb, s, strlen(s));                         \
    }
#define __SB_DEFINE_ADDS_ERR(sfx) \
    static inline int sb_adds_##sfx(sb_t *sb, const char *s) {  \
        return sb_add_##sfx(sb, s, strlen(s));                  \
    }


void sb_add_slashes(sb_t *sb, const void *data, int len,
                    const char *toesc, const char *esc);
static inline void
sb_adds_slashes(sb_t *sb, const char *s, const char *toesc, const char *esc)
{
    sb_add_slashes(sb, s, strlen(s), toesc, esc);
}
void sb_add_unslashes(sb_t *sb, const void *data, int len,
                      const char *tounesc, const char *unesc);
static inline void
sb_adds_unslashes(sb_t *sb, const char *s, const char *tounesc, const char *unesc)
{
    sb_add_unslashes(sb, s, strlen(s), tounesc, unesc);
}

void sb_add_unquoted(sb_t *sb, const void *data, int len);
__SB_DEFINE_ADDS(unquoted);

void sb_add_urlencode(sb_t *sb, const void *data, int len);
void sb_add_urldecode(sb_t *sb, const void *data, int len);
void sb_urldecode(sb_t *sb);
__SB_DEFINE_ADDS(urlencode);
__SB_DEFINE_ADDS(urldecode);

void sb_add_hex(sb_t *sb, const void *data, int len);
int  sb_add_unhex(sb_t *sb, const void *data, int len);
__SB_DEFINE_ADDS(hex);
__SB_DEFINE_ADDS_ERR(unhex);

/* this all assumes utf8 data ! */
void sb_add_xmlescape(sb_t *sb, const void *data, int len);
int  sb_add_xmlunescape(sb_t *sb, const void *data, int len);
__SB_DEFINE_ADDS(xmlescape);
__SB_DEFINE_ADDS_ERR(xmlunescape);

void sb_add_qpe(sb_t *sb, const void *data, int len);
void sb_add_unqpe(sb_t *sb, const void *data, int len);
__SB_DEFINE_ADDS(qpe);
__SB_DEFINE_ADDS(unqpe);

typedef struct sb_b64_ctx_t {
    short packs_per_line;
    short pack_num;
    byte  trail[2];
    byte  trail_len;
} sb_b64_ctx_t;

void sb_add_b64_start(sb_t *sb, int len, int width, sb_b64_ctx_t *ctx);
void sb_add_b64_update(sb_t *sb, const void *src0, int len, sb_b64_ctx_t *ctx);
void sb_add_b64_finish(sb_t *sb, sb_b64_ctx_t *ctx);

void sb_add_b64(sb_t *sb, const void *data, int len, int width);
int  sb_add_unb64(sb_t *sb, const void *data, int len);
static inline void sb_adds_b64(sb_t *sb, const char *s, int width)
{
    sb_add_b64(sb, s, strlen(s), width);
}
__SB_DEFINE_ADDS_ERR(unb64);


/**************************************************************************/
/* charset conversions (when implicit, charset is utf8)                   */
/**************************************************************************/

void sb_conv_from_latin1(sb_t *sb, const void *s, int len);
void sb_conv_from_latin9(sb_t *sb, const void *s, int len);
int  sb_conv_to_latin1(sb_t *sb, const void *s, int len, int rep);

/* unpacked gsm: one septet per octet */
int  sb_conv_from_gsm(sb_t *sb, const void *src, int len);
int  sb_conv_from_gsm_hex(sb_t *sb, const void *src, int len);
bool sb_conv_to_gsm_isok(const void *src, int len);
void sb_conv_to_gsm(sb_t *sb, const void *src, int len);
void sb_conv_to_gsm_hex(sb_t *sb, const void *src, int len);

/* packed gsm */
int  gsm7_charlen(int c);
int  sb_conv_to_gsm7(sb_t *sb, int gsm_start, const char *utf8, int unknown);
int  sb_conv_from_gsm7(sb_t *sb, const void *src, int gsmlen, int udhlen);

#endif /* IS_LIB_COMMON_STR_BUF_H */
