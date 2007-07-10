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

#ifndef MINGCC
#include <sys/mman.h>
#else
#define MAP_FAILED      ((void *) -1)
extern void *mmap(void *__addr, size_t __len, int __prot,
                  int __flags, int __fd, off_t __offset);
extern int munmap(void *__addr, size_t __len);
extern int msync(void *__addr, size_t __len, int __flags);
extern void *mremap(void *__addr, size_t __old_len, size_t __new_len,
                    int __flags, ...);
extern int posix_fallocate(int fd, off_t offset, off_t len);

/* Flags to `msync'.  */
#define MS_ASYNC        1               /* Sync memory asynchronously.  */
#define MS_SYNC         4               /* Synchronous memory sync.  */
#define MS_INVALIDATE   2               /* Invalidate the caches.  */

#define PROT_READ       0x1             /* Page can be read.  */
#define PROT_WRITE      0x2             /* Page can be written.  */
#define PROT_EXEC       0x4             /* Page can be executed.  */
#define PROT_NONE       0x0             /* Page can not be accessed.  */

#define MAP_SHARED      0x01            /* Share changes.  */
#define MAP_PRIVATE     0x02            /* Changes are private.  */
#define MAP_FIXED       0x10            /* Interpret addr exactly.  */
#define MAP_ANONYMOUS   0x20            /* Don't use a file.  */
#endif

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

/* @see ftruncate(2)
 *
 * XXX: mmfile may sometimes be wiped if remap fails !
 *      in that particular case, it returns -2 instead of -1
 */
__must_check__ int mmfile_truncate(mmfile *mf, off_t length);

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
        return mmfile_truncate((mmfile *)mf, length);                   \
    }


#endif /* IS_LIB_COMMON_MMAPPEDFILE_H */
