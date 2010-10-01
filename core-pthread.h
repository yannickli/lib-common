/**************************************************************************/
/*                                                                        */
/*  Copyright (C) 2004-2010 INTERSEC SAS                                  */
/*                                                                        */
/*  Should you receive a copy of this source code, you must check you     */
/*  have a proper, written authorization of INTERSEC to hold it. If you   */
/*  don't have such an authorization, you must DELETE all source code     */
/*  files in your possession, and inform INTERSEC of the fact you obtain  */
/*  these files. Should you not comply to these terms, you can be         */
/*  prosecuted in the extent permitted by applicable law.                 */
/*                                                                        */
/**************************************************************************/

#if !defined(IS_LIB_COMMON_CORE_H) || defined(IS_LIB_COMMON_CORE_PTHREAD_H)
#  error "you must include core.h instead"
#else
#define IS_LIB_COMMON_CORE_PTHREAD_H

void intersec_phtread_init(void (*fn)(void));
void intersec_phtread_exit(void (*fn)(void));

#define thread_init(fn) \
    static __attribute__((constructor)) void PT_##fn##_init(void) {  \
        intersec_phtread_init(fn);                                   \
    }

#define thread_exit(fn) \
    static __attribute__((constructor)) void PT_##fn##_exit(void) {  \
        intersec_phtread_exit(fn);                                   \
    }

void intersec_thread_on_init(void);
void intersec_thread_on_exit(void *);

#endif
