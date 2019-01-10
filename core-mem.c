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

#ifndef NDEBUG
#  include <valgrind/valgrind.h>
#  include <valgrind/memcheck.h>
#else
#  define VALGRIND_MAKE_MEM_DEFINED_IF_ADDRESSABLE(...) ((void)0)
#  define VALGRIND_CREATE_MEMPOOL(...)           ((void)0)
#  define VALGRIND_DESTROY_MEMPOOL(...)          ((void)0)
#  define VALGRIND_MAKE_MEM_DEFINED(...)         ((void)0)
#  define VALGRIND_MAKE_MEM_NOACCESS(...)        ((void)0)
#  define VALGRIND_MAKE_MEM_UNDEFINED(...)       ((void)0)
#  define VALGRIND_MALLOCLIKE_BLOCK(...)         ((void)0)
#  define VALGRIND_FREELIKE_BLOCK(...)           ((void)0)
#endif

#include "core.h"

void *libc_malloc(size_t size, size_t alignment, mem_flags_t flags)
{
    void *res;

    if (alignment <= sizeof(void *)) {
        if (flags & MEM_RAW) {
            res = malloc(size);
        } else {
            res = calloc(1, size);
        }
        if (unlikely(res == NULL)) {
            if (flags & MEM_ERRORS_OK)
                return NULL;
            e_panic("out of memory");
        }
    } else {
        int ret = posix_memalign(&res, alignment, size);

        if (unlikely(ret != 0)) {
            errno = ret;
            if (flags & MEM_ERRORS_OK)
                return NULL;
            e_panic("cannot allocate memory: %m");
        }
        if (!(flags & MEM_RAW)) {
            memset(res, 0, size);
        }
    }
    return res;
}

void *libc_realloc(void *mem, size_t oldsize, size_t size,
                   size_t alignment, mem_flags_t flags)
{
    byte *res = NULL;

    if (alignment > sizeof(void *) && mem == NULL) {
        return libc_malloc(size, alignment, flags);
    }

    res = realloc(mem, size);

    if (unlikely(res == NULL)) {
        if (!size || (flags & MEM_ERRORS_OK))
            return NULL;
        e_panic("out of memory");
    }

    if (alignment > sizeof(void *)
    &&  ((uintptr_t)res & (alignment - 1)))
    {
        byte *cpy = libc_malloc(size, alignment, flags | MEM_RAW);

        p_copy(cpy, res, oldsize == MEM_UNKNOWN ? size : oldsize);
        libc_free(res, flags);
        res = cpy;
    }

    if (!(flags & MEM_RAW) && oldsize < size)
        memset(res + oldsize, 0, size - oldsize);
    return res;
}

void *__imalloc(size_t size, size_t alignment, mem_flags_t flags)
{
    if (size > MEM_ALLOC_MAX)
        e_panic("you cannot allocate that amount of memory");
    switch (flags & MEM_POOL_MASK) {
      case MEM_STATIC:
      default:
        e_panic("you cannot allocate from pool %d with imalloc",
                flags & MEM_POOL_MASK);
      case MEM_LIBC:
        return libc_malloc(size, alignment, flags);
      case MEM_STACK:
        return stack_malloc(size, alignment, flags);
    }
}

void __ifree(void *mem, mem_flags_t flags)
{
    switch (flags & MEM_POOL_MASK) {
      case MEM_STATIC:
      case MEM_STACK:
        return;
      case MEM_LIBC:
        libc_free(mem, flags);
        return;
      default:
        e_panic("pool memory cannot be deallocated with ifree");
    }
}

void *__irealloc(void *mem, size_t oldsize, size_t size, size_t alignment,
                 mem_flags_t flags)
{
    if (size == 0) {
        ifree(mem, flags);
        return NULL;
    }
    if (size > MEM_ALLOC_MAX)
        e_panic("you cannot allocate that amount of memory");

    switch (flags & MEM_POOL_MASK) {
      case MEM_STATIC:
        e_panic("you cannot realloc alloca-ed memory");
      case MEM_STACK:
        return stack_realloc(mem, oldsize, size, alignment, flags);
      case MEM_LIBC:
        return libc_realloc(mem, oldsize, size, alignment, flags);
      default:
        e_panic("pool memory cannot be reallocated with ifree");
    }
}

char *mp_fmt(mem_pool_t *mp, int *out, const char *fmt, ...)
{
#define MP_FMT_LEN   1024
    va_list ap;
    char *res;
    int len;

    res = mp_new_raw(mp, char, MP_FMT_LEN);
    va_start(ap, fmt);
    len = vsnprintf(res, MP_FMT_LEN, fmt, ap);
    va_end(ap);
    if (likely(len < MP_FMT_LEN)) {
        res = (*mp->realloc)(mp, res, MP_FMT_LEN, len + 1, MEM_RAW);
    } else {
        res = (*mp->realloc)(mp, res, 0, len + 1, MEM_RAW);
        va_start(ap, fmt);
        len = vsnprintf(res, len + 1, fmt, ap);
        va_end(ap);
    }
    if (out)
        *out = len;
    return res;
#undef MP_FMT_LEN
}

