/**************************************************************************/
/*                                                                        */
/*  Copyright (C) 2004-2019 INTERSEC SA                                   */
/*                                                                        */
/*  Should you receive a copy of this source code, you must check you     */
/*  have a proper, written authorization of INTERSEC to hold it. If you   */
/*  don't have such an authorization, you must DELETE all source code     */
/*  files in your possession, and inform INTERSEC of the fact you obtain  */
/*  these files. Should you not comply to these terms, you can be         */
/*  prosecuted in the extent permitted by applicable law.                 */
/*                                                                        */
/**************************************************************************/

#ifndef IS_LIB_COMMON_CORE_H
#define IS_LIB_COMMON_CORE_H

#define LIB_COMMON_VERSION  "2016.3"
#define LIB_COMMON_MAJOR    2016
#define LIB_COMMON_MINOR    3
#define __LIB_COMMON_VER(Maj, Min)  (((Maj) << 16) | Min)

#define LIB_COMMON_PREREQ(Maj, Min)                                          \
    (__LIB_COMMON_VER(Maj, Min) <= __LIB_COMMON_VER(LIB_COMMON_MAJOR,        \
                                                    LIB_COMMON_MINOR))

#if HAS_LIBCOMMON_REPOSITORY
# define LIBCOMMON_PATH "lib-common/"
# if HAS_PLATFORM_REPOSITORY
#   define PLATFORM_PATH "platform/"
# else
#   define PLATFORM_PATH ""
# endif
#else
# define LIBCOMMON_PATH ""
# define PLATFORM_PATH ""
#endif

#include <Block.h>
#include <alloca.h>
#include <assert.h>
#include <ctype.h>
#include <endian.h>
#include <errno.h>
#include <dirent.h>
#include <fcntl.h>
#include <glob.h>
#include <inttypes.h>
#include <limits.h>
#include <signal.h>
#include <stdarg.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <stdatomic.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/mman.h>
#include <sys/param.h>
#include <sys/stat.h>
#include <sys/time.h>
#include <sys/types.h>
#include <sys/user.h>
#include <sys/uio.h>
#ifdef __APPLE__
# include <sys/socket.h>
#endif
#include <time.h>
#include <unistd.h>
#include <sched.h>

#define __ISLIBC__

#define IPRINTF_HIDE_STDIO 1
#include "core-os-features.h"
#include "core-macros.h"
#ifdef __cplusplus
}
#endif
#include "core-macros++.h"
#ifdef __cplusplus
extern "C" {
#endif
#include "core-bithacks.h"
#include "core-blocks.h"
#include "core-errors.h"
#include "core-mem.h"
#include "core-mem-stack.h"
#include "core-types.h"
#include "core-stdlib.h"
#include "core-obj.h"

#include "str-ctype.h"
#include "str-iprintf.h"
#include "str-num.h"
#include "str-conv.h"
#include "str-l.h"
#include "str-buf.h"
#include "str-stream.h"
#include "core-str.h"
#include "core-module.h"

#endif
