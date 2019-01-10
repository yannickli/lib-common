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

#if !defined(__x86_64__) && !defined(__i386__)
#  error "not in the proper architecture"
#endif
#if !defined(IS_LIB_COMMON_CORE_H) || defined(IS_LIB_COMMON_CORE_ATOMIC_ARCH_H)
#  error "you must include core.h instead"
#else
#define IS_LIB_COMMON_CORE_ATOMIC_ARCH_H

#ifdef __x86_64__
#  define mb()          asm volatile("mfence":::"memory")
#  define rmb()         asm volatile("lfence":::"memory")
#  define wmb()         asm volatile("sfence":::"memory")
#else
#  define mb()          asm volatile("lock; addl $0,0(%%esp)":::"memory")
#  define rmb()         asm volatile("lock; addl $0,0(%%esp)":::"memory")
#  define wmb()         asm volatile("lock; addl $0,0(%%esp)":::"memory")
#endif

#define mc()            barrier()
#define rmc()           barrier()
#define wmc()           barrier()

#define cpu_relax()     asm volatile("rep; nop":::"memory")

#define shared_read(p)      ({ rmc(); access_once(p); })
#define shared_write(p, v)  ({ typeof(v) __v = (v); \
                               access_once(p) = __v; wmc(); __v; })

#endif
