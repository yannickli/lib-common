/**************************************************************************/
/*                                                                        */
/*  Copyright (C) 2004-2007 INTERSEC SAS                                  */
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

#include <sys/types.h>
#include <fcntl.h>

#include <lib-common/macros.h>

#define MMFILE_ALIAS(ptr_type)      \
    {                               \
        ssize_t   size;             \
        char     *path;             \
        ptr_type *area;             \
        flag_t    writeable : 1;    \
        int      (*lock)(void*);    \
        int      (*unlock)(void*);  \
        int      (*destroy)(void*); \
    }

typedef struct mmfile MMFILE_ALIAS(byte) mmfile;

/* Atrocious kludge for MAP_POPULATE */
#define MMAP_O_PRELOAD  00200000  // O_DIRECTORY

mmfile *mmfile_open(const char *path, int flags, off_t minsize);
static inline mmfile *mmfile_creat(const char *path, off_t initialsize) {
    return mmfile_open(path, O_CREAT | O_TRUNC | O_RDWR, initialsize);
}
void mmfile_close(mmfile **mf, void *mutex);

/* @see ftruncate(2) */
__must_check__ int mmfile_truncate(mmfile *mf, off_t length, void *mutex);

#define MMFILE_FUNCTIONS(type, prefix) \
    static inline type *prefix##_open(const char *path, int fl, off_t sz) { \
        return (type *)mmfile_open(path, fl, sz);                           \
    }                                                                       \
                                                                            \
    static inline type *prefix##_creat(const char *path, off_t size) {      \
        return prefix##_open(path, O_CREAT | O_TRUNC | O_RDWR, size);       \
    }                                                                       \
                                                                            \
    static inline void prefix##_close(type **mmf) {                         \
        mmfile_close((mmfile **)mmf, NULL);                                 \
    }                                                                       \
                                                                            \
    __must_check__                                                          \
    static inline int prefix##_truncate(type *mf, off_t length) {           \
        return mmfile_truncate((mmfile *)mf, length, NULL);                 \
    }


#endif /* IS_LIB_COMMON_MMAPPEDFILE_H */
