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

#if !defined(IS_LIB_COMMON_CORE_H) || defined(IS_LIB_COMMON_CORE_ATOMIC_H)
#  error "you must include <lib-common/core.h> instead"
#else
#define IS_LIB_COMMON_CORE_ATOMIC_H

#ifdef __GNUC__

typedef int spinlock_t;

#define atomic_add(ptr, v)          IGNORE(__sync_add_and_fetch(ptr, v))
#define atomic_sub(ptr, v)          IGNORE(__sync_sub_and_fetch(ptr, v))
#define atomic_add_and_get(ptr, v)  __sync_add_and_fetch(ptr, v)
#define atomic_sub_and_get(ptr, v)  __sync_sub_and_fetch(ptr, v)
#define atomic_get_and_add(ptr, v)  __sync_fetch_and_add(ptr, v)
#define atomic_get_and_sub(ptr, v)  __sync_fetch_and_sub(ptr, v)
#define atomic_bool_cas(ptr, b, a)  __sync_bool_compare_and_swap(ptr, b, a)
#define atomic_va_cas(ptr, b, a)    __sync_va_compare_and_swap(ptr, b, a)
#define memory_barrier()            __sync_synchronize()

#define spin_trylock(ptr)  (!__sync_lock_test_and_set(ptr, 1))
#define spin_lock(ptr)     do { } while(unlikely(!spin_trylock(ptr)))
#define spin_unlock(ptr)   __sync_lock_release(ptr)

#endif

#endif