/* Instrumentation {{{ */

#ifndef NDEBUG

bool mem_tool_is_running(unsigned tools)
{
    if (tools & MEM_TOOL_VALGRIND && RUNNING_ON_VALGRIND) {
        return true;
    }
#ifdef __has_asan
    if (tools & MEM_TOOL_ASAN) {
        return true;
    }
#endif
    return false;
}

#if __GNUC_PREREQ(4, 6) && !__VALGRIND_PREREQ(3, 7)
#  pragma GCC diagnostic push
#  pragma GCC diagnostic ignored "-Wunused-but-set-variable"
#endif

#if !__VALGRIND_PREREQ(3, 7)
# define IGNORE_RET(expr)  ({ (expr); })
#else
# define IGNORE_RET(expr)  expr
#endif

#ifdef __has_asan

// Marks memory region [addr, addr+size) as unaddressable.
// This memory must be previously allocated by the user program. Accessing
// addresses in this region from instrumented code is forbidden until
// this region is unpoisoned. This function is not guaranteed to poison
// the whole region - it may poison only subregion of [addr, addr+size) due
// to ASan alignment restrictions.
// Method is NOT thread-safe in the sense that no two threads can
// (un)poison memory in the same memory region simultaneously.
void __asan_poison_memory_region(void const volatile *addr, size_t size);
// Marks memory region [addr, addr+size) as addressable.
// This memory must be previously allocated by the user program. Accessing
// addresses in this region is allowed until this region is poisoned again.
// This function may unpoison a superregion of [addr, addr+size) due to
// ASan alignment restrictions.
// Method is NOT thread-safe in the sense that no two threads can
// (un)poison memory in the same memory region simultaneously.
void __asan_unpoison_memory_region(void const volatile *addr, size_t size);

#else
# define __asan_poison_memory_region(...)
# define __asan_unpoison_memory_region(...)
#endif

void mem_tool_allow_memory(const void *mem, size_t len, bool defined)
{
    if (!mem || !len) {
        return;
    }

    if (defined) {
        (void)VALGRIND_MAKE_MEM_DEFINED(mem, len);
    } else {
        (void)VALGRIND_MAKE_MEM_UNDEFINED(mem, len);
    }
    __asan_unpoison_memory_region(mem, len);
}

void mem_tool_allow_memory_if_addressable(const void *mem, size_t len,
                                          bool defined)
{
    if (!mem || !len) {
        return;
    }

    if (defined) {
        (void)VALGRIND_MAKE_MEM_DEFINED_IF_ADDRESSABLE(mem, len);
    }
    __asan_unpoison_memory_region(mem, len);
}

void mem_tool_disallow_memory(const void *mem, size_t len)
{
    if (!mem || !len) {
        return;
    }

    __asan_poison_memory_region(mem, len);
    VALGRIND_MAKE_MEM_NOACCESS(mem, len);
}

void mem_tool_malloclike(const void *mem, size_t len, size_t rz, bool zeroed)
{
    if (!mem) {
        return;
    }

    VALGRIND_MALLOCLIKE_BLOCK(mem, len, rz, zeroed);
    if (!len) {
        return;
    }

    __asan_unpoison_memory_region(mem, len);
    if (rz) {
        __asan_poison_memory_region((const byte *)mem - rz, rz);
        __asan_poison_memory_region((const byte *)mem + len, rz);
    }
}

void mem_tool_freelike(const void *mem, size_t len, size_t rz)
{
    if (!mem) {
        return;
    }
    if (len > 0) {
        __asan_poison_memory_region(mem, len);
    }
    VALGRIND_FREELIKE_BLOCK(mem, rz);
}

#if __GNUC_PREREQ(4, 6) && !__VALGRIND_PREREQ(3, 7)
#  pragma GCC diagnostic pop
#endif

#endif

/* }}} */
/*{{{ Versions */

/* This version will be visible using the "ident" command */
extern const char libcommon_id[];
const char *__libcomon_version(void);
const char *__libcomon_version(void)
{
    return libcommon_id;
}

core_version_t core_versions_g[8];
int core_versions_nb_g;

void core_push_version(bool is_main_version, const char *name,
                       const char *version, const char *git_revision)
{
    int ind = core_versions_nb_g++;

    assert (ind < countof(core_versions_g));
    core_versions_g[ind].is_main_version = is_main_version;
    core_versions_g[ind].name            = name;
    core_versions_g[ind].version         = version;
    core_versions_g[ind].git_revision    = git_revision;
}

extern const char libcommon_git_revision[];
__attribute__((constructor))
static void core_versions_initialize(void)
{
    core_push_version(false, "lib-common", LIB_COMMON_VERSION,
                      libcommon_git_revision);
}

/*}}} */
