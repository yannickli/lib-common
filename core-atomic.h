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

#if !defined(IS_LIB_COMMON_CORE_H) || defined(IS_LIB_COMMON_CORE_ATOMIC_H)
#  error "you must include core.h instead"
#else
#define IS_LIB_COMMON_CORE_ATOMIC_H

#ifdef __GNUC__

#define barrier()          asm volatile("":::"memory")
#define access_once(x)     (*(volatile typeof(x) *)&(x))

#if defined(__i386__) || defined(__x86_64__)
#  include "core-atomic-x86.h"
#else
#  include <sched.h>
#  define cpu_relax()  sched_yield()

#  error "please implement core-atomic-$arch.h"
#endif


typedef int spinlock_t;

#define atomic_add(ptr, v)          IGNORE(__sync_add_and_fetch(ptr, v))
#define atomic_sub(ptr, v)          IGNORE(__sync_sub_and_fetch(ptr, v))
#define atomic_add_and_get(ptr, v)  __sync_add_and_fetch(ptr, v)
#define atomic_sub_and_get(ptr, v)  __sync_sub_and_fetch(ptr, v)
#define atomic_get_and_add(ptr, v)  __sync_fetch_and_add(ptr, v)
#define atomic_get_and_sub(ptr, v)  __sync_fetch_and_sub(ptr, v)

#define atomic_xchg(ptr, v)         ((typeof(*(ptr)))__sync_lock_test_and_set(ptr, v))
#define atomic_bool_cas(ptr, b, a)  __sync_bool_compare_and_swap(ptr, b, a)
#define atomic_val_cas(ptr, b, a)   ((typeof(*(ptr)))__sync_val_compare_and_swap(ptr, b, a))

#define spin_trylock(ptr)  (!__sync_lock_test_and_set(ptr, 1))
#define spin_lock(ptr)     ({ while (unlikely(!spin_trylock(ptr))) { cpu_relax(); }})
#define spin_unlock(ptr)   __sync_lock_release(ptr)

#endif

#endif
