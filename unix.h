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

#ifndef IS_LIB_COMMON_UNIX_H
#define IS_LIB_COMMON_UNIX_H

#include "core.h"

#define O_ISWRITE(m)      (((m) & (O_RDONLY|O_WRONLY|O_RDWR)) != O_RDONLY)

/****************************************************************************/
/* process related                                                          */
/****************************************************************************/

int pid_get_starttime(pid_t pid, struct timeval *tv);

/****************************************************************************/
/* Filesystem related                                                       */
/****************************************************************************/

int mkdir_p(const char *dir, mode_t mode);

int get_mtime(const char *filename, time_t *t);

int filecopy(const char *pathin, const char *pathout);

int p_lockf(int fd, int mode, int cmd, off_t start, off_t len);
int p_unlockf(int fd, off_t start, off_t len);

int tmpfd(void);
void devnull_dup(int fd);

/****************************************************************************/
/* file descriptor related                                                  */
/****************************************************************************/

__attribute__((warn_unused_result))
int xwrite(int fd, const void *data, ssize_t dlen);
__attribute__((warn_unused_result))
int xread(int fd, void *data, ssize_t dlen);
bool is_fd_open(int fd);
/* FIXME: Find a better name. */
int close_fds_higher_than(int fd);
bool is_fancy_fd(int fd);

/****************************************************************************/
/* Misc                                                                     */
/****************************************************************************/

static inline void getopt_init(void) {
    /* XXX this is not portable, BSD want it to be set to -1 *g* */
    optind = 0;
}

void unix_initialize(void);

#endif /* IS_LIB_COMMON_UNIX_H */
