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

#if !defined(IS_LIB_COMMON_STR_H) || defined(IS_LIB_COMMON_STR_PATH_H)
#  error "you must include <lib-common/str.h> instead"
#else
#define IS_LIB_COMMON_STR_PATH_H

/*----- simple file name splits -----*/

ssize_t path_dirpart(char *dir, ssize_t size, const char *filename);

__attribute__((nonnull)) const char *path_filepart(const char *filename);
__attribute__((nonnull)) static inline char *vpath_filepart(char *path) {
    return (char*)path_filepart(path);
}

__attribute__((nonnull)) const char *path_extnul(const char *filename);
__attribute__((nonnull)) static inline char *vpath_extnul(char *path) {
    return (char*)path_extnul(path);
}
const char *path_ext(const char *filename);
static inline char *vpath_ext(char *path) {
    return (char*)path_ext(path);
}

/*----- libgen like functions -----*/

int path_dirname(char *buf, int len, const char *path);
int path_basename(char *buf, int len, const char *path);

/*----- path manipulations -----*/

int path_join(char *buf, int len, const char *path);
int path_simplify2(char *path, bool keep_trailing_slash);
#define path_simplify(path)   path_simplify2(path, false)
int path_canonify(char *buf, int len, const char *path);
char *path_expand(char *buf, int len, const char *path);

bool path_is_safe(const char *path);

#endif /* IS_LIB_COMMON_STR_PATH_H */
