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

typedef struct sb_t {
    char *data;
    int len, size;
    flag_t must_free  :  1;
    unsigned int skip : 31;
} sb_t;

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

#define SB(name, size) \
    sb_t name = {                                                   \
        .data = alloca(size),                                       \
        .size = (STATIC_ASSERT((size) < (64 << 10)), size),         \
    }

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
static inline void sb_adds(sb_t *sb, const char *s)
{
    sb_add(sb, s, strlen(s));
}

static inline void sb_set(sb_t *sb, const void *data, int dlen)
{
    sb->len = 0;
    sb_add(sb, data, dlen);
}

/* data == NULL means: please fill with raw data.  */
void __sb_splice(sb_t *sb, int pos, int len, const void *data, int dlen);
static inline void
sb_splice(sb_t *sb, int pos, int len, const void *data, int dlen)
{
    if (pos == sb->len && data)
        sb_add(sb, data, dlen);
    __sb_splice(sb, pos, len, data, dlen);
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


/**************************************************************************/
/* syscall/system wrappers                                                */
/**************************************************************************/

struct sockaddr;

int sb_getline(sb_t *sb, FILE *f);
int sb_read(sb_t *sb, int fd, int hint);
int sb_recv(sb_t *sb, int fd, int hint, int flags);
int sb_recvfrom(sb_t *sb, int fd, int hint, int flags,
                struct sockaddr *addr, socklen_t *alen);

#endif /* IS_LIB_COMMON_STR_BUF_H */
