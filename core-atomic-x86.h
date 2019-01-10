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
#  error "you must include <lib-common/core.h> instead"
#else
#define IS_LIB_COMMON_CORE_ATOMIC_ARCH_H

static inline ALWAYS_INLINE unsigned int
atomic_xchg32(volatile unsigned int *p, unsigned int v)
{
    unsigned int res;
    asm volatile("xchgl %0, %1"
                 : "=&r"(res), "=m"(*p)
                 : "0"(v), "m"(*p)
                 : "memory");
    return res;
}

#ifdef __x86_64__
static inline ALWAYS_INLINE unsigned long
atomic_xchg64(volatile unsigned long *p, unsigned long v)
{
    unsigned long res;
    asm volatile("xchgq %0, %1"
                 : "=&r"(res), "=m"(*p)
                 : "0"(v), "m"(*p)
                 : "memory");
    return res;
}

static inline ALWAYS_INLINE unsigned long
atomic_xchg_(volatile void *p, unsigned long v, int len)
{
    switch (len) {
      case 4: return atomic_xchg32(p, v);
      case 8: return atomic_xchg64(p, v);
      default: assert(false); return 0;
    }
}
#define xchg(p, v) \
  (STATIC_ASSERT(sizeof(*(p)) == 4 || sizeof(*(p)) == 8), \
   (typeof(*(p)))atomic_xchg_((p), (unsigned long)(v), sizeof(*(p))))
#else
#define xchg(p, v) \
  (STATIC_ASSERT(sizeof(*(p)) == 4), \
   (typeof(*(p)))atomic_xchg32((p), (unsigned int)(v), sizeof(*(p))))
#endif

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
