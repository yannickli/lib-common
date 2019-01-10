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

#if !defined(IS_LIB_COMMON_STR_H) || defined(IS_LIB_COMMON_STR_NUM_H)
#  error "you must include str.h instead"
#else
#define IS_LIB_COMMON_STR_NUM_H

/* Wrappers to fix constness issue in strtol() */
__attr_nonnull__((1))
static inline unsigned long cstrtoul(const char *str, const char **endp, int base) {
    return (strtoul)(str, (char **)endp, base);
}

__attr_nonnull__((1))
static inline unsigned long vstrtoul(char *str, char **endp, int base) {
    return (strtoul)(str, endp, base);
}
#define strtoul(str, endp, base)  cstrtoul(str, endp, base)

__attr_nonnull__((1))
static inline long cstrtol(const char *str, const char **endp, int base) {
    return (strtol)(str, (char **)endp, base);
}

__attr_nonnull__((1))
static inline long vstrtol(char *str, char **endp, int base) {
    return (strtol)(str, endp, base);
}
#define strtol(str, endp, base)  cstrtol(str, endp, base)

__attr_nonnull__((1))
static inline long long cstrtoll(const char *str, const char **endp, int base) {
    return (strtoll)(str, (char **)endp, base);
}
__attr_nonnull__((1))
static inline long long vstrtoll(char *str, char **endp, int base) {
    return (strtoll)(str, endp, base);
}
#define strtoll(str, endp, base)  cstrtoll(str, endp, base)

__attr_nonnull__((1))
static inline unsigned long long
cstrtoull(const char *str, const char **endp, int base) {
    return (strtoull)(str, (char **)endp, base);
}
__attr_nonnull__((1))
static inline unsigned long long
vstrtoull(char *str, char **endp, int base) {
    return (strtoull)(str, endp, base);
}
#define strtoull(str, endp, base)  cstrtoull(str, endp, base)

int strtoip(const char *p, const char **endp)
    __leaf __attr_nonnull__((1));
static inline int vstrtoip(char *p, char **endp) {
    return strtoip(p, (const char **)endp);
}
int memtoip(const void *p, int len, const byte **endp)
    __leaf __attr_nonnull__((1));
int64_t memtollp(const void *s, int len, const byte **endp)
    __leaf __attr_nonnull__((1));
int64_t parse_number(const char *str) __leaf;

#define STRTOLP_IGNORE_SPACES  (1 << 0)
#define STRTOLP_CHECK_END      (1 << 1)
#define STRTOLP_EMPTY_OK       (1 << 2)
#define STRTOLP_CHECK_RANGE    (1 << 3)
#define STRTOLP_CLAMP_RANGE    (1 << 4)
/* returns 0 if success, negative errno if error */
int strtolp(const char *p, const char **endp, int base, long *res,
            int flags, long min, long max) __leaf;

#endif
