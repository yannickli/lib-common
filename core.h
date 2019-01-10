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

#ifndef IS_LIB_COMMON_CORE_H
#define IS_LIB_COMMON_CORE_H

#define LIB_COMMON_VERSION  "v4"
#define LIB_COMMON_MAJOR    4
#define LIB_COMMON_MINOR    0
#define __LIB_COMMON_VER(Maj, Min)  (((Maj) << 16) | Min)

#define LIB_COMMON_PREREQ(Maj, Min)                                          \
    (__LIB_COMMON_VER(Maj, Min) <= __LIB_COMMON_VER(LIB_COMMON_MAJOR,        \
                                                    LIB_COMMON_MINOR))

#include <alloca.h>
#include <assert.h>
#include <ctype.h>
#include <endian.h>
#include <errno.h>
#include <fcntl.h>
#include <glob.h>
#include <inttypes.h>
#include <limits.h>
#include <malloc.h>
#include <signal.h>
#include <stdarg.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
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
#include <time.h>
#include <unistd.h>

#define __ISLIBC__

#define IPRINTF_HIDE_STDIO 1
#include "core-os-features.h"
#include "core-macros.h"
#include "core-test.h"
#include "core-atomic.h"
#include "core-errors.h"
#include "core-mem.h"
#include "core-refcount.h"
#include "core-stdlib.h"
#include "core-obj.h"
#include "str.h"

#endif
