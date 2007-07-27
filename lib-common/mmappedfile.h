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

#define MMFILE_ALIAS(ptr_type)     \
    {                              \
        ssize_t   size;            \
        char     *path;            \
        ptr_type *area;            \
        int       ro;              \
    }

typedef struct mmfile MMFILE_ALIAS(byte) mmfile;

/* Atrocious kludge for MAP_POPULATE */
#define MMAP_O_PRELOAD  00200000  // O_DIRECTORY

mmfile *mmfile_open(const char *path, int flags);
mmfile *mmfile_creat(const char *path, off_t initialsize);
mmfile *mmfile_open_or_creat(const char *path, int flags, off_t initialsize,
                             bool *created);
void mmfile_close(mmfile **mf);

/* @see ftruncate(2) */
__must_check__
int mmfile_truncate(mmfile *mf, off_t length, int (*lock)(void*),
                    int (*unlock)(void*), void*);

#define MMFILE_FUNCTIONS(type, prefix) \
    static inline type *prefix##_open(const char *path, int flags) {    \
        return (type *)mmfile_open(path, flags);                        \
    }                                                                   \
                                                                        \
    static inline type *                                                \
    prefix##_open_or_creat(const char *path, int flags,                 \
                           off_t initialsize, bool *created)            \
    {                                                                   \
        return (type *)mmfile_open_or_creat(path, flags,                \
                                            initialsize, created);      \
    }                                                                   \
                                                                        \
    static inline type *prefix##_creat(const char *path, off_t size) {  \
        return (type *)mmfile_creat(path, size);                        \
    }                                                                   \
                                                                        \
    static inline void prefix##_close(type **mmf) {                     \
        mmfile_close((mmfile **)mmf);                                   \
    }                                                                   \
                                                                        \
    __must_check__                                                      \
    static inline int prefix##_truncate(type *mf, off_t length) {       \
        return mmfile_truncate((mmfile *)mf, length, NULL, NULL, NULL); \
    }


#endif /* IS_LIB_COMMON_MMAPPEDFILE_H */
