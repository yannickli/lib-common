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

#ifndef IS_LIB_COMMON_MMAPPEDFILE_H
#define IS_LIB_COMMON_MMAPPEDFILE_H

#include "core.h"

enum {
    MMO_POPULATE = 0x0001,
    MMO_LOCK     = 0x0002,
    MMO_TLOCK    = 0x0004,
    MMO_RANDOM   = 0x0008,
};

#define MMFILE_ALIAS(ptr_type)      \
    {                               \
        ssize_t   size;             \
        char     *path;             \
        ptr_type *area;             \
        flag_t    writeable : 1;    \
        flag_t    locked    : 1;    \
        int16_t   refcnt;           \
        int       fd;               \
        void      *mutex;           \
        int      (*rlock)(void*);   \
        int      (*wlock)(void*);   \
        int      (*unlock)(void*);  \
    }

typedef struct mmfile MMFILE_ALIAS(byte) mmfile;

mmfile *mmfile_open(const char *path, int flags, int oflags, off_t minsize);

/* XXX: caller must ensure mmfile_close is not called */
static inline mmfile *mmfile_unlocked_dup(mmfile *mf) {
    mf->refcnt++;
    return mf;
}
static inline mmfile *mmfile_creat(const char *path, off_t initialsize) {
    return mmfile_open(path, O_CREAT | O_TRUNC | O_RDWR, 0, initialsize);
}
int mmfile_unlockfile(mmfile *mf);
void mmfile_close(mmfile **mf);
void mmfile_close_wlocked(mmfile **mf);

/* @see ftruncate(2) */
__must_check__ int mmfile_truncate(mmfile *mf, off_t length);
__must_check__ int mmfile_truncate_unlocked(mmfile *mf, off_t length);

static inline int mmfile_rlock(mmfile *mf) {
    return mf->mutex ? (mf->rlock)(mf->mutex) : 0;
}

static inline int mmfile_wlock(mmfile *mf) {
    return mf->mutex ? (mf->wlock)(mf->mutex) : 0;
}

static inline int mmfile_unlock(mmfile *mf) {
    return mf->mutex ? (mf->unlock)(mf->mutex) : 0;
}


#define MMFILE_FUNCTIONS(type, prefix) \
    static inline type *prefix##_open(const char *path, int fl, int ofl,    \
                                      off_t sz)                             \
    {                                                                       \
        return (type *)mmfile_open(path, fl, ofl, sz);                      \
    }                                                                       \
                                                                            \
    static inline type *prefix##_unlocked_dup(type *mf) {                   \
        return (type *)mmfile_unlocked_dup((mmfile *)mf);                   \
    }                                                                       \
                                                                            \
    static inline int prefix##_unlockfile(type *mf) {                       \
        return mmfile_unlockfile((mmfile *)mf);                             \
    }                                                                       \
                                                                            \
    static inline type *prefix##_creat(const char *path, off_t size) {      \
        return prefix##_open(path, O_CREAT | O_TRUNC | O_RDWR, 0, size);    \
    }                                                                       \
                                                                            \
    static inline void prefix##_close(type **mmf) {                         \
        mmfile_close((mmfile **)mmf);                                       \
    }                                                                       \
                                                                            \
    static inline void prefix##_close_wlocked(type **mmf) {                 \
        mmfile_close_wlocked((mmfile **)mmf);                               \
    }                                                                       \
                                                                            \
    __must_check__                                                          \
    static inline int prefix##_truncate(type *mf, off_t length) {           \
        return mmfile_truncate((mmfile *)mf, length);                       \
    }                                                                       \
                                                                            \
    __must_check__                                                          \
    static inline int prefix##_truncate_unlocked(type *mf, off_t length) {  \
        return mmfile_truncate_unlocked((mmfile *)mf, length);              \
    }                                                                       \
                                                                            \
    static inline int prefix##_rlock(type *mf) {                            \
        return mf->mutex ? (mf->rlock)(mf->mutex) : 0;                      \
    }                                                                       \
                                                                            \
    static inline int prefix##_wlock(type *mf) {                            \
        return mf->mutex ? (mf->wlock)(mf->mutex) : 0;                      \
    }                                                                       \
                                                                            \
    static inline int prefix##_unlock(type *mf) {                           \
        return mf->mutex ? (mf->unlock)(mf->mutex) : 0;                     \
    }


#endif /* IS_LIB_COMMON_MMAPPEDFILE_H */
