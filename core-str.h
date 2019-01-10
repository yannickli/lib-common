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

#if !defined(IS_LIB_COMMON_CORE_H) || defined(IS_LIB_COMMON_CORE_STR__H)
#  error "you must include core.h instead"
#else
#define IS_LIB_COMMON_CORE_STR_H

/* Simple helpers {{{ */

const char *skipspaces(const char *s)  __attr_nonnull__((1));
__attr_nonnull__((1))
static inline char *vskipspaces(char *s) {
    return (char*)skipspaces(s);
}

const char *strnextspace(const char *s)  __attr_nonnull__((1));
__attr_nonnull__((1))
static inline char *vstrnextspace(char *s) {
    return (char*)strnextspace(s);
}

const char *skipblanks(const char *s)  __attr_nonnull__((1));
__attr_nonnull__((1))
static inline char *vskipblanks(char *s) {
    return (char*)skipblanks(s);
}

/* Trim spaces at end of string, return pointer to '\0' */
char *strrtrim(char *str);

int strstart(const char *str, const char *p, const char **pp);
static inline int vstrstart(char *str, const char *p, char **pp) {
    return strstart(str, p, (const char **)pp);
}

int stristart(const char *str, const char *p, const char **pp);
static inline int vstristart(char *str, const char *p, char **pp) {
    return stristart(str, p, (const char **)pp);
}

const char *stristrn(const char *haystack, const char *needle, size_t nlen)
          __attr_nonnull__((1, 2));

__attr_nonnull__((1, 2))
static inline char *
vstristrn(char *haystack, const char *needle, size_t nlen) {
    return (char *)stristrn(haystack, needle, nlen);
}

__attr_nonnull__((1, 2))
static inline const char *
stristr(const char *haystack, const char *needle) {
    return stristrn(haystack, needle, strlen(needle));
}

__attr_nonnull__((1, 2))
static inline char *vstristr(char *haystack, const char *needle) {
    return (char *)stristr(haystack, needle);
}

/* Implement inline using strcmp, unless strcmp is hopelessly fucked up */
__attr_nonnull__((1, 2))
static inline bool strequal(const char *str1, const char *str2) {
    return !strcmp(str1, str2);
}

/* find a word in a list of words separated by sep.
 */
bool strfind(const char *keytable, const char *str, int sep);

int buffer_increment(char *buf, int len);
int buffer_increment_hex(char *buf, int len);
ssize_t pstrrand(char *dest, ssize_t size, int offset, ssize_t len);
size_t strrand(char dest[], size_t dest_size, lstr_t alphabet);

/* Return the number of occurences replaced */
/* OG: need more general API */
int str_replace(const char search, const char replace, char *subject);

/* }}} */
/* Path helpers {{{ */

/*----- simple file name splits -----*/

ssize_t path_dirpart(char *dir, ssize_t size, const char *filename)
    __leaf;

__attr_nonnull__() const char *path_filepart(const char *filename);
__attr_nonnull__() static inline char *vpath_filepart(char *path)
{
    return (char*)path_filepart(path);
}

__attr_nonnull__() const char *path_extnul(const char *filename);
__attr_nonnull__() static inline char *vpath_extnul(char *path)
{
    return (char*)path_extnul(path);
}
const char *path_ext(const char *filename);
static inline char *vpath_ext(char *path)
{
    return (char*)path_ext(path);
}

/*----- libgen like functions -----*/

int path_dirname(char *buf, int len, const char *path) __leaf;
int path_basename(char *buf, int len, const char *path) __leaf;

/*----- path manipulations -----*/

int path_join(char *buf, int len, const char *path) __leaf;
int path_simplify2(char *path, bool keep_trailing_slash) __leaf;
#define path_simplify(path)   path_simplify2(path, false)
int path_canonify(char *buf, int len, const char *path) __leaf;
char *path_expand(char *buf, int len, const char *path) __leaf;

bool path_is_safe(const char *path) __leaf;

#ifndef __cplusplus

/** Extend a relative path from a prefix.
 *
 *      prefix + fmt = prefix + / + fmt
 *
 * A '/' will be added between /p prefix and /p fmt if necessary.
 *
 * If /p fmt begins with a '/', the prefix will be ignored.
 * If /p fmt begins with '~/', the prefix will be ignored, and ~ replaced by
 * the HOME prefix of the current user.
 *
 * \param[out] buf    buffer where the path will be written.
 * \param[in]  prefix
 * \param[in]  fmt    format of the suffix
 * \param[in]  args   va_list containing the arguments for the \p fmt
 *                    formatting
 *
 * \return -1 if the path overflows the buffer,
 *            length of the resulting path otherwise.
 */
__attribute__((format(printf, 3, 0)))
int path_va_extend(char buf[static PATH_MAX], const char *prefix,
                   const char *fmt, va_list args);
__attribute__((format(printf, 3, 4)))
int path_extend(char buf[static PATH_MAX], const char *prefix,
                const char *fmt, ...);

/** Create a relative path from one path to another path.
 *
 * \p from and \p to can be absolute or relative paths from the current
 * working directory.
 * If \p from ends with a '/', it is treated as a directory and the created
 * relative path startpoint is within the directory.
 *
 * Examples:
 *  - The relative path from "/a/b/c/d" to "/a/b/e/f" is "../e/f".
 *  - The relative path from "/a/b/c/d/" to "/a/b/e/f" is "../../e/f".
 *  - The relative path from "a/b/c" to "d/e/" is "../../d/e".
 *  - The relative path from "a/b/c/" to "a/b/c" is "c".
 *  - The relative path from "/a/b/c" to "d/e" is "../d/e" with the current
 *    working directory set to "/a".
 *
 * \param[out] buf  buffer where the path will be written.
 * \param[in]  len  length of the buffer.
 * \param[in]  from the path that defines the startpoint of the relative path.
 * \param[in]  to   the path that defines the endpoint of the relative path.
 *
 * \return -1 if the length of the absolute path of \p from or \p to, or the
 *         length of the resulting path exceed PATH_MAX.
 *         Length of the resulting path otherwise.
 */
int path_relative_to(char buf[static PATH_MAX], const char *from,
                     const char *to);

#endif

/* }}} */

#endif /* IS_LIB_COMMON_STR_IS_H */
