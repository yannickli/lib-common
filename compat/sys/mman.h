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

#ifndef IS_COMPAT_SYS_MMAN_H
#define IS_COMPAT_SYS_MMAN_H

#if defined(__MINGW) || defined(__MINGW32__)
#include <unistd.h>
#  define MAP_FAILED      ((void *) -1)
extern void *mmap(void *__addr, size_t __len, int __prot,
                  int __flags, int __fd, off_t __offset);
extern int munmap(void *__addr, size_t __len);
extern int msync(void *__addr, size_t __len, int __flags);
extern void *mremap(void *__addr, size_t __old_len, size_t __new_len,
                    int __flags);

/* Flags to `msync'.  */
#  define MS_ASYNC        1               /* Sync memory asynchronously.  */
#  define MS_SYNC         4               /* Synchronous memory sync.  */
#  define MS_INVALIDATE   2               /* Invalidate the caches.  */

#  define PROT_READ       0x1             /* Page can be read.  */
#  define PROT_WRITE      0x2             /* Page can be written.  */
#  define PROT_EXEC       0x4             /* Page can be executed.  */
#  define PROT_NONE       0x0             /* Page can not be accessed.  */

#  define MAP_SHARED      0x01            /* Share changes.  */
#  define MAP_PRIVATE     0x02            /* Changes are private.  */
#  define MAP_FIXED       0x10            /* Interpret addr exactly.  */
#  define MAP_ANONYMOUS   0x20            /* Don't use a file.  */
#else
#  include_next <sys/mman.h>

/* Fix mremap when using valgrind
 * @see: https://bugs.kde.org/show_bug.cgi?id=204484
 */
#ifndef NDEBUG
#include <valgrind/valgrind.h>
#include <valgrind/memcheck.h>

static inline void *
mremap_diverted(void *old_address, size_t old_size, size_t new_size, int flags)
{
    void *mres;
    mres = mremap(old_address, old_size, new_size, flags);

    if (mres != MAP_FAILED) {
        (void)VALGRIND_MAKE_MEM_NOACCESS(old_address, old_size);
        (void)VALGRIND_MAKE_MEM_DEFINED(mres, new_size);
    }

    return mres;
}
#define mremap(...) mremap_diverted(__VA_ARGS__)
#endif

#ifdef __sun
extern int madvise(void *, size_t, int);
#endif

#endif

#endif
