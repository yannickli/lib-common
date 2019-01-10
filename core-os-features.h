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

#if !defined(IS_LIB_COMMON_CORE_H) || defined(IS_LIB_COMMON_CORE_OS_FEATURES_H)
#  error "you must include core.h instead"
#else
#define IS_LIB_COMMON_CORE_OS_FEATURES_H

/*---------------- Guess the OS ----------------*/
#if defined(__linux__)
#  define OS_LINUX
#elif defined(__sun)
#  define OS_SOLARIS
#elif defined(__MINGW) || defined(__MINGW32__)
#  define OS_WINDOWS
#elif defined(__APPLE__)
#  define OS_APPLE
#else
#  error "we don't know about your OS"
#endif

/* <netinet/sctp.h> availability */
#if defined(OS_LINUX) /* || defined(__sun) */
#  define HAVE_NETINET_SCTP_H
#endif

/* <sys/poll.h> availability */
#ifndef OS_WINDOWS
# ifndef HAVE_SYS_POLL_H
#  define HAVE_SYS_POLL_H
# endif
#endif

/* <sys/inotify.h> availability */
#ifdef OS_LINUX
# ifndef HAVE_SYS_INOTIFY_H
#  define HAVE_SYS_INOTIFY_H
# endif
#endif

#ifdef OS_APPLE
# define SO_FILEEXT  ".dylib"
#endif

#ifndef SO_FILEEXT
# define SO_FILEEXT  ".so"
#endif

#endif
