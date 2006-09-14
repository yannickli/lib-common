/**************************************************************************/
/*                                                                        */
/*  Copyright (C) 2004-2006 INTERSEC                                      */
/*                                                                        */
/*  Should you receive a copy of this source code, you must check you     */
/*  have a proper, written authorization of INTERSEC to hold it. If you   */
/*  don't have such an authorization, you must DELETE all source code     */
/*  files in your possession, and inform INTERSEC of the fact you obtain  */
/*  these files. Should you not comply to these terms, you can be         */
/*  prosecuted in the extent permitted by applicable law.                 */
/*                                                                        */
/**************************************************************************/

#ifndef IS_UNIX_H
#define IS_UNIX_H
#include <unistd.h> /* for ssize_t */
#include <sys/types.h>
int mkdir_p(const char *dir, mode_t mode);

const char *get_basename(const char *filename);
int get_dirname(char *dir, ssize_t size, const char *filename);
const char *get_ext(const char *filename);

static inline char *vget_basename(char *path) {
    return (char*)get_basename(path);
}
static inline char *vget_ext(char *path) {
    return (char*)get_ext(path);
}
#endif
