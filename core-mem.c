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


/* Libc allocator {{{ */

static void *libc_malloc(mem_pool_t *m, size_t size, size_t alignment,
                         mem_flags_t flags)
{
    void *res;

    if (alignment <= 8) {
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

static void *libc_realloc(mem_pool_t *m, void *mem, size_t oldsize,
                          size_t size, size_t alignment, mem_flags_t flags)
{
    byte *res = NULL;

    if (alignment > 8 && mem == NULL) {
        return libc_malloc(m, size, alignment, flags);
    }

    res = realloc(mem, size);

    if (unlikely(res == NULL)) {
        if (!size || (flags & MEM_ERRORS_OK))
            return NULL;
        e_panic("out of memory");
    }

    if (alignment > 8 && ((uintptr_t)res & (alignment - 1))) {
        byte *cpy = libc_malloc(m, size, alignment, flags | MEM_RAW);

        p_copy(cpy, res, oldsize == MEM_UNKNOWN ? size : oldsize);
        free(res);
        res = cpy;
    }

    if (!(flags & MEM_RAW) && oldsize < size)
        memset(res + oldsize, 0, size - oldsize);
    return res;
}

static void libc_free(mem_pool_t *m, void *p)
{
    free(p);
}

mem_pool_t mem_pool_libc = {
    .malloc   = &libc_malloc,
    .realloc  = &libc_realloc,
    .free     = &libc_free,
    .mem_pool = MEM_LIBC | MEM_EFFICIENT_REALLOC,
    .min_alignment = sizeof(void *)
};

/* }}} */
/* Static allocator {{{ */

static void *static_malloc(mem_pool_t *m, size_t size, size_t alignement,
                           mem_flags_t flags)
{
    e_panic("allocation not possible on the static pool");
}

static void *static_realloc(mem_pool_t *m, void *mem, size_t oldsize,
                            size_t size, size_t alignment, mem_flags_t flags)
{
    e_panic("reallocation is not possible on the static pool");
}

static void static_free(mem_pool_t *m, void *p)
{
}

mem_pool_t mem_pool_static = {
    .malloc  = &static_malloc,
    .realloc = &static_realloc,
    .free    = &static_free,
    .mem_pool = MEM_STATIC | MEM_BY_FRAME,
    .min_alignment = 1,
    .realloc_fallback = &mem_pool_libc
};

/* }}} */

char *mp_vfmt(mem_pool_t *mp, int *lenp, const char *fmt, va_list va)
{
#define MP_FMT_LEN   1024
    char *res;
    int len;
    va_list cpy;

    res = mp_new_raw(mp, char, MP_FMT_LEN);
    va_copy(cpy, va);
    len = vsnprintf(res, MP_FMT_LEN, fmt, cpy);
    va_end(cpy);
    if (likely(len < MP_FMT_LEN)) {
        res = mp_irealloc(mp, res, MP_FMT_LEN, len + 1, 1, MEM_RAW);
    } else {
        res = mp_irealloc(mp, res, 0, len + 1, 1, MEM_RAW);
        len = vsnprintf(res, len + 1, fmt, va);
    }
    if (lenp) {
        *lenp = len;
    }
    return res;
#undef MP_FMT_LEN
}

char *mp_fmt(mem_pool_t *mp, int *lenp, const char *fmt, ...)
{
    char *res;
    va_list ap;

    va_start(ap, fmt);
    res = mp_vfmt(mp, lenp, fmt, ap);
    va_end(ap);
    return res;
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

    /* check sanity of PAGE_SIZE */
    if (sysconf(_SC_PAGESIZE) != PAGE_SIZE) {
        e_panic("System page size is different from defined PAGE_SIZE");
    }
}

/*}}} */
/* mem_pool_is_enabled {{{ */

#ifndef NDEBUG
bool mem_pool_is_enabled(void)
{
    static int mp_is_enabled = -1;

    if (unlikely(mp_is_enabled < 0)) {
        const char *val = getenv("BYPASS_MEMPOOL");

        mp_is_enabled = !val || strlen(val) == 0;
    }

    return mp_is_enabled;
}
#endif

/* }}} */
