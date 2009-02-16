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

#if !defined(IS_LIB_COMMON_STR_H) || defined(IS_LIB_COMMON_STR_BUF_H)
#  error "you must include <lib-common/str.h> instead"
#else
#define IS_LIB_COMMON_STR_BUF_H

/* blob_t is a wrapper type for a reallocatable byte array.
 * Its internal representation is accessible to client code but care
 * must be exercised to preserve blob invariants and avoid dangling
 * pointers when blob data is reallocated by blob functions.
 *
 * Here is a description of the blob fields:
 *
 * - <data> is a pointer to the active portion of the byte array.  As
 * <data> may be reallocated by blob operations, it is error prone to
 * copy its value to a local variable for content parsing or other such
 * manipulations.  As <data> may point to non allocated storage, static
 * or dynamic or automatic, returning its value is guaranteed to cause
 * potential problems, current of future.  If some blob data was
 * skipped, <data> may no longer point to the beginning of the original
 * or allocated array.  <data> cannot be NULL.  It is initialized as
 * the address of a global 1 byte array, or a buffer with automatic
 * storage.
 *
 * - <len> is the length in bytes of the blob contents.  A blob invariant
 * specifies that data[len] == '\0'.  This invariant must be kept at
 * all times, and implies that data storage extend at least one byte
 * beyond <len>. <len> is always positive or zero. <len> is initialized
 * to zero for an empty blob.
 *
 * - <size> is the number of bytes available for blob contents starting
 * at <data>.  The blob invariant implies that size > len.
 *
 * - <skip> is a count of byte skipped from the beginning of the
 * original data buffer.  It is used for efficient pruning of initial
 * bytes.
 *
 * - <allocated> is a 1 bit boolean to indicate if blob data was
 * allocated with p_new and must be freed.  Actual allocated buffer
 * starts at .data - .skip.
 *
 * blob invariants:
 * - blob.data != NULL
 * - blob.len >= 0
 * - blob.size > blob.len
 * - blob.data - blob.skip points to an array of at least
 * blob.size + blob.skip bytes.
 * - blob.data[blob.len] == '\0'
 * - if blob.allocated, blob.data - blob.skip is a pointer handled by
 * mem_alloc / mem_free.
 */
typedef struct sb_t {
    char *data;
    int len, size;
    flag_t must_free  :  1;
    unsigned int skip : 31;
} sb_t;

/* Default byte array for empty blobs. It should always stay equal to
 * \0 and is writeable to simplify some blob functions that enforce the
 * invariant by writing \0 at data[len].
 * This data is thread specific for obscure and ugly reasons: some blob
 * functions may temporarily overwrite the '\0' and would cause
 * spurious bugs in other threads sharing the same blob data.
 * It would be much cleaner if this array was const.
 */
extern char __sb_slop[1];


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
        mem_free(sb->data - sb->skip);
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
            mem_free(sb->data - sb->skip);
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

void __sb_rewind_adds(sb_t *sb, const sb_t *orig);
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
static inline void sb_addc(sb_t *sb, unsigned char c)
{
    sb_add(sb, &c, 1);
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
void __sb_splice(sb_t *sb, int pos, int len, const void *data, int dlen);
static inline void
sb_splice(sb_t *sb, int pos, int len, const void *data, int dlen)
{
    if (pos == sb->len && data) {
        sb_add(sb, data, dlen);
    } else {
        __sb_splice(sb, pos, len, data, dlen);
    }
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

#endif /* IS_LIB_COMMON_STR_BUF_H */
