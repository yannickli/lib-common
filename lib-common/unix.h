/**************************************************************************/
/*                                                                        */
/*  Copyright (C) 2004-2007 INTERSEC SAS                                  */
/*                                                                        */
/*  Should you receive a copy of this source code, you must check you     */
/*  have a proper, written authorization of INTERSEC to hold it. If you   */
/*  don't have such an authorization, you must DELETE all source code     */
/*  files in your possession, and inform INTERSEC of the fact you obtain  */
/*  these files. Should you not comply to these terms, you can be         */
/*  prosecuted in the extent permitted by applicable law.                 */
/*                                                                        */
/**************************************************************************/

#ifndef IS_LIB_COMMON_UNIX_H
#define IS_LIB_COMMON_UNIX_H

#include <unistd.h> /* for ssize_t */
#include <sys/types.h>
#include <sys/time.h>

/****************************************************************************/
/* process related                                                          */
/****************************************************************************/

int pid_get_starttime(pid_t pid, struct timeval *tv);

/****************************************************************************/
/* Filesystem related                                                       */
/****************************************************************************/

int mkdir_p(const char *dir, mode_t mode);

const char *get_basename(const char *filename);
ssize_t get_dirname(char *dir, ssize_t size, const char *filename);
const char *get_ext(const char *filename);

static inline char *vget_basename(char *path) {
    return (char*)get_basename(path);
}

static inline char *vget_ext(char *path) {
    return (char*)get_ext(path);
}


/****************************************************************************/
/* Misc                                                                     */
/****************************************************************************/

static inline void getopt_init(void) {
    /* XXX this is not portable, BSD want it to be set to -1 *g* */
    optind = 0;
}

void unix_initialize(void);

#endif /* IS_LIB_COMMON_UNIX_H */
